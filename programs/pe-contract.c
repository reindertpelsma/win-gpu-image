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

static int line(const char *text, const char *marker) {
    size_t n = strlen(marker);
    for (const char *p = text; *p; ++p) {
        if ((p == text || p[-1] == '\n') && !strncmp(p, marker, n) &&
            (p[n] == '\r' || p[n] == '\n' || p[n] == 0)) return 1;
    }
    return 0;
}

int wgi_terminal(const char *text, int child_rc, int timed_out) {
    return child_rc || timed_out || line(text, "WGI_GPU_FAIL") ||
        !line(text, "WGI_GPU_PASS");
}
