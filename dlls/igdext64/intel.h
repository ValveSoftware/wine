#include <stdarg.h>

#include "windef.h"
#include "winbase.h"

typedef struct
{
    UINT32    GPUMaxFreq;
    UINT32    GPUMinFreq;
    UINT32    GTGeneration;
    UINT32    EUCount;
    UINT32    PackageTDP;
    UINT32    MaxFillRate;
    wchar_t     GTGenerationName[ 64 ];
} INTCDeviceInfo;

typedef struct
{
    UINT32    HWFeatureLevel;
    UINT32    APIVersion;
    UINT32    Revision;
} INTCExtensionVersion;

typedef struct
{
    INTCExtensionVersion    RequestedExtensionVersion;  

    INTCDeviceInfo          IntelDeviceInfo;     
    const wchar_t*          pDeviceDriverDesc;     
    const wchar_t*          pDeviceDriverVersion;
    UINT32                DeviceDriverBuildNumber;
} INTCExtensionInfo;

typedef struct
{
    const wchar_t*  pApplicationName;
    UINT32        ApplicationVersion;
    const wchar_t*  pEngineName;
    UINT32        EngineVersion;
} INTCExtensionAppInfo;

typedef struct 
{
    INTCExtensionInfo info;
    INTCExtensionAppInfo app_info;
} INTCExtensionContext;
