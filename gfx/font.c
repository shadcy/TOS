/* ============================================================================
 * STAX — font.c
 * Authentic StackSans Anti-Aliased Alpha Blending Typography Engine
 * (Zero Cutoffs, Fast Parallel Alpha Blending, Robust Clipping & Microsecond Speed)
 * ============================================================================ */

#include "font.h"
#include "framebuffer.h"
#include "stacksans_font_data.h"
#include "string.h"

void font_init(void) {
    /* StackSans Engine Ready */
}

const font_glyph_t *font_get_glyph(char c, font_style_t style) {
    unsigned char uc = (unsigned char)c;
    if (uc < 32 || uc >= 127) return NULL;
    switch (style) {
        case FONT_STYLE_BOLD:
            return &stacksans_bold_glyphs[uc];
        case FONT_STYLE_LIGHT:
            return &stacksans_light_glyphs[uc];
        case FONT_STYLE_MONO:
            return &stacksans_mono_glyphs[uc];
        case FONT_STYLE_REGULAR:
        default:
            return &stacksans_regular_glyphs[uc];
    }
}

int font_get_char_width(char c, font_style_t style) {
    unsigned char uc = (unsigned char)c;
    if (uc < 32 || uc >= 127) return 8;
    if (style == FONT_STYLE_MONO) {
        return 8;
    }
    const font_glyph_t *g = font_get_glyph(c, style);
    return g ? g->advance : 8;
}

int font_get_height(font_style_t style) {
    (void)style;
    return 16;
}

int font_get_string_width(const char *str, font_style_t style) {
    if (!str) return 0;
    int w = 0;
    while (*str) {
        w += font_get_char_width(*str, style);
        str++;
    }
    return w;
}

void font_draw_char_clipped(int x, int y, char c, uint16_t color, font_style_t style,
                            int min_x, int min_y, int max_x, int max_y) {
    uint16_t *buf = fb_get_buffer();
    if (!buf) return;
    unsigned char uc = (unsigned char)c;
    if (uc < 32 || uc >= 127) return;

    if (y + 16 <= min_y || y >= max_y) return;

    const font_glyph_t *g = font_get_glyph(c, style);
    if (!g) return;
    
    int gw = g->width;
    int gh = g->height;
    const uint8_t *alpha_ptr = g->alpha;

    if (x + gw > min_x && x < max_x && alpha_ptr) {
        int r_start = 0;
        int r_end = gh;
        if (y < min_y) r_start = min_y - y;
        if (y + gh > max_y) r_end = max_y - y;

        int c_start = 0;
        int c_end = gw;
        if (x < min_x) c_start = min_x - x;
        if (x + gw > max_x) c_end = max_x - x;

        for (int r = r_start; r < r_end; r++) {
            int py = y + r;
            uint16_t *line_ptr = buf + py * fb_width;
            const uint8_t *a_row = alpha_ptr + r * gw;
            for (int col = c_start; col < c_end; col++) {
                uint8_t a = a_row[col];
                if (a > 8) {
                    line_ptr[x + col] = blend_rgb565(line_ptr[x + col], color, a);
                }
            }
        }
    }
}

void font_draw_char(int x, int y, char c, uint16_t color, font_style_t style) {
    font_draw_char_clipped(x, y, c, color, style, 0, 0, (int)fb_width, (int)fb_height);
}

int font_draw_text_clipped(int x, int y, const char *str, uint16_t color, font_style_t style,
                           int min_x, int min_y, int max_x, int max_y) {
    uint16_t *buf = fb_get_buffer();
    if (!str || !buf) return 0;
    if (y + 16 <= min_y || y >= max_y) return 0;
    
    int cur_x = x;
    const unsigned char *p = (const unsigned char *)str;
    
    while (*p) {
        unsigned char c = *p++;
        if (c < 32 || c >= 127) {
            cur_x += (style == FONT_STYLE_MONO) ? 8 : 4;
            continue;
        }

        const font_glyph_t *g = font_get_glyph(c, style);
        if (!g) {
            cur_x += 8;
            continue;
        }
        
        int gw = g->width;
        int gh = g->height;
        const uint8_t *alpha_ptr = g->alpha;
        
        if (cur_x + gw > min_x && cur_x < max_x && alpha_ptr) {
            int r_start = 0;
            int r_end = gh;
            if (y < min_y) r_start = min_y - y;
            if (y + gh > max_y) r_end = max_y - y;

            int c_start = 0;
            int c_end = gw;
            if (cur_x < min_x) c_start = min_x - cur_x;
            if (cur_x + gw > max_x) c_end = max_x - cur_x;

            for (int r = r_start; r < r_end; r++) {
                int py = y + r;
                uint16_t *line_ptr = buf + py * fb_width;
                const uint8_t *a_row = alpha_ptr + r * gw;
                for (int col = c_start; col < c_end; col++) {
                    uint8_t a = a_row[col];
                    if (a > 8) {
                        line_ptr[cur_x + col] = blend_rgb565(line_ptr[cur_x + col], color, a);
                    }
                }
            }
        }
        cur_x += g->advance;
        if (cur_x >= max_x) break;
    }
    
    return cur_x - x;
}

int font_draw_text(int x, int y, const char *str, uint16_t color, font_style_t style) {
    return font_draw_text_clipped(x, y, str, color, style, 0, 0, (int)fb_width, (int)fb_height);
}
