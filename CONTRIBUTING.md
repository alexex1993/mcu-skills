# Adding a board skill

The pipeline. Six stages, in order — the order matters, because stages 4–6 are only worth
anything if stages 1–3 happened on real hardware.

Expect a first skill to take a weekend. Most of that is stage 3, and none of it is wasted:
the skill is a by-product of getting the board working, not extra work bolted on afterwards.

```
1 spec  →  2 hello world  →  3 bring-up  →  4 extract  →  5 test cold  →  6 PR
   ↑                                            │
   └──────────── whatever failed ───────────────┘
```

---

## 0. Claim the board

Open an issue titled `board: <vendor> <board>` before you start, so two people don't write the
same skill. Say which MCU, which toolchain, and which peripherals you can actually test —
"I have the board, no camera module" is useful information.

Pick the skill name now: lowercase, hyphenated, `<mcu>-<board>` or `<vendor>-<board>`, e.g.
`stm32h750-weact`, `esp32s3-devkitc`, `rp2040-pico-w`. It becomes the directory name, the
frontmatter `name:`, and the `/slash-command`. It must be unique across the whole repo.

## 1. Collect the specification

Get these before writing a line of code, and keep them open:

- **MCU datasheet** — pinout, packages, electrical limits.
- **MCU reference manual** — the peripheral behaviour. This is the one that answers "why does
  this bit do nothing".
- **Board schematic** — the only source for what the board maker actually wired. Vendor
  marketing pages lie by omission; schematics do not.
- **Vendor SDK / BSP / examples** for that exact board, if any exist.
- **Errata sheet.** Skipped by everyone, and it is where the silent failures live.

Dump the findings into a scratch file as you go — a pin table, a power tree, a memory map, a
clock tree. That scratch file becomes `reference/board-hardware.md` in stage 4, so write it as
if someone else will read it.

> Claude is good at this stage: point it at the PDFs and ask for a pin-function table or a
> memory map. Verify every number it produces against the schematic before you trust it —
> a wrong pin in a reference file is worse than a missing one.

## 2. Hello world

The smallest thing that proves the whole chain: toolchain → build → flash → the chip runs your
code. Blink an LED. Nothing else — no display, no RTOS, no USB.

Do not move on until you can state, with the board on your desk:

- [ ] the exact toolchain and version (`platformio.ini`, `sdkconfig`, `Makefile`, …)
- [ ] the clock configuration it boots with, and where that is set
- [ ] the flashing procedure, keystroke by keystroke, including any BOOT/RESET dance
- [ ] how you recover a bricked board
- [ ] how you get `printf` (or equivalent) out of it

Those five bullets are half the value of the finished skill. A model that can flash and read
output can debug its own mistakes; one that can't is stuck.

## 3. Bring-up, one peripheral at a time

Grow hello world into a real project — display, storage, USB, ADC, whatever the board is for.
One peripheral per commit, each one verified on hardware.

Keep a running log of every trap you hit. Literally every one. The format that matters:

```
symptom  →  cause  →  fix
```

"Board runs but every timing is 3× off → HSE_VALUE defaulted to 8 MHz, crystal is 25 MHz →
`-DHSE_VALUE=25000000` in build_flags."

That log is the highest-value part of the skill, because each entry is a failure that *looks
like something else*. Anything you found by reading a forum post at 2 a.m. goes in it.

What ends up as this stage's output is a project that builds clean and runs — that is the
`template/` directory. Consider shipping two variants: a minimal one (LED only, proves the
toolchain) and a full one (everything working).

## 4. Extract the skill

Start from the skeleton:

```sh
cp -R templates/skill-template skills/<family>/<skill-name>
```

Three parts, and the split is what keeps the skill usable:

| Part | Holds | Budget |
|---|---|---|
| `SKILL.md` | frontmatter, orientation table, the rules that prevent expensive mistakes, pointers | **under ~500 lines** |
| `reference/*.md` | the full pin map, memory map, peripheral cookbook, pitfall table, copy-paste recipes | as long as it needs to be |
| `template/` | the working project from stage 3, plus a scaffold script | — |

`SKILL.md` is loaded into context whenever the skill triggers; `reference/` is read only when
needed. So `SKILL.md` gets what must be true *before the model writes any code*, and everything
else gets a pointer. [docs/AUTHORING.md](docs/AUTHORING.md) covers how to write each part, and
how to write a `description:` that actually triggers.

Turn the stage-3 log into the "rules" section: each trap becomes one imperative line with the
consequence attached. Not "consider setting HSE_VALUE" — "**`-DHSE_VALUE=25000000`** in
`build_flags`. Without it the PLL still locks but every derived timing is wrong by ~3×."

> Anthropic ships a **`skill-creator`** skill (in the public
> [anthropics/skills](https://github.com/anthropics/skills) repo) that interviews you and
> scaffolds a skill directory. It is a decent way to get the shape right if this is your first
> one — install it alongside this repo's skills and run `/skill-creator`. It knows nothing
> about MCUs, so the content is still yours to write; use it for structure, use
> [docs/AUTHORING.md](docs/AUTHORING.md) for substance.

## 5. Test it cold

A skill that reads well and does not work is the normal failure mode. Test it the way it will
actually be used:

```sh
./scripts/install.sh <skill-name>
./scripts/validate.sh <skill-name>
```

Then open a **fresh Claude Code session in an empty directory** — no context, no hints — and
run these tasks. Each one must succeed without you correcting the model:

1. "Set up a new project for `<board>` that blinks the LED." → builds, flashes, blinks.
2. "Add `<the board's headline peripheral>`." → works on first flash, or fails in a way the
   skill's own reference explains.
3. Ask it something the skill deliberately warns about. It must apply the warning unprompted.
4. Describe a symptom from your stage-3 log without naming the cause. It should name the cause.

Whatever it gets wrong is a gap in the skill, not a gap in the model. Go fix the skill and
re-run cold. Repeat until all four pass.

Check the trigger too: with the skill installed but not mentioned, does a plain
"help me with my `<board>` project" load it? If not, the `description:` is too narrow.

## 6. Pull request

```
skills/<family>/<skill-name>/
```

One skill per PR. In the description, state:

- board, MCU, toolchain and version you used
- **what you verified on hardware vs. what came from the datasheet** — mark it in the skill
  itself too, honestly; a documented "untested" is fine, an undocumented guess is not
- which peripherals you could not test and why
- the results of the four stage-5 tasks

`./scripts/validate.sh --all` must pass. Someone with the same board reviews against
[docs/CHECKLIST.md](docs/CHECKLIST.md); if nobody has one, it merges on the strength of your
hardware notes.

Then add your row to the table in [README.md](README.md).

## Improving an existing skill

Small fixes go straight to a PR. If you are changing a rule or a pin, say how you verified it —
same bar as a new skill. Contradicting an existing claim is fine and welcome; the previous
author may have had a different board revision, so note the revision rather than deleting
their line.
