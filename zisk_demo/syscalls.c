#include <sys/stat.h>
#include <sys/types.h>
#include <sys/reent.h>
#include <errno.h>
#include <wchar.h>

/*
 * Zisk VM Syscalls for EVM Block Demo
 * ====================================
 *
 * Minimal syscall implementation for bare metal RISC-V.
 * Based on hello_zisk toolchain.
 *
 * MEMORY-MAPPED I/O:
 * ------------------
 * - UART Output: 0xa0000200
 *   Writing a single byte outputs to stdout (used by printf)
 *
 * SYSCALLS VIA ECALL:
 * -------------------
 * - Syscall 93: exit - Clean program termination
 */

#undef errno
extern int errno;

/* Heap management */
extern char __heap_start;
static char *heap_end = 0;

void *_sbrk(int incr) {
    char *prev_heap_end;

    if (heap_end == 0) {
        heap_end = &__heap_start;
    }

    prev_heap_end = heap_end;
    heap_end += incr;

    return (void *)prev_heap_end;
}

/* Write to file descriptor
 * For Zisk VM, write to memory-mapped UART at 0xa0000200
 */
int _write(int file, char *ptr, int len) {
    volatile char *uart = (volatile char *)0xa0000200;

    // Only handle stdout (1) and stderr (2)
    if (file != 1 && file != 2) {
        errno = EBADF;
        return -1;
    }

    // Write each byte to the UART
    for (int i = 0; i < len; i++) {
        *uart = ptr[i];
    }

    return len;
}

int _close(int file) {
    (void)file;
    return -1;
}

int _fstat(int file, struct stat *st) {
    (void)file;
    st->st_mode = S_IFCHR;
    return 0;
}

int _isatty(int file) {
    (void)file;
    return 1;
}

int _lseek(int file, int ptr, int dir) {
    (void)file;
    (void)ptr;
    (void)dir;
    return 0;
}

int _read(int file, char *ptr, int len) {
    (void)file;
    (void)ptr;
    (void)len;
    return 0;
}

void _exit(int status) {
    (void)status;
    /* Hang forever */
    while(1) {
        __asm__ volatile ("wfi");
    }
}

int _kill(int pid, int sig) {
    (void)pid;
    (void)sig;
    errno = EINVAL;
    return -1;
}

int _getpid(void) {
    return 1;
}

/* Wide character stub for newlib-nano */
wint_t _fputwc_r(struct _reent *ptr, wchar_t wc, FILE *fp) {
    (void)ptr;
    (void)wc;
    (void)fp;
    return WEOF;
}
