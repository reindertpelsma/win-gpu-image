/* Original native WinPE preparation; no PowerShell or vendor-binary modification. */
#define _WIN32_WINNT 0x0602
#ifdef WGI_TEST
#include "../tests/winapi-gpu.h"
#else
#include <windows.h>
#include <setupapi.h>
#include <cfgmgr32.h>
#endif
#include <stdio.h>
#include <string.h>
#include "pe-contract.h"

static HDEVINFO devices = INVALID_HANDLE_VALUE;
static SP_DEVINFO_DATA device;
static HKEY class_key, service_key;
static const char *inf;
static DWORD class_value, service_value;
static int no_adapter;

static int property(DWORD key, char *buffer, DWORD count) {
    DWORD type = 0;
    memset(buffer, 0, count);
    return SetupDiGetDeviceRegistryPropertyA(devices, &device, key, &type,
                                             (BYTE *)buffer, count, NULL);
}

static int change(DWORD state) {
    SP_PROPCHANGE_PARAMS p;
    memset(&p, 0, sizeof(p));
    p.ClassInstallHeader.cbSize = sizeof(SP_CLASSINSTALL_HEADER);
    p.ClassInstallHeader.InstallFunction = DIF_PROPERTYCHANGE;
    p.StateChange = state;
    p.Scope = DICS_FLAG_CONFIGSPECIFIC;
    if (!SetupDiSetClassInstallParamsA(devices, &device, &p.ClassInstallHeader, sizeof(p)) ||
        !SetupDiCallClassInstaller(DIF_PROPERTYCHANGE, devices, &device)) return 0;
    SP_DEVINSTALL_PARAMS_A params;
    memset(&params, 0, sizeof(params)); params.cbSize = sizeof(params);
    if (!SetupDiGetDeviceInstallParamsA(devices, &device, &params) ||
        (params.Flags & (DI_NEEDREBOOT | DI_NEEDRESTART))) return 0;
    ULONG status = 0, problem = 0;
    if (CM_Get_DevNode_Status(&status, &problem, device.DevInst, 0) != CR_SUCCESS) return 0;
    return state == DICS_DISABLE ? problem == CM_PROB_DISABLED :
        ((status & DN_STARTED) && problem == 0);
}

static int find_adapter(void) {
    devices = SetupDiGetClassDevsA(NULL, "PCI", NULL, DIGCF_ALLCLASSES | DIGCF_PRESENT);
    if (devices == INVALID_HANDLE_VALUE) return 0;
    int found = 0;
    for (DWORD i = 0; ; ++i) {
        SP_DEVINFO_DATA candidate;
        memset(&candidate, 0, sizeof(candidate)); candidate.cbSize = sizeof(candidate);
        if (!SetupDiEnumDeviceInfo(devices, i, &candidate)) {
            if (GetLastError() != ERROR_NO_MORE_ITEMS) return 0;
            break;
        }
        char id[1024], compatible[4096]; DWORD type;
        if (!SetupDiGetDeviceInstanceIdA(devices, &candidate, id, sizeof(id), NULL) ||
            _strnicmp(id, "PCI\\VEN_10DE&", 13)) continue;
        memset(compatible, 0, sizeof(compatible));
        if (!SetupDiGetDeviceRegistryPropertyA(devices, &candidate, SPDRP_COMPATIBLEIDS,
            &type, (BYTE *)compatible, sizeof(compatible), NULL) || type != REG_MULTI_SZ) continue;
        int display = 0;
        for (char *p = compatible; *p; p += strlen(p) + 1) {
            if (!_strnicmp(p, "PCI\\CC_0300", 11) ||
                !_strnicmp(p, "PCI\\CC_0302", 11)) display = 1;
        }
        if (!display) continue;
        if (++found != 1) return 0;
        device = candidate;
        printf("WGI_ADAPTER instance=%s\n", id);
    }
    no_adapter = found == 0;
    return found == 1;
}

static int write_one(HKEY key, const char *name, DWORD value) {
    return RegSetValueExA(key, name, 0, REG_DWORD, (const BYTE *)&value, sizeof(value)) ==
        ERROR_SUCCESS;
}

static int read_one(HKEY key, DWORD *value) {
    DWORD type = 0, bytes = sizeof(*value);
    return RegQueryValueExA(key, "EnableGpuFirmware", NULL, &type, (BYTE *)value, &bytes) ==
        ERROR_SUCCESS && type == REG_DWORD && bytes == sizeof(*value) && *value == 1;
}

