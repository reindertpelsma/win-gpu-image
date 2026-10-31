/* Original CPU test doubles for the narrow Win32 calls used by gpu-prepare.c. */
#include <stddef.h>
#include <string.h>
#ifndef _WIN32
#include <strings.h>
#endif
typedef unsigned long DWORD;
typedef unsigned long ULONG;
typedef unsigned char BYTE;
typedef void *HANDLE;
typedef void *HKEY;
typedef void *HDEVINFO;
typedef struct { DWORD cbSize, DevInst; } SP_DEVINFO_DATA;
typedef struct { DWORD cbSize, InstallFunction; } SP_CLASSINSTALL_HEADER;
typedef struct {
    SP_CLASSINSTALL_HEADER ClassInstallHeader;
    DWORD StateChange, Scope, HwProfile;
} SP_PROPCHANGE_PARAMS;
typedef struct { DWORD cbSize, Flags; char DriverPath[260]; } SP_DEVINSTALL_PARAMS_A;
typedef struct { DWORD cbSize; char ProviderName[256]; } SP_DRVINFO_DATA_A;
#define INVALID_HANDLE_VALUE ((HANDLE)(long)-1)
#define HKEY_LOCAL_MACHINE ((HKEY)1)
#ifndef _WIN32
#define _stricmp strcasecmp
#define _strnicmp strncasecmp
#endif
#define DICS_FLAG_CONFIGSPECIFIC 1
#define DICS_FLAG_GLOBAL 2
#define DICS_DISABLE 2
#define DICS_ENABLE 1
#define DIF_PROPERTYCHANGE 1
#define DIF_SELECTBESTCOMPATDRV 2
#define DIF_INSTALLDEVICE 3
#define DI_NEEDREBOOT 1
#define DI_NEEDRESTART 2
#define DI_ENUMSINGLEINF 4
#define DI_DONOTCALLCONFIGMG 8
#define SPDIT_COMPATDRIVER 1
#define DIGCF_ALLCLASSES 1
#define DIGCF_PRESENT 2
#define SPDRP_COMPATIBLEIDS 1
#define SPDRP_DRIVER 2
#define SPDRP_SERVICE 3
#define DIREG_DRV 1
#define KEY_SET_VALUE 1
#define KEY_QUERY_VALUE 2
#define REG_MULTI_SZ 7
#define REG_DWORD 4
#define CR_SUCCESS 0
#define CM_PROB_DISABLED 22
#define DN_STARTED 8
#define ERROR_SUCCESS 0
#define ERROR_NO_MORE_ITEMS 259
#define SERVICE_DEMAND_START 3
DWORD GetLastError(void);
int SetupDiGetDeviceRegistryPropertyA(HDEVINFO, SP_DEVINFO_DATA *, DWORD,
                                    DWORD *, BYTE *, DWORD, DWORD *);
int SetupDiSetClassInstallParamsA(HDEVINFO, SP_DEVINFO_DATA *, SP_CLASSINSTALL_HEADER *, DWORD);
int SetupDiCallClassInstaller(DWORD, HDEVINFO, SP_DEVINFO_DATA *);
int SetupDiGetDeviceInstallParamsA(HDEVINFO, SP_DEVINFO_DATA *, SP_DEVINSTALL_PARAMS_A *);
int SetupDiSetDeviceInstallParamsA(HDEVINFO, SP_DEVINFO_DATA *, SP_DEVINSTALL_PARAMS_A *);
int SetupDiBuildDriverInfoList(HDEVINFO, SP_DEVINFO_DATA *, DWORD);
int SetupDiGetSelectedDriverA(HDEVINFO, SP_DEVINFO_DATA *, SP_DRVINFO_DATA_A *);
HDEVINFO SetupDiGetClassDevsA(void *, const char *, void *, DWORD);
int SetupDiEnumDeviceInfo(HDEVINFO, DWORD, SP_DEVINFO_DATA *);
int SetupDiGetDeviceInstanceIdA(HDEVINFO, SP_DEVINFO_DATA *, char *, DWORD, DWORD *);
int SetupDiDestroyDeviceInfoList(HDEVINFO);
int CM_Get_DevNode_Status(ULONG *, ULONG *, DWORD, DWORD);
HKEY SetupDiOpenDevRegKey(HDEVINFO, SP_DEVINFO_DATA *, DWORD, DWORD, DWORD, DWORD);
int RegOpenKeyExA(HKEY, const char *, DWORD, DWORD, HKEY *);
int RegSetValueExA(HKEY, const char *, DWORD, DWORD, const BYTE *, DWORD);
int RegQueryValueExA(HKEY, const char *, DWORD *, DWORD *, BYTE *, DWORD *);
int RegCloseKey(HKEY);
