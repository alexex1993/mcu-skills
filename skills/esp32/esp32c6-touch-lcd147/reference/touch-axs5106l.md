# The AXS5106L touch controller

The board's headline peripheral, and the one with the most ways to fail silently.
Everything here was found on hardware while bringing up `template/`; the protocol
matches Waveshare's own AXS5106L Arduino library.

Pin map and electricals are in [board-hardware.md](board-hardware.md); pasteable code is
in [recipes.md](recipes.md) §8–9 and in `template/components/axs5106l/`.

---

## 1. What it is

| | |
|---|---|
| Part | AXS5106L capacitive touch controller |
| Bus | I²C, **7-bit address 0x63**, 400 kHz works |
| Wiring | `TP_SDA` GPIO18, `TP_SCL` GPIO19 — **shared with the QMI8658A IMU**, 10 k pull-ups on board |
| `TP_RST` | GPIO20, active low |
| `TP_INT` | GPIO21, active low, open-drain-ish; the template polls instead |
| Contacts | up to 2 usable (the frame carries two records) |

## 2. Register map, as used

| Register | Length | Contents |
|---|---|---|
| `0x01` | **14 bytes** | the touch frame — see §3 |
| `0x05` | 2 | firmware version, big-endian |
| `0x08` | 3 | chip ID |

## 3. The four rules

Each of these produced a distinct, misleading failure during bring-up.

### 3.1 Read the whole 14-byte frame, every time

```
byte 0   gesture
byte 1   number of contacts
byte 2.. 6 bytes per contact:  XH, XL, YH, YL, pressure, reserved
```

`x = ((XH & 0x0F) << 8) | XL`, and the same for `y` — 12-bit values, the top nibble of
the high byte is status, not coordinate.

**Reading only the six bytes you care about leaves the controller misaligned**, and every
following frame comes back shifted. Coordinates then land in random places and no amount
of calibration fixes it, because the data is not wrong — it is offset.

### 3.2 No repeated START

The chip does **not** answer a combined write-then-read transaction. Write the register
address, let the bus go idle with a STOP, then read in a *second* transaction:

```c
i2c_master_transmit(dev, &reg, 1, timeout);   /* ends with STOP */
i2c_master_receive(dev, buf, len, timeout);   /* fresh START */
```

`i2c_master_transmit_receive()` uses a repeated START and **every read fails while the
address itself still ACKs** — which reads as "the chip is there but broken".

### 3.3 Reset generously

`TP_RST` low for **200 ms**, release, then wait **300 ms** before the first transaction.
Short pulses appear to work — the address ACKs — but the controller reports nonsense
coordinates afterwards.

Do **not** send the `0xF0 B3 55 AA 34 01` soft-reset sequence found in some AXS5106L
code. That belongs to a different board's firmware-upgrade path.

### 3.4 Detect presence by address ACK, not by chip ID

Some firmware revisions report an **all-zero chip ID** while being perfectly alive.
`i2c_master_probe(bus, 0x63, 50)` is the right check; treating a zero ID as "absent"
throws away a working controller.

## 4. Coordinate mapping

The vendor library's **rotation 0** for this panel is *mirror X, leave Y alone*:

```
screen_x = 171 - raw_x      /* BSP_LCD_H_RES - 1 - raw_x */
screen_y = raw_y
```

Its other rotations, for reference (`w` = 172, `h` = 320):

| Rotation | screen_x | screen_y |
|---|---|---|
| 0 | `w-1-raw_x` | `raw_y` |
| 90 | `raw_y` | `raw_x` |
| 180 | `raw_x` | `h-1-raw_y` |
| 270 | `w-1-raw_y` | `h-1-raw_x` |

The raw full scale equals the panel resolution, so no scaling is needed at rotation 0 —
only the mirror.

Symptom → which one you got wrong:

