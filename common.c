#include <unistd.h>
#include <errno.h>
#include "common.h"

/* Loop until all bytes are received — a single read() may return less than requested */
ssize_t read_full(int fd, void *buf, size_t count) {
    size_t total = 0;
    while (total < count) {
        ssize_t n = read(fd, (char *)buf + total, count - total);
        if (n < 0) {
            if (errno == EINTR) continue; /* signal interrupted — retry */
            return -1;
        }
        if (n == 0) break; /* EOF */
        total += n;
    }
    return (ssize_t)total;
}

/* Loop until all bytes are sent — mirrors read_full on the write side */
ssize_t write_full(int fd, const void *buf, size_t count) {
    size_t total = 0;
    while (total < count) {
        ssize_t n = write(fd, (const char *)buf + total, count - total);
        if (n < 0) {
            if (errno == EINTR) continue; /* signal interrupted — retry */
            return -1;
        }
        total += n;
    }
    return (ssize_t)total;
}
