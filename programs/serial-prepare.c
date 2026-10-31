#ifndef WGI_SERIAL_TEST
#define _WIN32_WINNT 0x0602
#endif
/* Original conditional standard Ports installation. No INF forcing or name writes. */
#include <stdio.h>
#include <string.h>
#include <stddef.h>

#define WGI_REBOOT 0x100u
#define WGI_RESTART 0x80u

typedef struct {
    unsigned count, started, problem;
    int name_state, com_state, installer_ok;
    char id[1024], class_name[80], service[128], inf[260], provider[256];
    char section[256], extension[64], matching[256];
} WGI_SERIAL_SNAPSHOT;
typedef struct {
    int (*select_current)(void *, const WGI_SERIAL_SNAPSHOT *);
    int (*call)(void *, unsigned);
    int (*inspect)(void *, WGI_SERIAL_SNAPSHOT *, unsigned *);
} WGI_SERIAL_OPS;

static int equal_text(const char *a, const char *b) {
    while (*a && *b) {
        unsigned x = (unsigned char)*a++, y = (unsigned char)*b++;
        if (x >= 'A' && x <= 'Z') x += 'a' - 'A';
        if (y >= 'A' && y <= 'Z') y += 'a' - 'A';
        if (x != y) return 0;
    }
    return *a == *b;
}
static int wgi_serial_string(const char *s, unsigned type, unsigned bytes, size_t cap) {
    return type == 1 && bytes && bytes <= cap && memchr(s, 0, bytes) == s + bytes - 1;
}
static int eligible(const WGI_SERIAL_SNAPSHOT *s) {
    return s->count == 1 && equal_text(s->id, "ACPI\\PNP0501\\1") &&
        equal_text(s->class_name, "{4d36e978-e325-11ce-bfc1-08002be10318}") &&
        equal_text(s->service, "Serial") && s->started && !s->problem &&
        equal_text(s->inf, "msports.inf") && equal_text(s->provider, "Microsoft") &&
        s->section[0] && equal_text(s->matching, "acpi\\pnp0501") && s->installer_ok &&
        s->name_state == 0 && s->com_state == 0;
}
static int same_identity(const WGI_SERIAL_SNAPSHOT *a, const WGI_SERIAL_SNAPSHOT *b) {
    return a->count == b->count && b->started && !b->problem &&
        equal_text(a->id, b->id) && equal_text(a->class_name, b->class_name) &&
        equal_text(a->service, b->service) && equal_text(a->inf, b->inf) &&
        equal_text(a->provider, b->provider) && equal_text(a->section, b->section) &&
        equal_text(a->extension, b->extension) && equal_text(a->matching, b->matching);
}
static int checked(const WGI_SERIAL_SNAPSHOT *s, const WGI_SERIAL_OPS *ops, void *ctx,
                   unsigned *retained_flags) {
    WGI_SERIAL_SNAPSHOT now; unsigned flags = 0;
    memset(&now, 0, sizeof(now));
    if (!ops->inspect(ctx, &now, &flags)) return 0;
    *retained_flags |= flags;
    printf("WGI_SERIAL_PREP check restart_flags=%u\n", *retained_flags);
    return !(*retained_flags & (WGI_REBOOT | WGI_RESTART)) && same_identity(s, &now);
}
static int wgi_serial_run(const WGI_SERIAL_SNAPSHOT *s, const WGI_SERIAL_OPS *ops, void *ctx) {
    unsigned retained_flags = 0;
    if (s->com_state == 1) {
        puts("WGI_SERIAL_PREP action=skip reason=existing_COM1_not_install_acceptance");
        return 0;
    }
    if (!eligible(s)) {
        puts("WGI_SERIAL_PREP action=STOP reason=unproved_preconditions"); return 2;
    }
    if (!ops->select_current(ctx, s) || !checked(s, ops, ctx, &retained_flags)) return 2;
    for (unsigned step = 0; step < 3; ++step) {
        int ok = ops->call(ctx, step);
        printf("WGI_SERIAL_PREP step=%u success=%d\n", step, ok);
        if (!ok || !checked(s, ops, ctx, &retained_flags)) return 2;
    }
    puts("WGI_SERIAL_PREP action=completed API_only_NOT_COM_acceptance");
    return 0;
}

