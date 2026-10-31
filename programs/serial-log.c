/* Guest-written COM1 protocol. Disk/stdout are diagnostic tees, never serial substitutes. */
#define _WIN32_WINNT 0x0602
#include <windows.h>
#include <cfgmgr32.h>
#include <setupapi.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pe-contract.h"

static HANDLE serial = INVALID_HANDLE_VALUE, result = INVALID_HANDLE_VALUE;
static int serial_failed, diagnostic_failed, diagnostic_only, capture_failed;
static HANDLE receipt = INVALID_HANDLE_VALUE, runtime = INVALID_HANDLE_VALUE;
static unsigned receipt_bytes, runtime_bytes;
static int receipt_truncated, runtime_truncated;
static char result_drive;
#define RECEIPT_LIMIT 131072u
#define RUNTIME_LIMIT 262144u
static ULONGLONG write_end;
static int serial_write(const void *bytes, unsigned count, unsigned *written) {
    DWORD n = 0;
    if (GetTickCount64() >= write_end) return 0;
    int ok = WriteFile(serial, bytes, count, &n, NULL);
    *written = n;
    return ok;
}
static int diagnostic_write(const void *bytes, unsigned count, unsigned *written) {
    DWORD n = 0;
    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), bytes, count, &n, NULL);
    if (result != INVALID_HANDLE_VALUE &&
        (!WriteFile(result, bytes, count, &n, NULL) || n != count || !FlushFileBuffers(result))) {
        diagnostic_failed = 1;
        *written = 0;
        return 0;
    }
    *written = count;
    return 1;
}
static int receipt_raw_write(const void *bytes, unsigned count, unsigned *written) {
    DWORD n = 0;
    int ok = WriteFile(receipt, bytes, count, &n, NULL);
    *written = n;
    return ok;
}
static int runtime_raw_write(const void *bytes, unsigned count, unsigned *written) {
    DWORD n = 0;
    int ok = WriteFile(runtime, bytes, count, &n, NULL);
    *written = n;
    return ok;
}
static void close_receipt(void) {
    if (receipt != INVALID_HANDLE_VALUE) CloseHandle(receipt);
    receipt = INVALID_HANDLE_VALUE;
}
static void close_runtime(void) {
    if (runtime != INVALID_HANDLE_VALUE) CloseHandle(runtime);
    runtime = INVALID_HANDLE_VALUE;
}
static void receipt_record(const char *bytes, unsigned count) {
    if (receipt == INVALID_HANDLE_VALUE) return;
    if (wgi_bounded_write(receipt_raw_write, &receipt_bytes, &receipt_truncated,
        RECEIPT_LIMIT, bytes, count) || !FlushFileBuffers(receipt)) {
        capture_failed = 1;
        close_receipt();
    }
}
static void runtime_record(const char *bytes, unsigned count) {
    if (runtime == INVALID_HANDLE_VALUE) return;
    if (wgi_bounded_write(runtime_raw_write, &runtime_bytes, &runtime_truncated,
        RUNTIME_LIMIT, bytes, count) || !FlushFileBuffers(runtime)) {
        capture_failed = 1;
        close_runtime();
    }
}
static void diagnostic(const char *format, ...) {
    char buffer[2048];
    va_list args;
    va_start(args, format);
    int n = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    if (n < 0) { diagnostic_failed = 1; return; }
    unsigned count = n < (int)sizeof(buffer) ? (unsigned)n : sizeof(buffer) - 1;
    receipt_record(buffer, count);
    if (wgi_write_all(diagnostic_write, buffer, count)) diagnostic_failed = 1;
}
static int emit(const char *bytes, unsigned count) {
    write_end = GetTickCount64() + 2000;
    if (wgi_tee(diagnostic_write, serial_write, bytes, count, !diagnostic_only)) {
        if (!diagnostic_only) serial_failed = 1;
        return 0;
    }
    return 1;
}
static int text(const char *s) {
    receipt_record(s, (unsigned)strlen(s));
    return emit(s, (unsigned)strlen(s));
}
static void result_open(void) {
    for (char drive = 'C'; drive <= 'Z'; ++drive) {
        if (drive == 'X') continue;
        char tag[] = "C:\\WGI_RESULTS.TAG"; tag[0] = drive;
        if (GetFileAttributesA(tag) != INVALID_FILE_ATTRIBUTES) {
            char path[] = "C:\\wgi-results.txt"; path[0] = drive;
            result = CreateFileA(path, FILE_APPEND_DATA, FILE_SHARE_READ, NULL,
                                 OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
            result_drive = drive;
            char summary[] = "C:\\wgi-serial-receipt.txt"; summary[0] = drive;
            close_receipt();
            receipt = CreateFileA(summary, GENERIC_READ | FILE_APPEND_DATA, FILE_SHARE_READ,
                                  NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
            LARGE_INTEGER size = {0};
            int size_ok = receipt != INVALID_HANDLE_VALUE && GetFileSizeEx(receipt, &size);
            if (wgi_receipt_resume(size.QuadPart, size_ok, RECEIPT_LIMIT,
                &receipt_bytes, &receipt_truncated)) {
                capture_failed = 1;
                close_receipt();
            }
            break;
        }
    }
}
static void result_close(void) {
    close_receipt();
    close_runtime();
    if (result != INVALID_HANDLE_VALUE) CloseHandle(result);
}
static void runtime_begin(const char *phase) {
    close_runtime();
    runtime_bytes = 0; runtime_truncated = 0;
    if (!result_drive) return;
    char path[] = "C:\\wgi-runtime-setup.txt"; path[0] = result_drive;
    runtime = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS,
                          FILE_ATTRIBUTE_NORMAL, NULL);
    if (runtime == INVALID_HANDLE_VALUE) { capture_failed = 1; return; }
    char header[128];
    int n = snprintf(header, sizeof(header), "WGI_RUNTIME_SNAPSHOT phase=%s\r\n", phase);
    if (n > 0 && n < (int)sizeof(header)) runtime_record(header, (unsigned)n);
}
static int bounded_string(const char *s, DWORD bytes, DWORD capacity) {
    return bytes && bytes <= capacity && memchr(s, 0, bytes) == s + bytes - 1;
}
static int device_string(HDEVINFO set, SP_DEVINFO_DATA *device, DWORD property,
                         char *buffer, DWORD capacity) {
    DWORD type = 0, bytes = 0;
    memset(buffer, 0, capacity);
    return SetupDiGetDeviceRegistryPropertyA(set, device, property, &type,
        (BYTE *)buffer, capacity, &bytes) && type == REG_SZ &&
        bounded_string(buffer, bytes, capacity);
}
static void registry_string(HKEY key, const char *name, const char *label) {
    char value[512]; DWORD bytes = sizeof(value), type = 0;
    memset(value, 0, sizeof(value));
    LONG rc = RegQueryValueExA(key, name, NULL, &type, (BYTE *)value, &bytes);
    if (rc == ERROR_SUCCESS && type == REG_SZ &&
        bounded_string(value, bytes, sizeof(value)))
        diagnostic("WGI_UART_%s value=%s\r\n", label, value);
    else diagnostic("WGI_UART_%s unavailable rc=%ld type=%lu bytes=%lu\r\n",
                    label, rc, type, bytes);
}
static void registry_dword(HKEY key, const char *name, const char *label) {
    DWORD value = 0, bytes = sizeof(value), type = 0;
    LONG rc = RegQueryValueExA(key, name, NULL, &type, (BYTE *)&value, &bytes);
    if (rc == ERROR_SUCCESS && type == REG_DWORD && bytes == sizeof(value))
        diagnostic("WGI_UART_%s value=%lu\r\n", label, value);
    else diagnostic("WGI_UART_%s unavailable rc=%ld type=%lu bytes=%lu\r\n",
                    label, rc, type, bytes);
}
static void resources(DEVINST device, ULONG kind) {
    LOG_CONF config;
    CONFIGRET rc = CM_Get_First_Log_Conf(&config, device, kind);
    diagnostic("WGI_UART_RESOURCES kind=%lu config_rc=%lu\r\n", kind, rc);
    if (rc != CR_SUCCESS) return;
    RES_DES current = (RES_DES)config;
    for (unsigned i = 0; i < 32; ++i) {
        RES_DES next; RESOURCEID type;
        rc = CM_Get_Next_Res_Des(&next, current, ResType_All, &type, 0);
        if (current != (RES_DES)config) CM_Free_Res_Des_Handle(current);
        current = (RES_DES)config;
        if (rc != CR_SUCCESS) {
            diagnostic("WGI_UART_RESOURCE_END rc=%lu\r\n", rc); break;
        }
        current = next;
        ULONG bytes = 0; BYTE data[4096] = {0};
        rc = CM_Get_Res_Des_Data_Size(&bytes, current, 0);
        if (rc == CR_SUCCESS && bytes && bytes <= sizeof(data))
            rc = CM_Get_Res_Des_Data(current, data, bytes, 0);
        else {
            diagnostic("WGI_UART_RESOURCE type=%lu size=%lu size_rc=%lu\r\n", type, bytes, rc);
            continue;
        }
        char hex[257]; unsigned count = bytes < 128 ? bytes : 128;
        for (unsigned j = 0; j < count; ++j) snprintf(hex + j * 2, 3, "%02x", data[j]);
        hex[count * 2] = 0;
        diagnostic("WGI_UART_RESOURCE type=%lu bytes=%lu rc=%lu raw_prefix=%s\r\n",
                   type, bytes, rc, rc == CR_SUCCESS ? hex : "UNAVAILABLE");
    }
    if (current != (RES_DES)config) CM_Free_Res_Des_Handle(current);
    CM_Free_Log_Conf_Handle(config);
}
static void uart_devices(void) {
    HDEVINFO set = SetupDiGetClassDevsA(NULL, NULL, NULL, DIGCF_ALLCLASSES | DIGCF_PRESENT);
    if (set == INVALID_HANDLE_VALUE) {
        diagnostic("WGI_UART_ENUM failed error=%lu\r\n", GetLastError()); return;
    }
    unsigned selected = 0;
    for (DWORD i = 0; i < 4096; ++i) {
        SP_DEVINFO_DATA device;
        memset(&device, 0, sizeof(device)); device.cbSize = sizeof(device);
        if (!SetupDiEnumDeviceInfo(set, i, &device)) {
            diagnostic("WGI_UART_ENUM_END error=%lu selected=%u\r\n", GetLastError(), selected);
            break;
        }
        char id[1024] = {0}, service[128], cls[128], driver[512]; DWORD id_bytes = 0;
        if (!SetupDiGetDeviceInstanceIdA(set, &device, id, sizeof(id), &id_bytes) ||
            !bounded_string(id, id_bytes, sizeof(id))) continue;
        int service_ok = device_string(set, &device, SPDRP_SERVICE, service, sizeof(service));
        int class_ok = device_string(set, &device, SPDRP_CLASS, cls, sizeof(cls));
        char hwids[4096]; DWORD bytes = 0, type = 0;
        memset(hwids, 0, sizeof(hwids));
        int hw_ok = SetupDiGetDeviceRegistryPropertyA(set, &device, SPDRP_HARDWAREID,
            &type, (BYTE *)hwids, sizeof(hwids), &bytes) && type == REG_MULTI_SZ &&
            bytes >= 2 && bytes <= sizeof(hwids) && !hwids[bytes - 1] && !hwids[bytes - 2];
        int uart = !_strnicmp(id, "ACPI\\PNP0501", 12) || !_strnicmp(id, "ACPI\\PNP0500", 12) ||
            (service_ok && (!_stricmp(service, "Serial") || !_stricmp(service, "Serenum"))) ||
            (class_ok && !_stricmp(cls, "Ports"));
        if (hw_ok) {
            DWORD off = 0;
            while (off < bytes - 1 && hwids[off]) {
                const char *p = hwids + off;
                const char *end = memchr(p, 0, bytes - off);
                if (!end) { hw_ok = 0; break; }
                if (!_stricmp(p, "*PNP0501") || !_stricmp(p, "*PNP0500") ||
                    !_stricmp(p, "ACPI\\PNP0501") || !_stricmp(p, "ACPI\\PNP0500")) uart = 1;
                off += (DWORD)(end - p) + 1;
            }
            if (off != bytes - 1) hw_ok = 0;
        }
        if (!uart) continue;
        ++selected;
        ULONG status = 0, problem = 0;
        CONFIGRET status_rc = CM_Get_DevNode_Status(&status, &problem, device.DevInst, 0);
        int driver_ok = device_string(set, &device, SPDRP_DRIVER, driver, sizeof(driver));
        diagnostic("WGI_UART id=%s class=%s service=%s status_rc=%lu status=%lu problem=%lu\r\n",
                   id, class_ok ? cls : "UNKNOWN", service_ok ? service : "UNKNOWN",
                   status_rc, status, problem);
        diagnostic("WGI_UART_DRIVER key=%s hwids_valid=%d\r\n",
                   driver_ok ? driver : "UNKNOWN", hw_ok);
        if (hw_ok) {
            DWORD off = 0;
            while (off < bytes - 1 && hwids[off]) {
                const char *end = memchr(hwids + off, 0, bytes - off);
                if (!end) break;
                diagnostic("WGI_UART_HWID value=%s\r\n", hwids + off);
                off += (DWORD)(end - (hwids + off)) + 1;
            }
        }
        HKEY hardware = SetupDiOpenDevRegKey(set, &device, DICS_FLAG_GLOBAL, 0,
                                             DIREG_DEV, KEY_QUERY_VALUE);
        if (hardware != INVALID_HANDLE_VALUE) {
            registry_string(hardware, "PortName", "PORTNAME");
            registry_string(hardware, "Identifier", "IDENTIFIER");
            registry_dword(hardware, "SerialSkipExternalNaming", "SKIP_EXTERNAL_NAMING");
            RegCloseKey(hardware);
        } else diagnostic("WGI_UART_PORTNAME open_error=%lu\r\n", GetLastError());
        HKEY software = SetupDiOpenDevRegKey(set, &device, DICS_FLAG_GLOBAL, 0,
                                             DIREG_DRV, KEY_QUERY_VALUE);
        if (software != INVALID_HANDLE_VALUE) {
            registry_string(software, "InfPath", "INF"); RegCloseKey(software);
        } else diagnostic("WGI_UART_INF open_error=%lu\r\n", GetLastError());
        resources(device.DevInst, ALLOC_LOG_CONF);
        resources(device.DevInst, BOOT_LOG_CONF);
    }
    SetupDiDestroyDeviceInfoList(set);
}
static void serial_namespace(void) {
    char target[512] = {0};
    DWORD bytes = QueryDosDeviceA("COM1", target, sizeof(target));
    diagnostic("WGI_COM1_NAMESPACE mapping=%s query_error=%lu\r\n",
        bytes && bytes <= sizeof(target) && memchr(target, 0, bytes) ? target : "UNKNOWN",
        bytes ? 0 : GetLastError());
    HKEY key;
    LONG rc = RegOpenKeyExA(HKEY_LOCAL_MACHINE, "HARDWARE\\DEVICEMAP\\SERIALCOMM", 0,
                            KEY_QUERY_VALUE, &key);
    diagnostic("WGI_SERIALCOMM open_rc=%ld\r\n", rc);
    if (rc != ERROR_SUCCESS) return;
    for (DWORD i = 0; i < 128; ++i) {
        char name[256], value[512]; DWORD names = sizeof(name), size = sizeof(value), type = 0;
        memset(name, 0, sizeof(name)); memset(value, 0, sizeof(value));
        rc = RegEnumValueA(key, i, name, &names, NULL, &type, (BYTE *)value, &size);
        if (rc == ERROR_NO_MORE_ITEMS) break;
        if (rc == ERROR_SUCCESS && names < sizeof(name) && name[names] == 0 && type == REG_SZ &&
            bounded_string(value, size, sizeof(value)))
            diagnostic("WGI_SERIALCOMM name=%s port=%s\r\n", name, value);
        else diagnostic("WGI_SERIALCOMM index=%lu rc=%ld type=%lu bytes=%lu\r\n", i, rc,type,size);
    }
    RegCloseKey(key);
}
static void log_state(const char *path, int runtime_evidence, long long size,
                      long long offset, unsigned expected, unsigned actual, int size_ok,
                      DWORD size_error, int seek_ok, DWORD seek_error, DWORD read_error,
                      int early_eof) {
    int read_complete = size_ok && !wgi_log_status(expected, actual, seek_ok,
                                                  read_error == 0 && !early_eof);
    int complete = read_complete && !diagnostic_failed &&
        (!runtime_evidence || (runtime != INVALID_HANDLE_VALUE && !runtime_truncated));
    char state[1024];
    int n = snprintf(state, sizeof(state),
        "WGI_LOG_STATE path=%s size_ok=%d initial_size=%lld offset=%lld expected=%u read=%u "
        "size_error=%lu seek_ok=%d seek_error=%lu read_error=%lu early_eof=%d read_complete=%d "
        "complete=%d runtime_available=%d runtime_truncated=%d "
        "diagnostic_error=%d capture_error=%d\r\n",
        path, size_ok, size, offset, expected, actual, size_error, seek_ok, seek_error,
        read_error, early_eof, read_complete, complete, runtime != INVALID_HANDLE_VALUE,
        runtime_truncated, diagnostic_failed, capture_failed);
    if (n > 0 && n < (int)sizeof(state)) {
        diagnostic("%s", state);
        if (runtime_evidence) runtime_record(state, (unsigned)n);
    }
}
static void log_tail(const char *path, int runtime_evidence) {
    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE |
                              FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        diagnostic("WGI_LOG path=%s open_error=%lu optional_unavailable=1\r\n", path, error);
        log_state(path, runtime_evidence, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0);
        return;
    }
    LARGE_INTEGER size = {0}, offset = {0};
    BOOL size_ok = GetFileSizeEx(file, &size);
    DWORD size_error = size_ok ? 0 : GetLastError();
    if (!size_ok || size.QuadPart < 0) {
        if (size_ok) size_error = ERROR_INVALID_DATA;
        capture_failed = 1;
        log_state(path, runtime_evidence, -1, -1, 0, 0, 0, size_error, 0, 0, 0, 0);
        CloseHandle(file); return;
    }
    offset.QuadPart = size.QuadPart > 65536 ? size.QuadPart - 65536 : 0;
    unsigned expected = (unsigned)(size.QuadPart - offset.QuadPart), actual = 0;
    diagnostic("WGI_LOG_BEGIN path=%s total=%lld offset=%lld expected=%u\r\n",
               path, size.QuadPart, offset.QuadPart, expected);
    if (runtime_evidence) {
        char header[512];
        int n = snprintf(header, sizeof(header),
            "WGI_RUNTIME_LOG_BEGIN path=%s total=%lld offset=%lld expected=%u\r\n",
            path, size.QuadPart, offset.QuadPart, expected);
        if (n > 0 && n < (int)sizeof(header)) runtime_record(header, (unsigned)n);
    }
    BOOL seek_ok = SetFilePointerEx(file, offset, NULL, FILE_BEGIN);
    DWORD seek_error = seek_ok ? 0 : GetLastError(), read_error = 0;
    int early_eof = 0;
    if (seek_ok) {
        while (actual < expected) {
            char buffer[4096]; DWORD n = 0;
            unsigned remaining = expected - actual;
            DWORD request = remaining < sizeof(buffer) ? remaining : sizeof(buffer);
            if (!ReadFile(file, buffer, request, &n, NULL)) {
                read_error = GetLastError();
                if (!read_error) read_error = ERROR_READ_FAULT;
                break;
            }
            if (!n) { early_eof = 1; break; }
            if (n > request) { read_error = ERROR_INVALID_DATA; break; }
            actual += n;
            unsigned written = 0;
            if (!diagnostic_write(buffer, n, &written)) break;
            if (runtime_evidence) runtime_record(buffer, n);
        }
    }
    if (wgi_log_status(expected, actual, seek_ok, read_error == 0 && !early_eof))
        capture_failed = 1;
    diagnostic("\r\nWGI_LOG_END\r\n");
    if (runtime_evidence) {
        const char end[] = "\r\nWGI_RUNTIME_LOG_END\r\n";
        runtime_record(end, sizeof(end) - 1);
    }
    log_state(path, runtime_evidence, size.QuadPart, offset.QuadPart, expected, actual,
              1, 0, seek_ok, seek_error, read_error, early_eof);
    CloseHandle(file);
}
static void service_snapshot(const char *name, const char *phase);
static void serial_diagnostics(const char *phase) {
    diagnostic("WGI_SERIAL_SNAPSHOT phase=%s\r\n", phase);
    service_snapshot("Serial", phase);
    service_snapshot("Serenum", phase);
    serial_namespace();
    uart_devices();
    runtime_begin(phase);
    log_tail("X:\\Windows\\System32\\wpeinit.log", 1);
    log_tail("X:\\Windows\\inf\\setupapi.dev.log", 1);
    log_tail("X:\\Windows\\inf\\setupapi.offline.log", 0);
    char state[160];
    int n = snprintf(state, sizeof(state),
        "WGI_RUNTIME_STATE receipt_truncated=%d runtime_truncated=%d error=%d capture_error=%d\r\n",
        receipt_truncated, runtime_truncated, diagnostic_failed, capture_failed);
    if (n > 0 && n < (int)sizeof(state)) runtime_record(state, (unsigned)n);
    diagnostic("WGI_DIAGNOSTIC_STATE receipt_truncated=%d runtime_truncated=%d error=%d "
               "capture_error=%d receipt_available=%d runtime_available=%d\r\n",
               receipt_truncated, runtime_truncated, diagnostic_failed, capture_failed,
               receipt != INVALID_HANDLE_VALUE, runtime != INVALID_HANDLE_VALUE);
}

