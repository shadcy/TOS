#include "app_terminal.h"
#include "wm.h"
#include "framebuffer.h"
#include "font.h"
#include "heap.h"
#include "string.h"
#include "command.h"
#include "console.h"
#include "pty.h"

#define TERM_COLS       80
#define TERM_ROWS       64
#define TERM_INLEN      128
#define TERM_HIST_MAX   16

typedef struct {
    char     text[TERM_ROWS][TERM_COLS];
    uint16_t color[TERM_ROWS][TERM_COLS];
    int      head;                  /* Current write row index */
    int      cur_x;                 /* Write column on head row */
    uint16_t cur_col;               /* Current active text color */
    
    char     input[TERM_INLEN];
    int      input_pos;
    
    /* Command History */
    char     history[TERM_HIST_MAX][TERM_INLEN];
    int      history_count;
    int      history_idx;
    
    /* ANSI state machine */
    int      ansi_state;            /* 0=normal, 1=ESC, 2=bracket */
    int      ansi_param;
    
    int      blink_n;
    int      cur_on;

    pty_t   *pty;                   /* Attached POSIX pseudo-terminal device */
} terminal_state_t;

/* Advance to the next line in circular ring buffer */
static void term_advance(terminal_state_t *st) {
    st->head = (st->head + 1) % TERM_ROWS;
    st->cur_x = 0;
    for (int c = 0; c < TERM_COLS; c++) {
        st->text[st->head][c]  = 0;
        st->color[st->head][c] = st->cur_col;
    }
}

/* Output single character into terminal ring buffer */
static void term_putc(terminal_state_t *st, char c, uint16_t default_col) {
    /* Basic ANSI escape code parser for color formatting */
    if (st->ansi_state == 0) {
        if (c == 0x1B) {
            st->ansi_state = 1;
            return;
        }
    } else if (st->ansi_state == 1) {
        if (c == '[') {
            st->ansi_state = 2;
            st->ansi_param = 0;
            return;
        }
        st->ansi_state = 0;
    } else if (st->ansi_state == 2) {
        if (c >= '0' && c <= '9') {
            st->ansi_param = st->ansi_param * 10 + (c - '0');
            return;
        } else if (c == 'm') {
            if (st->ansi_param == 0) st->cur_col = COLOR_WHITE;
            else if (st->ansi_param == 31) st->cur_col = rgb565(255, 85, 85);       /* Red / Error */
            else if (st->ansi_param == 32) st->cur_col = theme_get_primary_accent();  /* Dynamic Active Theme Primary */
            else if (st->ansi_param == 33) st->cur_col = rgb565(250, 215, 90);      /* Yellow / Warning */
            else if (st->ansi_param == 34) st->cur_col = rgb565(90, 150, 255);      /* Deep Blue */
            else if (st->ansi_param == 35) st->cur_col = theme_get_secondary_accent();/* Dynamic Active Theme Secondary */
            else if (st->ansi_param == 36) st->cur_col = rgb565(100, 220, 255);     /* Cyan */
            else if (st->ansi_param == 37) st->cur_col = COLOR_WHITE;
            st->ansi_state = 0;
            return;
        } else {
            st->ansi_state = 0;
            return;
        }
    }

    uint16_t col = (st->cur_col != COLOR_WHITE) ? st->cur_col : default_col;

    if (c == '\n') {
        term_advance(st);
    } else if (c == '\r') {
        st->cur_x = 0;
    } else if (c == '\t') {
        int next_tab = (st->cur_x + 8) & ~7;
        while (st->cur_x < next_tab && st->cur_x < TERM_COLS) {
            st->text[st->head][st->cur_x]  = ' ';
            st->color[st->head][st->cur_x] = col;
            st->cur_x++;
        }
        if (st->cur_x >= TERM_COLS) term_advance(st);
    } else if (c == '\b' || c == 0x7F) {
        if (st->cur_x > 0) {
            st->cur_x--;
            st->text[st->head][st->cur_x] = 0;
        }
    } else if ((unsigned char)c >= 32) {
        if (st->cur_x >= TERM_COLS) {
            term_advance(st);
        }
        st->text[st->head][st->cur_x]  = c;
        st->color[st->head][st->cur_x] = col;
        st->cur_x++;
    }
}

