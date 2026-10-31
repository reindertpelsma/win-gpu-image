#ifndef CUDA_TEST_WINDOWS_H
#define CUDA_TEST_WINDOWS_H
#if !defined(_MSC_VER) && !defined(__stdcall)
#define __stdcall
#endif
/* These CPU doubles must never interpose the host's own kernel32 imports. */
#define LoadLibraryA wgi_mock_LoadLibraryA
#define GetProcAddress wgi_mock_GetProcAddress
#define GetLastError wgi_mock_GetLastError
#define FreeLibrary wgi_mock_FreeLibrary
typedef void *HMODULE;
typedef void (*FARPROC)(void);
HMODULE LoadLibraryA(const char *name);
FARPROC GetProcAddress(HMODULE module, const char *name);
unsigned long GetLastError(void);
int FreeLibrary(HMODULE module);
#endif
