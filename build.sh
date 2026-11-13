#!/bin/bash

# 1. Bake sprites
./tools/bake_sprites.sh
# (or: python3 tools/png2c.py tools/Sprites kernel/desktop/de_sprites.c include/quark/de_sprites.h)

# 2. Full rebuild
make clean
make

# 3. Run
make run
