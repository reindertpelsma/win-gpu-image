#ifndef CUDA_TEST_WINDOWS_H
#define CUDA_TEST_WINDOWS_H
#ifndef _MSC_VER
#define __stdcall
#endif
typedef void *HMODULE;
typedef void (*FARPROC)(void);
HMODULE LoadLibraryA(const char *name);
FARPROC GetProcAddress(HMODULE module, const char *name);
unsigned long GetLastError(void);
int FreeLibrary(HMODULE module);
#endif
