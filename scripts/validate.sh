#!/usr/bin/env bash
# Structural checks for board skills. Not a substitute for testing on hardware —
# see CONTRIBUTING.md §5.
#
#   ./scripts/validate.sh <skill-name>
#   ./scripts/validate.sh --all
set -uo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
FAIL=0; WARN=0
err()  { echo "  FAIL  $1"; FAIL=$((FAIL+1)); }
warn() { echo "  warn  $1"; WARN=$((WARN+1)); }
ok()   { echo "  ok    $1"; }

check_skill() {
  local dir="$1" name; name="$(basename "$dir")"
  echo "$name  ($dir)"

  local skill="$dir/SKILL.md"
  [ -f "$skill" ] || { err "no SKILL.md"; return; }

  # --- frontmatter -----------------------------------------------------------
  if [ "$(head -1 "$skill")" != "---" ]; then
    err "SKILL.md does not start with a '---' frontmatter block"; return
  fi
  local fm_end; fm_end=$(awk 'NR>1 && /^---[[:space:]]*$/ {print NR; exit}' "$skill")
  [ -n "$fm_end" ] || { err "frontmatter is not closed with '---'"; return; }
  local fm; fm=$(sed -n "2,$((fm_end-1))p" "$skill")

  local fm_name; fm_name=$(printf '%s\n' "$fm" | sed -n 's/^name:[[:space:]]*//p' | head -1 | tr -d '"'"'"'')
  if [ -z "$fm_name" ]; then err "frontmatter has no 'name:'"
  elif [ "$fm_name" != "$name" ]; then err "name: '$fm_name' != directory '$name'"
  elif ! printf '%s' "$fm_name" | grep -qE '^[a-z0-9]+(-[a-z0-9]+)*$'; then
    err "name '$fm_name' must be lowercase letters, digits and single hyphens"
  else ok "name: $fm_name"; fi

  local desc; desc=$(printf '%s\n' "$fm" | awk '/^description:/{f=1} f&&/^[a-z-]+:/&&!/^description:/{f=0} f' | sed 's/^description:[[:space:]]*//')
  local dlen=${#desc}
  if [ "$dlen" -eq 0 ]; then err "frontmatter has no 'description:'"
  elif [ "$dlen" -lt 120 ]; then err "description is $dlen chars — too short to trigger reliably (aim for 300+)"
  elif [ "$dlen" -lt 250 ]; then warn "description is $dlen chars — consider naming more peripherals and task shapes"
  elif [ "$dlen" -gt 1024 ]; then err "description is $dlen chars — over the 1024 limit"
  else ok "description: $dlen chars"; fi
  printf '%s' "$desc" | grep -qiE '\buse when\b|\bwhen (working|the user|you)' \
    || warn "description has no 'Use when …' clause — it says what, not when"

  case "$fm_name" in
    REPLACE*|*'<'*) err "frontmatter still contains skeleton placeholders" ;;
  esac

  # --- body ------------------------------------------------------------------
  local lines; lines=$(wc -l < "$skill" | tr -d ' ')
  if   [ "$lines" -gt 700 ]; then err "SKILL.md is $lines lines — move detail into reference/"
  elif [ "$lines" -gt 500 ]; then warn "SKILL.md is $lines lines — aim for under 500"
  else ok "SKILL.md: $lines lines"; fi

  grep -qiE '^#+ .*(flash|upload|program|burn)' "$skill" \
    || warn "no flashing section in SKILL.md"
  grep -qiE '(rule|pitfall|gotcha|mistake|trap)' "$skill" \
    || warn "no rules/pitfalls section — that is where most of a skill's value lives"

  # --- directories -----------------------------------------------------------
  [ -d "$dir/reference" ] && ok "reference/ ($(find "$dir/reference" -name '*.md' | wc -l | tr -d ' ') files)" \
                          || warn "no reference/ directory"
  if [ -d "$dir/template" ]; then
    ok "template/"
    local sc="$dir/template/variants/new-project.sh"
    if [ -f "$sc" ]; then
      [ -x "$sc" ] || err "template/variants/new-project.sh is not executable (chmod +x)"
    else warn "no template/variants/new-project.sh scaffold script"; fi
    [ -f "$dir/template/README.md" ] || warn "no template/README.md mapping files to subsystems"
  else warn "no template/ directory — a skill with no buildable project is much weaker"; fi

  # every reference/ file should be mentioned by SKILL.md
  while read -r f; do
    grep -q "$(basename "$f")" "$skill" || warn "reference/$(basename "$f") is never referenced from SKILL.md"
  done < <(find "$dir/reference" -name '*.md' 2>/dev/null)

  # --- hygiene ---------------------------------------------------------------
  local abs; abs=$(grep -rlE '(/Users/[a-zA-Z0-9_.-]+|/home/[a-zA-Z0-9_.-]+|[A-Z]:\\\\)' "$dir" 2>/dev/null | head -5)
  [ -z "$abs" ] || err "machine-specific absolute paths in: $(echo "$abs" | tr '\n' ' ')"

  local junk; junk=$(find "$dir" \( -name '.DS_Store' -o -name '*.elf' -o -name '*.bin' -o -name '*.o' \
                                    -o -name '.pio' -o -name 'build' -o -name '.vscode' -o -name 'node_modules' \) | head -5)
  [ -z "$junk" ] || err "build output or IDE files committed: $(echo "$junk" | tr '\n' ' ')"

  echo
}

if [ "${1:---all}" = "--all" ]; then
  while read -r d; do check_skill "$d"; done < <(find "$REPO/skills" -mindepth 2 -maxdepth 2 -type d | sort)
else
  for n in "$@"; do
    d="$(find "$REPO/skills" -mindepth 2 -maxdepth 2 -type d -name "$n" -print -quit)"
    [ -n "$d" ] || { echo "no such skill: $n"; FAIL=$((FAIL+1)); continue; }
    check_skill "$d"
  done
fi

echo "$FAIL failed, $WARN warnings"
[ "$FAIL" -eq 0 ]
