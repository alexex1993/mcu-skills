# PlatformIO extra script: send `pio run -t upload` through the WeAct HID
# bootloader (scripts/weact_hid_flash.py) instead of a probe or ROM DFU.
#
# PlatformIO's built-in `upload_protocol = hid` is NOT this: it runs
# Serasidis' hid-flash, which looks for 1209:BEBA and never finds a WeAct
# bootloader (0483:572A). Hence `upload_protocol = custom` and this script.

import importlib.util
import os

Import("env")  # noqa: F821 - provided by SCons

FLASHER = os.path.join(env.subst("$PROJECT_DIR"), "scripts", "weact_hid_flash.py")


def _ensure_hidapi(source, target, env):
    # find_spec, not `import hid`: with hidapi loaded into the SCons process,
    # `pio run -t upload` reports FAILED (exit 1) after a successful flash on
    # macOS. The flasher runs in its own process and imports it there.
    if importlib.util.find_spec("hid") is None:
        print("Installing the 'hidapi' Python package into PlatformIO's environment ...")
        env.Execute('"$PYTHONEXE" -m pip install --quiet hidapi')


env.AddPreAction("upload", _ensure_hidapi)
env.Replace(UPLOADCMD='"$PYTHONEXE" "%s" "$SOURCE"' % FLASHER)
