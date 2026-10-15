#!/usr/bin/env python3
"""
Flash a .bin through the WeAct Studio HID bootloader (USB 0483:572A).

    python weact_hid_flash.py firmware.bin      # flash + reboot into it
    python weact_hid_flash.py --info            # bootloader version, DEVID, flash size
    python weact_hid_flash.py --reboot          # leave the bootloader, start the app

The bootloader lives in flash sector 0 (0x08000000, 16 KB) and writes whatever
it receives to 0x08004000 onwards, so the .bin MUST be linked for 0x08004000.
A .bin linked for 0x08000000 is accepted, written, and then never starts.

Getting into the bootloader, in the order this script tries:
  1. it is already there (HID device present; it also stays resident by itself
     when 0x08004000 holds no valid vector table);
  2. the template's --full firmware is running: its USB CDC port is opened at
     1200 baud, which that firmware takes as "reboot into the bootloader";
  3. by hand: hold KEY, tap NRST, release KEY once PC13 flickers fast.

Protocol (WeAct_HID_Bootloader_F4x1, reimplemented from its CLI): 65-byte
HID OUT reports = report ID 0 + 64 bytes. Commands are "WeAct:" + one byte.
Data is streamed in 1024-byte pages (16 reports); the bootloader erases each
flash sector on its first page, writes the page, and answers "WeAct:" + 0x03.

Needs the `hidapi` package (pip install hidapi); step 2 needs `pyserial`,
which ships with PlatformIO.
"""

import argparse
import sys
import time

VID, PID = 0x0483, 0x572A            # "WeAct Studio HID Bootloader"
APP_VID, APP_PID = 0x0483, 0x5740    # template firmware's CDC port (ST VCP IDs)
APP_PRODUCT = "Black Pill F401RC"    # its product string, to tell it from other ST VCPs

SIG = b"WeAct:"
CMD_RESET_PAGES, CMD_REBOOT, CMD_FW_VER, CMD_ACK = 0x00, 0x01, 0x02, 0x03
PAGE = 1024
CHUNK = 64
APP_OFFSET = 0x4000                  # bootloader size; app starts at 0x08004000

try:
    import hid
except ImportError:
    sys.exit("error: Python module 'hid' missing - run:  python -m pip install hidapi")


def find_bootloader():
    return hid.enumerate(VID, PID)


def open_bootloader():
    dev = hid.device()
    dev.open(VID, PID)
    return dev


def send(dev, payload):
    report = bytes([0]) + payload
    report += bytes(65 - len(report))
    if dev.write(report) != 65:
        raise IOError("HID write failed")


def command(dev, cmd):
    send(dev, SIG + bytes([cmd]))


def wait_reply(dev, timeout_s):
    deadline = time.time() + timeout_s
    while time.time() < deadline:
        r = dev.read(65, 200)
        if r and bytes(r[:6]) == SIG:
            return bytes(r)
    return None


def touch_1200(port_hint=None):
    """Ask the running template firmware to reboot into the bootloader."""
    try:
        import serial
        from serial.tools import list_ports
    except ImportError:
        return False

    if port_hint:
        ports = [port_hint]
    else:
        ports = [p.device for p in list_ports.comports()
                 if p.vid == APP_VID and p.pid == APP_PID
                 and (p.product is None or APP_PRODUCT in p.product)]
    if len(ports) != 1:
        if len(ports) > 1:
            print("> several candidate CDC ports, pass --port:", ", ".join(ports))
        return False

    print("> 1200-baud touch on %s" % ports[0])
    try:
        s = serial.Serial(ports[0], 1200)
        time.sleep(0.1)
        s.close()
    except (OSError, serial.SerialException) as e:
        print("> could not open %s: %s" % (ports[0], e))
        return False
    return True


def wait_for_bootloader(seconds):
    deadline = time.time() + seconds
    while time.time() < deadline:
        if find_bootloader():
            time.sleep(0.3)          # let it finish enumerating
            return True
        time.sleep(0.1)
    return False


