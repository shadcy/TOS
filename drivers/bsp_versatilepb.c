/**
 * @file    bsp_versatilepb.c
 * @author  shadcy
 * @brief   Board Support Package (BSP) Implementation for ARM VersatilePB.
 *
 * Part of the STAX Operating System.
 *
 * @license GPL-3.0-or-later
 * Copyright (c) 2026 Shreyash Wanjari (Shadcy)
 */

#include "bsp.h"
#include "framebuffer.h"
#include "timer.h"
#include "vic.h"
#include "irq.h"
#include "string.h"

/* Global boot info instance */
boot_info_t g_boot_info;

#define UART0_BASE   0x101f1000UL
#define UART_DR      (*(volatile unsigned int *)(UART0_BASE + 0x000))
#define UART_FR      (*(volatile unsigned int *)(UART0_BASE + 0x018))
#define UART_IBRD    (*(volatile unsigned int *)(UART0_BASE + 0x024))
#define UART_FBRD    (*(volatile unsigned int *)(UART0_BASE + 0x028))
#define UART_LCRH    (*(volatile unsigned int *)(UART0_BASE + 0x02C))
#define UART_CR      (*(volatile unsigned int *)(UART0_BASE + 0x030))

const char *bsp_get_name(void) {
    return "ARM VersatilePB (ARM926EJ-S)";
}

void bsp_uart_init(void) {
    UART_CR = 0;
    UART_IBRD = 13;
    UART_FBRD = 1;
    UART_LCRH = (0x3 << 5) | (1 << 4);
    UART_CR = (1 << 0) | (1 << 8) | (1 << 9);
}

void bsp_uart_putc(char c) {
    if (c == '\n') {
        while (UART_FR & (1 << 5));
        UART_DR = '\r';
    }
    while (UART_FR & (1 << 5));
    UART_DR = (unsigned int)c;
}

void bsp_uart_puts(const char *s) {
    while (*s) {
        bsp_uart_putc(*s++);
    }
}

int bsp_uart_getc(void) {
    if (UART_FR & (1 << 4)) {
        return -1;
    }
    return (int)(UART_DR & 0xFF);
}

void bsp_timer_init(uint32_t freq_hz, void (*tick_cb)(void)) {
    (void)tick_cb;
    uint32_t interval_us = 1000;
    if (freq_hz > 0) {
        interval_us = 1000000 / freq_hz;
    }
    timer_init(interval_us);
}

void bsp_timer_ack(void) {
    timer_ack();
}

uint64_t bsp_get_monotonic_us(void) {
    extern volatile unsigned int tick_count;
    return (uint64_t)tick_count * 1000ULL;
}

void bsp_irq_init(void) {
    vic_init();
}

void bsp_irq_enable(int irq_num) {
    vic_enable_source((uint32_t)irq_num);
}

void bsp_irq_disable(int irq_num) {
    vic_disable_source((uint32_t)irq_num);
}

void bsp_irq_ack(int irq_num) {
    (void)irq_num;
    vic_acknowledge();
}

int bsp_display_init(boot_fb_info_t *out_fb) {
    if (fb_init() != 0) return -1;
    if (out_fb) {
        out_fb->phys_base = 0x10120000;
        out_fb->width = fb_width;
        out_fb->height = fb_height;
        out_fb->pitch = fb_width * 2;
        out_fb->bpp = 16;
    }
    return 0;
}

void bsp_display_swap(void) {
    /* Direct linear frame-buffer refresh */
}

void bsp_early_init(boot_info_t *bi) {
    bsp_uart_init();
    if (bi) {
        memcpy(&g_boot_info, bi, sizeof(boot_info_t));
    } else {
        memset(&g_boot_info, 0, sizeof(boot_info_t));
        g_boot_info.magic = STAX_BOOT_MAGIC;
        g_boot_info.version = STAX_BOOT_VERSION;
        g_boot_info.arch = BOOT_ARCH_ARM32_VERSATILEPB;
        g_boot_info.mem_region_count = 1;
        g_boot_info.mem_regions[0].phys_start = 0x00000000;
        g_boot_info.mem_regions[0].size_bytes = 32 * 1024 * 1024;
        g_boot_info.mem_regions[0].type = MEM_TYPE_USABLE;
    }
}

void bsp_reboot(void) {
    /* ARM soft reset */
    void (*reset)(void) = (void (*)(void))0x00000000;
    reset();
    while (1);
}

void bsp_poweroff(void) {
    bsp_uart_puts("\n[BSP] System halted.\n");
    while (1) {
        uint32_t zero = 0;
        __asm__ volatile("mcr p15, 0, %0, c7, c0, 4" : : "r"(zero));
    }
}
