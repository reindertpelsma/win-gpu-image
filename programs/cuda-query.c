// Original source: no CUDA toolkit, sample source or runtime redistribution.
// Hypothesis: the NVIDIA driver exposes at least one CUDA device.
// Falsifier: missing API, failed cuInit/cuDeviceGetCount, or zero devices.
#include <windows.h>
#include <stdio.h>
typedef int (__stdcall *init_fn)(unsigned int);
typedef int (__stdcall *count_fn)(int*);
typedef int (__stdcall *name_fn)(char*, int, int);
typedef int (__stdcall *version_fn)(int*);
int main(void) {
    HMODULE lib = LoadLibraryA("nvcuda.dll");
    if (!lib) { printf("CUDA_FAIL LoadLibrary error=%lu\n", GetLastError()); return 1; }
    init_fn init = (init_fn)GetProcAddress(lib, "cuInit");
    count_fn count = (count_fn)GetProcAddress(lib, "cuDeviceGetCount");
    name_fn name = (name_fn)GetProcAddress(lib, "cuDeviceGetName");
    version_fn version = (version_fn)GetProcAddress(lib, "cuDriverGetVersion");
    int devices = 0, api = 0, rc;
    if (!init || !count || !name || !version) { puts("CUDA_FAIL missing API"); return 2; }
    rc = init(0);
    if (rc) { printf("CUDA_FAIL cuInit=%d\n", rc); return 3; }
    rc = count(&devices);
    if (rc || devices < 1) { printf("CUDA_FAIL count_rc=%d devices=%d\n", rc, devices); return 4; }
    rc = version(&api);
    if (rc) { printf("CUDA_FAIL version_rc=%d\n", rc); return 5; }
    for (int i = 0; i < devices; ++i) {
        char text[256] = {0};
        rc = name(text, sizeof(text), i);
        if (rc) { printf("CUDA_FAIL name_rc=%d\n", rc); return 6; }
        printf("CUDA_DEVICE index=%d name=%s\n", i, text);
    }
    printf("CUDA_PASS devices=%d driver_api=%d\n", devices, api);
    FreeLibrary(lib);
    return 0;
}
