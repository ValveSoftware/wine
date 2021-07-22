/*
 * Copyright 2021 Connor Mcadams for CodeWeavers
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#define COBJMACROS
#include "uia_private.h"

#include "wine/debug.h"
#include "wine/heap.h"

WINE_DEFAULT_DEBUG_CHANNEL(uiautomation);

struct uia_data {
    IUIAutomation IUIAutomation_iface;
    LONG ref;
};

static inline struct uia_data *impl_from_IUIAutomation(IUIAutomation *iface)
{
    return CONTAINING_RECORD(iface, struct uia_data, IUIAutomation_iface);
}

static HRESULT WINAPI uia_QueryInterface(IUIAutomation *iface, REFIID riid,
        void **ppvObject)
{
    struct uia_data *This = impl_from_IUIAutomation(iface);

    TRACE("(%p)->(%s %p)\n", This, debugstr_guid(riid), ppvObject);

    if (IsEqualIID(riid, &IID_IUIAutomation) ||
            IsEqualIID(riid, &IID_IUnknown))
        *ppvObject = iface;
    else
    {
        WARN("no interface: %s\n", debugstr_guid(riid));
        *ppvObject = NULL;
        return E_NOINTERFACE;
    }

    IUIAutomation_AddRef(iface);

    return S_OK;
}

static ULONG WINAPI uia_AddRef(IUIAutomation *iface)
{
    struct uia_data *This = impl_from_IUIAutomation(iface);
    ULONG ref = InterlockedIncrement(&This->ref);

    TRACE("(%p) ref = %u\n", This, ref);
    return ref;
}

static FORCEINLINE ULONG WINAPI uia_Release(IUIAutomation *iface)
{
    struct uia_data *This = impl_from_IUIAutomation(iface);
    ULONG ref = InterlockedDecrement(&This->ref);

    TRACE("(%p) ref = %u\n", This, ref);

    if(!ref)
        heap_free(This);

    return ref;
}

/*
 * IUIAutomation methods.
 */
