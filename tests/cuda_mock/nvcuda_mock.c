/* CPU-only synthetic DLL/WinAPI boundary; never load NVIDIA or launch GPU work. */
#include "windows.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t memory[32];
static int ctx, module, allocated, launched, synced;
static int fail(const char *stage) {
    const char *value = getenv("CUDA_MOCK_FAIL");
    return value && !strcmp(value, stage);
}
static int event(const char *stage) {
    fprintf(stderr, "MOCK %s\n", stage);
    return fail(stage) ? 719 : 0;
}
HMODULE LoadLibraryA(const char *name) {
    return !strcmp(name, "nvcuda.dll") && !fail("library") ? (void *)1 : NULL;
}
unsigned long GetLastError(void) { return 126; }
int FreeLibrary(HMODULE lib) {
    (void)lib;
    return !event("library_free") && !ctx && !module && !allocated;
}
static int __stdcall init(unsigned flags) { return flags ? 1 : event("init"); }
static int __stdcall count(int *n) {
    *n = fail("zero_devices") ? 0 : (fail("excess_devices") ? 1000000 : 1);
    return event("count");
}
static int __stdcall name(char *p, int bytes, int device) {
    if (bytes < 16 || device != 0) return 1;
    memcpy(p, "Synthetic CUDA", sizeof("Synthetic CUDA"));
    return event("name");
}
static int __stdcall version(int *v) { *v = 13000; return event("version"); }
static int __stdcall device(int *d, int ordinal) {
    if (ordinal != 0) return 1;
    *d = 0;
    return event("device");
}
static int __stdcall create(void **p, unsigned flags, int d) {
    int rc = event("create");
    if (!rc && !flags && d == 0) { ctx = 1; *p = (void *)2; }
    return rc;
}
static int __stdcall load(void **p, const void *image) {
    const char *ptx = image;
    int rc = event("load");
    if (!ctx || !strstr(ptx, ".version 6.3") || !strstr(ptx, ".target sm_75") ||
        !strstr(ptx, "ld.global.u32") || !strstr(ptx, "st.global.u32") ||
        !strstr(ptx, "mul.lo.u32 %r3, %r2, 3") || !strstr(ptx, "add.u32 %r3, %r3, 7"))
        return 218;
    if (!rc) { module = 1; *p = (void *)3; }
    return rc;
}
static int __stdcall function(void **p, void *m, const char *n) {
    int rc = event("function");
    if (!module || m != (void *)3 || strcmp(n, "wgi_compute")) return 1;
    if (!rc) *p = (void *)4;
    return rc;
}
static int __stdcall alloc(uint64_t *p, size_t bytes) {
    int rc = event("alloc");
    if (!ctx || bytes != sizeof(memory)) return 1;
    if (!rc) { allocated = 1; *p = 0x12340000; }
    return rc;
}
static int __stdcall upload(uint64_t p, const void *src, size_t bytes) {
    int rc = event("upload");
    if (!allocated || p != 0x12340000 || bytes != sizeof(memory)) return 1;
    if (!rc) memcpy(memory, src, bytes);
    return rc;
}
static int __stdcall launch(void *f, unsigned gx, unsigned gy, unsigned gz,
                           unsigned bx, unsigned by, unsigned bz, unsigned shared,
                           void *stream, void **args, void **extra) {
    int rc = event("launch");
    if (!allocated || f != (void *)4 || gx != 1 || gy != 1 || gz != 1 || bx != 32 ||
        by != 1 || bz != 1 || shared || stream || extra || !args ||
        *(uint64_t *)args[0] != 0x12340000 || *(unsigned *)args[1] != 32) return 1;
    if (!rc) {
        for (unsigned i = 0; i < 32; ++i) memory[i] = memory[i] * 3 + 7;
        launched = 1;
    }
    return rc;
}
static int __stdcall sync_ctx(void) {
    int rc = event("sync");
    if (!launched) return 1;
    if (!rc) synced = 1;
    return rc;
}
static int __stdcall download(void *dst, uint64_t p, size_t bytes) {
    int rc = event("download");
    if (!allocated || !synced || p != 0x12340000 || bytes != sizeof(memory)) return 1;
    if (!rc) {
        memcpy(dst, memory, bytes);
        if (fail("corrupt_first")) ((uint32_t *)dst)[0] ^= 1;
        if (fail("corrupt_last")) ((uint32_t *)dst)[31] ^= 1;
    }
    return rc;
}
static int __stdcall free_mem(uint64_t p) {
    if (!allocated || p != 0x12340000) return 1;
    allocated = 0;
    return event("free");
}
static int __stdcall unload(void *m) {
    if (!module || m != (void *)3 || allocated) return 1;
    module = 0;
    return event("unload");
}
static int __stdcall destroy(void *c) {
    if (!ctx || c != (void *)2 || module || allocated) return 1;
    ctx = 0;
    return event("destroy");
}
FARPROC GetProcAddress(HMODULE lib, const char *symbol) {
    const char *missing = getenv("CUDA_MOCK_MISSING");
    (void)lib;
    if (missing && !strcmp(missing, symbol)) return NULL;
#define EXPORT(symbol_name, impl) if (!strcmp(symbol, symbol_name)) return (FARPROC)impl
    EXPORT("cuInit", init);
    EXPORT("cuDeviceGetCount", count);
    EXPORT("cuDeviceGetName", name);
    EXPORT("cuDriverGetVersion", version);
    EXPORT("cuDeviceGet", device);
    EXPORT("cuCtxCreate_v2", create);
    EXPORT("cuModuleLoadData", load);
    EXPORT("cuModuleGetFunction", function);
    EXPORT("cuMemAlloc_v2", alloc);
    EXPORT("cuMemcpyHtoD_v2", upload);
    EXPORT("cuLaunchKernel", launch);
    EXPORT("cuCtxSynchronize", sync_ctx);
    EXPORT("cuMemcpyDtoH_v2", download);
    EXPORT("cuMemFree_v2", free_mem);
    EXPORT("cuModuleUnload", unload);
    EXPORT("cuCtxDestroy_v2", destroy);
#undef EXPORT
    return NULL;
}
