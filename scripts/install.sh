#!/usr/bin/env bash
# Install a board skill from this repo into ~/.claude/skills.
#
#   ./scripts/install.sh <skill-name>            symlink (default; git pull updates it)
#   ./scripts/install.sh <skill-name> --copy     copy, so you can edit locally
#   ./scripts/install.sh --all [--copy]
#   ./scripts/install.sh --list
#   ./scripts/install.sh --uninstall <skill-name>
#
# CLAUDE_SKILLS_DIR overrides the destination (e.g. a project's .claude/skills).
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEST="${CLAUDE_SKILLS_DIR:-$HOME/.claude/skills}"
MODE=link
ACTION=install
NAMES=()

for arg in "$@"; do
  case "$arg" in
    --copy)      MODE=copy ;;
    --link)      MODE=link ;;
    --all)       ACTION=all ;;
    --list)      ACTION=list ;;
    --uninstall) ACTION=uninstall ;;
    -h|--help)   sed -n '2,10p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
    -*)          echo "unknown option: $arg" >&2; exit 2 ;;
    *)           NAMES+=("$arg") ;;
  esac
done

find_skill() {  # name -> path, or empty
  find "$REPO/skills" -mindepth 2 -maxdepth 2 -type d -name "$1" -print -quit
}

list_skills() { find "$REPO/skills" -mindepth 2 -maxdepth 2 -type d | sort; }

if [ "$ACTION" = list ]; then
  printf '%-28s %-10s %s\n' SKILL FAMILY INSTALLED
  while read -r p; do
    n=$(basename "$p"); f=$(basename "$(dirname "$p")")
    if   [ -L "$DEST/$n" ]; then s="symlink"
    elif [ -d "$DEST/$n" ]; then s="copy"
    else s="-"; fi
    printf '%-28s %-10s %s\n' "$n" "$f" "$s"
  done < <(list_skills)
  exit 0
fi

if [ "$ACTION" = all ]; then
  while read -r p; do NAMES+=("$(basename "$p")"); done < <(list_skills)
fi

[ ${#NAMES[@]} -gt 0 ] || { echo "nothing to do — pass a skill name, --all or --list" >&2; exit 2; }

mkdir -p "$DEST"
for name in "${NAMES[@]}"; do
  target="$DEST/$name"

  if [ "$ACTION" = uninstall ]; then
    if [ -L "$target" ] || [ -d "$target" ]; then rm -rf "$target"; echo "removed  $target"
    else echo "not installed: $name"; fi
    continue
  fi

  src="$(find_skill "$name")"
  [ -n "$src" ] || { echo "no such skill: $name (try --list)" >&2; exit 1; }

  if [ -e "$target" ] || [ -L "$target" ]; then
    if [ -L "$target" ] && [ "$(readlink "$target")" = "$src" ]; then
      echo "ok       $name (already linked)"; continue
    fi
    read -r -p "$target exists — replace? [y/N] " reply
    case "$reply" in y|Y) rm -rf "$target" ;; *) echo "skipped  $name"; continue ;; esac
  fi

  if [ "$MODE" = link ]; then ln -s "$src" "$target"; echo "linked   $target -> $src"
  else cp -R "$src" "$target"; echo "copied   $target"; fi
done
