# Review checklist

For the reviewer of a new-board PR, and for the author before opening it. Ideally the reviewer
has the same board; if not, the hardware-verification notes carry the PR.

## Mechanics

- [ ] `./scripts/validate.sh --all` passes
- [ ] directory is `skills/<family>/<skill-name>/`, and `<skill-name>` == frontmatter `name`
- [ ] the name is unique across the repo (all skills share one flat namespace once installed)
- [ ] one skill per PR
- [ ] README table has a new row
- [ ] no absolute paths (`/Users/…`, `/home/…`, `C:\…`), no `.DS_Store`, no build output
      (`.pio/`, `build/`, `*.elf`, `*.bin`), no IDE directories
- [ ] vendored third-party code keeps its license header and is listed in `template/README.md`

## description

- [ ] names the board, the MCU part number, the toolchain, and the main peripherals
- [ ] contains the strings a user would actually type, including chip number and siblings
- [ ] says *when to use it*, in task terms, and includes the debugging case
- [ ] with the skill installed and unmentioned, "help me with my `<board>` project" loads it

## SKILL.md

- [ ] under ~500 lines; the bulk of the detail lives in `reference/`
- [ ] orientation table: MCU, core, flash/RAM, crystals, working clock tree, key pins **with
      polarity**, storage, USB, debug
- [ ] a rules section, and every rule states its consequence, not just the instruction
- [ ] the rules are non-obvious — nothing that generic knowledge of that MCU family already covers
- [ ] every `reference/` file is named, with what it answers
- [ ] project setup points at `template/`, with variants and their flash cost
- [ ] flashing: full keystroke sequence, what enumerates, how to recover a wedged board
- [ ] settings that are outside published specs are labelled as such

## reference/

- [ ] pin map is schematic-derived, complete, and marks shared/conflicting pins
- [ ] memory map includes external flash/PSRAM and XIP addresses where applicable
- [ ] a symptom → cause → fix table exists
- [ ] recipes are extracted from `template/`, not retyped — spot-check two against the source
- [ ] core-specific hazards covered (cache vs DMA, alignment, TCM reachability, boot modes)

## template/

- [ ] builds clean from a fresh clone, unmodified
- [ ] the stated flash/RAM figures match what the build actually reports
- [ ] `variants/new-project.sh <dir>` produces a tree that builds
- [ ] copying the tree by hand works identically (nothing generated, nothing embedded)
- [ ] `template/README.md` maps files to subsystems

## Hardware verification

- [ ] the PR says what was verified on hardware and what came from the datasheet
- [ ] the skill itself says the same, per claim where it matters
- [ ] untestable peripherals are named, with the reason
- [ ] the four cold-start tasks from [CONTRIBUTING.md §5](../CONTRIBUTING.md#5-test-it-cold) pass,
      and the PR reports their results

## Reviewer's own pass

If you have the board: run task 1 and task 2 cold yourself. If you don't: read the rules section
and ask, for each rule, *what wrong action does this prevent?* Any rule with no answer should
come out.
