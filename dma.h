/**
 * @file dma.h
 * @author ztnel (christian.sargusingh@gbatteries.com)
 * @brief 
 * @version 0.1
 * @date 2022-04
 * 
 * @copyright Copyright © 2022 Christian Sargusingh
 * 
 * This is free and unencumbered software released into the public domain.
 * Anyone is free to copy, modify, publish, use, compile, sell, or distribute this software, either 
 * in source code form or as a compiled binary, for any purpose, commercial or non-commercial, 
 * and by any means. In jurisdictions that recognize copyright laws, the author or authors of this 
 * software dedicate any and all copyright interest in thesoftware to the public domain. We make 
 * this dedication for the benefitof the public at large and to the detriment of our heirs and
 * successors. We intend this dedication to be an overt act ofrelinquishment in perpetuity of all 
 * present and future rights to thissoftware under copyright law.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,EXPRESS OR IMPLIED, INCLUDING BUT
 * NOT LIMITED TO THE WARRANTIES OFMERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND 
 * NONINFRINGEMENT.IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OROTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,ARISING FROM, OUT OF OR IN CONNECTION WITH 
 * THE SOFTWARE OR THE USE OROTHER DEALINGS IN THE SOFTWARE.
 * For more information, please refer to <http://unlicense.org/>
 */

#ifndef DMA_H_
#define DMA_H_

#include <stdint.h>
#include <stdlib.h>

/*
 * Check more about Raspberry Pi's peripheral mapping at:
 * https://datasheets.raspberrypi.com/bcm2711/bcm2711-peripherals.pdf
 */
#define PAGE_SIZE               4096
#define BCM2711_PERI_BASE       0xFE000000
#define BCM2711_PERI_BUS_BASE   0x7E000000
#define BUS_TO_PHYS(x)          ((x) & 0x3FFFFFFF)

#define PWM_BASE                (BCM2711_PERI_BASE + 0x0020C000)
#define PWM_BUS_BASE            (BCM2711_PERI_BUS_BASE + 0x0020C000)
#define PWM_FIFO                (PWM_BUS_BASE + 0x18)
#define PWM_LEN                 0x28

#define GPIO_CLK_BASE           (BCM2711_PERI_BASE + 0x00101000)
#define GPIO_CLK_LEN            0xA8
#define GPIO_CLK_PWM            0xA0

// NOTE: the system timer base is defined relative to the bus address not the physical!
#define SYST_BASE               (BCM2711_PERI_BUS_BASE + 0x00003000)
#define SYST_LEN                0x1C
#define SYST_CLO                (SYST_BASE + 0x04)

#define DMA_BASE                (BCM2711_PERI_BASE + 0x00007000)
#define DMA_OFFSET              0x100
#define DMA_CHANNEL             6

/**
 * @brief GPIO Clock manager control and status bits
 * 5.4. General Purpose GPIO Clocks
 */
#define CM_PASSWD               (0x5A << 24)
#define CLK_CTL_KILL            (CM_PASSWD | (1 << 5))
#define CLK_CTL_BUSY            (CM_PASSWD | (1 << 7))
#define CLK_CTL_ENAB            (CM_PASSWD | (1 << 4))
#define CLK_DIV_DIVI(x)         (CM_PASSWD | ((x) << 12))
#define CLK_CTL_SRC_PLLD        (CM_PASSWD | 6)
#define CLK_CTL_SRC_OSC         (CM_PASSWD | 1)
#define CLK_DIVI                5
#define CLK_MICROS              1

/**
 * @brief PWM Register control and status bits
 * 8.6. Control and Status Registers
 */
#define PWM_DMAC_ENAB           (1 << 31)
#define PWM_DMAC_PANIC(x)       ((x) << 8)
#define PWM_CTL_CLRF            (1 << 6)
#define PWM_CTL_USEF            (1 << 5)
#define PWM_CTL_MODE1           (1 << 1)
#define PWM_CTL_PWEN1           (1 << 0)


/**
 * @brief DMA CS Control and Status bits
 * 
 */
#define DMA_ENABLE              (0xFF0 / 4)
#define DMA_CHANNEL_RESET       (1 << 31)
#define DMA_CHANNEL_ABORT       (1 << 30)
#define DMA_WAIT_ON_WRITES      (1 << 28)
#define DMA_PANIC_PRIORITY(x)   ((x) << 20)
#define DMA_PRIORITY(x)         ((x) << 16)
#define DMA_INTERRUPT_STATUS    (1 << 2)
#define DMA_END_FLAG            (1 << 1)
#define DMA_ACTIVE              (1 << 0)
#define DMA_DISDEBUG            (1 << 28)

