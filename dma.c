/**
 * @file dma.c
 * @author ztnel (christian.sargusingh@gbatteries.com)
 * @brief 
 * @version 0.1
 * @date 2022-04
 * 
 */

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include "dma.h"

static int mailbox_fd = -1;
static dma_mem_t *dma_cbs;
static dma_mem_t *dma_ticks;
static volatile dma_reg_t *dma_reg;

static inline dma_cb_t *get_cb(int offset) { return (dma_cb_t *)(dma_cbs->v_addr) + offset; }
static inline uint32_t get_cb_bus_addr(int offset) { return dma_cbs->b_addr + offset * sizeof(dma_cb_t); }
static inline uint32_t *get_tick_virt_addr(int offset) { return (uint32_t *)dma_ticks->v_addr + offset; }
static inline uint32_t get_tick_bus_addr(int offset) { return dma_ticks->b_addr + offset * sizeof(uint32_t); }

static void *map_mem(uint32_t base, size_t size) {
  int mem_fd;
  printf("Requested base=0x%08x, Requested size=%lu\n", base, size);
  // ensure the base memory is inline with the page size
  const uint32_t offset = base % PAGE_SIZE;
  base = base - offset;
  size = size + offset;
  if ((mem_fd = open("/dev/mem", O_RDWR | O_SYNC)) < 0) {
    perror("Failed to open /dev/mem");
    return NULL;
  }
  printf("base=0x%08x, offset=%i, size=%lu\n", base, offset, size);
  // allow read and writes on the content
  void *mem = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, base);
  printf("base=0x%08x, mem=%p\n", base, mem);
  close(mem_fd);
  if (mem == MAP_FAILED) {
    perror("mmap error");
    exit(-1);
  }
  return (uint8_t *)mem + offset;
}

static void unmap_mem(void *addr, size_t size) {
  const intptr_t offset = (intptr_t)addr % PAGE_SIZE;
  addr = (uint8_t *)addr - offset;
  size = size + offset;
  if (munmap(addr, size) != 0) {
    perror("munmap error: ");
    exit(-1);
  }
}

static int mbox_open() {
  int file_desc;
  // open a char device file used for communicating with kernel mbox driver
  file_desc = open(DEVICE_FILE_NAME, 0);
  if (file_desc < 0) {
    printf("Can't open device file: %s\n", DEVICE_FILE_NAME);
    printf("Try creating a device file with: sudo mknod %s c %d 0\n", DEVICE_FILE_NAME, MAJOR_NUM);
    return EXIT_FAILURE;
  }
  return file_desc;
}

static void mbox_close(int file_desc) {
  close(file_desc);
}

/**
 * @brief ioctl to interface with mailbox property interface
 * 
 * @param file_desc 
 * @param buf 
 * @return int 
 */
static int mbox_property(int file_desc, void *buf) {
  int ret_val = ioctl(file_desc, IOCTL_MBOX_PROPERTY, buf);
  if (ret_val < 0) {
    printf("ioctl_set_msg failed:%d\n", ret_val);
  }
  uint32_t *p = buf; uint32_t i; uint32_t size = *(uint32_t *)buf;
  for (i=0; i<size/4; i++)
    printf("%04lx: 0x%08x\n", i*sizeof *p, p[i]);
  return ret_val;
}

static uint32_t mem_alloc(int file_desc, uint32_t size, uint32_t align, uint32_t flags) {
  int i=0;
  uint32_t p[32];
  p[i++] = 0; // size
  p[i++] = 0x00000000; // process request
  p[i++] = 0x3000c; // (the tag id)
  p[i++] = 12; // (size of the buffer)
  p[i++] = 12; // (size of the data)
  p[i++] = size; // (num bytes? or pages?)
  p[i++] = align; // (alignment)
  p[i++] = flags; // (MEM_FLAG_L1_NONALLOC)
  p[i++] = 0x00000000; // end tag
  p[0] = i*sizeof *p; // actual size
  mbox_property(file_desc, p);
  return p[5];
}

static uint32_t mem_free(int file_desc, uint32_t handle) {
  int i=0;
  uint32_t p[32];
  p[i++] = 0; // size
  p[i++] = 0x00000000; // process request
  p[i++] = 0x3000f; // (the tag id)
  p[i++] = 4; // (size of the buffer)
  p[i++] = 4; // (size of the data)
  p[i++] = handle;
  p[i++] = 0x00000000; // end tag
  p[0] = i*sizeof *p; // actual size
  mbox_property(file_desc, p);
  return p[5];
}

static uint32_t mem_lock(int file_desc, uint32_t handle) {
  int i=0;
  uint32_t p[32];
  p[i++] = 0; // size
  p[i++] = 0x00000000; // process request
  p[i++] = 0x3000d; // (the tag id)
  p[i++] = 4; // (size of the buffer)
  p[i++] = 4; // (size of the data)
  p[i++] = handle;
  p[i++] = 0x00000000; // end tag
  p[0] = i*sizeof *p; // actual size
  mbox_property(file_desc, p);
  return p[5];
}

