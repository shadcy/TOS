#!/usr/bin/env python3
"""
STAX — StackSans Typography Generator
Generates anti-aliased C glyph atlas for StackSans (Bold, Regular, Light, Mono)
"""
import os
import sys
from PIL import Image, ImageFont, ImageDraw

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FONTS_DIR = os.path.join(ROOT_DIR, "assets", "fonts")
GFX_DIR = os.path.join(ROOT_DIR, "gfx")
INC_DIR = os.path.join(ROOT_DIR, "include")

def generate_glyph_table(font_path, font_size, name_prefix, is_mono=False, mono_width=8, y_offset=-1, contrast_boost=False):
    font = ImageFont.truetype(font_path, font_size)
    GLYPH_H = 16
    GLYPH_MAX_W = 16
    
    glyph_arrays = []
    glyph_entries = []
    
    for c in range(32, 127):
        char_str = chr(c)
        if is_mono:
            adv_float = font.getlength(char_str)
            img = Image.new('L', (mono_width, GLYPH_H), color=0)
            draw = ImageDraw.Draw(img)
            off_x = (mono_width - adv_float) / 2.0
            draw.text((off_x, y_offset), char_str, font=font, fill=255)
            w = mono_width
            adv = mono_width
        else:
            length = font.getlength(char_str)
            adv = max(int(round(length)), 3)
            if c == 32:
                adv = 4
            img = Image.new('L', (GLYPH_MAX_W, GLYPH_H), color=0)
            draw = ImageDraw.Draw(img)
            draw.text((0, y_offset), char_str, font=font, fill=255)
            bbox = font.getbbox(char_str)
            if bbox:
                w = max(bbox[2], adv)
            else:
                w = adv
            if w > GLYPH_MAX_W:
                w = GLYPH_MAX_W
            if w < 1:
                w = 1
        
        array_name = f"glyph_{name_prefix}_{c}"
        pixels = []
        for y in range(GLYPH_H):
            for x in range(w):
                raw = img.getpixel((x, y))
                if contrast_boost and raw > 0:
                    val = int(min(255, (pow(raw / 255.0, 0.75) * 255.0 * 1.15)))
                else:
                    val = raw
                pixels.append(f"{val:3d}")
        
        arr_str = f"static const uint8_t {array_name}[] = {{\n  " + ", ".join(pixels) + "\n};"
        glyph_arrays.append(arr_str)
        glyph_entries.append(f"  [{c}] = {{ .width = {w}, .height = {GLYPH_H}, .advance = {adv}, .alpha = {array_name} }}")
    
    return glyph_arrays, glyph_entries

def main():
    bold_path = os.path.join(FONTS_DIR, "StackSansHeadline-Bold.ttf")
    reg_path = os.path.join(FONTS_DIR, "StackSansText-Regular.ttf")
    light_path = os.path.join(FONTS_DIR, "StackSansText-Light.ttf")
    head_reg_path = os.path.join(FONTS_DIR, "StackSansHeadline-Regular.ttf")

    if not os.path.exists(bold_path) or not os.path.exists(reg_path) or not os.path.exists(light_path):
        print(f"Error: Missing font files in {FONTS_DIR}")
        sys.exit(1)

    print("Generating StackSans font atlas...")
    
    # Generate tables
    bold_arrays, bold_entries = generate_glyph_table(bold_path, 13, "bold", is_mono=False, y_offset=-1)
    reg_arrays, reg_entries = generate_glyph_table(reg_path, 13, "reg", is_mono=False, y_offset=-1)
    light_arrays, light_entries = generate_glyph_table(light_path, 13, "light", is_mono=False, y_offset=-1)
    
    # Precise, high-legibility terminal mono glyphs
    mono_font = head_reg_path if os.path.exists(head_reg_path) else reg_path
    mono_arrays, mono_entries = generate_glyph_table(mono_font, 11, "mono", is_mono=True, mono_width=8, y_offset=-1, contrast_boost=True)

    # Write Header file
    header_path = os.path.join(INC_DIR, "stacksans_font_data.h")
    header_content = """/* ============================================================================
 * STAX — stacksans_font_data.h
 * Authentic StackSans Typography Atlas Declarations (Bold, Regular, Light, Mono)
 * ============================================================================ */

#ifndef STACKSANS_FONT_DATA_H
#define STACKSANS_FONT_DATA_H

#include <stdint.h>
#include "font.h"

extern const font_glyph_t stacksans_regular_glyphs[128];
extern const font_glyph_t stacksans_bold_glyphs[128];
extern const font_glyph_t stacksans_light_glyphs[128];
extern const font_glyph_t stacksans_mono_glyphs[128];

#endif /* STACKSANS_FONT_DATA_H */
"""
    with open(header_path, "w") as f:
        f.write(header_content)
    print(f"Wrote {header_path}")

    # Write C source file
    out_c = []
    out_c.append("/* ============================================================================")
    out_c.append(" * STAX — stacksans_font_data.c")
    out_c.append(" * StackSans Vector Glyph Atlas (Bold, Regular, Light, Mono)")
    out_c.append(" * Anti-Aliased 8-bit Alpha Typography with Microsecond Blending")
    out_c.append(" * ============================================================================ */")
    out_c.append("")
    out_c.append("#include <stdint.h>")
    out_c.append("#include \"stacksans_font_data.h\"")
    out_c.append("")
    
    # 1. Bold Glyphs
    out_c.append("/* --- 1. StackSans Headline Bold (Titles, Headers, Window Titlebars) --- */")
    out_c.extend(bold_arrays)
    out_c.append("")
    out_c.append("const font_glyph_t stacksans_bold_glyphs[128] = {")
    out_c.append(",\n".join(bold_entries))
    out_c.append("};")
    out_c.append("")

    # 2. Regular Glyphs
    out_c.append("/* --- 2. StackSans Text Regular (Standard UI, Buttons, Menus) --- */")
    out_c.extend(reg_arrays)
    out_c.append("")
    out_c.append("const font_glyph_t stacksans_regular_glyphs[128] = {")
    out_c.append(",\n".join(reg_entries))
    out_c.append("};")
    out_c.append("")

    # 3. Light Glyphs
    out_c.append("/* --- 3. StackSans Text Light (Body Text, Subtitles, Telemetry Hints) --- */")
    out_c.extend(light_arrays)
    out_c.append("")
    out_c.append("const font_glyph_t stacksans_light_glyphs[128] = {")
    out_c.append(",\n".join(light_entries))
    out_c.append("};")
    out_c.append("")

    # 4. Mono Glyphs
    out_c.append("/* --- 4. StackSans Text Mono (Terminal & Fixed 8x16 Grid Cells) --- */")
    out_c.extend(mono_arrays)
    out_c.append("")
    out_c.append("const font_glyph_t stacksans_mono_glyphs[128] = {")
    out_c.append(",\n".join(mono_entries))
    out_c.append("};")
    out_c.append("")

    c_path = os.path.join(GFX_DIR, "stacksans_font_data.c")
    with open(c_path, "w") as f:
        f.write("\n".join(out_c))
    print(f"Wrote {c_path} successfully.")

if __name__ == "__main__":
    main()
