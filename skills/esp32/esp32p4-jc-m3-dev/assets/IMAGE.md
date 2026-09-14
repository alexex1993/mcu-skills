Photos of the Guition JC-ESP32P4-M3-DEV go here.

Worth having, in this order:

1. **Top shot** — the module lid readable. It prints `JC-ESP32P4-M3 · GUITION`,
   `SOC: ESP32P4NRW32`, `Memory: 32M PSRAM`, `Flash: 16M`, `WiFi: ESP32-C6`, and that is
   the fastest way to confirm the board and the memory configuration at once.
2. **The port edge**, close enough to tell the **three USB-C sockets apart** and to see
   the RJ45 above them. Which physical socket is the native USB-Serial/JTAG, which is the
   CH340C and which is the high-speed OTG is the one thing this skill cannot state from
   the schematic — it names them only by what they enumerate as. A labelled photo would
   close that gap.
3. **The JP1 header**, close enough to read pin 1 and any silkscreen. The 26-pin map in
   `reference/board-hardware.md` §2.3 is transcribed from the schematic; a reader with the
   board in hand should be able to check it, especially pins 20/22/24/26, which belong to
   the ESP32-C6 rather than the P4.
4. **The two tactile switches**, so SW1 (`BOOTMODE`) and SW2 (reset) can be told apart
   without a meter.

Upload images into a GitHub issue or PR and paste the resulting
`https://github.com/user-attachments/...` URLs here as `<img …>` tags, the way the other
skills in this repo do.
