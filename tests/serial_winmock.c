/* Exercise the real WinAPI adapter with original deterministic CPU doubles. */
#define WGI_SERIAL_API_TEST
#define main actual_serial_main
#include "../programs/serial-prepare.c"
#undef main
static const char *scenario;
static DWORD last_error = ERROR_NO_MORE_ITEMS;
static SP_DEVINSTALL_PARAMS_A params;
static int dif_calls, builds;
DWORD GetLastError(void) { return last_error; }
UINT GetWindowsDirectoryA(char *out, UINT cap) {
    (void)cap; strcpy(out,"X:\\Windows"); return (UINT)strlen(out);
}
UINT GetSystemDirectoryA(char *out, UINT cap) {
    (void)cap; strcpy(out,"X:\\Windows\\System32"); return (UINT)strlen(out);
}
DWORD GetFileAttributesA(const char *path) {
    (void)path; return !strcmp(scenario,"dll_missing") ? INVALID_FILE_ATTRIBUTES : 0;
}
DWORD QueryDosDeviceA(const char *name, char *out, DWORD cap) {
    (void)name; (void)out; (void)cap;
    last_error = !strcmp(scenario,"namespace_error") ? 5 : ERROR_FILE_NOT_FOUND;
    return 0;
}
HDEVINFO SetupDiGetClassDevsA(const GUID *g, const char *a, void *b, DWORD flags) {
    (void)g; (void)a; (void)b; return (HDEVINFO)(ptrdiff_t)flags;
}
BOOL SetupDiEnumDeviceInfo(HDEVINFO a, DWORD i, SP_DEVINFO_DATA *d) {
    (void)a; d->DevInst = 7; last_error = ERROR_NO_MORE_ITEMS;
    return i == 0 || (i == 1 && !strcmp(scenario,"duplicate"));
}
BOOL SetupDiGetDeviceInstanceIdA(HDEVINFO a, SP_DEVINFO_DATA *d, char *out,
                                DWORD cap, DWORD *size) {
    (void)a; (void)d; strcpy(out,"ACPI\\PNP0501\\1"); *size = (DWORD)strlen(out) + 1;
    if (!strcmp(scenario,"instance_size")) *size = cap + 1;
    return 1;
}
BOOL SetupDiGetDeviceRegistryPropertyA(HDEVINFO a, SP_DEVINFO_DATA *d, DWORD key,
                                      DWORD *type, BYTE *out, DWORD cap, DWORD *size) {
    (void)a; (void)d; (void)cap; *type = REG_SZ;
    if (key == SPDRP_HARDWAREID) {
        static const char ids[] = "ACPI\\PNP0501\0*PNP0501\0";
        memcpy(out,ids,sizeof(ids)); *size = sizeof(ids); *type = REG_MULTI_SZ;
        if (!strcmp(scenario,"hardware_wrong_type")) *type = REG_SZ;
        if (!strcmp(scenario,"hardware_size")) *size = cap + 1;
        if (!strcmp(scenario,"hardware_no_double_nul")) out[*size - 1] = 'x';
        if (!strcmp(scenario,"hardware_unterminated")) memset(out,'x',*size);
        if (!strcmp(scenario,"hardware_trailing")) { out[*size] = 0; ++*size; }
        if (!strcmp(scenario,"hardware_early_match_bad_tail")) out[*size - 1] = 'x';
        if (!strcmp(scenario,"hardware_early_empty")) {
            static const char malformed[] = "ACPI\\PNP0501\0\0*PNP0501\0";
            memcpy(out,malformed,sizeof(malformed)); *size = sizeof(malformed);
        }
        if (!strcmp(scenario,"hardware_after_dif") && dif_calls) *type = REG_SZ;
        if (!strcmp(scenario,"hardware_member_lost") && dif_calls) {
            static const char lost[] = "ACPI\\PNP0501\0";
            memcpy(out,lost,sizeof(lost)); *size = sizeof(lost);
        }
        if (!strcmp(scenario,"hardware_read_error")) return 0;
        return 1;
    }
    const char *text = key == SPDRP_SERVICE ? "Serial" : key == SPDRP_CLASSGUID ?
        "{4d36e978-e325-11ce-bfc1-08002be10318}" : "diagnostic";
    strcpy((char *)out,text); *size = (DWORD)strlen(text) + 1;
    if (key == SPDRP_SERVICE && !strcmp(scenario,"property_type")) *type = 7;
    return 1;
}
CONFIGRET CM_Get_DevNode_Status(ULONG *status, ULONG *problem, DWORD a, DWORD b) {
    (void)a; (void)b; *status = DN_STARTED; *problem = 0; return CR_SUCCESS;
}
HKEY SetupDiOpenDevRegKey(HDEVINFO a, SP_DEVINFO_DATA *b, DWORD c, DWORD d, DWORD kind,
                          DWORD access) {
    (void)a; (void)b; (void)c; (void)d; (void)access;
    return (HKEY)(ptrdiff_t)(kind == DIREG_DRV ? 11 : 12);
}
LONG RegQueryValueExA(HKEY key, const char *name, void *reserved, DWORD *type,
                     BYTE *out, DWORD *size) {
    (void)key; (void)reserved; DWORD capacity = *size;
    if (!strcmp(name,"PortName") || !strcmp(name,"Identifier") ||
        !strcmp(name,"InfSectionExt")) return ERROR_FILE_NOT_FOUND;
    const char *s = !strcmp(name,"InfPath") ? "msports.inf" :
        !strcmp(name,"ProviderName") ? "Microsoft" :
        !strcmp(name,"InfSection") ? "ComPort.NT" :
        !strcmp(name,"MatchingDeviceId") ?
        ((!strcmp(scenario,"hardware_early_match_bad_tail") ||
          !strcmp(scenario,"hardware_early_empty") ||
          (!strcmp(scenario,"matching_after_dif") && dif_calls)) ? "ACPI\\PNP0501" :
         !strcmp(scenario,"hardware_nonmember") ? "*PNP0400" :
         !strcmp(scenario,"hardware_star_not_glob") ? "*PNP05*" :
         !strcmp(scenario,"hardware_case") ? "*pnp0501" : "*PNP0501") :
        !strcmp(scenario,"installer_untrusted") ? "Other.dll,Other" :
        "MsPorts.dll,PortsClassInstaller";
    strcpy((char *)out,s); *size = (DWORD)strlen(s) + 1; *type = REG_SZ;
    if (!strcmp(name,"MatchingDeviceId") &&
        (!strcmp(scenario,"matching_more_data") || !strcmp(scenario,"matching_access_error"))) {
        memset(out,'x',capacity); *size = capacity + 1;
        return !strcmp(scenario,"matching_more_data") ? 234 : 5;
    }
    if (!strcmp(name,"InfPath") && !strcmp(scenario,"registry_type")) *type = 7;
    if (!strcmp(name,"ProviderName") && !strcmp(scenario,"registry_bounds"))
        *size = capacity + 1;
    if (!strcmp(name,"InfSection") && !strcmp(scenario,"registry_nul"))
        memset(out,'x',*size);
    return ERROR_SUCCESS;
}
LONG RegCloseKey(HKEY a) { (void)a; return 0; }
CONFIGRET CM_Open_DevNode_Key(DWORD a,DWORD b,DWORD c,DWORD d,HKEY *key,DWORD e) {
    (void)a; (void)b; (void)c; (void)d; (void)e; *key = (HKEY)12;
    return !strcmp(scenario,"hardware_key_error") ? 5 : CR_SUCCESS;
}
HKEY SetupDiOpenClassRegKeyExA(const GUID *g,DWORD a,DWORD b,void *c,void *d) {
    (void)g; (void)a; (void)b; (void)c; (void)d;
    return !strcmp(scenario,"installer_missing") ? INVALID_HANDLE_VALUE : (HKEY)13;
}
BOOL SetupDiEnumDeviceInterfaces(HDEVINFO a,void *b,const GUID *g,DWORD i,
                                SP_DEVICE_INTERFACE_DATA *f) {
    (void)a; (void)b; (void)g; (void)i; (void)f;
    last_error = ERROR_NO_MORE_ITEMS; return 0;
}
BOOL SetupDiGetDeviceInterfaceDetailA(HDEVINFO a,SP_DEVICE_INTERFACE_DATA *b,
                                     SP_DEVICE_INTERFACE_DETAIL_DATA_A *c,DWORD d,DWORD *e,
                                     SP_DEVINFO_DATA *f) {
    (void)a; (void)b; (void)c; (void)d; (void)e; (void)f; return 0;
}
BOOL SetupDiDestroyDeviceInfoList(HDEVINFO a) { (void)a; return 1; }
BOOL SetupDiGetSelectedDriverA(HDEVINFO a,SP_DEVINFO_DATA *d,SP_DRVINFO_DATA_A *info) {
    (void)a; (void)d;
    strcpy(info->ProviderName,!strcmp(scenario,"selected_provider") ? "Other" : "Microsoft");
    info->DriverVersion = !strcmp(scenario,"selected_changed") && dif_calls ? 2 : 1;
    info->DriverDate = 1;
    if (!strcmp(scenario,"selected_provider_nul"))
        memset(info->ProviderName,'x',sizeof(info->ProviderName));
    return 1;
}
BOOL SetupDiGetDriverInfoDetailA(HDEVINFO a,SP_DEVINFO_DATA *b,SP_DRVINFO_DATA_A *c,
                                SP_DRVINFO_DETAIL_DATA_A *detail,DWORD capacity,DWORD *size) {
    (void)a; (void)b; (void)c; *size = sizeof(*detail);
    strcpy(detail->InfFileName,!strcmp(scenario,"selected_inf") ? "X:\\Other\\msports.inf" :
        "X:\\Windows\\INF\\msports.inf");
    strcpy(detail->SectionName,"ComPort");
    if (!strcmp(scenario,"detail_size")) *size = capacity + 1;
    if (!strcmp(scenario,"detail_nul")) memset(detail->SectionName,'x',sizeof(detail->SectionName));
    return 1;
}
HINF SetupOpenInfFileA(const char *a,void *b,DWORD c,void *d) {
    (void)a; (void)b; (void)c; (void)d; return (HINF)14;
}
void SetupCloseInfFile(HINF a) { (void)a; }
BOOL SetupDiGetActualSectionToInstallA(HINF a,const char *b,char *out,DWORD cap,
                                      DWORD *size,void *d) {
    (void)a; (void)b; (void)cap; (void)d;
    strcpy(out,!strcmp(scenario,"selected_section") ? "Other.NT" : "ComPort.NT");
    *size = (DWORD)strlen(out) + 1; return 1;
}
BOOL SetupDiGetDeviceInstallParamsA(HDEVINFO a,SP_DEVINFO_DATA *b,SP_DEVINSTALL_PARAMS_A *out) {
    (void)a; (void)b; DWORD cb = out->cbSize; *out = params; out->cbSize = cb;
    if (!strcmp(scenario,"cleared_flagsex") && builds) out->FlagsEx = 0;
    if (!strcmp(scenario,"reboot_first") && dif_calls == 1) out->Flags |= DI_NEEDREBOOT;
    if (!strcmp(scenario,"restart_second") && dif_calls == 2) out->Flags |= DI_NEEDRESTART;
    return 1;
}
BOOL SetupDiSetDeviceInstallParamsA(HDEVINFO a,SP_DEVINFO_DATA *b,SP_DEVINSTALL_PARAMS_A *in) {
    (void)a; (void)b; params = *in; return 1;
}
BOOL SetupDiBuildDriverInfoList(HDEVINFO a,SP_DEVINFO_DATA *b,DWORD kind) {
    (void)a; (void)b; (void)kind; ++builds;
    printf("WINMOCK_LIST flags=%lu flagsex=%lu\n",params.Flags,params.FlagsEx);
    return 1;
}
BOOL SetupDiEnumDriverInfoA(HDEVINFO a,SP_DEVINFO_DATA *b,DWORD c,DWORD i,SP_DRVINFO_DATA_A *d) {
    (void)a; (void)b; (void)c; (void)d; last_error = ERROR_NO_MORE_ITEMS;
    return i == 0 || (i == 1 && !strcmp(scenario,"multiple_drivers"));
}
BOOL SetupDiSetSelectedDriverA(HDEVINFO a,SP_DEVINFO_DATA *b,SP_DRVINFO_DATA_A *c) {
    (void)a; (void)b; (void)c; return 1;
}
BOOL SetupDiCallClassInstaller(DI_FUNCTION f,HDEVINFO a,SP_DEVINFO_DATA *b) {
    (void)a; (void)b; ++dif_calls;
    printf("WINMOCK_DIF function=%u\n",f); return 1;
}
BOOL SetupDiDestroyDriverInfoList(HDEVINFO a,SP_DEVINFO_DATA *b,DWORD c) {
    (void)a; (void)b; (void)c; return 1;
}
int main(int argc,char **argv) {
    if (argc != 2) return 3;
    scenario = argv[1]; char *args[] = {"serial-prepare", "--prepare", NULL};
    int rc = actual_serial_main(2,args);
    printf("WINMOCK_END rc=%d calls=%d builds=%d\n",rc,dif_calls,builds);
    return rc;
}