def enter_bootloader(port_hint, wait_s):
    if find_bootloader():
        return True
    if touch_1200(port_hint) and wait_for_bootloader(5):
        return True
    print("> HID bootloader %04X:%04X not found." % (VID, PID))
    print(">   Hold KEY, tap NRST, release KEY when the PC13 LED blinks fast.")
    if wait_s <= 0:
        return False
    print(">   Waiting %d s ..." % wait_s)
    deadline = time.time() + wait_s
    while time.time() < deadline:
        if find_bootloader():
            time.sleep(0.3)
            return True
        time.sleep(0.2)
    return False


def info(dev):
    command(dev, CMD_FW_VER)
    r = wait_reply(dev, 2)
    if r is None:
        raise IOError("no answer to the version command")
    return r[6:].split(b"\0", 1)[0].decode("ascii", "replace")


def flash_kb(info_str):
    # "V1.2 0x423 ROM: 256KB"
    try:
        return int(info_str.rsplit(":", 1)[1].strip().rstrip("KB"))
    except (IndexError, ValueError):
        return None


def check_image(image):
    if len(image) < 8:
        sys.exit("error: image is %d bytes" % len(image))
    sp = int.from_bytes(image[0:4], "little")
    pc = int.from_bytes(image[4:8], "little")
    if (sp & 0x2FF80000) != 0x20000000:
        sys.exit("error: first word 0x%08X is not a RAM stack pointer - not a "
                 "Cortex-M vector table; the bootloader would refuse to jump" % sp)
    if not (0x08004000 <= (pc & ~1) < 0x08100000):
        sys.exit("error: reset vector 0x%08X is below 0x08004000 - this image is "
                 "linked for 0x08000000 and would never start behind the bootloader. "
                 "Build [env:hid] (ld/STM32F401RCTX_APP_0x08004000.ld)." % pc)


def flash(dev, image):
    command(dev, CMD_RESET_PAGES)
    time.sleep(0.05)
    pad = (-len(image)) % PAGE
    image = image + b"\xff" * pad
    pages = len(image) // PAGE
    t0 = time.time()
    for n in range(pages):
        page = image[n * PAGE:(n + 1) * PAGE]
        for i in range(0, PAGE, CHUNK):
            send(dev, page[i:i + CHUNK])
            # The bootloader copies each report out of a single buffer from its
            # main loop; a report that lands before that copy overwrites the
            # previous one. WeAct's own CLI paces at 500 us, so do the same.
            time.sleep(0.0005)
        # A 64 or 128 KB sector erase stalls the CPU (and USB) for 1-2 s.
        r = wait_reply(dev, 5)
        if r is None or r[6] != CMD_ACK:
            raise IOError("no ACK for page %d (0x%08X)" %
                          (n, 0x08000000 + APP_OFFSET + n * PAGE))
        sys.stdout.write("\r> page %d/%d" % (n + 1, pages))
        sys.stdout.flush()
    print("\n> wrote %d bytes in %.1f s" % (len(image), time.time() - t0))


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("firmware", nargs="?", help=".bin linked for 0x08004000")
    ap.add_argument("--info", action="store_true", help="print bootloader info and exit")
    ap.add_argument("--reboot", action="store_true", help="just leave the bootloader")
    ap.add_argument("--no-reboot", action="store_true", help="stay in the bootloader after flashing")
    ap.add_argument("--port", help="CDC port of the running app, for the 1200-baud touch")
    ap.add_argument("--wait", type=int, default=30,
                    help="seconds to wait for a manual KEY+NRST (default 30, 0 = don't)")
    args = ap.parse_args()

    if not (args.firmware or args.info or args.reboot):
        ap.error("give a firmware file, --info or --reboot")

    image = None
    if args.firmware:
        with open(args.firmware, "rb") as f:
            image = f.read()
        check_image(image)

    if not enter_bootloader(args.port, args.wait):
        sys.exit("error: bootloader not reachable")

    dev = open_bootloader()
    try:
        s = info(dev)
        print("> bootloader: %s" % s)
        if args.info:
            return
        if image is not None:
            kb = flash_kb(s)
            if kb and len(image) > (kb * 1024 - APP_OFFSET):
                sys.exit("error: image %d B > %d KB flash - 16 KB bootloader" % (len(image), kb))
            flash(dev, image)
        if args.reboot or not args.no_reboot:
            print("> reboot")
            command(dev, CMD_REBOOT)
    finally:
        dev.close()


if __name__ == "__main__":
    main()
