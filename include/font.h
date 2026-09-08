/* ============================================================================
 * STAX — font.h
 * High-Performance Anti-Aliased StackSans Vector Typography Engine
 * (Bold Headlines, Regular UI, Light Text & Crisp Monospaced Terminal)
 * ============================================================================ */

#ifndef GFX_FONT_H
#define GFX_FONT_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    FONT_STYLE_REGULAR = 0, /* StackSans Text Regular */
    FONT_STYLE_BOLD    = 1, /* StackSans Headline Bold (Titles / Headers / Badges) */
    FONT_STYLE_LIGHT   = 2, /* StackSans Text Light (Body Text / Subtitles / Hints) */
    FONT_STYLE_MONO    = 3  /* StackSans Text Regular (Fixed 8x16 Grid for Terminal / Code) */
} font_style_t;

typedef struct {
    uint8_t width;
    uint8_t height;
    uint8_t advance;
    const uint8_t *alpha;   /* 8-bit anti-aliased alpha (0..255) */
} font_glyph_t;

/* Initialize font subsystem */
void font_init(void);

/* Fast 16-bit RGB565 Alpha Blending */
static inline uint16_t blend_rgb565(uint16_t bg, uint16_t fg, uint8_t alpha) {
    if (alpha == 0) return bg;
    if (alpha >= 250) return fg;
    
    uint32_t a = alpha >> 3; /* 0..31 */
    uint32_t inv_a = 32 - a;
    
    uint32_t bg_rb = bg & 0xF81F;
    uint32_t bg_g  = bg & 0x07E0;
    
    uint32_t fg_rb = fg & 0xF81F;
    uint32_t fg_g  = fg & 0x07E0;
    
    uint32_t res_rb = ((bg_rb * inv_a + fg_rb * a) >> 5) & 0xF81F;
    uint32_t res_g  = ((bg_g * inv_a + fg_g * a) >> 5) & 0x07E0;
    
    return (uint16_t)(res_rb | res_g);
}

/* Core String & Glyph Drawing APIs */
const font_glyph_t *font_get_glyph(char c, font_style_t style);
void font_draw_char(int x, int y, char c, uint16_t color, font_style_t style);
void font_draw_char_clipped(int x, int y, char c, uint16_t color, font_style_t style, int min_x, int min_y, int max_x, int max_y);
int  font_draw_text(int x, int y, const char *str, uint16_t color, font_style_t style);
int  font_draw_text_clipped(int x, int y, const char *str, uint16_t color, font_style_t style, int min_x, int min_y, int max_x, int max_y);
int  font_get_string_width(const char *str, font_style_t style);
int  font_get_char_width(char c, font_style_t style);
int  font_get_height(font_style_t style);

#ifdef __cplusplus
}
#endif

#endif /* GFX_FONT_H */
