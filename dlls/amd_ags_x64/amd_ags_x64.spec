@ stdcall agsDeInit(ptr)
@ stdcall agsDeInitialize(ptr)
@ stdcall agsCheckDriverVersion(ptr long)
@ stdcall -norelay -arch=win64 agsDriverExtensionsDX11_BeginUAVOverlap() DX11_BeginUAVOverlap_impl
@ stub agsDriverExtensionsDX11_CreateBuffer
@ stdcall agsDriverExtensionsDX11_CreateDevice(ptr ptr ptr ptr)
@ stub agsDriverExtensionsDX11_CreateFromDevice
@ stub agsDriverExtensionsDX11_CreateTexture1D
@ stub agsDriverExtensionsDX11_CreateTexture2D
@ stub agsDriverExtensionsDX11_CreateTexture3D
@ stdcall agsDriverExtensionsDX11_DeInit(ptr)
@ stub agsDriverExtensionsDX11_Destroy
@ stdcall -norelay -arch=win64 agsDriverExtensionsDX11_DestroyDevice()
@ stdcall -norelay -arch=win64 agsDriverExtensionsDX11_EndUAVOverlap() DX11_EndUAVOverlap_impl
@ stub agsDriverExtensionsDX11_GetMaxClipRects
@ stub agsDriverExtensionsDX11_IASetPrimitiveTopology
@ stdcall agsDriverExtensionsDX11_Init(ptr ptr long ptr)
@ stub agsDriverExtensionsDX11_MultiDrawIndexedInstancedIndirect
@ stub agsDriverExtensionsDX11_MultiDrawIndexedInstancedIndirectCountIndirect
@ stub agsDriverExtensionsDX11_MultiDrawInstancedIndirect
@ stub agsDriverExtensionsDX11_MultiDrawInstancedIndirectCountIndirect
@ stub agsDriverExtensionsDX11_NotifyResourceBeginAllAccess
@ stub agsDriverExtensionsDX11_NotifyResourceEndAllAccess
@ stub agsDriverExtensionsDX11_NotifyResourceEndWrites
@ stub agsDriverExtensionsDX11_NumPendingAsyncCompileJobs
@ stub agsDriverExtensionsDX11_SetClipRects
@ stdcall -norelay -arch=win64 agsDriverExtensionsDX11_SetDepthBounds() DX11_SetDepthBounds_impl
@ stub agsDriverExtensionsDX11_SetDiskShaderCacheEnabled
@ stub agsDriverExtensionsDX11_SetMaxAsyncCompileThreadCount
@ stub agsDriverExtensionsDX11_SetViewBroadcastMasks
@ stub agsDriverExtensionsDX11_WriteBreadcrumb
@ stdcall agsDriverExtensionsDX12_CreateDevice(ptr ptr ptr ptr)
@ stub agsDriverExtensionsDX12_CreateFromDevice
@ stub agsDriverExtensionsDX12_DeInit
@ stub agsDriverExtensionsDX12_Destroy
@ stdcall agsDriverExtensionsDX12_DestroyDevice(ptr ptr ptr)
@ stub agsDriverExtensionsDX12_Init
@ stub agsDriverExtensionsDX12_PopMarker
@ stub agsDriverExtensionsDX12_PushMarker
@ stub agsDriverExtensionsDX12_SetMarker
@ stdcall agsGetCrossfireGPUCount(ptr ptr)
@ stdcall agsGetVersionNumber()
@ stdcall agsInit(ptr ptr ptr)
@ stdcall agsInitialize(long ptr ptr ptr)
@ stdcall agsSetDisplayMode(ptr long long ptr)
@ stdcall agsGetTotalGPUCount(ptr ptr)
@ stdcall agsGetGPUMemorySize(ptr long ptr)
