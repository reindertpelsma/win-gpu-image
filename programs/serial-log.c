// Tee stdin (or command-line text) to COM1, stdout, and a marked result disk.
// No marker is synthesized by the host: the PE guest writes actual serial bytes.
#include <windows.h>
#include <stdio.h>
#include <string.h>
static HANDLE serial = INVALID_HANDLE_VALUE, result = INVALID_HANDLE_VALUE;
static void emit(const char* bytes, DWORD n) {
    DWORD written;
    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), bytes, n, &written, NULL);
    if (serial != INVALID_HANDLE_VALUE) WriteFile(serial, bytes, n, &written, NULL);
    if (result != INVALID_HANDLE_VALUE) { WriteFile(result, bytes, n, &written, NULL); FlushFileBuffers(result); }
}
int main(int argc, char** argv) {
    serial = CreateFileA("\\\\.\\COM1", GENERIC_WRITE | GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (serial != INVALID_HANDLE_VALUE) {
        DCB dcb = {0}; dcb.DCBlength = sizeof(dcb);
        if (GetCommState(serial, &dcb)) {
            dcb.BaudRate = CBR_115200; dcb.ByteSize = 8; dcb.Parity = NOPARITY; dcb.StopBits = ONESTOPBIT;
            dcb.fBinary = TRUE; dcb.fOutxCtsFlow = FALSE; dcb.fOutxDsrFlow = FALSE;
            dcb.fDtrControl = DTR_CONTROL_DISABLE; dcb.fRtsControl = RTS_CONTROL_DISABLE;
            dcb.fOutX = FALSE; dcb.fInX = FALSE;
            SetCommState(serial, &dcb);
        }
        COMMTIMEOUTS timeout = {0}; timeout.WriteTotalTimeoutConstant = 2000;
        SetCommTimeouts(serial, &timeout);
    } else { fprintf(stderr, "COM1_OPEN_FAILED error=%lu\n", GetLastError()); }
    for (char drive = 'C'; drive <= 'Z'; ++drive) {
        if (drive == 'X') continue;
        char tag[] = "C:\\WGI_RESULTS.TAG"; tag[0] = drive;
        if (GetFileAttributesA(tag) != INVALID_FILE_ATTRIBUTES) {
            char path[] = "C:\\wgi-results.txt"; path[0] = drive;
            result = CreateFileA(path, FILE_APPEND_DATA, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
            break;
        }
    }
    MEMORYSTATUSEX memory = {0}; memory.dwLength = sizeof(memory);
    if (GlobalMemoryStatusEx(&memory)) {
        char text[160];
        int n = snprintf(text, sizeof(text), "MEMORY_GUEST total_bytes=%llu available_bytes=%llu\r\n",
                         (unsigned long long)memory.ullTotalPhys, (unsigned long long)memory.ullAvailPhys);
        if (n > 0 && n < (int)sizeof(text)) emit(text, (DWORD)n);
    }
    if (argc > 1) {
        for (int i = 1; i < argc; ++i) { if (i > 1) emit(" ", 1); emit(argv[i], (DWORD)strlen(argv[i])); }
        emit("\r\n", 2);
    } else {
        char buffer[4096]; DWORD n;
        while (ReadFile(GetStdHandle(STD_INPUT_HANDLE), buffer, sizeof(buffer), &n, NULL) && n) emit(buffer, n);
    }
    int rc = serial == INVALID_HANDLE_VALUE ? 1 : 0;
    if (result != INVALID_HANDLE_VALUE) CloseHandle(result);
    if (serial != INVALID_HANDLE_VALUE) CloseHandle(serial);
    return rc;
}
