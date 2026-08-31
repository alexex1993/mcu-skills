#!/usr/bin/env bash
# Install a board skill from this repo into an agent's skills directory.
#
#   ./scripts/install.sh <skill-name>              symlink (default; git pull updates it)
#   ./scripts/install.sh <skill-name> --copy       copy, so you can edit locally
#   ./scripts/install.sh --all [--copy]
#   ./scripts/install.sh --list
#   ./scripts/install.sh --uninstall <skill-name>
#
#   --agent <claude|zcode|opencode>   target agent, claude by default
#   --project                         into ./.<agent>/skills of the current directory,
#                                     instead of the agent's user-level directory
#   --dest <dir>                      explicit destination directory, beats --agent
#
# SKILLS_DIR (or the older CLAUDE_SKILLS_DIR) overrides the destination as well.
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
AGENT=claude
SCOPE=user
DEST=""
MODE=link
ACTION=install
NAMES=()

while [ $# -gt 0 ]; do
  case "$1" in
    --copy)      MODE=copy ;;
    --link)      MODE=link ;;
    --all)       ACTION=all ;;
    --list)      ACTION=list ;;
    --uninstall) ACTION=uninstall ;;
    --project)   SCOPE=project ;;
    --agent|--dest)
      [ $# -ge 2 ] || { echo "$1 needs a value" >&2; exit 2; }
      if [ "$1" = --agent ]; then AGENT="$2"; else DEST="$2"; fi
      shift ;;
    --agent=*)   AGENT="${1#--agent=}" ;;
    --dest=*)    DEST="${1#--dest=}" ;;
    -h|--help)   sed -n '2,15p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
    -*)          echo "unknown option: $1" >&2; exit 2 ;;
    *)           NAMES+=("$1") ;;
  esac
  shift
done

case "$AGENT" in
  claude|zcode|opencode) ;;
  *) echo "unknown agent: $AGENT (claude, zcode, opencode)" >&2; exit 2 ;;
esac

user_dir() {
  case "$1" in
    claude)   echo "$HOME/.claude/skills" ;;
    zcode)    echo "$HOME/.zcode/skills" ;;
    opencode) echo "${XDG_CONFIG_HOME:-$HOME/.config}/opencode/skills" ;;
  esac
}

if [ -z "$DEST" ]; then
  if   [ -n "${SKILLS_DIR:-}" ];        then DEST="$SKILLS_DIR"
  elif [ -n "${CLAUDE_SKILLS_DIR:-}" ]; then DEST="$CLAUDE_SKILLS_DIR"   # legacy alias
  elif [ "$SCOPE" = project ];          then DEST="$PWD/.$AGENT/skills"
  else                                        DEST="$(user_dir "$AGENT")"
  fi
fi

find_skill() {  # name -> path, or empty
  find "$REPO/skills" -mindepth 2 -maxdepth 2 -type d -name "$1" -print -quit
}

list_skills() { find "$REPO/skills" -mindepth 2 -maxdepth 2 -type d | sort; }

if [ "$ACTION" = list ]; then
  echo "destination: $DEST"
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