static void service_state(SC_HANDLE service, const char *name, const char *phase) {
    SERVICE_STATUS_PROCESS status; DWORD bytes = 0;
    memset(&status, 0, sizeof(status));
    BOOL ok = QueryServiceStatusEx(service, SC_STATUS_PROCESS_INFO,
                                  (BYTE *)&status, sizeof(status), &bytes);
    diagnostic("WGI_SERIAL_SERVICE_STATE name=%s phase=%s ok=%d state=%lu exit=%lu error=%lu\r\n",
               name, phase, ok, status.dwCurrentState, status.dwWin32ExitCode,
               ok ? 0 : GetLastError());
    union { QUERY_SERVICE_CONFIGA config; BYTE bytes[4096]; } configuration;
    ok = QueryServiceConfigA(service, &configuration.config, sizeof(configuration), &bytes);
    diagnostic("WGI_SERIAL_SERVICE_CONFIG name=%s phase=%s ok=%d start=%lu error=%lu\r\n",
               name, phase, ok, ok ? configuration.config.dwStartType : 0,
               ok ? 0 : GetLastError());
}
static void service_snapshot(const char *name, const char *phase) {
    SC_HANDLE scm = OpenSCManagerA(NULL, NULL, SC_MANAGER_CONNECT);
    if (!scm) { diagnostic("WGI_SERIAL_SCM query_error=%lu\r\n", GetLastError()); return; }
    SC_HANDLE service = OpenServiceA(scm, name, SERVICE_QUERY_STATUS | SERVICE_QUERY_CONFIG);
    if (service) {
        service_state(service, name, phase);
        CloseServiceHandle(service);
    } else diagnostic("WGI_SERIAL_SERVICE name=%s phase=%s query_error=%lu\r\n",
                      name, phase, GetLastError());
    CloseServiceHandle(scm);
    char path[256];
    int n = snprintf(path, sizeof(path), "SYSTEM\\CurrentControlSet\\Services\\%s\\Parameters",
                     name);
    HKEY key;
    LONG rc = n > 0 && n < (int)sizeof(path) ? RegOpenKeyExA(HKEY_LOCAL_MACHINE, path, 0,
        KEY_QUERY_VALUE, &key) : ERROR_INVALID_PARAMETER;
    diagnostic("WGI_SERIAL_PARAMETERS name=%s open_rc=%ld\r\n", name, rc);
    if (rc == ERROR_SUCCESS) {
        registry_string(key, "Identifier", "SERVICE_IDENTIFIER");
        registry_dword(key, "SerialSkipExternalNaming", "SERVICE_SKIP_EXTERNAL_NAMING");
        RegCloseKey(key);
    }
}
static void service_start(const char *name) {
    SC_HANDLE scm = OpenSCManagerA(NULL, NULL, SC_MANAGER_CONNECT);
    if (!scm) { diagnostic("WGI_SERIAL_SCM open_error=%lu\r\n", GetLastError()); return; }
    SC_HANDLE svc = OpenServiceA(scm, name, SERVICE_START | SERVICE_CHANGE_CONFIG |
                                 SERVICE_QUERY_STATUS | SERVICE_QUERY_CONFIG);
    if (svc) {
        service_state(svc, name, "before");
        BOOL configured = ChangeServiceConfigA(svc, SERVICE_NO_CHANGE, SERVICE_DEMAND_START,
            SERVICE_NO_CHANGE, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
        DWORD config_error = configured ? 0 : GetLastError();
        BOOL ok = StartServiceA(svc, 0, NULL);
        DWORD start_error = ok ? 0 : GetLastError();
        diagnostic("WGI_SERIAL_SERVICE name=%s config_ok=%d config_error=%lu "
                   "start_ok=%d start_error=%lu\r\n",
                   name, configured, config_error, ok, start_error);
        service_state(svc, name, "after");
        CloseServiceHandle(svc);
    } else diagnostic("WGI_SERIAL_SERVICE name=%s open_error=%lu\r\n", name, GetLastError());
    CloseServiceHandle(scm);
}
static int serial_open(void) {
    service_start("Serial");
    service_start("Serenum");
    serial_diagnostics("after-wpeinit-before-com");
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

static int supervise(unsigned seconds, char *command, int gpu_required) {
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
                rc = wgi_supervised(marker_flags == 1 ? "WGI_GPU_PASS\n" : "",
                                    child_rc, 0, gpu_required);
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
    if (gpu_required) {
        if (timed_out) text("WGI_GPU_TIMEOUT\r\n");
        if (rc || serial_failed) text("WGI_GPU_FAIL\r\nWGI_GPU_END\r\n");
    } else diagnostic("WGI_BOOTSTRAP_END rc=%d timed_out=%d\r\n", rc, timed_out);
    if (pi.hThread) CloseHandle(pi.hThread);
    if (pi.hProcess) CloseHandle(pi.hProcess);
    if (read) CloseHandle(read);
    if (write) CloseHandle(write);
    if (job) CloseHandle(job);
    return rc || serial_failed || diagnostic_failed || timed_out;
}
static int cpu_diagnostic_write(const void *bytes, unsigned count, unsigned *written) {
    (void)bytes; *written = count; return 1;
}
static int cpu_failed_serial(const void *bytes, unsigned count, unsigned *written) {
    (void)bytes; (void)count; *written = 0; return 0;
}
int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IONBF, 0);
    if (argc == 2 && !strcmp(argv[1], "--cpu-test")) {
        /* No COM, SCM, registry, disk, process, device or GPU operations in this mode. */
        int ok = !wgi_terminal("WGI_GPU_PASS\n", 0, 0) &&
            wgi_terminal("WGI_GPU_PASS\n", 0, 1) &&
            wgi_terminal("WGI_PE_BOOT_OK_V1\n", 0, 0) &&
            wgi_terminal("WGI_GPU_PASS\rgarbage\n", 0, 0) &&
            wgi_terminal("WGI_GPU_PASS\nWGI_GPU_FAIL_SUFFIX\n", 0, 0) &&
            !wgi_supervised("", 0, 0, 0) && wgi_supervised("", 0, 0, 1) &&
            wgi_supervised("", 1, 0, 0) && wgi_supervised("", 0, 1, 0) &&
            !wgi_tee(cpu_diagnostic_write, cpu_failed_serial, "test", 4, 0) &&
            wgi_tee(cpu_diagnostic_write, cpu_failed_serial, "test", 4, 1);
        puts(ok ? "WGI_CPU_POLICY_PASS" : "WGI_CPU_POLICY_FAIL");
        return ok ? 0 : 1;
    }
    result_open();
    if (argc == 3 && !strcmp(argv[1], "--bootstrap")) {
        diagnostic_only = 1;
        serial_diagnostics("before-bootstrap");
        int bootstrap_rc = supervise(240, argv[2], 0);
        if (result == INVALID_HANDLE_VALUE) result_open();
        serial_diagnostics("after-bootstrap");
        diagnostic("WGI_BOOTSTRAP_RESULT rc=%d diagnostic_error=%d\r\n",
                   bootstrap_rc, diagnostic_failed);
        result_close();
        return bootstrap_rc || diagnostic_failed;
    }
    if (!serial_open()) {
        text("WGI_SERIAL_FAIL configuration_or_open\r\nWGI_GPU_FAIL\r\nWGI_GPU_END\r\n");
        if (serial != INVALID_HANDLE_VALUE) CloseHandle(serial);
        result_close();
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
            else rc = supervise((unsigned)seconds, argv[3], 1);
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
    result_close();
    return rc || serial_failed;
}
