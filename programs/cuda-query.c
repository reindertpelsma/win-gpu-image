// Original source and handwritten PTX: no CUDA toolkit, samples or runtime redistribution.
// Hypothesis: device0 can transform32 words on the GPU, with exact readback and cleanup.
// Falsifier: any missing API, failed call/cleanup, or mismatching output word.
#include <windows.h>
#include <stdint.h>
#include <stdio.h>

typedef int (__stdcall *init_fn)(unsigned int);
typedef int (__stdcall *count_fn)(int *);
typedef int (__stdcall *name_fn)(char *, int, int);
typedef int (__stdcall *version_fn)(int *);
typedef int (__stdcall *device_fn)(int *, int);
typedef int (__stdcall *create_fn)(void **, unsigned int, int);
typedef int (__stdcall *load_fn)(void **, const void *);
typedef int (__stdcall *function_fn)(void **, void *, const char *);
typedef int (__stdcall *alloc_fn)(uint64_t *, size_t);
typedef int (__stdcall *upload_fn)(uint64_t, const void *, size_t);
typedef int (__stdcall *launch_fn)(void *, unsigned int, unsigned int, unsigned int,
    unsigned int, unsigned int, unsigned int, unsigned int, void *, void **, void **);
typedef int (__stdcall *sync_fn)(void);
typedef int (__stdcall *download_fn)(void *, uint64_t, size_t);
typedef int (__stdcall *free_fn)(uint64_t);
typedef int (__stdcall *unload_fn)(void *);
typedef int (__stdcall *destroy_fn)(void *);

// PTX6.3 introduced sm_75 (Turing); the generic target can JIT on later architectures.
// One block,32 threads; no atomics, textures, feature-specific opcodes or external code.
static const char ptx[] =
    ".version 6.3\n.target sm_75\n.address_size 64\n"
    ".visible .entry wgi_compute(.param .u64 buffer, .param .u32 count) {\n"
    " .reg .pred %p; .reg .b32 %r<4>; .reg .b64 %rd<3>;\n"
    " ld.param.u64 %rd0, [buffer]; ld.param.u32 %r1, [count];\n"
    " mov.u32 %r0, %tid.x; setp.ge.u32 %p, %r0, %r1; @%p bra done;\n"
    " mul.wide.u32 %rd1, %r0, 4; add.u64 %rd2, %rd0, %rd1;\n"
    " ld.global.u32 %r2, [%rd2]; mul.lo.u32 %r3, %r2, 3;\n"
    " add.u32 %r3, %r3, 7; st.global.u32 [%rd2], %r3;\n"
    "done: ret; }\n";

int main(void) {
    HMODULE lib = LoadLibraryA("nvcuda.dll");
    int devices = 0, api = 0, device_id = 0, failed = 1;
    int have_context = 0, have_module = 0, have_allocation = 0;
    void *context = NULL, *module = NULL, *function = NULL;
    uint64_t memory = 0;
    uint32_t input[32], output[32] = {0};
    unsigned int words = 32;
    void *args[] = {&memory, &words};
    init_fn init = NULL;
    count_fn count = NULL;
    name_fn name = NULL;
    version_fn version = NULL;
    device_fn device = NULL;
    create_fn create = NULL;
    load_fn load = NULL;
    function_fn get_function = NULL;
    alloc_fn alloc = NULL;
    upload_fn upload = NULL;
    launch_fn launch = NULL;
    sync_fn sync = NULL;
    download_fn download = NULL;
    free_fn free_memory = NULL;
    unload_fn unload = NULL;
    destroy_fn destroy = NULL;
    if (!lib) {
        printf("CUDA_FAIL LoadLibrary error=%lu\n", GetLastError());
        return 1;
    }
#define GET(variable, type, symbol) do { \
    variable = (type)GetProcAddress(lib, symbol); \
    if (!variable) { printf("CUDA_FAIL missing=%s\n", symbol); goto cleanup; } \
} while (0)
    GET(init, init_fn, "cuInit");
    GET(count, count_fn, "cuDeviceGetCount");
    GET(name, name_fn, "cuDeviceGetName");
    GET(version, version_fn, "cuDriverGetVersion");
    GET(device, device_fn, "cuDeviceGet");
    GET(create, create_fn, "cuCtxCreate_v2");
    GET(load, load_fn, "cuModuleLoadData");
    GET(get_function, function_fn, "cuModuleGetFunction");
    GET(alloc, alloc_fn, "cuMemAlloc_v2");
    GET(upload, upload_fn, "cuMemcpyHtoD_v2");
    GET(launch, launch_fn, "cuLaunchKernel");
    GET(sync, sync_fn, "cuCtxSynchronize");
    GET(download, download_fn, "cuMemcpyDtoH_v2");
    GET(free_memory, free_fn, "cuMemFree_v2");
    GET(unload, unload_fn, "cuModuleUnload");
    GET(destroy, destroy_fn, "cuCtxDestroy_v2");
#undef GET
#define CHECK(call) do { \
    int status = (call); \
    if (status) { printf("CUDA_FAIL call=%s rc=%d\n", #call, status); goto cleanup; } \
} while (0)
    CHECK(init(0));
    CHECK(count(&devices));
    // Bound enumeration and output even if a driver returns an invalid device count.
    if (devices < 1 || devices > 64) {
        printf("CUDA_FAIL devices=%d allowed=1..64\n", devices);
        goto cleanup;
    }
    CHECK(version(&api));
    for (int i = 0; i < devices; ++i) {
        char text[256] = {0};
        CHECK(name(text, (int)sizeof(text), i));
        text[sizeof(text) - 1] = 0;
        printf("CUDA_DEVICE index=%d name=%s\n", i, text);
    }
    CHECK(device(&device_id, 0));
    CHECK(create(&context, 0, device_id));
    have_context = 1;
    CHECK(load(&module, ptx));
    have_module = 1;
    CHECK(get_function(&function, module, "wgi_compute"));
    CHECK(alloc(&memory, sizeof(input)));
    have_allocation = 1;
    for (unsigned int i = 0; i < words; ++i) input[i] = i * 17 + 5;
    CHECK(upload(memory, input, sizeof(input)));
    CHECK(launch(function, 1, 1, 1, words, 1, 1, 0, NULL, args, NULL));
    CHECK(sync());
    CHECK(download(output, memory, sizeof(output)));
    for (unsigned int i = 0; i < words; ++i) {
        uint32_t expected = input[i] * 3 + 7;
        if (output[i] != expected) {
            printf("CUDA_FAIL readback index=%u expected=%u actual=%u\n",
                i, (unsigned int)expected, (unsigned int)output[i]);
            goto cleanup;
        }
    }
    failed = 0;
#undef CHECK
cleanup:
    // Attempt every acquired-resource cleanup, even when a prior operation/cleanup failed.
#define RELEASE(call) do { \
    int status = (call); \
    if (status) { printf("CUDA_FAIL cleanup=%s rc=%d\n", #call, status); failed = 1; } \
} while (0)
    if (have_allocation) RELEASE(free_memory(memory));
    if (have_module) RELEASE(unload(module));
    if (have_context) RELEASE(destroy(context));
#undef RELEASE
    if (!FreeLibrary(lib)) {
        printf("CUDA_FAIL FreeLibrary error=%lu\n", GetLastError());
        failed = 1;
    }
    if (failed) return 1;
    printf("CUDA_PASS devices=%d driver_api=%d compute=PASS words=32 bytes=128 device=0\n",
        devices, api);
    return 0;
}