static void term_puts(terminal_state_t *st, const char *s, uint16_t col) {
    while (*s) term_putc(st, *s++, col);
}

static void term_clear(terminal_state_t *st) {
    st->head = 0;
    st->cur_x = 0;
    st->cur_col = COLOR_WHITE;
    st->ansi_state = 0;
    for (int r = 0; r < TERM_ROWS; r++) {
        for (int c = 0; c < TERM_COLS; c++) {
            st->text[r][c]  = 0;
            st->color[r][c] = COLOR_WHITE;
        }
    }
}

static void pty_terminal_on_output(pty_t *pty, const char *buf, size_t len) {
    terminal_state_t *st = (terminal_state_t *)pty->user_data;
    if (st && buf) {
        for (size_t i = 0; i < len; i++) {
            term_putc(st, buf[i], COLOR_WHITE);
        }
    }
}

static void term_init(terminal_state_t *st) {
    term_clear(st);
    st->input_pos = 0;
    st->input[0] = '\0';
    st->history_count = 0;
    st->history_idx = 0;
    st->blink_n = 0;
    st->cur_on = 1;

    /* Allocate dedicated POSIX Pseudo-TTY pair */
    st->pty = pty_create();
    if (st->pty) {
        st->pty->user_data = st;
        st->pty->on_output = pty_terminal_on_output;
    }

    /* Clean, simple welcome message using natural font */
    term_puts(st, "STAX Terminal\n", rgb565(240, 243, 250));
    term_puts(st, "Type 'help' for available commands.\n\n", rgb565(140, 145, 160));
}

/* Redirection hook for console output */
static void term_console_hook(char c, void *ctx) {
    terminal_state_t *st = (terminal_state_t *)ctx;
    if (st) {
        term_putc(st, c, COLOR_WHITE);
    }
}

void terminal_draw_window(struct window *win, int cx, int cy, int cw, int ch) {
    terminal_state_t *st = (terminal_state_t *)win->app_data;
    if (!st) {
        st = (terminal_state_t *)kmalloc(sizeof(terminal_state_t));
        if (!st) {
            fb_fillrect(cx, cy, cw, ch, rgb565(80, 0, 0));
            draw_text(cx + 8, cy + 8, "Out of Memory", COLOR_WHITE);
            return;
        }
        term_init(st);
        win->app_data = st;
    }

    uint16_t theme_pri = theme_get_primary_accent();

    /* Terminal Window Background (Clean Dark Slate) */
    fb_fillrect(cx, cy, cw, ch, rgb565(15, 17, 23));

    /* Cursor blink cadence */
    if (++st->blink_n >= 25) {
        st->blink_n = 0;
        st->cur_on = !st->cur_on;
    }

    int text_margin_x = cx + 12;
    int clip_top = cy + 6;
    int clip_bot = cy + ch - 30;

    int text_area_h = ch - 36;
    int max_rows = text_area_h / 16;
    if (max_rows > TERM_ROWS) max_rows = TERM_ROWS;

    /* Render ring buffer ending at st->head using natural font */
    for (int r = 0; r < max_rows; r++) {
        int ring_row = (st->head - (max_rows - 1 - r) + TERM_ROWS * 2) % TERM_ROWS;
        int py = cy + 8 + r * 16;
        int cur_x_pos = text_margin_x;

        for (int c = 0; c < TERM_COLS; c++) {
            char ch_val = st->text[ring_row][c];
            if (!ch_val) break;
            if (ch_val >= 32 && ch_val <= 126) {
                int ch_w = font_get_char_width(ch_val, FONT_STYLE_REGULAR);
                if (cur_x_pos + ch_w > cx + cw - 10) break;
                font_draw_char_clipped(cur_x_pos, py, ch_val, st->color[ring_row][c], FONT_STYLE_REGULAR, cx + 6, clip_top, cx + cw - 6, clip_bot);
                cur_x_pos += ch_w;
            }
        }
    }

    /* Bottom Command Input Bar */
    int bar_y = cy + ch - 26;
    fb_fillrect(cx, bar_y, cw, 26, rgb565(19, 22, 30));
    fb_drawline(cx, bar_y, cx + cw - 1, bar_y, rgb565(32, 36, 48));

    /* Clean, simple prompt */
    int px = cx + 12, py2 = bar_y + 5;
    const char *prompt = "stax:~$ ";
    font_draw_text(px, py2, prompt, theme_pri, FONT_STYLE_REGULAR);
    px += font_get_string_width(prompt, FONT_STYLE_REGULAR);

    /* User Input Buffer */
    for (int i = 0; i < st->input_pos; i++) {
        char in_c = st->input[i];
        int ch_w = font_get_char_width(in_c, FONT_STYLE_REGULAR);
        font_draw_char_clipped(px, py2, in_c, COLOR_WHITE, FONT_STYLE_REGULAR, cx, bar_y, cx + cw - 10, cy + ch);
        px += ch_w;
    }

    /* Clean vertical cursor line */
    if (st->cur_on) {
        fb_fillrect(px, py2 + 1, 2, 13, theme_pri);
    }
}