static HRESULT WINAPI uia_CompareElements(IUIAutomation *iface,
        IUIAutomationElement *el1, IUIAutomationElement *el2, BOOL *areSame)
{
    struct uia_data *This = impl_from_IUIAutomation(iface);
    FIXME("This %p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_CompareRuntimeIds(IUIAutomation *iface,
        SAFEARRAY *runtimeId1, SAFEARRAY *runtimeId2, BOOL *areSame)
{
    struct uia_data *This = impl_from_IUIAutomation(iface);
    FIXME("This %p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_GetRootElement(IUIAutomation *iface,
        IUIAutomationElement **root)
{
    struct uia_data *This = impl_from_IUIAutomation(iface);
    FIXME("This %p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_ElementFromHandle(IUIAutomation *iface, UIA_HWND hwnd,
        IUIAutomationElement **element)
{
    struct uia_data *This = impl_from_IUIAutomation(iface);
    FIXME("This %p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_ElementFromPoint(IUIAutomation *iface, POINT pt,
        IUIAutomationElement **element)
{
    struct uia_data *This = impl_from_IUIAutomation(iface);
    FIXME("This %p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_GetFocusedElement(IUIAutomation *iface,
        IUIAutomationElement **element)
{
    struct uia_data *This = impl_from_IUIAutomation(iface);
    FIXME("This %p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_GetRootElementBuildCache(IUIAutomation *iface,
        IUIAutomationCacheRequest *cacheRequest,IUIAutomationElement **root)
{
    struct uia_data *This = impl_from_IUIAutomation(iface);
    FIXME("This %p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_ElementFromHandleBuildCache(IUIAutomation *iface,
        UIA_HWND hwnd, IUIAutomationCacheRequest *cacheRequest,
        IUIAutomationElement **root)
{
    struct uia_data *This = impl_from_IUIAutomation(iface);
    FIXME("This %p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_ElementFromPointBuildCache(IUIAutomation *iface, POINT pt,
        IUIAutomationCacheRequest *cacheRequest, IUIAutomationElement **element)
{
    struct uia_data *This = impl_from_IUIAutomation(iface);
    FIXME("This %p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_GetFocusedElementBuildCache(IUIAutomation *iface,
        IUIAutomationCacheRequest *cacheRequest, IUIAutomationElement **element)
{
    struct uia_data *This = impl_from_IUIAutomation(iface);
    FIXME("This %p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_CreateTreeWalker(IUIAutomation *iface,
        IUIAutomationCondition *pCondition, IUIAutomationTreeWalker **walker)
{
    struct uia_data *This = impl_from_IUIAutomation(iface);
    FIXME("This %p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_ControlViewWalker(IUIAutomation *iface,
        IUIAutomationTreeWalker **walker)
{
    struct uia_data *This = impl_from_IUIAutomation(iface);
    FIXME("This %p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_ContentViewWalker(IUIAutomation *iface,
        IUIAutomationTreeWalker **walker)
{
    struct uia_data *This = impl_from_IUIAutomation(iface);
    FIXME("This %p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_RawViewWalker(IUIAutomation *iface,
        IUIAutomationTreeWalker **walker)
{
    struct uia_data *This = impl_from_IUIAutomation(iface);
    FIXME("This %p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_RawViewCondition(IUIAutomation *iface,
        IUIAutomationCondition **condition)
{
    struct uia_data *This = impl_from_IUIAutomation(iface);
    FIXME("This %p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_ControlViewCondition(IUIAutomation *iface,
        IUIAutomationCondition **condition)
{
    struct uia_data *This = impl_from_IUIAutomation(iface);
    FIXME("This %p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_ContentViewCondition(IUIAutomation *iface,
        IUIAutomationCondition **condition)
{
    struct uia_data *This = impl_from_IUIAutomation(iface);
    FIXME("This %p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_CreateCacheRequest(IUIAutomation *iface,
        IUIAutomationCacheRequest **cacheRequest)
{
    struct uia_data *This = impl_from_IUIAutomation(iface);
    FIXME("This %p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_CreateTrueCondition(IUIAutomation *iface,
        IUIAutomationCondition **newCondition)
{
    struct uia_data *This = impl_from_IUIAutomation(iface);
    FIXME("This %p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_CreateFalseCondition(IUIAutomation *iface,
        IUIAutomationCondition **newCondition)
{
    struct uia_data *This = impl_from_IUIAutomation(iface);
    FIXME("This %p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_CreatePropertyCondition(IUIAutomation *iface,
        PROPERTYID propertyId, VARIANT value, IUIAutomationCondition **newCondition)
{
    struct uia_data *This = impl_from_IUIAutomation(iface);
    FIXME("This %p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_CreatePropertyConditionEx(IUIAutomation *iface,
        PROPERTYID propertyId, VARIANT value, enum PropertyConditionFlags flags,
        IUIAutomationCondition **newCondition)
{
    struct uia_data *This = impl_from_IUIAutomation(iface);
    FIXME("This %p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_CreateAndCondition(IUIAutomation *iface,
        IUIAutomationCondition *condition1, IUIAutomationCondition *condition2,
        IUIAutomationCondition **newCondition)
{
    struct uia_data *This = impl_from_IUIAutomation(iface);
    FIXME("This %p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_CreateAndConditionFromArray(IUIAutomation *iface,
        SAFEARRAY *conditions, IUIAutomationCondition **newCondition)
{
    struct uia_data *This = impl_from_IUIAutomation(iface);
    FIXME("This %p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_CreateAndConditionFromNativeArray(IUIAutomation *iface,
        IUIAutomationCondition **conditions, int conditionCount,
        IUIAutomationCondition **newCondition)
{
    struct uia_data *This = impl_from_IUIAutomation(iface);
    FIXME("This %p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_CreateOrCondition(IUIAutomation *iface,
        IUIAutomationCondition *condition1, IUIAutomationCondition *condition2,
        IUIAutomationCondition **newCondition)
{
    struct uia_data *This = impl_from_IUIAutomation(iface);
    FIXME("This %p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_CreateOrConditionFromArray(IUIAutomation *iface,
        SAFEARRAY *conditions, IUIAutomationCondition **newCondition)
{
    struct uia_data *This = impl_from_IUIAutomation(iface);
    FIXME("This %p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_CreateOrConditionFromNativeArray(IUIAutomation *iface,
        IUIAutomationCondition **conditions, int conditionCount,
        IUIAutomationCondition **newCondition)
{
    struct uia_data *This = impl_from_IUIAutomation(iface);
    FIXME("This %p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_CreateNotCondition(IUIAutomation *iface,
        IUIAutomationCondition *condition, IUIAutomationCondition **newCondition)
{
    struct uia_data *This = impl_from_IUIAutomation(iface);
    FIXME("This %p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_AddAutomationEventHandler(IUIAutomation *iface,
        EVENTID eventId, IUIAutomationElement *element, enum TreeScope scope,
        IUIAutomationCacheRequest *cacheRequest, IUIAutomationEventHandler *handler)
{
    struct uia_data *This = impl_from_IUIAutomation(iface);
    FIXME("This %p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_RemoveAutomationEventHandler(IUIAutomation *iface,
        EVENTID eventId, IUIAutomationElement *element,
        IUIAutomationEventHandler *handler)
{
    struct uia_data *This = impl_from_IUIAutomation(iface);
    FIXME("This %p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_AddPropertyChangedEventHandlerNativeArray(IUIAutomation *iface,
        IUIAutomationElement *element, enum TreeScope scope,
        IUIAutomationCacheRequest *cacheRequest,
        IUIAutomationPropertyChangedEventHandler *handler,
        PROPERTYID *propertyArray, int propertyCount)
{
    struct uia_data *This = impl_from_IUIAutomation(iface);
    FIXME("This %p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_AddPropertyChangedEventHandler(IUIAutomation *iface,
        IUIAutomationElement *element, enum TreeScope scope,
        IUIAutomationCacheRequest *cacheRequest,
        IUIAutomationPropertyChangedEventHandler *handler,
        SAFEARRAY *propertyArray)
{
    struct uia_data *This = impl_from_IUIAutomation(iface);
    FIXME("This %p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_RemovePropertyChangedEventHandler(IUIAutomation *iface,
        IUIAutomationElement *element,
        IUIAutomationPropertyChangedEventHandler *handler)
{
    struct uia_data *This = impl_from_IUIAutomation(iface);
    FIXME("This %p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_AddStructureChangedEventHandler(IUIAutomation *iface,
        IUIAutomationElement *element, enum TreeScope scope,
        IUIAutomationCacheRequest *cacheRequest,
        IUIAutomationStructureChangedEventHandler *handler)
{
    struct uia_data *This = impl_from_IUIAutomation(iface);
    FIXME("This %p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_RemoveStructureChangedEventHandler(IUIAutomation *iface,
        IUIAutomationElement *element, IUIAutomationStructureChangedEventHandler *handler)
{
    struct uia_data *This = impl_from_IUIAutomation(iface);
    FIXME("This %p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_AddFocusChangedEventHandler(IUIAutomation *iface,
        IUIAutomationCacheRequest *cacheRequest,
        IUIAutomationFocusChangedEventHandler *handler)
{
    struct uia_data *This = impl_from_IUIAutomation(iface);
    FIXME("This %p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_RemoveFocusChangedEventHandler(IUIAutomation *iface,
        IUIAutomationFocusChangedEventHandler *handler)
{
    struct uia_data *This = impl_from_IUIAutomation(iface);
    FIXME("This %p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_RemoveAllEventHandlers(IUIAutomation *iface)
{
    struct uia_data *This = impl_from_IUIAutomation(iface);
    FIXME("This %p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_IntNativeArrayToSafeArray(IUIAutomation *iface,
        int *array, int arrayCount, SAFEARRAY **safeArray)
{
    struct uia_data *This = impl_from_IUIAutomation(iface);
    FIXME("This %p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_IntSafeArrayToNativeArray(IUIAutomation *iface,
        SAFEARRAY *intArray, int **array, int *arrayCount)
{
    struct uia_data *This = impl_from_IUIAutomation(iface);
    FIXME("This %p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_RectToVariant(IUIAutomation *iface, RECT rc,
        VARIANT *var)
{
    struct uia_data *This = impl_from_IUIAutomation(iface);
    FIXME("This %p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_VariantToRect(IUIAutomation *iface, VARIANT var,
        RECT *rc)
{
    struct uia_data *This = impl_from_IUIAutomation(iface);
    FIXME("This %p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_SafeArrayToRectNativeArray(IUIAutomation *iface,
        SAFEARRAY *rects, RECT **rectArray, int *rectArrayCount)
{
    struct uia_data *This = impl_from_IUIAutomation(iface);
    FIXME("This %p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_CreateProxyFactoryEntry(IUIAutomation *iface,
        IUIAutomationProxyFactory *factory, IUIAutomationProxyFactoryEntry **factoryEntry)
{
    struct uia_data *This = impl_from_IUIAutomation(iface);
    FIXME("This %p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_get_ProxyFactoryMapping(IUIAutomation *iface,
        IUIAutomationProxyFactoryMapping **factoryMapping)
{
    struct uia_data *This = impl_from_IUIAutomation(iface);
    FIXME("This %p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_GetPropertyProgrammaticName(IUIAutomation *iface,
        PROPERTYID property,BSTR *name)
{
    struct uia_data *This = impl_from_IUIAutomation(iface);
    FIXME("This %p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_GetPatternProgrammaticName(IUIAutomation *iface,
        PATTERNID pattern, BSTR *name)
{
    struct uia_data *This = impl_from_IUIAutomation(iface);
    FIXME("This %p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_PollForPotentialSupportedPatterns(IUIAutomation *iface,
        IUIAutomationElement *pElement, SAFEARRAY **patternIds,
        SAFEARRAY **patternNames)
{
    struct uia_data *This = impl_from_IUIAutomation(iface);
    FIXME("This %p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_PollForPotentialSupportedProperties(IUIAutomation *iface,
        IUIAutomationElement *pElement, SAFEARRAY **propertyIds,
        SAFEARRAY **propertyNames)
{
    struct uia_data *This = impl_from_IUIAutomation(iface);
    FIXME("This %p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_CheckNotSupported(IUIAutomation *iface, VARIANT value,
        BOOL *isNotSupported)
{
    struct uia_data *This = impl_from_IUIAutomation(iface);
    FIXME("This %p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_get_ReservedNotSupportedValue(IUIAutomation *iface,
        IUnknown **notSupportedValue)
{
    struct uia_data *This = impl_from_IUIAutomation(iface);
    FIXME("This %p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_get_ReservedMixedAttributeValue(IUIAutomation *iface,
        IUnknown **mixedAttributeValue)
{
    struct uia_data *This = impl_from_IUIAutomation(iface);
    FIXME("This %p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_ElementFromIAccessible(IUIAutomation *iface,
        IAccessible *accessible, int childId, IUIAutomationElement **element)
{
    struct uia_data *This = impl_from_IUIAutomation(iface);
    FIXME("This %p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_ElementFromIAccessibleBuildCache(IUIAutomation *iface,
        IAccessible *accessible, int childId,
        IUIAutomationCacheRequest *cacheRequest, IUIAutomationElement **element)
{
    struct uia_data *This = impl_from_IUIAutomation(iface);
    FIXME("This %p\n", This);
    return E_NOTIMPL;
}

static const IUIAutomationVtbl uia_vtbl = {
    uia_QueryInterface,
    uia_AddRef,
    uia_Release,
    uia_CompareElements,
    uia_CompareRuntimeIds,
    uia_GetRootElement,
    uia_ElementFromHandle,
    uia_ElementFromPoint,
    uia_GetFocusedElement,
    uia_GetRootElementBuildCache,
    uia_ElementFromHandleBuildCache,
    uia_ElementFromPointBuildCache,
    uia_GetFocusedElementBuildCache,
    uia_CreateTreeWalker,
    uia_ControlViewWalker,
    uia_ContentViewWalker,
    uia_RawViewWalker,
    uia_RawViewCondition,
    uia_ControlViewCondition,
    uia_ContentViewCondition,
    uia_CreateCacheRequest,
    uia_CreateTrueCondition,
    uia_CreateFalseCondition,
    uia_CreatePropertyCondition,
    uia_CreatePropertyConditionEx,
    uia_CreateAndCondition,
    uia_CreateAndConditionFromArray,
    uia_CreateAndConditionFromNativeArray,
    uia_CreateOrCondition,
    uia_CreateOrConditionFromArray,
    uia_CreateOrConditionFromNativeArray,
    uia_CreateNotCondition,
    uia_AddAutomationEventHandler,
    uia_RemoveAutomationEventHandler,
    uia_AddPropertyChangedEventHandlerNativeArray,
    uia_AddPropertyChangedEventHandler,
    uia_RemovePropertyChangedEventHandler,
    uia_AddStructureChangedEventHandler,
    uia_RemoveStructureChangedEventHandler,
    uia_AddFocusChangedEventHandler,
    uia_RemoveFocusChangedEventHandler,
    uia_RemoveAllEventHandlers,
    uia_IntNativeArrayToSafeArray,
    uia_IntSafeArrayToNativeArray,
    uia_RectToVariant,
    uia_VariantToRect,
    uia_SafeArrayToRectNativeArray,
    uia_CreateProxyFactoryEntry,
    uia_get_ProxyFactoryMapping,
    uia_GetPropertyProgrammaticName,
    uia_GetPatternProgrammaticName,
    uia_PollForPotentialSupportedPatterns,
    uia_PollForPotentialSupportedProperties,
    uia_CheckNotSupported,
    uia_get_ReservedNotSupportedValue,
    uia_get_ReservedMixedAttributeValue,
    uia_ElementFromIAccessible,
    uia_ElementFromIAccessibleBuildCache,
};

HRESULT create_uia_iface(IUIAutomation **iface)
{
    struct uia_data *uia;

    uia = heap_alloc_zero(sizeof(*uia));
    if (!uia)
        return E_OUTOFMEMORY;

    uia->IUIAutomation_iface.lpVtbl = &uia_vtbl;
    uia->ref = 1;
    *iface = &uia->IUIAutomation_iface;

    return S_OK;
}
