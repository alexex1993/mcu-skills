#!/usr/bin/env bash
# Install board skills for Claude Code (default) or Codex (--codex).
#
#   ./scripts/install.sh <skill-name>            symlink (default; git pull updates it)
#   ./scripts/install.sh <skill-name> --copy     copy, so you can edit locally
#   ./scripts/install.sh --all [--copy]
#   ./scripts/install.sh --list                show Claude and Codex installations
#   ./scripts/install.sh --uninstall <skill-name>
#   ./scripts/install.sh --codex --all          install all skills for Codex
#
# Claude: ~/.claude/skills, overridden by CLAUDE_SKILLS_DIR.
# Codex:  ~/.agents/skills, overridden by CODEX_SKILLS_DIR.
# Add --yes to replace existing installations without prompting.
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
AGENT=claude
REPLACE=no
MODE=link
ACTION=install
NAMES=()

for arg in "$@"; do
  case "$arg" in
    --codex)     AGENT=codex ;;
    --claude)    AGENT=claude ;;
    --yes)       REPLACE=yes ;;
    --copy)      MODE=copy ;;
    --link)      MODE=link ;;
    --all)       ACTION=all ;;
    --list)      ACTION=list ;;
    --uninstall) ACTION=uninstall ;;
    -h|--help)   sed -n '2,13p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
    -*)          echo "unknown option: $arg" >&2; exit 2 ;;
    *)           NAMES+=("$arg") ;;
  esac
done

CLAUDE_DEST="${CLAUDE_SKILLS_DIR:-$HOME/.claude/skills}"
CODEX_DEST="${CODEX_SKILLS_DIR:-$HOME/.agents/skills}"
if [ "$AGENT" = codex ]; then DEST="$CODEX_DEST"
else DEST="$CLAUDE_DEST"; fi

find_skill() {  # name -> path, or empty
  find "$REPO/skills" -mindepth 2 -maxdepth 2 -type d -name "$1" -print -quit
}

list_skills() { find "$REPO/skills" -mindepth 2 -maxdepth 2 -type d | sort; }

install_status() {
  if [ -L "$1" ]; then
    if [ -e "$1" ]; then printf symlink; else printf 'broken link'; fi
  elif [ -d "$1" ]; then printf copy
  elif [ -e "$1" ]; then printf file
  else printf -- '-'; fi
}

show_install_path() {
  if [ -L "$2" ]; then printf '  %s: %s -> %s\n' "$1" "$2" "$(readlink "$2")"
  elif [ -e "$2" ]; then printf '  %s: %s\n' "$1" "$2"; fi
}

if [ "$ACTION" = list ]; then
  printf 'Claude: %s\nCodex:  %s\n\n' "$CLAUDE_DEST" "$CODEX_DEST"
  printf '%-28s %-10s %-12s %s\n' SKILL FAMILY CLAUDE CODEX
  while read -r p; do
    n=$(basename "$p"); f=$(basename "$(dirname "$p")")
    printf '%-28s %-10s %-12s %s\n' "$n" "$f" \
      "$(install_status "$CLAUDE_DEST/$n")" "$(install_status "$CODEX_DEST/$n")"
    show_install_path Claude "$CLAUDE_DEST/$n"
    show_install_path Codex "$CODEX_DEST/$n"
  done < <(list_skills)
  exit 0
fi

if [ "$ACTION" = all ]; then
  while read -r p; do NAMES+=("$(basename "$p")"); done < <(list_skills)
fi

[ ${#NAMES[@]} -gt 0 ] || { echo "nothing to do — pass a skill name, --all or --list" >&2; exit 2; }

# Validate the entire request before changing any installation. In particular,
# uninstall must never accept paths or glob patterns as skill names.
for name in "${NAMES[@]}"; do
  [[ "$name" =~ ^[a-z0-9]+(-[a-z0-9]+)*$ ]] || { echo "invalid skill name: $name" >&2; exit 2; }
  if [ "$ACTION" != uninstall ]; then
    src="$(find_skill "$name")"
    [ -n "$src" ] && [ -f "$src/SKILL.md" ] || { echo "no such skill: $name (try --list)" >&2; exit 1; }
  fi
done

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
    if [ "$MODE" = link ] && [ -L "$target" ] && [ "$(readlink "$target")" = "$src" ]; then
      echo "ok       $name (already linked)"; continue
    fi
    reply=y
    if [ "$REPLACE" != yes ]; then
      read -r -p "$target exists — replace? [y/N] " reply || reply=n
    fi
    case "$reply" in y|Y) rm -rf "$target" ;; *) echo "skipped  $name"; continue ;; esac
  fi

  if [ "$MODE" = link ]; then ln -s "$src" "$target"; echo "linked   $target -> $src"
  else cp -R "$src" "$target"; echo "copied   $target"; fi
done