| Symptom | Cause |
|---|---|
| dot mirrored left↔right | mirror-X missing |
| dot in the opposite corner | both axes flipped (rotation 180 applied) |
| dot moves along the wrong axis | X and Y swapped (rotation 90/270 applied) |
| dot tracks but compressed | raw full scale ≠ panel resolution — scale, do not clamp |

## 5. The calibration escape hatch

The stock mapping above is correct for this panel, so `template/` uses it by default and
asks the user for nothing on a fresh board. Calibration exists to rescue a panel that
behaves differently — a revision with swapped axes or a different raw range.

**Hold BOOT (GPIO9) while pressing RESET.** The firmware then asks for three taps:

1. one at `(w/4, h/4)` — the origin,
2. one at `(3w/4, h/4)` — displaced along **screen X only**,
3. one at `(w/4, 3h/4)` — displaced along **screen Y only**.

Three points is exactly enough. Target 1 → 2 moved along screen X, so whichever *raw*
axis changed is the one feeding screen X; the sign of the change gives the mirror; the
magnitude gives the scale. That covers all eight orientations plus a different raw full
scale, with two independent two-point affine maps and one `swap` flag — mirroring falls
out of the slope's sign and needs no separate flag.

The result is stored in NVS, namespace `touch`, key `cal_v2`, as the packed struct in
`template/src/main.c`. Bump the key name if the struct changes; a stale blob of the
right size deserializes into nonsense.

The capture routine waits for the finger from the *previous* target to leave (five
consecutive released reads) before accepting the next one, and averages the samples of
one press. Without the release wait, one long drag registers as three taps.

Note that the button is **GPIO9**, not GPIO8 — see board-hardware.md §4.2. Firmware that
reads GPIO8 finds a floating header pin and the hatch never opens.

## 6. Polling vs. the interrupt

`TP_INT` (GPIO21) goes low when there is a frame. The template **polls at 50 Hz and
ignores it**, on purpose:

- polling gives the release edge for free, which an INT-driven design has to chase
  separately (the interrupt fires on contact, not on lift);
- a 14-byte read at 400 kHz is ~0.4 ms, so 50 Hz costs ~2 % of the bus and nothing
  measurable of the CPU;
- there is no sleep story on this board worth optimising for.

Use the interrupt when the HP core has to sleep between touches. Configure GPIO21 as an
input with a pull-up and a negative-edge interrupt, and still read the whole frame.

## 7. Bus recovery

Strong interference can wedge SDA low. The driver counts consecutive failed reads and
calls `i2c_master_bus_reset()` after three, which clocks the bus free. Resetting on every
single failure instead makes the log unreadable and masks real wiring problems — the
streak is the point.

## 8. Components to avoid

| Component | Why not |
|---|---|
| `mydazy/esp_lcd_touch_axs5106l` | built for a 284 × 240 ESP32-S3 board, drags in LVGL, and flashes a firmware blob that is wrong for this panel |
| `esp_lcd_touch` + a generic driver | there is no upstream AXS5106L driver to plug into it |

`template/components/axs5106l/` is ~280 lines, has no LVGL dependency and returns raw
coordinates alongside mapped ones, which is what the calibration needs.

## 9. Using it with LVGL

The driver is deliberately framework-free. To feed LVGL, wrap `axs5106l_read()` in an
`lv_indev` read callback:

```c
static void indev_read(lv_indev_t *drv, lv_indev_data_t *data)
{
    axs5106l_data_t t;
    if (axs5106l_read(s_tp, &t) == ESP_OK && t.pressed) {
        data->point.x = t.x;
        data->point.y = t.y;
        data->state   = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}
```

Set `swap_xy` / `mirror_x` / `mirror_y` in `axs5106l_config_t` (rotation 0 here is
`mirror_x = true`) so `t.x`/`t.y` come out in screen coordinates and LVGL needs no
transform of its own. ⚠︎ Untested — the template has no LVGL.

Budget the memory first: LVGL plus a full framebuffer plus Wi-Fi does not fit in
~320 KB. See board-hardware.md §8 and §9.5.
