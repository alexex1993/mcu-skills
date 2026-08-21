# Authoring an MCU skill

How to write the three parts. [CONTRIBUTING.md](../CONTRIBUTING.md) covers the process around
them; this is the content.

The governing idea: a skill is not documentation. Documentation describes a board. A skill
changes what the model does before it writes a line of code. Every line should earn its place by
preventing a specific wrong action.

---

## Anatomy

```
skills/<family>/<skill-name>/
├── SKILL.md                       always loaded when the skill triggers  → keep it tight
├── reference/
│   ├── board-hardware.md          pin map, power tree, memory map, peripheral cookbook
│   └── recipes.md                 copy-paste-ready code that is known to build
└── template/                      a project that compiles and runs, as-is
    ├── platformio.ini | CMakeLists.txt | sdkconfig …
    ├── src/ include/ lib/
    ├── variants/new-project.sh    scaffold script
    └── README.md                  which file belongs to which subsystem
```

`reference/` filenames are a convention, not a rule — split by what a reader would go looking
for. Two or three files beat one 3000-line file and beat fifteen small ones.

---

## SKILL.md

### Frontmatter

```yaml
---
name: stm32h750-weact
description: Firmware development for the WeAct Studio MiniSTM32H7xx core board (STM32H750VBT6)
  — its 0.96" ST7735 TFT, DVP camera, QSPI/SPI flash, MicroSD, USB, and the STM32Cube HAL +
  PlatformIO setup around them. Use when working on this board or any STM32H750/H743 WeAct core
  board: project setup, platformio.ini, clock/PLL configuration, pin mapping, peripheral
  bring-up, LCD performance, D-cache/DMA problems, DFU flashing, or debugging why something on
  the board does not work.
---
```

`name` — lowercase, digits and hyphens, matches the directory name.

`description` — **the single highest-leverage field in the whole skill.** It is all the model
sees when deciding whether to load anything else. Two halves:

1. *What this covers* — board name, MCU part number, the peripherals, the toolchain. Include
   every string a user might type: the marketing name, the chip number, common misspellings,
   compatible siblings ("or any STM32H750/H743 WeAct core board").
2. *When to use it* — concrete task shapes, in the user's words, including debugging ones.
   "debugging why something on the board does not work" catches the half of real requests that
   never name a peripheral.

Failure mode to avoid: `description: STM32H750 board skill`. It triggers on almost nothing.

### Body

Aim for under ~500 lines. Suggested shape, in this order:

**1. Orientation table.** The facts needed to reason about anything else — MCU, core, flash and
RAM sizes, crystal frequencies, the working clock tree, display, LED and button pins with their
*polarity*, storage, USB, debug header.

**2. Rules that prevent the expensive mistakes.** The heart of the skill: your stage-3 trap log,
numbered, imperative, each with the consequence attached. The test for a good rule is that the
consequence is *not what you would expect from the symptom*:

> **`PWR_LDO_SUPPLY`** — this board has no SMPS. Configuring one leaves the core under-supplied
> and unresponsive to SWD until a BOOT0 power cycle. The setting latches on first write after
> reset.

Ten to fifteen of these. If you have forty, the weakest thirty belong in a pitfall table in
`reference/`.

**3. One or two "when the task is X" sections** for the things this board is actually used for,
where the naive approach is badly wrong. Give the mechanism and the number, not just the advice —
"~200× slower than the bus allows … 1660 SPI transactions for a 13-character line" tells the
model *why*, so it can generalise to a case you didn't cover.

**4. Starting a new project.** Point at `template/` and the scaffold script. Say what each
variant contains and how much flash it costs. Explicitly: *do not hand-assemble a project.*

**5. Flashing.** The full keystroke sequence, what enumerates, how to recover. Note anything the
board will not do for itself ("the board never resets itself into DFU; the sequence is manual
every time").

**6. Reporting.** What the model should tell the user about confidence — specifically, which of
your settings are outside published specs and merely work on your board.

### Style

- Imperative. "Enable the cache before `HAL_Init()`", not "it is recommended to…".
- Every claim carries its consequence. A rule without a symptom gets ignored under pressure.
- Numbers, pin names and register names in full. No "the appropriate GPIO".
- Tables for facts, prose for mechanisms, code only where the exact bytes matter — otherwise
  point to `reference/recipes.md`.
- No changelog, no "TODO", no apologies for what is missing. Say "untested" and move on.
- Nothing that is true of every board of that family and that the model already knows. If GPT-
  grade general STM32 knowledge covers it, cut it.

---

## reference/

Read on demand, so length is cheap here — but only if `SKILL.md` says clearly *which file
answers which question*. An unreferenced reference file is never opened.

**`board-hardware.md`** — everything physical, in the order someone debugging would want it:

- schematic-level pin map: every pin, its function, its polarity, whether it is shared
- connectors and headers with pinouts
- power tree and the supply mode the MCU must be configured for
- memory map, including external flash/PSRAM and their XIP addresses
- clock sources: crystal values, which are populated on which board revision
- inventory of vendor SDK/examples, with paths and what is wrong with each
- a development guide section: toolchain config, clock setup, peripheral cookbook, core-specific
  gotchas (cache/DMA/alignment), flashing, and a symptom → cause → fix pitfall table

**`recipes.md`** — code that is known to build, extracted from `template/`, not retyped. Config
file, clock init, each peripheral's init + working use, and any performance-critical path in the
form the model should copy. Every recipe self-contained enough to paste.

If the board has a big optional subsystem (camera, radio, motor driver), give it its own file.

---

## template/

The most important thing in the skill after the rules, because a scaffold that builds beats any
amount of prose about how to build one.

Requirements:

- **It builds and runs, unmodified, on a clean machine.** Verify by cloning to a fresh directory
  and building with nothing else installed.
- **No absolute paths, no machine-specific settings** anywhere. `validate.sh` checks this.
- **Two variants** where it makes sense: minimal (LED only — proves the toolchain and the
  flashing route in isolation) and full (the board's real capabilities). State the flash cost of
  each in `SKILL.md`.
- **A scaffold script** `variants/new-project.sh <target-dir> [--minimal|--full]` that copies the
  tree. Nothing generated, nothing templated — copying by hand must work identically.
- **`template/README.md`** mapping files to subsystems, so a full scaffold can be stripped back
  cleanly.
- Vendored third-party drivers keep their license headers, and `template/README.md` lists them.

---

## Testing what you wrote

Covered in [CONTRIBUTING.md §5](../CONTRIBUTING.md#5-test-it-cold) — the short form is: fresh
session, empty directory, four tasks, no hints from you. Also worth doing once the skill is
stable: `/skill-doctor` (built into Claude Code) reports structural problems and weak
descriptions.

The bar to hold yourself to: **a competent embedded engineer who has never seen this board
should be able to get a peripheral working from the skill alone, on the first flash.**
