# Описание на русском языке
Для рускоязычных пользователей есть статья на [Хабре](https://habr.com/ru/articles/1076082/) по поводу этого репозитория и скиллов к микроконтроллерам 

# mcu-skills

[Agent Skills](https://agentskills.io/specification) for embedded development — one skill per
board, holding the board-specific knowledge that is otherwise scattered across a datasheet,
a reference manual, a schematic and a weekend of debugging.

Every skill is a directory with a `SKILL.md` in the open Agent Skills format, so they work in
any agent that supports it — Claude Code, ZCode, OpenCode and the rest — not just one vendor's
tool.

A good MCU skill answers, without the model guessing: which pin does what, which clock tree
actually works, which HAL call silently does nothing, and how to get a build onto the chip.

## Skills in this repo

| Skill | Board | MCU | Image |
|---|---|---|---|
| [`stm32h750-weact`](skills/stm32/stm32h750-weact) | WeAct Studio MiniSTM32H7xx core board | STM32H750VBT6 (Cortex-M7) | <img src="https://github.com/user-attachments/assets/096199da-56a0-4aa2-9711-bba7d59dc035" width="120"> |
| [`stm32f411-blackpill`](skills/stm32/stm32f411-blackpill) | WeAct Studio "Black Pill" V3.x (and clones) | STM32F411CEU6 (Cortex-M4F) | <img src="https://github.com/user-attachments/assets/430196ed-a5b6-4651-b5eb-ee44fcd09235" width="120"> |
| [`esp32c6-lcd147`](skills/esp32/esp32c6-lcd147) | Waveshare ESP32-C6-LCD-1.47 | ESP32-C6FH4 (RISC-V) | <img src="https://github.com/user-attachments/assets/fa79311c-b137-4550-b5be-d545019ebe28" width="120"> |
| [`esp32c3-oled042`](skills/esp32/esp32c3-oled042) | ESP32-C3 0.42" OLED (ABRobot / 01Space) | ESP32-C3FH4 (RISC-V) | <img src="https://github.com/user-attachments/assets/06dbaf31-eb85-431a-8bfb-ae2e00d3e5cd" width="120"> |
| [`esp32-wroom-30pin`](skills/esp32/esp32-wroom-30pin) | 30-pin ESP32 devkit (DOIT V1 / CH340 Type-C) | ESP32-D0WDQ6 (Xtensa LX6) | <img src="https://github.com/user-attachments/assets/57fb4456-8897-47a7-a4ee-1edd831bc15b" width="120"> |
| [`esp32-wroom-36pin`](skills/esp32/esp32-wroom-36pin) | 36-pin ESP32 devkit (original DOIT DevKit V1) | ESP32-D0WDQ6 (Xtensa LX6) |  |
| [`esp32-wroom-38pin`](skills/esp32/esp32-wroom-38pin) | 38-pin ESP32 devkit (DevKitC V4 / NodeMCU-32S) | ESP32-D0WDQ6 (Xtensa LX6) | <img src="https://github.com/user-attachments/assets/b4b6fa6f-41f5-4d30-a3bc-1f3bb096aa9d" width="120"> |
| [`esp32s3-cam-40pin`](skills/esp32/esp32s3-cam-40pin) | 40-pin ESP32-S3 CAM board (Freenove FNK0085 / clones) | ESP32-S3-WROOM-1 N8R8/N16R8 (Xtensa LX7) | <img src="https://github.com/user-attachments/assets/6ce188ac-881b-44df-a7de-2650b5d04740" width="120"> |
| [`esp8266-nodemcu-30pin`](skills/esp8266/esp8266-nodemcu-30pin) | 30-pin NodeMCU devkit (DevKit V1.0 / Amica, LoLin V3) | ESP8266EX (ESP-12E/F, Tensilica L106) | <img src="https://github.com/user-attachments/assets/8b4c1691-a1ab-4c1f-9c16-42703e76c160" width="120"> |
| [`nrf52840-promicro`](skills/nrf/nrf52840-promicro) | ProMicro nRF52840 V1940 (nice!nano v2 clone / SuperMini) | nRF52840 QIAA (Cortex-M4F) | <img src="https://github.com/user-attachments/assets/eb74eab1-516c-4b16-b564-58ca096222fc" width="120"> |
| [`atmega328p-nano`](skills/avr/atmega328p-nano) | Arduino Nano (A000005) | ATmega328P (AVR 8-bit) | <img src="https://github.com/user-attachments/assets/e37f4c59-3746-4603-9f78-65207a957386" width="120"> |
| [`atmega32u4-beetle`](skills/avr/atmega32u4-beetle) | Beetle / CJMCU "Mini Arduino Leonardo" | ATmega32U4 (AVR 8-bit, native USB) | <img src="https://github.com/user-attachments/assets/35a02767-cf7f-43d3-9ee7-3ae55491f574" width="120"> |
| [`lgt8f328p-minievb`](skills/avr/lgt8f328p-minievb) | LGT8F328P-LQFP32 MiniEVB, Nano-style 30-pin (silkscreen `LGTBF32BP`) | LGT8F328P (Logic Green LGT8XM, AVR-compatible) | <img src="https://github.com/user-attachments/assets/88178e6d-4657-428b-af61-8f85fe6579d0" width="120"> |
| [`rp2040-pico`](skills/rp2/rp2040-pico) | Raspberry Pi Pico (SC0915, and Pico H) | RP2040 (2× Cortex-M0+) | <img src="https://github.com/user-attachments/assets/30a66b6a-8052-4b60-8900-98b393099929" width="120"> |
| [`rp2350a-weact`](skills/rp2/rp2350a-weact) | WeAct Studio RP2350A Core Board (V1.0 and V2.0) | RP2350A (2× Cortex-M33) | <img src="https://github.com/user-attachments/assets/b3b84cc3-8e8a-45be-bb49-882ce5f7499c" width="120"> |

`esp8266-nodemcu-30pin` covers both 30-pin NodeMCU revisions — they share the module and
the pin map, and differ only in USB bridge (CP2102 vs CH340G), board width and two pads.
Its §"Confirm the board first" has that table.

`esp32s3-cam-40pin` covers the 40-pin ESP32-S3 camera board sold under a dozen names — Freenove
FNK0085, "ESP32-S3 CAM", "ESP32-S3-WROOM N16R8 CAM" — which all copy one header and one camera
pin map. It is not the 44-pin DevKitC-1, the XIAO S3 Sense or an ESP32-S3-CAM-LCD board; those use
incompatible camera pins. Its §"Confirm the board first" has the decision table.

`lgt8f328p-minievb` sits in the `avr` family because the toolchain is avr-gcc and the pin
map is a Nano's, but the LGT8F328P is not an ATmega328P: it is a Logic Green LGT8XM core that
executes the AVR instruction set inside a different chip. Sketches compile and then behave
differently — the skill's rules section is mostly that list. It covers the LQFP32 board only;
its §"Confirm the board first" has the package/variant table for LQFP48 and SSOP20.

The three ESP32-WROOM-32 skills are deliberately separate: the boards share silicon but not
a header, and the pin map is what a skill is for. Pick by counting pins on one side — 15,
18 or 19. `esp32-wroom-36pin` §"Confirm the board first" has the decision table.

## Install

```sh
git clone https://github.com/alexex1993/mcu-skills.git
cd mcu-skills
./scripts/install.sh stm32h750-weact                    # Claude Code (default)
./scripts/install.sh stm32h750-weact --agent zcode      # ZCode
./scripts/install.sh stm32h750-weact --agent opencode   # OpenCode
./scripts/install.sh stm32h750-weact --copy             # copy, if you want to edit locally
./scripts/install.sh --list
```

Add `--project` to install into the current directory instead of your home
(`./.claude/skills/`, `./.zcode/skills/`, `./.opencode/skills/`). `--dest <dir>` and the
`SKILLS_DIR` env var install anywhere else; uninstall with `--uninstall <skill-name>` plus
the same `--agent`/`--dest` flags.

Where each agent keeps its skills:

| Agent | User-level | Project-level |
|---|---|---|
| Claude Code | `~/.claude/skills/` | `.claude/skills/` |
| ZCode | `~/.zcode/skills/` | `.zcode/skills/` |
| OpenCode | `~/.config/opencode/skills/` | `.opencode/skills/` |

OpenCode also reads `~/.claude/skills/` and `~/.agents/skills/` as-is, so a default install
works there too. Or skip the script and just copy or symlink a skill directory into the
right place.

Then the skill loads by itself when you work on that board, or on demand:

```
/stm32h750-weact
```

## Layout

```
skills/<family>/<skill-name>/     one skill, ready to copy into an agent's skills directory
  SKILL.md                        frontmatter + the load-bearing knowledge
  reference/                      deep detail, read on demand
  template/                       a project that actually builds
  assets/                         photos of the board, pinout images
docs/                             how to author and review a skill
templates/skill-template/         blank skeleton to start from
scripts/                          install.sh, validate.sh
```

Every skill has an `assets/` directory for pictures of the *physical* board — a top shot, a
bottom shot with a readable silkscreen, a pinout diagram. Photos can be uploaded into IMAGE.md

```
skills/rp2/rp2040-pico/assets/
  IMAGE.md
```

Families: `stm32`, `esp32`, `esp8266`, `rp2`, `nrf`, `avr`.
Add a new directory if yours does not fit — one level, family name only, no vendor nesting.

Skill directory name == the `name:` in its frontmatter, and it must be unique across the whole
repo: installed skills all land in one flat skills-directory namespace, whatever the agent.

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
