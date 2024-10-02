@ stdcall agsDeInit(ptr)
@ stdcall agsDeInitialize(ptr)
@ stdcall agsCheckDriverVersion(ptr long)
@ stdcall -norelay -arch=win64 agsDriverExtensions_BeginUAVOverlap() DX11_BeginUAVOverlap_impl
@ stdcall -norelay -arch=win64 agsDriverExtensionsDX11_BeginUAVOverlap() DX11_BeginUAVOverlap_impl
@ stub agsDriverExtensions_IASetPrimitiveTopology
@ stub agsDriverExtensionsDX11_CreateBuffer
@ stub agsDriverExtensions_CreateBuffer
@ stdcall agsDriverExtensionsDX11_CreateDevice(ptr ptr ptr ptr)
@ stub agsDriverExtensionsDX11_CreateFromDevice
@ stub agsDriverExtensionsDX11_CreateTexture1D
@ stub agsDriverExtensionsDX11_CreateTexture2D
@ stub agsDriverExtensionsDX11_CreateTexture3D
@ stub agsDriverExtensions_CreateTexture1D
@ stub agsDriverExtensions_CreateTexture2D
@ stub agsDriverExtensions_CreateTexture3D
@ stdcall agsDriverExtensions_DeInit(ptr)
@ stdcall agsDriverExtensionsDX11_DeInit(ptr)
@ stub agsDriverExtensionsDX11_Destroy
@ stdcall -norelay -arch=win64 agsDriverExtensionsDX11_DestroyDevice()
@ stdcall -norelay -arch=win64 agsDriverExtensions_EndUAVOverlap() DX11_EndUAVOverlap_impl
@ stdcall -norelay -arch=win64 agsDriverExtensionsDX11_EndUAVOverlap() DX11_EndUAVOverlap_impl
@ stub agsDriverExtensionsDX11_GetMaxClipRects
@ stub agsDriverExtensionsDX11_IASetPrimitiveTopology
@ stdcall agsDriverExtensions_Init(ptr ptr ptr)
@ stdcall agsDriverExtensionsDX11_Init(ptr ptr long ptr)
@ stdcall -norelay -arch=win64 agsDriverExtensions_MultiDrawIndexedInstancedIndirect() DX11_MultiDrawIndexedInstancedIndirect_impl
@ stdcall -norelay -arch=win64 agsDriverExtensionsDX11_MultiDrawIndexedInstancedIndirect() DX11_MultiDrawIndexedInstancedIndirect_impl
@ stdcall -norelay -arch=win64 agsDriverExtensionsDX11_MultiDrawIndexedInstancedIndirectCountIndirect() DX11_MultiDrawIndexedInstancedIndirectCountIndirect_impl
@ stdcall -norelay -arch=win64 agsDriverExtensions_MultiDrawInstancedIndirect() DX11_MultiDrawInstancedIndirect_impl
@ stdcall -norelay -arch=win64 agsDriverExtensionsDX11_MultiDrawInstancedIndirect() DX11_MultiDrawInstancedIndirect_impl
@ stdcall -norelay -arch=win64 agsDriverExtensionsDX11_MultiDrawInstancedIndirectCountIndirect() DX11_MultiDrawInstancedIndirectCountIndirect_impl
@ stub agsDriverExtensionsDX11_NotifyResourceBeginAllAccess
@ stub agsDriverExtensionsDX11_NotifyResourceEndAllAccess
@ stub agsDriverExtensionsDX11_NotifyResourceEndWrites
@ stub agsDriverExtensions_NotifyResourceBeginAllAccess
@ stub agsDriverExtensions_NotifyResourceEndAllAccess
@ stub agsDriverExtensions_NotifyResourceEndWrites
@ stub agsDriverExtensionsDX11_NumPendingAsyncCompileJobs
@ stub agsDriverExtensionsDX11_SetClipRects
@ stdcall -norelay -arch=win64 agsDriverExtensions_SetDepthBounds() DX11_SetDepthBounds_impl
@ stdcall -norelay -arch=win64 agsDriverExtensionsDX11_SetDepthBounds() DX11_SetDepthBounds_impl
@ stdcall agsDriverExtensionsDX11_SetDiskShaderCacheEnabled(ptr long)
@ stub agsDriverExtensionsDX11_SetMaxAsyncCompileThreadCount
@ stub agsDriverExtensionsDX11_SetViewBroadcastMasks
@ stub agsDriverExtensionsDX11_WriteBreadcrumb
@ stdcall agsDriverExtensionsDX12_CreateDevice(ptr ptr ptr ptr)
@ stub agsDriverExtensionsDX12_CreateFromDevice
@ stdcall agsDriverExtensionsDX12_DeInit(ptr)
@ stub agsDriverExtensionsDX12_Destroy
@ stdcall agsDriverExtensionsDX12_DestroyDevice(ptr ptr ptr)
@ stdcall agsDriverExtensionsDX12_Init(ptr ptr ptr)
@ stdcall agsDriverExtensionsDX12_PopMarker(ptr ptr)
@ stdcall agsDriverExtensionsDX12_PushMarker(ptr ptr ptr)
@ stdcall agsDriverExtensionsDX12_SetMarker(ptr ptr ptr)
@ stdcall agsGetCrossfireGPUCount(ptr ptr)
@ stdcall agsGetVersionNumber()
@ stdcall agsInit(ptr ptr ptr)
@ stdcall agsInitialize(long ptr ptr ptr)
@ stdcall agsSetDisplayMode(ptr long long ptr)
@ stdcall agsGetTotalGPUCount(ptr ptr)
@ stdcall agsGetGPUMemorySize(ptr long ptr)
@ stdcall agsGetEyefinityConfigInfo(ptr long ptr ptr ptr)
@ stdcall agsDriverExtensions_SetCrossfireMode(ptr long)
