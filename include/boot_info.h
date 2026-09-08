/**
 * @file    boot_info.h
 * @author  shadcy
 * @brief   Unified Boot Handoff Structure for STAX OS.
 *
 * Provides a standardized platform-agnostic handshake structure passed
 * from stage-2 bootloaders (Secure Boot, U-Boot, UEFI, or TF-A) to the kernel.
 *
 * Part of the STAX Operating System.
 *
 * @license GPL-3.0-or-later
 * Copyright (c) 2026 Shreyash Wanjari (Shadcy)
 */

#ifndef BOOT_INFO_H
#define BOOT_INFO_H

#include <stdint.h>
#include <stddef.h>

#define STAX_BOOT_MAGIC     0x53544158UL /* "STAX" */
#define STAX_BOOT_VERSION   2

/* Architecture Target IDs */
typedef enum {
    BOOT_ARCH_ARM32_VERSATILEPB = 1, /* ARM926EJ-S QEMU Reference Target */
    BOOT_ARCH_ARM64_RPI4        = 2, /* BCM2711 AArch64 Cortex-A72 */
    BOOT_ARCH_CORTEX_M_WALLET   = 3, /* STM32H7 / i.MX RT Embedded Hardware Wallet */
    BOOT_ARCH_X86_64_UEFI       = 4, /* Modern 64-bit UEFI PC / Bare-Metal VM */
} boot_arch_t;

/* Memory region types */
typedef enum {
    MEM_TYPE_USABLE             = 1,
    MEM_TYPE_RESERVED           = 2,
    MEM_TYPE_ACPI_RECLAIMABLE   = 3,
    MEM_TYPE_NVS                = 4,
    MEM_TYPE_MMIO               = 5,
    MEM_TYPE_FRAMEBUFFER        = 6,
} mem_region_type_t;

typedef struct {
    uint64_t            phys_start;
    uint64_t            size_bytes;
    mem_region_type_t   type;
    uint32_t            flags;
} boot_mem_region_t;

#define MAX_BOOT_MEM_REGIONS 16

/* Framebuffer descriptor */
typedef struct {
    uint64_t            phys_base;
    uint32_t            width;
    uint32_t            height;
    uint32_t            pitch;       /* Bytes per scanline */
    uint8_t             bpp;         /* Bits per pixel: 16 (RGB565) or 32 (ARGB8888) */
    uint8_t             red_mask_size;
    uint8_t             red_mask_shift;
    uint8_t             green_mask_size;
    uint8_t             green_mask_shift;
    uint8_t             blue_mask_size;
    uint8_t             blue_mask_shift;
} boot_fb_info_t;

/* Unified Boot Handshake Structure */
typedef struct {
    uint32_t            magic;          /* STAX_BOOT_MAGIC */
    uint32_t            version;        /* STAX_BOOT_VERSION */
    boot_arch_t         arch;           /* Architecture profile */
    uint32_t            flags;
    
    /* Memory Map */
    uint32_t            mem_region_count;
    boot_mem_region_t   mem_regions[MAX_BOOT_MEM_REGIONS];
    
    /* Display / Framebuffer */
    boot_fb_info_t      fb;
    
    /* Device Tree or ACPI Table Pointers */
    uint64_t            fdt_phys_addr;  /* Flat Device Tree (.dtb) pointer if ARM/RISC-V */
    uint64_t            acpi_rsdp_addr; /* ACPI RSDP physical address if x86/UEFI */
    
    /* Boot & Firmware Slot State */
    uint32_t            active_slot;    /* 0=Slot A, 1=Slot B */
    uint32_t            slot_version;   /* Running firmware version */
    uint8_t             slot_state;     /* Confirmed / Pending / Testing */
    uint8_t             boot_attempts;
    uint8_t             reserved[2];

    /* Kernel Command Line */
    char                cmdline[256];
} boot_info_t;

extern boot_info_t g_boot_info;

#endif /* BOOT_INFO_H */
