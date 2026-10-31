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
WGI_API int wgi_markers(const char *text);
WGI_API int wgi_tee(WgiWrite diagnostic, WgiWrite serial, const char *bytes,
                    unsigned count, int serial_required);
WGI_API int wgi_bounded_write(WgiWrite write, unsigned *used, int *truncated,
                              unsigned limit, const char *bytes, unsigned count);
WGI_API int wgi_receipt_resume(long long bytes, int size_ok, unsigned limit,
                               unsigned *used, int *truncated);
WGI_API int wgi_log_status(unsigned expected, unsigned actual, int seek_ok, int read_ok);
WGI_API int wgi_supervised(const char *text, int child_rc, int timed_out, int gpu_required);
WGI_API int wgi_terminal(const char *text, int child_rc, int timed_out);
#endif
