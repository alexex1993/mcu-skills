# mcu-skills

Claude Code [Agent Skills](https://docs.claude.com/en/docs/claude-code/skills) for embedded
development — one skill per board, holding the board-specific knowledge that is otherwise
scattered across a datasheet, a reference manual, a schematic and a weekend of debugging.

A good MCU skill answers, without the model guessing: which pin does what, which clock tree
actually works, which HAL call silently does nothing, and how to get a build onto the chip.

## Skills in this repo

| Skill | Board | MCU | Toolchain |
|---|---|---|---|
| [`stm32h750-weact`](skills/stm32/stm32h750-weact) | WeAct Studio MiniSTM32H7xx core board | STM32H750VBT6 (Cortex-M7) | PlatformIO + STM32Cube HAL |
| [`esp32c6-lcd147`](skills/esp32/esp32c6-lcd147) | Waveshare ESP32-C6-LCD-1.47 | ESP32-C6FH4 (RISC-V) | PlatformIO + ESP-IDF |
| [`atmega328p-nano`](skills/avr/atmega328p-nano) | Arduino Nano (A000005) | ATmega328P (AVR 8-bit) | PlatformIO + Arduino core |

## Install

```sh
git clone https://github.com/alexex1993/mcu-skills.git
cd mcu-skills
./scripts/install.sh stm32h750-weact          # symlink into ~/.claude/skills
./scripts/install.sh stm32h750-weact --copy   # or copy, if you want to edit locally
./scripts/install.sh --list
```

Then in Claude Code the skill loads by itself when you work on that board, or on demand:

```
/stm32h750-weact
```

Project-scoped install instead of user-scoped: copy the skill directory to `.claude/skills/`
inside your firmware project.

## Layout

```
skills/<family>/<skill-name>/     one skill, ready to copy into ~/.claude/skills
  SKILL.md                        frontmatter + the load-bearing knowledge
  reference/                      deep detail, read on demand
  template/                       a project that actually builds
docs/                             how to author and review a skill
templates/skill-template/         blank skeleton to start from
scripts/                          install.sh, validate.sh
```

Families: `stm32`, `esp32`, `rp2`, `nrf`, `avr`, `ch32`, `renesas`, `nxp`, `ti`. Add a new
directory if yours does not fit — one level, family name only, no vendor nesting.

Skill directory name == the `name:` in its frontmatter, and it must be unique across the whole
repo: installed skills all land in one flat `~/.claude/skills/` namespace.

## Contributing

The short version: find the spec, build a hello world that runs on real hardware, *then*
write the skill from what you learned. Full pipeline in **[CONTRIBUTING.md](CONTRIBUTING.md)**,
authoring detail in [docs/AUTHORING.md](docs/AUTHORING.md), review bar in
[docs/CHECKLIST.md](docs/CHECKLIST.md).

Rule of thumb for what belongs here: if the answer is in the vendor's getting-started page and
the model gets it right without help, leave it out. Everything that cost you an evening goes in.

## License

MIT — see [LICENSE](LICENSE). Vendored third-party code inside a skill's `template/lib/` keeps
its own license; note it in that skill's README.
