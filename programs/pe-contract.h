#ifndef WGI_PE_CONTRACT_H
#define WGI_PE_CONTRACT_H
/* Original portable policies, exercised with CPU-only API callbacks. */
#if defined(_WIN32) && defined(WGI_TEST)
#define WGI_API __declspec(dllexport)
#else
#define WGI_API
#endif
typedef int (*WgiStep)(int step);
typedef int (*WgiWrite)(const void *bytes, unsigned count, unsigned *written);
WGI_API int wgi_gsp(WgiStep step);
WGI_API int wgi_write_all(WgiWrite write, const char *bytes, unsigned count);
WGI_API int wgi_terminal(const char *text, int child_rc, int timed_out);
#endif