static int step(int n) {
    printf("WGI_GSP_STEP step=%d\n", n);
    if (n == 0) return change(DICS_DISABLE);
    if (n == 1) {
        SP_DEVINSTALL_PARAMS_A p;
        memset(&p, 0, sizeof(p)); p.cbSize = sizeof(p);
        if (!SetupDiGetDeviceInstallParamsA(devices, &device, &p) ||
            strlen(inf) >= sizeof(p.DriverPath)) return 0;
        p.Flags |= DI_ENUMSINGLEINF | DI_DONOTCALLCONFIGMG;
        strcpy(p.DriverPath, inf);
        if (!SetupDiSetDeviceInstallParamsA(devices, &device, &p) ||
            !SetupDiBuildDriverInfoList(devices, &device, SPDIT_COMPATDRIVER) ||
            !SetupDiCallClassInstaller(DIF_SELECTBESTCOMPATDRV, devices, &device)) return 0;
        SP_DRVINFO_DATA_A driver;
        memset(&driver, 0, sizeof(driver)); driver.cbSize = sizeof(driver);
        if (!SetupDiGetSelectedDriverA(devices, &device, &driver) ||
            _stricmp(driver.ProviderName, "NVIDIA")) return 0;
        return SetupDiCallClassInstaller(DIF_INSTALLDEVICE, devices, &device);
    }
    if (n == 2) {
        char driver[512], service[128];
        if (!property(SPDRP_DRIVER, driver, sizeof(driver)) ||
            !property(SPDRP_SERVICE, service, sizeof(service)) ||
            _stricmp(service, "nvlddmkm")) return 0;
        const char *prefix = "{4d36e968-e325-11ce-bfc1-08002be10318}\\";
        if (_strnicmp(driver, prefix, strlen(prefix)) ||
            strlen(driver + strlen(prefix)) != 4 ||
            strspn(driver + strlen(prefix), "0123456789") != 4) return 0;
        printf("WGI_DRIVER_KEY key=%s service=%s\n", driver, service);
        class_key = SetupDiOpenDevRegKey(devices, &device, DICS_FLAG_GLOBAL, 0,
                                        DIREG_DRV, KEY_SET_VALUE | KEY_QUERY_VALUE);
        if (class_key == INVALID_HANDLE_VALUE) { class_key = NULL; return 0; }
        return RegOpenKeyExA(HKEY_LOCAL_MACHINE,
            "SYSTEM\\CurrentControlSet\\Services\\nvlddmkm", 0,
            KEY_SET_VALUE | KEY_QUERY_VALUE, &service_key) == ERROR_SUCCESS;
    }
    if (n == 3) return write_one(service_key, "EnableGpuFirmware", 1);
    if (n == 4) return write_one(class_key, "EnableGpuFirmware", 1);
    if (n == 5) return read_one(class_key, &class_value);
    if (n == 6) {
        int ok = read_one(service_key, &service_value);
        printf("WGI_GSP class=%lu service=%lu readback_ok=%d\n",
               class_value, service_value, ok && class_value == 1);
        return ok;
    }
    if (n == 7) {
        SP_DEVINSTALL_PARAMS_A p;
        memset(&p, 0, sizeof(p)); p.cbSize = sizeof(p);
        if (!SetupDiGetDeviceInstallParamsA(devices, &device, &p)) return 0;
        p.Flags &= ~DI_DONOTCALLCONFIGMG;
        return SetupDiSetDeviceInstallParamsA(devices, &device, &p) &&
            write_one(service_key, "Start", SERVICE_DEMAND_START);
    }
    return change(DICS_ENABLE);
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IONBF, 0);
    if (argc != 2 || !find_adapter()) {
        printf("WGI_GSP class=0 service=0 readback_ok=0 reason=adapter_selection\n");
        return no_adapter ? 3 : 2;
    }
    int rc;
    if (!strcmp(argv[1], "--hold")) {
        char service[128]; ULONG status = 0, problem = 0;
        if (CM_Get_DevNode_Status(&status, &problem, device.DevInst, 0) != CR_SUCCESS) rc = 2;
        else if ((status & DN_STARTED) &&
                 (!property(SPDRP_SERVICE, service, sizeof(service)) ||
                  !_stricmp(service, "nvlddmkm"))) {
            printf("WGI_GSP reason=nvidia_already_started\n"); rc = 2;
        } else rc = change(DICS_DISABLE) ? 0 : 2;
    } else {
        inf = argv[1];
        rc = wgi_gsp(step);
        if (rc) printf("WGI_GSP class=%lu service=%lu readback_ok=0 failed_step=%d error=%lu\n",
                       class_value, service_value, rc, GetLastError());
    }
    if (class_key) RegCloseKey(class_key);
    if (service_key) RegCloseKey(service_key);
    SetupDiDestroyDeviceInfoList(devices);
    return rc;
}
