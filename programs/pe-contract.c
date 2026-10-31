#include "pe-contract.h"
#include <string.h>

int wgi_gsp(WgiStep step) {
    /* disable, install-held, exact-key, service/class writes, both reads, release, enable */
    for (int i = 0; i < 9; ++i) {
        if (!step(i)) return i + 1;
    }
    return 0;
}

int wgi_write_all(WgiWrite write, const char *bytes, unsigned count) {
    while (count) {
        unsigned written = 0;
        if (!write(bytes, count, &written) || !written || written > count) return 1;
        bytes += written;
        count -= written;
    }
    return 0;
}

/* 1=exact PASS, 2=exact FAIL, 4=malformed near-marker. Any fault poisons PASS. */
int wgi_markers(const char *text) {
    int flags = 0;
    const char *p = text;
    while (*p) {
        const char *end = strchr(p, '\n');
        size_t length = end ? (size_t)(end - p) : strlen(p);
        if (end && length && p[length - 1] == '\r') --length;
        const char *markers[] = {"WGI_GPU_PASS", "WGI_GPU_FAIL"};
        for (unsigned m = 0; m < 2; ++m) {
            size_t n = strlen(markers[m]);
            for (size_t i = 0; i + n <= length; ++i) {
                if (!memcmp(p + i, markers[m], n)) {
                    flags |= i == 0 && length == n ? (m == 0 ? 1 : 2) : 4;
                }
            }
        }
        if (!end) break;
        p = end + 1;
    }
    return flags;
}

int wgi_terminal(const char *text, int child_rc, int timed_out) {
    return child_rc || timed_out || wgi_markers(text) != 1;
}

int wgi_tee(WgiWrite diagnostic, WgiWrite serial, const char *bytes,
            unsigned count, int serial_required) {
    if (wgi_write_all(diagnostic, bytes, count)) return 1;
    return serial_required ? wgi_write_all(serial, bytes, count) : 0;
}

int wgi_supervised(const char *text, int child_rc, int timed_out, int gpu_required) {
    return child_rc || timed_out || (gpu_required && wgi_terminal(text, 0, 0));
}