/* Key Event Handler */
void terminal_key_event(struct window *win, char c) {
    terminal_state_t *st = (terminal_state_t *)win->app_data;
    if (!st) return;

    if (st->pty) {
        pty_set_active(st->pty);
    }

    if (c == '\r' || c == '\n') {
        st->input[st->input_pos] = '\0';

        /* Echo typed command with dynamic theme accent */
        term_puts(st, "stax:~$ ", theme_get_primary_accent());
        term_puts(st, st->input, COLOR_WHITE);
        term_putc(st, '\n', COLOR_WHITE);

        if (st->input_pos > 0) {
            /* Add to history */
            if (st->history_count < TERM_HIST_MAX) {
                strncpy(st->history[st->history_count++], st->input, TERM_INLEN);
            } else {
                for (int i = 0; i < TERM_HIST_MAX - 1; i++) {
                    strncpy(st->history[i], st->history[i + 1], TERM_INLEN);
                }
                strncpy(st->history[TERM_HIST_MAX - 1], st->input, TERM_INLEN);
            }
            st->history_idx = st->history_count;

            /* Check for clear/cls command */
            if (strcmp(st->input, "clear") == 0 || strcmp(st->input, "cls") == 0) {
                term_clear(st);
            } else {
                /* Route active PTY and console hook to this terminal instance */
                if (st->pty) pty_set_active(st->pty);
                console_set_hook(term_console_hook, st);
                command_process(st->input);
                console_set_hook(NULL, NULL);
            }
        }
        st->input_pos = 0;
        st->input[0] = '\0';

    } else if (c == '\b' || c == 0x7F) {
        if (st->input_pos > 0) {
            st->input[--st->input_pos] = '\0';
        }
    } else if (c >= 32 && c <= 126 && st->input_pos < TERM_INLEN - 1) {
        st->input[st->input_pos++] = c;
        st->input[st->input_pos]   = '\0';
    }
}

static int g_term_counter = 0;

struct window *terminal_open_new(void) {
    int idx = g_term_counter++;
    int ox = 70 + (idx % 6) * 30;
    int oy = 48 + (idx % 6) * 30;
    char title[32];
    strcpy(title, "Terminal");
    if (idx > 0) {
        int tlen = 8;
        title[tlen++] = ' ';
        title[tlen++] = '#';
        title[tlen++] = '1' + (idx % 9);
        title[tlen] = '\0';
    }
    struct window *tw = wm_add_window(ox, oy, 560, 360, title, terminal_draw_window);
    if (tw) {
        tw->key_event = terminal_key_event;
    }
    return tw;
}