/**
 * @brief DMA control block transfer info field bits
 * Table 40. 0_TI, 1_TI, ..., 5_TI, 6_TI Registers
 */
#define DMA_NO_WIDE_BURSTS      (1 << 26)
#define DMA_PERI_MAPPING(x)     ((x) << 16)
#define DMA_BURST_LENGTH(x)     ((x) << 12)
#define DMA_SRC_DREQ            (1 << 10)
#define DMA_DEST_DREQ           (1 << 6)
#define DMA_WAIT_RESP           (1 << 3)

/**
 * @brief Constants for the mailbox interface
 * https://github.com/raspberrypi/firmware/wiki/Mailbox-property-interface
 */
#define MAJOR_NUM               100
#define IOCTL_MBOX_PROPERTY     _IOWR(MAJOR_NUM, 0, char *)
#define DEVICE_FILE_NAME        "/dev/vcio"
#define MEM_FLAG_DIRECT         (1 << 2)
#define MEM_FLAG_COHERENT       (2 << 2)
#define MEM_FLAG_L1_NONALLOC    (MEM_FLAG_DIRECT | MEM_FLAG_COHERENT)

/**
 * @brief DMA control channel specification.
 */
typedef struct dma_reg_t {
  uint32_t cs;      // control / status register
  uint32_t cb;      // control block address 
} dma_reg_t;

/**
 * @brief GPIO Clock register specification:
 * 5.4. General Purpose GPIO Clocks (Table 98. General Purpose Clocks Registers)
 */
typedef struct clk_reg_t {
  uint32_t ctl;     // control resgister
  uint32_t div;     // divisor register
} clk_reg_t;

/**
 * @brief PWM control channel specification:
 * 8.6. Control and Status Registers (Table 152. PWM Register Map)
 */
typedef struct pwm_reg_t {
  uint32_t ctl;     // control register
  uint32_t sta;     // status register
  uint32_t dmac;    // pwm dma configuration
  uint32_t rsv1;    // 4 byte padding
  uint32_t rng1;    // pwm channel 1 range
  uint32_t dat1;    // pwm channel 1 data
  uint32_t fifo;    // pwm fifo input
  uint32_t rsv2;    // 4 byte padding
  uint32_t rng2;    // pwm channel 2 range
  uint32_t dat2;    // pwm channel 2 data
} pwm_reg_t;

/**
 * @brief Standard DMA control block specification.
 * Table 34. DMA Control Block Definition
 */
typedef struct dma_cb_t {
  uint32_t ti;      // transfer information
  uint32_t src;     // source (bus) address
  uint32_t dest;    // destination (bus) address
  uint32_t len;     // transfer length in bytes
  uint32_t stride;  // 2D stride
  uint32_t next;    // next control block (bus) address. 32 byte aligned.
  uint32_t pad[2];  // reserved space
} dma_cb_t;

/**
 * @brief DMA Lite control block specification.
 * Table 35. DMA Lite Control Block Definition
 */
typedef struct dma_lite_cb_t {
  uint32_t ti;      // transfer information
  uint32_t src;     // source (bus) address
  uint32_t dest;    // destination (bus) address
  uint32_t len;     // transfer length in bytes
  uint32_t res[1];  // reserved space
  uint32_t next;    // next control block (bus) address. 32 byte aligned.
  uint32_t pad[2];  // reserved space
} dma_lite_cb_t;

/**
 * @brief DMA4 control block specification.
 * Table 36. DMA4 Control Block Definition
 */
typedef struct dma4_cb_t {
  uint32_t ti;      // transfer information
  uint32_t src;     // source (bus) address
  uint32_t si;      // source information
  uint32_t dest;    // destination (bus) address
  uint32_t di;      // source information
  uint32_t len;     // transfer length in bytes
  uint32_t next;    // next control block (bus) address. 32 byte aligned.
  uint32_t pad[1];  // reserved space
} dma4_cb_t;

typedef struct dma_mem_t {
  void *v_addr;       // virtual address of the page
  uint32_t b_addr;  // bus address of the page (not a ptr since it is not valid in virtual memory)
  uint32_t mb;      // mailbox property interface  
  size_t size;      // size of the page
} dma_mem_t;

dma_mem_t *dma_malloc(size_t size);
void dma_free(dma_mem_t *mem);
void dma_start();
void dma_end();

#endif  // DMA_H_