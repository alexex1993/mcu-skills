---
name: REPLACE-with-skill-name
description: >-
  Firmware development for the <VENDOR> <BOARD NAME> (<MCU PART NUMBER>) — its <peripheral>,
  <peripheral>, <peripheral>, and the <SDK/HAL> + <build system> setup around them. Use when
  working on this board or any <compatible siblings>: project setup, <config file>, clock
  configuration, pin mapping, peripheral bring-up, <the board's headline concern>, flashing, or
  debugging why something on the board does not work.
---

# <Vendor> <Board> (<MCU>)

<One paragraph: what this board is, and what the reference files hold. Say explicitly that the
detail lives in the reference files and should be read rather than guessed at.>

- `reference/board-hardware.md` — <what it answers>
- `reference/recipes.md` — <what it answers>
- `template/` — a complete working project that builds and flashes, plus a scaffolding script.

## Orientation

| | |
|---|---|
| MCU | <part number, core, flash, RAM, package> |
| Clocks | <crystals populated, and the clock tree that is known to work> |
| Display | <controller, resolution, bus, pins> |
| LED / button | <pin and **polarity** for each> |
| Storage | <internal / external flash, SD, PSRAM, with addresses> |
| USB | <peripheral, pins, bootloader behaviour> |
| Debug | <SWD/JTAG/UART header and pins> |

<Delete rows that don't apply; add rows the board needs.>

## Rules that prevent the expensive mistakes

<10–15 numbered rules from your bring-up log. Each is imperative and carries its consequence.
Template for one entry:>

1. **`<the setting, in code form>`** — <why the board needs it>. Without it, <the symptom, which
   should not obviously point back at this setting>.

## When the task is <the thing this board is used for>

<The one or two areas where the naive approach is badly wrong. Give the mechanism and the
numbers, so the model can generalise past the case you wrote down. Delete if the board has
none.>

## Starting a new project

Do not hand-assemble one. `template/` is a verified-working project — scaffold from it:

```sh
~/.claude/skills/<skill-name>/template/variants/new-project.sh <target-dir> [--full|--minimal]
cd <target-dir> && <build command>
```

- `--full` (default) — <what it contains>. <flash cost>.
- `--minimal` — <what it contains>. <flash cost>. Use it to prove the toolchain and the flashing
  route before adding anything.

When the user already has a project, prefer bringing it in line with the template's
`<config file>` over rewriting their code.

## Flashing

<The full keystroke-level sequence. What enumerates as what. What the board will not do for
itself. How to recover a bricked board. The alternative route (probe/SWD) and when it's needed.>

## Reporting

State honestly what was verified on hardware versus derived from the datasheet. <Name the
specific settings here that are outside published specs and merely work on this board.>
