/**
 * @file    bsp.h
 * @author  shadcy
 * @brief   Board Support Package (BSP) Hardware Abstraction Interface.
 *
 * Decouples platform-specific hardware logic (QEMU VersatilePB, Raspberry Pi,
 * STM32/i.MX RT Hardware Wallet, x86_64 UEFI) from the core STAX OS kernel.
 *
 * Part of the STAX Operating System.
 *
 * @license GPL-3.0-or-later
 * Copyright (c) 2026 Shreyash Wanjari (Shadcy)
 */

#ifndef BSP_H
#define BSP_H

#include "boot_info.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Early initialization called before MMU/scheduler */
void bsp_early_init(boot_info_t *bi);

/* Architecture / Board Name */
const char *bsp_get_name(void);

/* Low-level UART & Debug Console */
void bsp_uart_init(void);
void bsp_uart_putc(char c);
void bsp_uart_puts(const char *s);
int  bsp_uart_getc(void);

/* Hardware Timers & Real-Time Clock */
void bsp_timer_init(uint32_t freq_hz, void (*tick_cb)(void));
void bsp_timer_ack(void);
uint64_t bsp_get_monotonic_us(void);

/* Interrupt Subsystem */
void bsp_irq_init(void);
void bsp_irq_enable(int irq_num);
void bsp_irq_disable(int irq_num);
void bsp_irq_ack(int irq_num);

/* Display & Linear Framebuffer */
int  bsp_display_init(boot_fb_info_t *out_fb);
void bsp_display_swap(void);

/* System Power & Reset */
void bsp_reboot(void) __attribute__((noreturn));
void bsp_poweroff(void) __attribute__((noreturn));

#ifdef __cplusplus
}
#endif

#endif /* BSP_H */