#ifndef WGI_SERIAL_TEST
#ifdef WGI_SERIAL_API_TEST
#include "../tests/winapi-serial.h"
#else
#include <windows.h>
#include <setupapi.h>
#include <cfgmgr32.h>
#endif
#include <stdint.h>

static const GUID ports_guid =
    {0x4d36e978, 0xe325, 0x11ce, {0xbf, 0xc1, 0x08, 0x00, 0x2b, 0xe1, 0x03, 0x18}};
static const GUID comport_guid =
    {0x86e0d1e0, 0x8089, 0x11d0, {0x9c, 0xe4, 0x08, 0x00, 0x3e, 0x30, 0x1f, 0x73}};
typedef struct {
    HDEVINFO set;
    SP_DEVINFO_DATA node;
    WGI_SERIAL_SNAPSHOT before;
    SP_DRVINFO_DATA_A selected;
    char inf_path[MAX_PATH], selected_section[256];
    int built, selected_valid;
} SERIAL_CONTEXT;

static int prop(SERIAL_CONTEXT *c, DWORD key, char *out, DWORD capacity) {
    DWORD type = 0, size = 0;
    memset(out, 0, capacity);
    BOOL ok = SetupDiGetDeviceRegistryPropertyA(c->set, &c->node, key, &type,
                                               (BYTE *)out, capacity, &size);
    return ok && wgi_serial_string(out, type, size, capacity);
}
static LONG reg_text(HKEY key, const char *name, char *out, DWORD capacity) {
    DWORD type = 0, size = capacity;
    memset(out, 0, capacity);
    LONG rc = RegQueryValueExA(key, name, NULL, &type, (BYTE *)out, &size);
    printf("WGI_SERIAL_PREP registry name=%s rc=%ld type=%lu bytes=%lu\n",
           name, rc, type, size);
    if (rc != ERROR_SUCCESS) return rc;
    if (!wgi_serial_string(out, type, size, capacity)) {
        out[0] = 0; return ERROR_INVALID_DATA;
    }
    /* Registry strings are diagnostic text, never a second protocol line. */
    if (strpbrk(out, "\r\n")) { out[0] = 0; return ERROR_INVALID_DATA; }
    printf("WGI_SERIAL_PREP registry name=%s value=%s\n", name, out);
    return ERROR_SUCCESS;
}
static int instance(HDEVINFO set, SP_DEVINFO_DATA *node, char *out, DWORD capacity) {
    DWORD bytes = 0;
    memset(out, 0, capacity);
    return SetupDiGetDeviceInstanceIdA(set, node, out, capacity, &bytes) &&
        wgi_serial_string(out, REG_SZ, bytes, capacity) && !strpbrk(out, "\r\n");
}
static int find_node(SERIAL_CONTEXT *c, WGI_SERIAL_SNAPSHOT *s) {
    int complete = 0;
    for (DWORD i = 0; i < 4096; ++i) {
        SP_DEVINFO_DATA node; char id[1024];
        memset(&node, 0, sizeof(node)); node.cbSize = sizeof(node);
        if (!SetupDiEnumDeviceInfo(c->set, i, &node)) {
            complete = GetLastError() == ERROR_NO_MORE_ITEMS; break;
        }
        if (!instance(c->set, &node, id, sizeof(id))) return 0;
        if (_strnicmp(id, "ACPI\\PNP0501\\", 13)) continue;
        ++s->count;
        if (s->count == 1) { c->node = node; strcpy(s->id, id); }
    }
    printf("WGI_SERIAL_PREP target count=%u id=%s enum_complete=%d\n",
           s->count, s->id, complete);
    return complete && s->count == 1 && equal_text(s->id, "ACPI\\PNP0501\\1");
}
static int current(SERIAL_CONTEXT *c, WGI_SERIAL_SNAPSHOT *s) {
    memset(s, 0, sizeof(*s));
    if (!find_node(c, s)) return 0;
    ULONG status = 0, problem = 0;
    if (CM_Get_DevNode_Status(&status, &problem, c->node.DevInst, 0) != CR_SUCCESS) return 0;
    s->started = !!(status & DN_STARTED); s->problem = problem;
    if (!prop(c, SPDRP_CLASSGUID, s->class_name, sizeof(s->class_name)) ||
        !prop(c, SPDRP_SERVICE, s->service, sizeof(s->service))) return 0;
    HKEY key = SetupDiOpenDevRegKey(c->set, &c->node, DICS_FLAG_GLOBAL, 0,
                                   DIREG_DRV, KEY_QUERY_VALUE);
    if (key == INVALID_HANDLE_VALUE) return 0;
    int ok = reg_text(key, "InfPath", s->inf, sizeof(s->inf)) == ERROR_SUCCESS;
    ok &= reg_text(key, "ProviderName", s->provider, sizeof(s->provider)) == ERROR_SUCCESS;
    ok &= reg_text(key, "InfSection", s->section, sizeof(s->section)) == ERROR_SUCCESS;
    LONG ext = reg_text(key, "InfSectionExt", s->extension, sizeof(s->extension));
    if (ext != ERROR_SUCCESS && ext != ERROR_FILE_NOT_FOUND) ok = 0;
    ok &= reg_text(key, "MatchingDeviceId", s->matching, sizeof(s->matching)) == ERROR_SUCCESS;
    RegCloseKey(key);
    printf("WGI_SERIAL_PREP current status=%lu problem=%lu metadata_ok=%d\n",status,problem,ok);
    return ok;
}
static void extra_properties(SERIAL_CONTEXT *c) {
    const DWORD keys[] = {SPDRP_FRIENDLYNAME, SPDRP_PHYSICAL_DEVICE_OBJECT_NAME, SPDRP_DRIVER};
    const char *names[] = {"FriendlyName", "PDOName", "DriverKey"};
    for (unsigned i = 0; i < 3; ++i) {
        char value[1024]; int ok = prop(c, keys[i], value, sizeof(value));
        printf("WGI_SERIAL_PREP property name=%s valid=%d value=%s\n",
               names[i], ok, ok && !strpbrk(value, "\r\n") ? value : "UNKNOWN");
    }
}
static int name_key(HKEY key, const char *api) {
    char port[128], identifier[128];
    LONG a = reg_text(key, "PortName", port, sizeof(port));
    LONG b = reg_text(key, "Identifier", identifier, sizeof(identifier));
    int result = a == ERROR_FILE_NOT_FOUND && b == ERROR_FILE_NOT_FOUND ? 0 :
        (a == ERROR_SUCCESS || b == ERROR_SUCCESS ? 1 : -1);
    printf("WGI_SERIAL_PREP names api=%s port_rc=%ld identifier_rc=%ld state=%d\n",api,a,b,result);
    return result;
}
static int names(SERIAL_CONTEXT *c) {
    HKEY a = SetupDiOpenDevRegKey(c->set, &c->node, DICS_FLAG_GLOBAL, 0,
                                 DIREG_DEV, KEY_QUERY_VALUE), b = NULL;
    CONFIGRET rc = CM_Open_DevNode_Key(c->node.DevInst, KEY_QUERY_VALUE, 0,
                                      RegDisposition_OpenExisting, &b, CM_REGISTRY_HARDWARE);
    int first = a == INVALID_HANDLE_VALUE ? -1 : name_key(a, "SetupDi");
    int second = rc != CR_SUCCESS ? -1 : name_key(b, "CM");
    printf("WGI_SERIAL_PREP hardware_crosscheck cm_rc=%lu same_state=%d\n",rc,first==second);
    if (a != INVALID_HANDLE_VALUE) RegCloseKey(a);
    if (rc == CR_SUCCESS) RegCloseKey(b);
    return first == second ? first : -1;
}
static int com_state(void) {
    char target[1024]; DWORD n = QueryDosDeviceA("COM1", target, sizeof(target));
    DWORD error = n ? 0 : GetLastError();
    printf("WGI_SERIAL_PREP namespace COM1=%s error=%lu\n",n ? "PRESENT" : "ABSENT",error);
    if (n) return 1;
    return error == ERROR_FILE_NOT_FOUND ? 0 : -1;
}
static int installer(void) {
    HKEY key = SetupDiOpenClassRegKeyExA(&ports_guid, KEY_QUERY_VALUE, DIOCR_INSTALLER,
                                        NULL, NULL);
    if (key == INVALID_HANDLE_VALUE) {
        printf("WGI_SERIAL_PREP installer open_error=%lu\n",GetLastError()); return 0;
    }
    char value[1024], system[MAX_PATH], path[MAX_PATH];
    LONG rc = reg_text(key, "Installer32", value, sizeof(value)); RegCloseKey(key);
    if (rc != ERROR_SUCCESS) return 0;
    /* Only the existing normal stock registration; never create or load it directly. */
    if (!equal_text(value, "MsPorts.dll,PortsClassInstaller")) return 0;
    UINT n = GetSystemDirectoryA(system, sizeof(system));
    if (!n || n >= sizeof(system)) return 0;
    int length = snprintf(path, sizeof(path), "%s\\MsPorts.dll", system);
    if (length < 0 || (unsigned)length >= sizeof(path)) return 0;
    DWORD attr = GetFileAttributesA(path);
    int present = attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY);
    printf("WGI_SERIAL_PREP installer registered_stock=1 dll_present=%d error=%lu\n",
           present, present ? 0 : GetLastError());
    return present;
}
static void interfaces(SERIAL_CONTEXT *c, const char *phase) {
    HDEVINFO set = SetupDiGetClassDevsA(&comport_guid, NULL, NULL, DIGCF_DEVICEINTERFACE);
    if (set == INVALID_HANDLE_VALUE) {
        printf("WGI_SERIAL_PREP interfaces phase=%s open_error=%lu\n",phase,GetLastError());
        return;
    }
    unsigned matched = 0; int complete = 0;
    for (DWORD i = 0; i < 4096; ++i) {
        SP_DEVICE_INTERFACE_DATA face;
        memset(&face, 0, sizeof(face)); face.cbSize = sizeof(face);
        if (!SetupDiEnumDeviceInterfaces(set, NULL, &comport_guid, i, &face)) {
            complete = GetLastError() == ERROR_NO_MORE_ITEMS; break;
        }
        unsigned char buffer[8192]; DWORD needed = 0;
        SP_DEVICE_INTERFACE_DETAIL_DATA_A *detail = (void *)buffer;
        SP_DEVINFO_DATA node;
        memset(buffer, 0, sizeof(buffer)); detail->cbSize = sizeof(*detail);
        memset(&node, 0, sizeof(node)); node.cbSize = sizeof(node);
        if (!SetupDiGetDeviceInterfaceDetailA(set, &face, detail, sizeof(buffer), &needed,&node) ||
            needed < sizeof(*detail) || needed > sizeof(buffer) ||
            !memchr(detail->DevicePath, 0, needed - offsetof(SP_DEVICE_INTERFACE_DETAIL_DATA_A,
                                                            DevicePath))) break;
        char id[1024];
        if (!instance(set, &node, id, sizeof(id))) break;
        if (!equal_text(id, c->before.id)) continue;
        ++matched;
        printf("WGI_SERIAL_PREP interface phase=%s id=%s flags=%lu path=%s\n",phase,id,face.Flags,
               strpbrk(detail->DevicePath, "\r\n") ? "INVALID_TEXT" : detail->DevicePath);
    }
    printf("WGI_SERIAL_PREP interfaces phase=%s matched=%u complete=%d\n",phase,matched,complete);
    SetupDiDestroyDeviceInfoList(set);
}
static int selected_matches(SERIAL_CONTEXT *c, const WGI_SERIAL_SNAPSHOT *s) {
    SP_DRVINFO_DATA_A info; unsigned char buffer[16384]; DWORD needed = 0;
    memset(&info, 0, sizeof(info)); info.cbSize = sizeof(info);
    memset(buffer, 0, sizeof(buffer));
    SP_DRVINFO_DETAIL_DATA_A *detail = (void *)buffer; detail->cbSize = sizeof(*detail);
    if (!SetupDiGetSelectedDriverA(c->set, &c->node, &info) ||
        !memchr(info.ProviderName, 0, sizeof(info.ProviderName)) ||
        !equal_text(info.ProviderName, s->provider) ||
        !SetupDiGetDriverInfoDetailA(c->set, &c->node, &info, detail, sizeof(buffer), &needed) ||
        needed < sizeof(*detail) || needed > sizeof(buffer) ||
        !memchr(detail->InfFileName, 0, sizeof(detail->InfFileName)) ||
        !memchr(detail->SectionName, 0, sizeof(detail->SectionName))) return 0;
    char windows[MAX_PATH], expected[MAX_PATH], section[256], actual[256];
    UINT n = GetWindowsDirectoryA(windows, sizeof(windows));
    if (!n || n >= sizeof(windows)) return 0;
    int length = snprintf(expected, sizeof(expected), "%s\\INF\\%s",windows,s->inf);
    if (length < 0 || (unsigned)length >= sizeof(expected) ||
        !equal_text(detail->InfFileName, expected)) return 0;
    length = snprintf(section, sizeof(section), "%s%s",s->section,s->extension);
    if (length < 0 || (unsigned)length >= sizeof(section)) return 0;
    HINF inf = SetupOpenInfFileA(expected, NULL, INF_STYLE_WIN4, NULL);
    if (inf == INVALID_HANDLE_VALUE) return 0;
    DWORD bytes = 0;
    BOOL ok = SetupDiGetActualSectionToInstallA(inf, detail->SectionName, actual,
                                               sizeof(actual), &bytes, NULL);
    SetupCloseInfFile(inf);
    if (!ok || !wgi_serial_string(actual, REG_SZ, bytes, sizeof(actual)) ||
        !equal_text(actual, section)) return 0;
    if (c->selected_valid &&
        (memcmp(&info.DriverDate, &c->selected.DriverDate, sizeof(info.DriverDate)) ||
         info.DriverVersion != c->selected.DriverVersion ||
         !equal_text(c->inf_path, expected) || !equal_text(c->selected_section, actual))) return 0;
    c->selected = info; strcpy(c->inf_path, expected); strcpy(c->selected_section,actual);
    c->selected_valid = 1;
    printf("WGI_SERIAL_PREP selected same_installed=1 inf=%s section=%s provider=%s\n",
           s->inf,actual,info.ProviderName);
    return 1;
}
static int select_current(void *ctx, const WGI_SERIAL_SNAPSHOT *s) {
    SERIAL_CONTEXT *c = ctx; SP_DEVINSTALL_PARAMS_A p;
    memset(&p, 0, sizeof(p)); p.cbSize = sizeof(p);
    if (!SetupDiGetDeviceInstallParamsA(c->set, &c->node, &p) ||
        (p.Flags & (DI_NEEDREBOOT | DI_NEEDRESTART))) return 0;
    p.Flags |= DI_QUIETINSTALL;
    p.FlagsEx |= DI_FLAGSEX_INSTALLEDDRIVER | DI_FLAGSEX_ALLOWEXCLUDEDDRVS;
    if (!SetupDiSetDeviceInstallParamsA(c->set, &c->node, &p) ||
        !SetupDiBuildDriverInfoList(c->set, &c->node, SPDIT_COMPATDRIVER)) return 0;
    c->built = 1;
    SP_DRVINFO_DATA_A only, extra;
    memset(&only, 0, sizeof(only)); only.cbSize = sizeof(only);
    memset(&extra, 0, sizeof(extra)); extra.cbSize = sizeof(extra);
    if (!SetupDiEnumDriverInfoA(c->set, &c->node, SPDIT_COMPATDRIVER, 0, &only) ||
        SetupDiEnumDriverInfoA(c->set, &c->node, SPDIT_COMPATDRIVER, 1, &extra) ||
        GetLastError() != ERROR_NO_MORE_ITEMS ||
        !SetupDiSetSelectedDriverA(c->set, &c->node, &only)) return 0;
    return selected_matches(c, s);
}
static int call_install(void *ctx, unsigned step) {
    SERIAL_CONTEXT *c = ctx;
    const DI_FUNCTION steps[] =
        {DIF_REGISTER_COINSTALLERS, DIF_INSTALLINTERFACES, DIF_INSTALLDEVICE};
    if (step >= 3) return 0;
    BOOL ok = SetupDiCallClassInstaller(steps[step], c->set, &c->node);
    printf("WGI_SERIAL_PREP DIF function=%u success=%d error=%lu\n",steps[step],ok,
           ok ? 0 : GetLastError());
    return ok;
}
static int inspect(void *ctx, WGI_SERIAL_SNAPSHOT *s, unsigned *flags) {
    SERIAL_CONTEXT *c = ctx; SP_DEVINSTALL_PARAMS_A p;
    memset(&p, 0, sizeof(p)); p.cbSize = sizeof(p);
    /* Query restart outputs first: never let another DIF call overwrite them. */
    if (!SetupDiGetDeviceInstallParamsA(c->set, &c->node, &p)) return 0;
    *flags = p.Flags & (DI_NEEDREBOOT | DI_NEEDRESTART);
    printf("WGI_SERIAL_PREP install_params restart_flags=%u\n", *flags);
    if (!(p.Flags & DI_QUIETINSTALL) ||
        (p.FlagsEx & (DI_FLAGSEX_INSTALLEDDRIVER | DI_FLAGSEX_ALLOWEXCLUDEDDRVS)) !=
        (DI_FLAGSEX_INSTALLEDDRIVER | DI_FLAGSEX_ALLOWEXCLUDEDDRVS)) return 0;
    return current(c, s) && selected_matches(c, &c->before);
}
int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IONBF, 0);
    if (argc != 2 || strcmp(argv[1], "--prepare")) return 2;
    SERIAL_CONTEXT c; memset(&c, 0, sizeof(c));
    c.set = SetupDiGetClassDevsA(NULL, "ACPI", NULL, DIGCF_ALLCLASSES | DIGCF_PRESENT);
    if (c.set == INVALID_HANDLE_VALUE) return 2;
    int metadata_ok = current(&c, &c.before);
    c.before.com_state = com_state();
    if (c.before.count == 1) {
        extra_properties(&c); interfaces(&c, "before");
        c.before.name_state = names(&c);
    } else c.before.name_state = -1;
    c.before.installer_ok = installer();
    printf("WGI_SERIAL_PREP before metadata_ok=%d names=%d COM1=%d installer=%d\n",
           metadata_ok,c.before.name_state,c.before.com_state,c.before.installer_ok);
    WGI_SERIAL_OPS ops = {select_current, call_install, inspect};
    int rc = !metadata_ok && c.before.com_state != 1 ? 2 : wgi_serial_run(&c.before,&ops,&c);
    WGI_SERIAL_SNAPSHOT after;
    int after_ok = current(&c, &after);
    if (after.count == 1) { extra_properties(&c); interfaces(&c, "after"); (void)names(&c); }
    int final_com = com_state();
    printf("WGI_SERIAL_PREP end rc=%d metadata_ok=%d COM1=%d API_only_NOT_GPU_acceptance\n",
           rc,after_ok,final_com);
    if (c.built) SetupDiDestroyDriverInfoList(c.set, &c.node, SPDIT_COMPATDRIVER);
    SetupDiDestroyDeviceInfoList(c.set);
    return rc;
}
#endif
