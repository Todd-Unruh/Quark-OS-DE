#!/usr/bin/env bash
# =============================================================================
# count.sh — Count lines of code in a Quark-OS project tree.
#
# Excludes:
#   - .o object files and other build artifacts
#   - .git and other VCS metadata
#   - the iso_root staging dir and .iso images
#   - binary files (images, .bin blobs, etc.)
#
# Usage:
#   ./count.sh            # summary by directory + total
#   ./count.sh --by-file  # also list every file and its line count
#   ./count.sh -v         # verbose (same as --by-file)
# =============================================================================

set -e

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT"

BY_FILE=0
[[ "$1" == "--by-file" || "$1" == "-v" ]] && BY_FILE=1

# --- Directories / patterns to skip -----------------------------------------
# Add or remove as you like. Case-sensitive (GNU find uses -path).
EXCLUDES=(
    "./.git"
    "./iso_root"
    "./node_modules"
    "./.cache"
)

# --- File extensions we DO count (as code) ----------------------------------
# Anything not listed here is treated as a binary asset and skipped.
CODE_EXTS=(
    "c" "h" "asm" "s" "S" "inc"
    "sh" "bash"
    "ld" "lds"
    "py" "pl" "rb"
    "mk"
)

# --- Build the find command -------------------------------------------------
# Start from all regular files.
find_args=( . -type f )

# Skip excluded directories.
for d in "${EXCLUDES[@]}"; do
    find_args+=( ! -path "$d/*" )
done

# Skip object files, ISOs, images, binaries, etc. We use extension matching.
find_args+=(
    ! -name "*.o"
    ! -name "*.elf"
    ! -name "*.bin"
    ! -name "*.iso"
    ! -name "*.img"
    ! -name "*.a"
    ! -name "*.so"
    ! -name "*.d"
    ! -name "*.png"
    ! -name "*.jpg"
    ! -name "*.jpeg"
    ! -name "*.gif"
    ! -name "*.bmp"
    ! -name "*.ico"
    ! -name "*.ttf"
    ! -name "*.woff"
    ! -name "*.woff2"
    ! -name "*.zip"
    ! -name "*.tar"
    ! -name "*.gz"
    ! -name "*.xz"
    ! -name "*.7z"
)

# Match only the extensions we want.
if [[ ${#CODE_EXTS[@]} -gt 0 ]]; then
    ext_cond=( -false )
    for ext in "${CODE_EXTS[@]}"; do
        ext_cond+=( -o -name "*.$ext" )
    done
    # Wrap in parentheses: ( -name "*.c" -o -name "*.h" ... )
    find_args+=( \( "${ext_cond[@]}" \) )
fi

# --- Collect files ----------------------------------------------------------
mapfile -t FILES < <(find "${find_args[@]}" | sort)

if [[ ${#FILES[@]} -eq 0 ]]; then
    echo "No source files found."
    exit 0
fi

# --- Count ------------------------------------------------------------------
TOTAL_LINES=0
TOTAL_FILES=0

# Associative array: directory -> lines
declare -A DIR_LINES
declare -A DIR_FILES

for f in "${FILES[@]}"; do
    # wc -l on binary-ish files can still work, but we already excluded them.
    lines=$(wc -l < "$f" 2>/dev/null || echo 0)
    # Strip leading whitespace
    lines=$((lines))

    dir=$(dirname "$f")
    # Normalize "dir" for the current directory
    [[ "$dir" == "." ]] && dir="."

    DIR_LINES["$dir"]=$(( ${DIR_LINES["$dir"]:-0} + lines ))
    DIR_FILES["$dir"]=$(( ${DIR_FILES["$dir"]:-0} + 1 ))

    TOTAL_LINES=$((TOTAL_LINES + lines))
    TOTAL_FILES=$((TOTAL_FILES + 1))
done

# --- Output -----------------------------------------------------------------
if [[ $BY_FILE -eq 1 ]]; then
    echo "Per-file line counts:"
    echo "--------------------------------------------------------------"
    printf "%-55s %10s\n" "FILE" "LINES"
    printf "%-55s %10s\n" "----" "-----"
    for f in "${FILES[@]}"; do
        lines=$(wc -l < "$f" 2>/dev/null || echo 0)
        printf "%-55s %10d\n" "$f" "$lines"
    done
    echo "--------------------------------------------------------------"
    echo
fi

echo "Lines by directory:"
echo "--------------------------------------------------------------"
printf "%-55s %10s %10s\n" "DIRECTORY" "FILES" "LINES"
printf "%-55s %10s %10s\n" "---------" "-----" "-----"
# Sort by directory path for stable output
for dir in $(printf '%s\n' "${!DIR_LINES[@]}" | sort); do
    printf "%-55s %10d %10d\n" "$dir" "${DIR_FILES[$dir]}" "${DIR_LINES[$dir]}"
done
echo "--------------------------------------------------------------"

echo
printf "TOTAL: %d files, %d lines\n" "$TOTAL_FILES" "$TOTAL_LINES"