/* Original CPU-only declarations, not Windows headers or a driver emulator. */
#include <stddef.h>
#ifndef _WIN32
#include <strings.h>
#define _strnicmp strncasecmp
#endif
#define GetLastError wgi_serial_mock_error
#define GetWindowsDirectoryA wgi_serial_mock_windows
#define GetSystemDirectoryA wgi_serial_mock_system
#define GetFileAttributesA wgi_serial_mock_attributes
#define QueryDosDeviceA wgi_serial_mock_namespace
#define REG_SZ 1
#define MAX_PATH 260
#define TRUE 1
#define FALSE 0
#define ERROR_SUCCESS 0
#define ERROR_FILE_NOT_FOUND 2
#define ERROR_INVALID_DATA 13
#define ERROR_NO_MORE_ITEMS 259
#define INVALID_HANDLE_VALUE ((void *)(ptrdiff_t)-1)
#define INVALID_FILE_ATTRIBUTES ((DWORD)-1)
#define FILE_ATTRIBUTE_DIRECTORY 16
#define CR_SUCCESS 0
#define DN_STARTED 8
#define DICS_FLAG_GLOBAL 1
#define DIREG_DRV 2
#define DIREG_DEV 1
#define KEY_QUERY_VALUE 1
#define CM_REGISTRY_HARDWARE 0
#define RegDisposition_OpenExisting 1
#define DIOCR_INSTALLER 1
#define DIGCF_ALLCLASSES 4
#define DIGCF_PRESENT 2
#define DIGCF_DEVICEINTERFACE 16
#define SPDRP_SERVICE 4
#define SPDRP_CLASSGUID 8
#define SPDRP_DRIVER 9
#define SPDRP_FRIENDLYNAME 12
#define SPDRP_PHYSICAL_DEVICE_OBJECT_NAME 14
#define DI_NEEDRESTART 0x80u
#define DI_NEEDREBOOT 0x100u
#define DI_QUIETINSTALL 0x00800000u
#define DI_FLAGSEX_ALLOWEXCLUDEDDRVS 0x800u
#define DI_FLAGSEX_INSTALLEDDRIVER 0x04000000u
#define DIF_REGISTER_COINSTALLERS 0x22
#define DIF_INSTALLINTERFACES 0x20
#define DIF_INSTALLDEVICE 2
#define SPDIT_COMPATDRIVER 2
#define INF_STYLE_WIN4 2

typedef unsigned long DWORD, ULONG;
typedef unsigned char BYTE;
typedef unsigned UINT;
typedef long LONG;
typedef int BOOL;
typedef void *HDEVINFO, *HKEY, *HINF;
typedef unsigned long CONFIGRET;
typedef unsigned DI_FUNCTION;
typedef struct { unsigned long a; unsigned short b, c; unsigned char d[8]; } GUID;
typedef struct { DWORD cbSize, DevInst; } SP_DEVINFO_DATA;
typedef struct { DWORD cbSize, Flags, FlagsEx; } SP_DEVINSTALL_PARAMS_A;
typedef struct {
    DWORD cbSize;
    char ProviderName[256];
    unsigned long long DriverDate, DriverVersion;
} SP_DRVINFO_DATA_A;
typedef struct { DWORD cbSize, Flags; } SP_DEVICE_INTERFACE_DATA;
typedef struct { DWORD cbSize; char DevicePath[1]; } SP_DEVICE_INTERFACE_DETAIL_DATA_A;
typedef struct {
    DWORD cbSize;
    char SectionName[256], InfFileName[260], HardwareID[1];
} SP_DRVINFO_DETAIL_DATA_A;
DWORD GetLastError(void);
UINT GetWindowsDirectoryA(char *, UINT);
UINT GetSystemDirectoryA(char *, UINT);
DWORD GetFileAttributesA(const char *);
DWORD QueryDosDeviceA(const char *, char *, DWORD);
HDEVINFO SetupDiGetClassDevsA(const GUID *, const char *, void *, DWORD);
BOOL SetupDiEnumDeviceInfo(HDEVINFO, DWORD, SP_DEVINFO_DATA *);
BOOL SetupDiGetDeviceInstanceIdA(HDEVINFO, SP_DEVINFO_DATA *, char *, DWORD, DWORD *);
BOOL SetupDiGetDeviceRegistryPropertyA(HDEVINFO, SP_DEVINFO_DATA *, DWORD, DWORD *,
                                      BYTE *, DWORD, DWORD *);
CONFIGRET CM_Get_DevNode_Status(ULONG *, ULONG *, DWORD, DWORD);
HKEY SetupDiOpenDevRegKey(HDEVINFO, SP_DEVINFO_DATA *, DWORD, DWORD, DWORD, DWORD);
LONG RegQueryValueExA(HKEY, const char *, void *, DWORD *, BYTE *, DWORD *);
LONG RegCloseKey(HKEY);
CONFIGRET CM_Open_DevNode_Key(DWORD, DWORD, DWORD, DWORD, HKEY *, DWORD);
HKEY SetupDiOpenClassRegKeyExA(const GUID *, DWORD, DWORD, void *, void *);
BOOL SetupDiEnumDeviceInterfaces(HDEVINFO, void *, const GUID *, DWORD,
                                SP_DEVICE_INTERFACE_DATA *);
BOOL SetupDiGetDeviceInterfaceDetailA(HDEVINFO, SP_DEVICE_INTERFACE_DATA *,
                                     SP_DEVICE_INTERFACE_DETAIL_DATA_A *, DWORD, DWORD *,
                                     SP_DEVINFO_DATA *);
BOOL SetupDiDestroyDeviceInfoList(HDEVINFO);
BOOL SetupDiGetSelectedDriverA(HDEVINFO, SP_DEVINFO_DATA *, SP_DRVINFO_DATA_A *);
BOOL SetupDiGetDriverInfoDetailA(HDEVINFO, SP_DEVINFO_DATA *, SP_DRVINFO_DATA_A *,
                                SP_DRVINFO_DETAIL_DATA_A *, DWORD, DWORD *);
HINF SetupOpenInfFileA(const char *, void *, DWORD, void *);
void SetupCloseInfFile(HINF);
BOOL SetupDiGetActualSectionToInstallA(HINF, const char *, char *, DWORD, DWORD *, void *);
BOOL SetupDiGetDeviceInstallParamsA(HDEVINFO, SP_DEVINFO_DATA *, SP_DEVINSTALL_PARAMS_A *);
BOOL SetupDiSetDeviceInstallParamsA(HDEVINFO, SP_DEVINFO_DATA *, SP_DEVINSTALL_PARAMS_A *);
BOOL SetupDiBuildDriverInfoList(HDEVINFO, SP_DEVINFO_DATA *, DWORD);
BOOL SetupDiEnumDriverInfoA(HDEVINFO, SP_DEVINFO_DATA *, DWORD, DWORD, SP_DRVINFO_DATA_A *);
BOOL SetupDiSetSelectedDriverA(HDEVINFO, SP_DEVINFO_DATA *, SP_DRVINFO_DATA_A *);
BOOL SetupDiCallClassInstaller(DI_FUNCTION, HDEVINFO, SP_DEVINFO_DATA *);
BOOL SetupDiDestroyDriverInfoList(HDEVINFO, SP_DEVINFO_DATA *, DWORD);
