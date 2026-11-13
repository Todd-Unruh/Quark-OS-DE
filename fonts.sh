#!/bin/bash
set -e
FONT=tools/fonts/static/Inter_18pt-Regular.ttf
python3 tools/gen_font.py "$FONT" kernel/desktop/de_font_data_body.c  de_font_body  9  32 126
python3 tools/gen_font.py "$FONT" kernel/desktop/de_font_data_title.c de_font_title 13 32 126
echo "done"