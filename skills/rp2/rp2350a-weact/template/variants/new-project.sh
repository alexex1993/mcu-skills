#!/usr/bin/env bash
# Scaffold a WeAct Studio RP2350A Core Board project from this template.
#
#   ./new-project.sh <target-dir> [--full|--minimal] [--v10|--v20] [--16mb]
#
#   --full     (default) LED heartbeat + USB-CDC report + real flash size from
#              the JEDEC ID + ADC + die temp + EEPROM boot counter, plus the
#              per-revision extras (VSYS/VBUS on V2.0, LED2/KEY on V1.0)
#   --minimal  blink on GP25, nothing else
#   --v20      (default) build for RP2350A_V20
#   --v10      build for RP2350A_V10
#   --16mb     use the 16 MB flash env instead of the 4 MB one
#
# The revision/flash flags only pick which env goes in default_envs; all four
# envs stay in platformio.ini either way. Nothing is generated and no paths
# are embedded: copying template/ by hand and editing that one line gives
# exactly the same result.
set -euo pipefail

TEMPLATE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TARGET=""
VARIANT=full
REV=v20
FLASH=""

for arg in "$@"; do
  case "$arg" in
    --full)    VARIANT=full ;;
    --minimal) VARIANT=minimal ;;
    --v10)     REV=v10 ;;
    --v20)     REV=v20 ;;
    --16mb)    FLASH=_16mb ;;
    --4mb)     FLASH="" ;;
    -h|--help) sed -n '2,17p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
    -*)        echo "unknown option: $arg" >&2; exit 2 ;;
    *)         TARGET="$arg" ;;
  esac
done

[ -n "$TARGET" ] || { echo "usage: $0 <target-dir> [--full|--minimal] [--v10|--v20] [--16mb]" >&2; exit 2; }
[ -e "$TARGET" ] && { echo "$TARGET already exists" >&2; exit 1; }

ENV_NAME="weact_rp2350a_${REV}${FLASH}"

mkdir -p "$TARGET"
cp -R "$TEMPLATE/src" "$TEMPLATE/include" "$TARGET/"
cp "$TEMPLATE/platformio.ini" "$TEMPLATE/.gitignore" "$TARGET/"

if [ "$VARIANT" = minimal ]; then
  cp "$TEMPLATE/variants/minimal/main.cpp" "$TARGET/src/main.cpp"
fi

# point default_envs at the chosen board; the other three envs stay available
sed -i.bak "s/^default_envs = .*/default_envs = ${ENV_NAME}/" "$TARGET/platformio.ini"
rm -f "$TARGET/platformio.ini.bak"

echo "scaffolded $VARIANT project in $TARGET (default_envs = $ENV_NAME)"
echo "next:  cd $TARGET && pio run -t upload -t monitor"