static uint32_t mem_unlock(int file_desc, uint32_t handle) {
  int i=0;
  uint32_t p[32];
  p[i++] = 0; // size
  p[i++] = 0x00000000; // process request
  p[i++] = 0x3000e; // (the tag id)
  p[i++] = 4; // (size of the buffer)
  p[i++] = 4; // (size of the data)
  p[i++] = handle;
  p[i++] = 0x00000000; // end tag
  p[0] = i*sizeof *p; // actual size
  mbox_property(file_desc, p);
  return p[5];
}

dma_mem_t *dma_malloc(size_t size) {
  // ensure we obtain a mailbox file descriptor
  if (mailbox_fd < 0) {
    mailbox_fd = mbox_open();
    if (mailbox_fd < 0)
      return NULL;
  }
  // set size to a multiple of the system page size
  size = ((size + PAGE_SIZE -1) / PAGE_SIZE) * PAGE_SIZE;
  dma_mem_t *mem = (dma_mem_t *)malloc(sizeof(dma_mem_t));
  mem->mb = mem_alloc(mailbox_fd, size, PAGE_SIZE, MEM_FLAG_L1_NONALLOC);
  mem->b_addr = mem_lock(mailbox_fd, mem->mb);
  mem->v_addr = map_mem(BUS_TO_PHYS(mem->b_addr), size);
  mem->size = size;
  if (mem->b_addr == 0)
    return NULL;
  printf("MBox alloc: %ld bytes, bus: 0x%08x, virt: 0x%08lx\n", mem->size, mem->b_addr, (intptr_t)(mem->v_addr));
  printf("DMA memory allocation success.\n");
  return mem;
}

void dma_free(dma_mem_t *mem) {
  if (mem->v_addr == NULL)
    return;
  unmap_mem(mem->v_addr, mem->size);
  mem_unlock(mailbox_fd, mem->mb);
  mem_free(mailbox_fd, mem->mb);
  mem->v_addr = NULL;
  printf("DMA memory free success.\n");
}

int dma_init(uint16_t cb_cnt, uint16_t ticks) {
  dma_cb_t *cb;
  dma_cbs = dma_malloc(cb_cnt * sizeof(dma_cb_t));
  dma_ticks = dma_malloc(ticks * sizeof(uint32_t));
  if (dma_cbs == NULL || dma_ticks == NULL)
    return 1;
  sleep(0.5); // is this required?
  // populate dma control blocks
  for (int i = 0; i < ticks; i++) {
    cb = get_cb(i);
    cb->ti = DMA_NO_WIDE_BURSTS | DMA_WAIT_RESP;
    cb->src = BCM2711_PERI_BASE + SYST_BASE + SYST_CLO;
    cb->dest = get_tick_bus_addr(i);
    cb->len = 4;
    cb->next = get_cb_bus_addr((i + 1) % cb_cnt);
  }
  printf("Init: %d cbs, %d ticks\n", cb_cnt, ticks);
  return 0;
}

void dma_start() {
  // Reset the DMA channel
  dma_reg->cs = DMA_CHANNEL_ABORT;
  dma_reg->cs = 0;
  dma_reg->cs = DMA_CHANNEL_RESET;
  dma_reg->cb = 0;
  dma_reg->cs = DMA_INTERRUPT_STATUS | DMA_END_FLAG;
  // Make cb_addr point to the first DMA control block and enable DMA transfer
  dma_reg->cb = get_cb_bus_addr(0);
  dma_reg->cs = DMA_PRIORITY(8) | DMA_PANIC_PRIORITY(8) | DMA_DISDEBUG;
  dma_reg->cs |= DMA_WAIT_ON_WRITES | DMA_ACTIVE;
}

void dma_end() {
  // Shutdown DMA channel, otherwise it won't stop after program exits
  dma_reg->cs |= DMA_CHANNEL_ABORT;
  sleep(0.5);
  dma_reg->cs &= ~DMA_ACTIVE;
  dma_reg->cs |= DMA_CHANNEL_RESET;
  sleep(0.5);
  // Release the memory used by DMA, otherwise the memory will be leaked after program exits
  dma_free(dma_ticks);
  dma_free(dma_cbs);
  mbox_close(mailbox_fd);
}

int main(void) {
  int tick_cnt = 10;
  uint8_t *dma_base_ptr = map_mem(BCM2711_PERI_BASE + DMA_BASE, PAGE_SIZE);
  dma_reg = (dma_reg_t *)(dma_base_ptr + (DMA_CHANNEL * DMA_OFFSET));
  dma_init(tick_cnt, tick_cnt);
  sleep(0.5);
  dma_start();
  sleep(0.5);
  uint32_t ticks[tick_cnt];
  memcpy(ticks, get_tick_virt_addr(0), tick_cnt * sizeof(uint32_t));
  for (int i = 0; i < tick_cnt; i++) {
    printf("DMA %d: %u\n", i, ticks[i]);
  }
  dma_end();
  return 0;
}