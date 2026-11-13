#!/bin/bash
set -e
cd "$(dirname "$0")/.."

python3 tools/png2c.py tools/Sprites \
    kernel/desktop/de_sprites.c \
    include/quark/de_sprites.h

echo "done"