/* Execute the actual native wrapper against deterministic API doubles, no resources. */
#define WGI_TEST
#define main gpu_main
#include "../programs/gpu-prepare.c"
#undef main
#include <stdlib.h>

static const char *scenario;
static DWORD flags, state = DICS_DISABLE, class_written, service_written;
static int reads, enabled;
DWORD GetLastError(void) { return ERROR_NO_MORE_ITEMS; }
HDEVINFO SetupDiGetClassDevsA(void *a,const char*b,void*c,DWORD d) {
    (void)a;(void)b;(void)c;(void)d;return (HDEVINFO)2;
}
int SetupDiEnumDeviceInfo(HDEVINFO a,DWORD i,SP_DEVINFO_DATA*b) {
    (void)a;b->DevInst=4;return i==0 && strcmp(scenario,"absent");
}
int SetupDiGetDeviceInstanceIdA(HDEVINFO a,SP_DEVINFO_DATA*b,char*c,DWORD d,DWORD*e) {
    (void)a;(void)b;strcpy(c,"PCI\\VEN_10DE&DEV_2684");
    if(e) *e=!strcmp(scenario,"instance_size")?d+1:(DWORD)strlen(c)+1;
    return 1;
}
int SetupDiGetDeviceRegistryPropertyA(HDEVINFO a,SP_DEVINFO_DATA*b,DWORD p,
                                    DWORD*t,BYTE*v,DWORD n,DWORD*r) {
    (void)a;(void)b;(void)n;(void)r;
    *t=p==SPDRP_COMPATIBLEIDS?REG_MULTI_SZ:REG_SZ;
    if(p==SPDRP_COMPATIBLEIDS) strcpy((char*)v,"PCI\\CC_030000");
    if(p==SPDRP_DRIVER) strcpy((char*)v,
        !strcmp(scenario,"badkey")?"{4d36e968-e325-11ce-bfc1-08002be10318}\\99999":
        "{4d36e968-e325-11ce-bfc1-08002be10318}\\0007");
    if(p==SPDRP_SERVICE) strcpy((char*)v,"nvlddmkm");
    if(r) *r=(DWORD)strlen((char*)v)+(p==SPDRP_COMPATIBLEIDS?2:1);
    if((p==SPDRP_DRIVER && !strcmp(scenario,"driver_nul_size")) ||
       (p==SPDRP_SERVICE && !strcmp(scenario,"service_nul_size"))) {
        if(r) *r=(DWORD)strlen((char*)v);
    }
    if((p==SPDRP_DRIVER && !strcmp(scenario,"driver_type")) ||
       (p==SPDRP_SERVICE && !strcmp(scenario,"service_type_prop"))) *t=REG_MULTI_SZ;
    if((p==SPDRP_DRIVER && !strcmp(scenario,"driver_size")) ||
       (p==SPDRP_SERVICE && !strcmp(scenario,"service_size")) ||
       (p==SPDRP_COMPATIBLEIDS && !strcmp(scenario,"compatible_size"))) {
        if(r) *r=n+1;
    }
    if((p==SPDRP_DRIVER && !strcmp(scenario,"driver_unterminated")) ||
       (p==SPDRP_COMPATIBLEIDS && !strcmp(scenario,"compatible_unterminated"))) {
        memset(v,'x',n); if(r) *r=n;
    }
    return 1;
}
int SetupDiSetClassInstallParamsA(HDEVINFO a,SP_DEVINFO_DATA*b,
                                SP_CLASSINSTALL_HEADER*p,DWORD n) {
    (void)a;(void)b;(void)n;state=((SP_PROPCHANGE_PARAMS*)p)->StateChange;return 1;
}
int SetupDiCallClassInstaller(DWORD f,HDEVINFO a,SP_DEVINFO_DATA*b) {
    (void)a;(void)b;
    if(f==DIF_INSTALLDEVICE) {
        printf("MOCK_INSTALL flags=%lu disabled=%d\n",flags,state==DICS_DISABLE);
        return (flags & DI_DONOTCALLCONFIGMG) && state==DICS_DISABLE;
    }
    if(f==DIF_PROPERTYCHANGE && state==DICS_ENABLE) {
        ++enabled;printf("MOCK_ENABLE reads=%d class=%lu service=%lu\n",
                         reads,class_written,service_written);
        return reads==2 && class_written==1 && service_written==1;
    }
    return strcmp(scenario,"disablefail");
}
int SetupDiGetDeviceInstallParamsA(HDEVINFO a,SP_DEVINFO_DATA*b,SP_DEVINSTALL_PARAMS_A*p) {
    (void)a;(void)b;p->Flags=flags;return 1;
}
int SetupDiSetDeviceInstallParamsA(HDEVINFO a,SP_DEVINFO_DATA*b,SP_DEVINSTALL_PARAMS_A*p) {
    (void)a;(void)b;flags=p->Flags;return 1;
}
int SetupDiBuildDriverInfoList(HDEVINFO a,SP_DEVINFO_DATA*b,DWORD c) {
    (void)a;(void)b;(void)c;return 1;
}
int SetupDiGetSelectedDriverA(HDEVINFO a,SP_DEVINFO_DATA*b,SP_DRVINFO_DATA_A*p) {
    (void)a;(void)b;strcpy(p->ProviderName,!strcmp(scenario,"provider")?"OTHER":"NVIDIA");
    if(!strcmp(scenario,"provider_unterminated"))
        memset(p->ProviderName,'N',sizeof(p->ProviderName));
    return 1;
}
int CM_Get_DevNode_Status(ULONG*s,ULONG*p,DWORD d,DWORD f) {
    (void)d;(void)f;*s=state==DICS_ENABLE?DN_STARTED:0;
    *p=state==DICS_DISABLE?CM_PROB_DISABLED:0;return 0;
}
HKEY SetupDiOpenDevRegKey(HDEVINFO a,SP_DEVINFO_DATA*b,DWORD s,DWORD p,DWORD t,DWORD k) {
    (void)a;(void)b;(void)s;(void)p;(void)t;(void)k;return (HKEY)11;
}
int RegOpenKeyExA(HKEY a,const char*b,DWORD c,DWORD d,HKEY*e) {
    (void)a;(void)b;(void)c;(void)d;*e=(HKEY)12;return 0;
}
int RegSetValueExA(HKEY k,const char*n,DWORD r,DWORD t,const BYTE*v,DWORD z) {
    (void)r;(void)t;(void)z;
    if(!strcmp(n,"EnableGpuFirmware")) {
        if(k==(HKEY)11) class_written=*(const DWORD*)v;else service_written=*(const DWORD*)v;
    }
    return 0;
}
int RegQueryValueExA(HKEY k,const char*n,DWORD*r,DWORD*t,BYTE*v,DWORD*z) {
    (void)n;(void)r;++reads;*t=REG_DWORD;*z=sizeof(DWORD);*(DWORD*)v=1;
    if((k==(HKEY)11 && !strcmp(scenario,"class_type")) ||
       (k==(HKEY)12 && !strcmp(scenario,"service_type"))) *t=1;
    if((k==(HKEY)11 && !strcmp(scenario,"class_zero")) ||
       (k==(HKEY)12 && !strcmp(scenario,"service_zero"))) *(DWORD*)v=0;
    return 0;
}
int RegCloseKey(HKEY a) {(void)a;return 0;}
int SetupDiDestroyDeviceInfoList(HDEVINFO a) {(void)a;return 1;}
int main(int argc,char**argv) {
    scenario=argc>1?argv[1]:"ok";
    char *args[]={"gpu-prepare","test.inf",NULL};
    int rc=gpu_main(2,args);
    printf("MOCK_END rc=%d enabled=%d reads=%d\n",rc,enabled,reads);
    return rc;
}
