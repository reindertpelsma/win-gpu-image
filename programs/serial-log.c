/* Guest-written COM1 protocol. Disk/stdout are diagnostic tees, never serial substitutes. */
#define _WIN32_WINNT 0x0602
#include <windows.h>
#include <cfgmgr32.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pe-contract.h"

static HANDLE serial = INVALID_HANDLE_VALUE, result = INVALID_HANDLE_VALUE;
static int serial_failed;
static ULONGLONG write_end;
static int serial_write(const void *bytes, unsigned count, unsigned *written) {
    DWORD n = 0;
    if (GetTickCount64() >= write_end) return 0;
    int ok = WriteFile(serial, bytes, count, &n, NULL);
    *written = n;
    return ok;
}
static int emit(const char *bytes, unsigned count) {
    DWORD n;
    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), bytes, count, &n, NULL);
    if (result != INVALID_HANDLE_VALUE) {
        WriteFile(result, bytes, count, &n, NULL);
        FlushFileBuffers(result);
    }
    write_end = GetTickCount64() + 2000;
    if (serial == INVALID_HANDLE_VALUE || wgi_write_all(serial_write, bytes, count)) {
        serial_failed = 1;
        return 0;
    }
    return 1;
}
static int text(const char *s) { return emit(s, (unsigned)strlen(s)); }
static void result_open(void) {
    for (char drive = 'C'; drive <= 'Z'; ++drive) {
        if (drive == 'X') continue;
        char tag[] = "C:\\WGI_RESULTS.TAG"; tag[0] = drive;
        if (GetFileAttributesA(tag) != INVALID_FILE_ATTRIBUTES) {
            char path[] = "C:\\wgi-results.txt"; path[0] = drive;
            result = CreateFileA(path, FILE_APPEND_DATA, FILE_SHARE_READ, NULL,
                                 OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
            break;
        }
    }
}
static void service_start(const char *name) {
    SC_HANDLE scm = OpenSCManagerA(NULL, NULL, SC_MANAGER_CONNECT);
    if (!scm) return;
    SC_HANDLE svc = OpenServiceA(scm, name, SERVICE_START | SERVICE_CHANGE_CONFIG);
    if (svc) {
        ChangeServiceConfigA(svc, SERVICE_NO_CHANGE, SERVICE_DEMAND_START,
                             SERVICE_NO_CHANGE, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
        BOOL ok = StartServiceA(svc, 0, NULL);
        printf("WGI_SERIAL_SERVICE name=%s start_ok=%d error=%lu\n",
               name, ok, ok ? 0 : GetLastError());
        CloseServiceHandle(svc);
    } else printf("WGI_SERIAL_SERVICE name=%s open_error=%lu\n", name, GetLastError());
    CloseServiceHandle(scm);
}
static int serial_open(void) {
    service_start("Serial");
    service_start("Serenum");
    DEVINST root;
    if (CM_Locate_DevNodeA(&root, NULL, CM_LOCATE_DEVNODE_NORMAL) == CR_SUCCESS)
        CM_Reenumerate_DevNode(root, CM_REENUMERATE_ASYNCHRONOUS);
    ULONGLONG end = GetTickCount64() + 20000;
    DWORD error = 0;
    do {
        serial = CreateFileA("\\\\.\\COM1", GENERIC_WRITE | GENERIC_READ, 0, NULL,
                             OPEN_EXISTING, 0, NULL);
        if (serial != INVALID_HANDLE_VALUE) break;
        error = GetLastError();
        Sleep(200);
    } while (GetTickCount64() < end);
    if (serial == INVALID_HANDLE_VALUE) {
        char target[512] = {0}, diagnostic[768];
        DWORD mapped = QueryDosDeviceA("COM1", target, sizeof(target));
        snprintf(diagnostic, sizeof(diagnostic),
                 "WGI_SERIAL_FAIL open_error=%lu com1_mapping=%s query_error=%lu\r\n",
                 error, mapped ? target : "ABSENT", mapped ? 0 : GetLastError());
        text(diagnostic);
        return 0;
    }
    DCB dcb;
    memset(&dcb, 0, sizeof(dcb)); dcb.DCBlength = sizeof(dcb);
    if (!GetCommState(serial, &dcb)) return 0;
    dcb.BaudRate = CBR_115200; dcb.ByteSize = 8; dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT; dcb.fBinary = TRUE;
    dcb.fOutxCtsFlow = FALSE; dcb.fOutxDsrFlow = FALSE;
    dcb.fDtrControl = DTR_CONTROL_DISABLE; dcb.fRtsControl = RTS_CONTROL_DISABLE;
    dcb.fOutX = FALSE; dcb.fInX = FALSE; dcb.fDsrSensitivity = FALSE;
    dcb.fAbortOnError = FALSE;
    COMMTIMEOUTS timeout;
    memset(&timeout, 0, sizeof(timeout)); timeout.WriteTotalTimeoutConstant = 2000;
    return SetCommState(serial, &dcb) && SetCommTimeouts(serial, &timeout);
}

static int supervise(unsigned seconds, char *command) {
    HANDLE read = NULL, write = NULL, job = NULL;
    PROCESS_INFORMATION pi;
    STARTUPINFOA si;
    SECURITY_ATTRIBUTES sa = {sizeof(sa), NULL, TRUE};
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits;
    memset(&pi, 0, sizeof(pi)); memset(&si, 0, sizeof(si));
    memset(&limits, 0, sizeof(limits)); si.cb = sizeof(si);
    int rc = 2, timed_out = 0, marker_flags = 0;
    char line[4096]; unsigned used = 0; DWORD total = 0;
    ULONGLONG end = GetTickCount64() + (ULONGLONG)seconds * 1000;
    if (!CreatePipe(&read, &write, &sa, 0) ||
        !SetHandleInformation(read, HANDLE_FLAG_INHERIT, 0)) goto done;
    job = CreateJobObjectA(NULL, NULL);
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!job || !SetInformationJobObject(job, JobObjectExtendedLimitInformation,
                                         &limits, sizeof(limits))) goto done;
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE); si.hStdOutput = write; si.hStdError = write;
    if (!CreateProcessA(NULL, command, NULL, NULL, TRUE, CREATE_SUSPENDED,
                        NULL, NULL, &si, &pi)) goto done;
    if (!AssignProcessToJobObject(job, pi.hProcess)) goto done;
    CloseHandle(write); write = NULL;
    if (ResumeThread(pi.hThread) == (DWORD)-1) goto done;
    while (1) {
        DWORD available = 0;
        if (!PeekNamedPipe(read, NULL, 0, NULL, &available, NULL) &&
            GetLastError() != ERROR_BROKEN_PIPE) break;
        if (available) {
            char buffer[4096]; DWORD n = 0;
            if (!ReadFile(read, buffer, sizeof(buffer), &n, NULL) || !n) break;
            if (!emit(buffer, n)) break;
            total += n;
            if (total > 4 * 1024 * 1024) { text("WGI_OUTPUT_LIMIT\r\n"); break; }
            for (DWORD i = 0; i < n; ++i) {
                if (!buffer[i]) marker_flags |= 4;
                if (used < sizeof(line) - 1) line[used++] = buffer[i];
                else marker_flags |= 4;
                if (buffer[i] == '\n') {
                    line[used] = 0;
                    marker_flags |= wgi_markers(line);
                    used = 0;
                }
            }
        } else if (WaitForSingleObject(pi.hProcess, 0) == WAIT_OBJECT_0) {
            DWORD child_rc;
            if (used) { line[used] = 0; marker_flags |= wgi_markers(line); }
            if (GetExitCodeProcess(pi.hProcess, &child_rc))
                rc = wgi_terminal(marker_flags == 1 ? "WGI_GPU_PASS\n" : "", child_rc, 0);
            break;
        } else Sleep(20);
        if (GetTickCount64() >= end) { timed_out = 1; break; }
    }
done:
    /* The only child was suspended until assigned. Never orphan even on assignment failure. */
    if (pi.hProcess && WaitForSingleObject(pi.hProcess, 0) != WAIT_OBJECT_0) {
        if (job) TerminateJobObject(job, 2);
        TerminateProcess(pi.hProcess, 2);
        WaitForSingleObject(pi.hProcess, 5000);
    }
    if (timed_out) text("WGI_GPU_TIMEOUT\r\n");
    if (rc || serial_failed) text("WGI_GPU_FAIL\r\nWGI_GPU_END\r\n");
    if (pi.hThread) CloseHandle(pi.hThread);
    if (pi.hProcess) CloseHandle(pi.hProcess);
    if (read) CloseHandle(read);
    if (write) CloseHandle(write);
    if (job) CloseHandle(job);
    return rc || serial_failed || timed_out;
}
int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IONBF, 0);
    if (argc == 2 && !strcmp(argv[1], "--cpu-test")) {
        /* No COM, SCM, registry, disk, process, device or GPU operations in this mode. */
        int ok = !wgi_terminal("WGI_GPU_PASS\n", 0, 0) &&
            wgi_terminal("WGI_GPU_PASS\n", 0, 1) &&
            wgi_terminal("WGI_PE_BOOT_OK_V1\n", 0, 0) &&
            wgi_terminal("WGI_GPU_PASS\rgarbage\n", 0, 0) &&
            wgi_terminal("WGI_GPU_PASS\nWGI_GPU_FAIL_SUFFIX\n", 0, 0);
        puts(ok ? "WGI_CPU_POLICY_PASS" : "WGI_CPU_POLICY_FAIL");
        return ok ? 0 : 1;
    }
    result_open();
    if (!serial_open()) {
        text("WGI_SERIAL_FAIL configuration_or_open\r\nWGI_GPU_FAIL\r\nWGI_GPU_END\r\n");
        if (serial != INVALID_HANDLE_VALUE) CloseHandle(serial);
        if (result != INVALID_HANDLE_VALUE) CloseHandle(result);
        return 2;
    }
    int rc = 0;
    if (argc == 4 && !strcmp(argv[1], "--run")) {
        char *end = NULL;
        unsigned long seconds = strtoul(argv[2], &end, 10);
        if (!end || *end || !seconds || seconds > 600) rc = 2;
        else {
            char marker[128];
            snprintf(marker, sizeof(marker), "WGI_PE_START_V2 terminal_budget_s=%lu\r\n", seconds);
            if (!text(marker)) rc = 2;
            else rc = supervise((unsigned)seconds, argv[3]);
        }
    } else if (argc > 1) {
        for (int i = 1; i < argc; ++i) {
            if (i > 1) text(" ");
            text(argv[i]);
        }
        text("\r\n");
    } else {
        char buffer[4096]; DWORD n;
        while (ReadFile(GetStdHandle(STD_INPUT_HANDLE), buffer, sizeof(buffer), &n, NULL) && n)
            if (!emit(buffer, n)) break;
    }
    CloseHandle(serial);
    if (result != INVALID_HANDLE_VALUE) CloseHandle(result);
    return rc || serial_failed;
}
