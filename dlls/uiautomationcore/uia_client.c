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
#include "winuser.h"

#include "wine/debug.h"
#include "wine/heap.h"

WINE_DEFAULT_DEBUG_CHANNEL(uiautomation);

struct uia_data {
    IUIAutomation IUIAutomation_iface;
    LONG ref;
};

struct uia_elem_data {
    IUIAutomationElement IUIAutomationElement_iface;
    LONG ref;

    IRawElementProviderSimple *elem_prov;

    IAccessible *acc;
    VARIANT child_id;
};

static inline struct uia_elem_data *impl_from_IUIAutomationElement(IUIAutomationElement *iface);

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
    IUIAutomationElement *elem;
    LRESULT lres;
    HRESULT hr;

    TRACE("iface %p, hwnd %p, element %p\n", iface, hwnd, element);

    /* FIXME: Is this what actually gets returned? */
    if (!IsWindow(hwnd))
        return E_FAIL;

    lres = SendMessageW(hwnd, WM_GETOBJECT, 0, UiaRootObjectId);
    if (FAILED(lres))
        return lres;

    /*
     * If lres is 0, this is probably not a UIA provider, but instead an
     * MSAA server.
     */
    if (!lres)
    {
        IAccessible *acc;

        hr = AccessibleObjectFromWindow((HWND)hwnd, OBJID_CLIENT,
                &IID_IAccessible, (void **)&acc);
        if (FAILED(hr))
            return hr;

        hr = create_uia_elem_from_msaa_acc(&elem, acc, CHILDID_SELF);
        if (FAILED(hr))
            return hr;
    }
    else
    {
        IRawElementProviderSimple *elem_prov;

        hr = ObjectFromLresult(lres, &IID_IRawElementProviderSimple, 0,
                (void **)&elem_prov);
        if (FAILED(hr))
            return hr;

        hr = create_uia_elem_from_raw_provider(&elem, elem_prov);
        if (FAILED(hr))
            return hr;
    }

    *element = elem;

    return S_OK;
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

static inline struct uia_elem_data *impl_from_IUIAutomationElement(IUIAutomationElement *iface)
{
    return CONTAINING_RECORD(iface, struct uia_elem_data, IUIAutomationElement_iface);
}

static HRESULT WINAPI uia_elem_QueryInterface(IUIAutomationElement *iface, REFIID riid,
        void **ppvObject)
{
    struct uia_elem_data *This = impl_from_IUIAutomationElement(iface);

    TRACE("(%p)->(%s %p)\n", This, debugstr_guid(riid), ppvObject);

    if (IsEqualIID(riid, &IID_IUIAutomationElement) ||
            IsEqualIID(riid, &IID_IUnknown))
        *ppvObject = iface;
    else
    {
        WARN("no interface: %s\n", debugstr_guid(riid));
        *ppvObject = NULL;
        return E_NOINTERFACE;
    }

    IUIAutomationElement_AddRef(iface);

    return S_OK;
}

static ULONG WINAPI uia_elem_AddRef(IUIAutomationElement *iface)
{
    struct uia_elem_data *This = impl_from_IUIAutomationElement(iface);
    ULONG ref = InterlockedIncrement(&This->ref);

    TRACE("(%p) ref = %u\n", This, ref);
    return ref;
}

static ULONG WINAPI uia_elem_Release(IUIAutomationElement *iface)
{
    struct uia_elem_data *This = impl_from_IUIAutomationElement(iface);
    ULONG ref = InterlockedDecrement(&This->ref);

    TRACE("(%p) ref = %u\n", This, ref);

    if(!ref)
    {
        if (This->elem_prov)
            IRawElementProviderSimple_Release(This->elem_prov);
        else if (This->acc)
            IAccessible_Release(This->acc);

        heap_free(This);
    }

    return ref;
}

static HRESULT WINAPI uia_elem_SetFocus(IUIAutomationElement *iface)
{
    struct uia_elem_data *This = impl_from_IUIAutomationElement(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_elem_GetRuntimeId(IUIAutomationElement *iface,
        SAFEARRAY **runtimeId)
{
    struct uia_elem_data *This = impl_from_IUIAutomationElement(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_elem_FindFirst(IUIAutomationElement *iface,
        enum TreeScope scope, IUIAutomationCondition *condition,
        IUIAutomationElement **found)
{
    struct uia_elem_data *This = impl_from_IUIAutomationElement(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_elem_FindAll(IUIAutomationElement *iface,
        enum TreeScope scope, IUIAutomationCondition *condition,
        IUIAutomationElementArray **found)
{
    struct uia_elem_data *This = impl_from_IUIAutomationElement(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_elem_FindFirstBuildCache(IUIAutomationElement *iface,
        enum TreeScope scope, IUIAutomationCondition *condition,
        IUIAutomationCacheRequest *cacheRequest,
        IUIAutomationElement **found)
{
    struct uia_elem_data *This = impl_from_IUIAutomationElement(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_elem_FindAllBuildCache(IUIAutomationElement *iface,
        enum TreeScope scope, IUIAutomationCondition *condition,
        IUIAutomationCacheRequest *cacheRequest, IUIAutomationElementArray **found)
{
    struct uia_elem_data *This = impl_from_IUIAutomationElement(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_elem_BuildUpdatedCache(IUIAutomationElement *iface,
        IUIAutomationCacheRequest *cacheRequest, IUIAutomationElement **updatedElement)
{
    struct uia_elem_data *This = impl_from_IUIAutomationElement(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_elem_GetCurrentPropertyValue(IUIAutomationElement *iface,
        PROPERTYID propertyId, VARIANT *retVal)
{
    struct uia_elem_data *This = impl_from_IUIAutomationElement(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_elem_GetCurrentPropertyValueEx(IUIAutomationElement *iface,
        PROPERTYID propertyId, BOOL ignoreDefaultValue, VARIANT *retVal)
{
    struct uia_elem_data *This = impl_from_IUIAutomationElement(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_elem_GetCachedPropertyValue(IUIAutomationElement *iface,
        PROPERTYID propertyId, VARIANT *retVal)
{
    struct uia_elem_data *This = impl_from_IUIAutomationElement(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_elem_GetCachedPropertyValueEx(IUIAutomationElement *iface,
        PROPERTYID propertyId, BOOL ignoreDefaultValue, VARIANT *retVal)
{
    struct uia_elem_data *This = impl_from_IUIAutomationElement(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_elem_GetCurrentPatternAs(IUIAutomationElement *iface,
        PATTERNID patternId, REFIID riid, void **patternObject)
{
    struct uia_elem_data *This = impl_from_IUIAutomationElement(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_elem_GetCachedPatternAs(IUIAutomationElement *iface,
        PATTERNID patternId, REFIID riid, void **patternObject)
{
    struct uia_elem_data *This = impl_from_IUIAutomationElement(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_elem_GetCurrentPattern(IUIAutomationElement *iface,
        PATTERNID patternId, IUnknown **patternObject)
{
    struct uia_elem_data *This = impl_from_IUIAutomationElement(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_elem_GetCachedPattern(IUIAutomationElement *iface,
        PATTERNID patternId, IUnknown **patternObject)
{
    struct uia_elem_data *This = impl_from_IUIAutomationElement(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_elem_GetCachedParent(IUIAutomationElement *iface,
        IUIAutomationElement **parent)
{
    struct uia_elem_data *This = impl_from_IUIAutomationElement(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_elem_GetCachedChildren(IUIAutomationElement *iface,
        IUIAutomationElementArray **children)
{
    struct uia_elem_data *This = impl_from_IUIAutomationElement(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_elem_get_CurrentProcessId(IUIAutomationElement *iface,
        int *retVal)
{
    struct uia_elem_data *This = impl_from_IUIAutomationElement(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_elem_get_CurrentControlType(IUIAutomationElement *iface,
        CONTROLTYPEID *retVal)
{
    struct uia_elem_data *This = impl_from_IUIAutomationElement(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_elem_get_CurrentLocalizedControlType(IUIAutomationElement *iface,
        BSTR *retVal)
{
    struct uia_elem_data *This = impl_from_IUIAutomationElement(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_elem_get_CurrentName(IUIAutomationElement *iface,
        BSTR *retVal)
{
    struct uia_elem_data *This = impl_from_IUIAutomationElement(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_elem_get_CurrentAcceleratorKey(IUIAutomationElement *iface,
        BSTR *retVal)
{
    struct uia_elem_data *This = impl_from_IUIAutomationElement(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_elem_get_CurrentAccessKey(IUIAutomationElement *iface,
        BSTR *retVal)
{
    struct uia_elem_data *This = impl_from_IUIAutomationElement(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_elem_get_CurrentHasKeyboardFocus(IUIAutomationElement *iface,
        BOOL *retVal)
{
    struct uia_elem_data *This = impl_from_IUIAutomationElement(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_elem_get_CurrentIsKeyboardFocusable(IUIAutomationElement *iface,
        BOOL *retVal)
{
    struct uia_elem_data *This = impl_from_IUIAutomationElement(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_elem_get_CurrentIsEnabled(IUIAutomationElement *iface,
        BOOL *retVal)
{
    struct uia_elem_data *This = impl_from_IUIAutomationElement(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_elem_get_CurrentAutomationId(IUIAutomationElement *iface,
        BSTR *retVal)
{
    struct uia_elem_data *This = impl_from_IUIAutomationElement(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_elem_get_CurrentClassName(IUIAutomationElement *iface,
        BSTR *retVal)
{
    struct uia_elem_data *This = impl_from_IUIAutomationElement(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_elem_get_CurrentHelpText(IUIAutomationElement *iface,
        BSTR *retVal)
{
    struct uia_elem_data *This = impl_from_IUIAutomationElement(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_elem_get_CurrentCulture(IUIAutomationElement *iface,
        int *retVal)
{
    struct uia_elem_data *This = impl_from_IUIAutomationElement(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_elem_get_CurrentIsControlElement(IUIAutomationElement *iface,
        BOOL *retVal)
{
    struct uia_elem_data *This = impl_from_IUIAutomationElement(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_elem_get_CurrentIsContentElement(IUIAutomationElement *iface,
        BOOL *retVal)
{
    struct uia_elem_data *This = impl_from_IUIAutomationElement(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_elem_get_CurrentIsPassword(IUIAutomationElement *iface,
        BOOL *retVal)
{
    struct uia_elem_data *This = impl_from_IUIAutomationElement(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_elem_get_CurrentNativeWindowHandle(IUIAutomationElement *iface,
        UIA_HWND *retVal)
{
    struct uia_elem_data *This = impl_from_IUIAutomationElement(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_elem_get_CurrentItemType(IUIAutomationElement *iface,
        BSTR *retVal)
{
    struct uia_elem_data *This = impl_from_IUIAutomationElement(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_elem_get_CurrentIsOffscreen(IUIAutomationElement *iface,
        BOOL *retVal)
{
    struct uia_elem_data *This = impl_from_IUIAutomationElement(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_elem_get_CurrentOrientation(IUIAutomationElement *iface,
        enum OrientationType *retVal)
{
    struct uia_elem_data *This = impl_from_IUIAutomationElement(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_elem_get_CurrentFrameworkId(IUIAutomationElement *iface,
        BSTR *retVal)
{
    struct uia_elem_data *This = impl_from_IUIAutomationElement(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_elem_get_CurrentIsRequiredForForm(IUIAutomationElement *iface,
        BOOL *retVal)
{
    struct uia_elem_data *This = impl_from_IUIAutomationElement(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_elem_get_CurrentItemStatus(IUIAutomationElement *iface,
        BSTR *retVal)
{
    struct uia_elem_data *This = impl_from_IUIAutomationElement(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_elem_get_CurrentBoundingRectangle(IUIAutomationElement *iface,
        RECT *retVal)
{
    struct uia_elem_data *This = impl_from_IUIAutomationElement(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_elem_get_CurrentLabeledBy(IUIAutomationElement *iface,
        IUIAutomationElement **retVal)
{
    struct uia_elem_data *This = impl_from_IUIAutomationElement(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_elem_get_CurrentAriaRole(IUIAutomationElement *iface,
        BSTR *retVal)
{
    struct uia_elem_data *This = impl_from_IUIAutomationElement(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_elem_get_CurrentAriaProperties(IUIAutomationElement *iface,
        BSTR *retVal)
{
    struct uia_elem_data *This = impl_from_IUIAutomationElement(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_elem_get_CurrentIsDataValidForForm(IUIAutomationElement *iface,
        BOOL *retVal)
{
    struct uia_elem_data *This = impl_from_IUIAutomationElement(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_elem_get_CurrentControllerFor(IUIAutomationElement *iface,
        IUIAutomationElementArray **retVal)
{
    struct uia_elem_data *This = impl_from_IUIAutomationElement(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_elem_get_CurrentDescribedBy(IUIAutomationElement *iface,
        IUIAutomationElementArray **retVal)
{
    struct uia_elem_data *This = impl_from_IUIAutomationElement(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_elem_get_CurrentFlowsTo(IUIAutomationElement *iface,
        IUIAutomationElementArray **retVal)
{
    struct uia_elem_data *This = impl_from_IUIAutomationElement(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_elem_get_CurrentProviderDescription(IUIAutomationElement *iface,
        BSTR *retVal)
{
    struct uia_elem_data *This = impl_from_IUIAutomationElement(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_elem_get_CachedProcessId(IUIAutomationElement *iface,
        int *retVal)
{
    struct uia_elem_data *This = impl_from_IUIAutomationElement(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_elem_get_CachedControlType(IUIAutomationElement *iface,
        CONTROLTYPEID *retVal)
{
    struct uia_elem_data *This = impl_from_IUIAutomationElement(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_elem_get_CachedLocalizedControlType(IUIAutomationElement *iface,
        BSTR *retVal)
{
    struct uia_elem_data *This = impl_from_IUIAutomationElement(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_elem_get_CachedName(IUIAutomationElement *iface,
        BSTR *retVal)
{
    struct uia_elem_data *This = impl_from_IUIAutomationElement(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_elem_get_CachedAcceleratorKey(IUIAutomationElement *iface,
        BSTR *retVal)
{
    struct uia_elem_data *This = impl_from_IUIAutomationElement(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_elem_get_CachedAccessKey(IUIAutomationElement *iface,
        BSTR *retVal)
{
    struct uia_elem_data *This = impl_from_IUIAutomationElement(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_elem_get_CachedHasKeyboardFocus(IUIAutomationElement *iface,
        BOOL *retVal)
{
    struct uia_elem_data *This = impl_from_IUIAutomationElement(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_elem_get_CachedIsKeyboardFocusable(IUIAutomationElement *iface,
        BOOL *retVal)
{
    struct uia_elem_data *This = impl_from_IUIAutomationElement(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_elem_get_CachedIsEnabled(IUIAutomationElement *iface,
        BOOL *retVal)
{
    struct uia_elem_data *This = impl_from_IUIAutomationElement(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_elem_get_CachedAutomationId(IUIAutomationElement *iface,
        BSTR *retVal)
{
    struct uia_elem_data *This = impl_from_IUIAutomationElement(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_elem_get_CachedClassName(IUIAutomationElement *iface,
        BSTR *retVal)
{
    struct uia_elem_data *This = impl_from_IUIAutomationElement(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_elem_get_CachedHelpText(IUIAutomationElement *iface,
        BSTR *retVal)
{
    struct uia_elem_data *This = impl_from_IUIAutomationElement(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_elem_get_CachedCulture(IUIAutomationElement *iface,
        int *retVal)
{
    struct uia_elem_data *This = impl_from_IUIAutomationElement(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_elem_get_CachedIsControlElement(IUIAutomationElement *iface,
        BOOL *retVal)
{
    struct uia_elem_data *This = impl_from_IUIAutomationElement(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_elem_get_CachedIsContentElement(IUIAutomationElement *iface,
        BOOL *retVal)
{
    struct uia_elem_data *This = impl_from_IUIAutomationElement(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_elem_get_CachedIsPassword(IUIAutomationElement *iface,
        BOOL *retVal)
{
    struct uia_elem_data *This = impl_from_IUIAutomationElement(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_elem_get_CachedNativeWindowHandle(IUIAutomationElement *iface,
        UIA_HWND *retVal)
{
    struct uia_elem_data *This = impl_from_IUIAutomationElement(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_elem_get_CachedItemType(IUIAutomationElement *iface,
        BSTR *retVal)
{
    struct uia_elem_data *This = impl_from_IUIAutomationElement(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_elem_get_CachedIsOffscreen(IUIAutomationElement *iface,
        BOOL *retVal)
{
    struct uia_elem_data *This = impl_from_IUIAutomationElement(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_elem_get_CachedOrientation(IUIAutomationElement *iface,
        enum OrientationType *retVal)
{
    struct uia_elem_data *This = impl_from_IUIAutomationElement(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_elem_get_CachedFrameworkId(IUIAutomationElement *iface,
        BSTR *retVal)
{
    struct uia_elem_data *This = impl_from_IUIAutomationElement(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_elem_get_CachedIsRequiredForForm(IUIAutomationElement *iface,
        BOOL *retVal)
{
    struct uia_elem_data *This = impl_from_IUIAutomationElement(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_elem_get_CachedItemStatus(IUIAutomationElement *iface,
        BSTR *retVal)
{
    struct uia_elem_data *This = impl_from_IUIAutomationElement(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_elem_get_CachedBoundingRectangle(IUIAutomationElement *iface,
        RECT *retVal)
{
    struct uia_elem_data *This = impl_from_IUIAutomationElement(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_elem_get_CachedLabeledBy(IUIAutomationElement *iface,
        IUIAutomationElement **retVal)
{
    struct uia_elem_data *This = impl_from_IUIAutomationElement(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_elem_get_CachedAriaRole(IUIAutomationElement *iface,
        BSTR *retVal)
{
    struct uia_elem_data *This = impl_from_IUIAutomationElement(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_elem_get_CachedAriaProperties(IUIAutomationElement *iface,
        BSTR *retVal)
{
    struct uia_elem_data *This = impl_from_IUIAutomationElement(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_elem_get_CachedIsDataValidForForm(IUIAutomationElement *iface,
        BOOL *retVal)
{
    struct uia_elem_data *This = impl_from_IUIAutomationElement(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_elem_get_CachedControllerFor(IUIAutomationElement *iface,
        IUIAutomationElementArray **retVal)
{
    struct uia_elem_data *This = impl_from_IUIAutomationElement(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_elem_get_CachedDescribedBy(IUIAutomationElement *iface,
        IUIAutomationElementArray **retVal)
{
    struct uia_elem_data *This = impl_from_IUIAutomationElement(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_elem_get_CachedFlowsTo(IUIAutomationElement *iface,
        IUIAutomationElementArray **retVal)
{
    struct uia_elem_data *This = impl_from_IUIAutomationElement(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_elem_get_CachedProviderDescription(IUIAutomationElement *iface,
        BSTR *retVal)
{
    struct uia_elem_data *This = impl_from_IUIAutomationElement(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_elem_GetClickablePoint(IUIAutomationElement *iface,
        POINT *clickable, BOOL *gotClickable)
{
    struct uia_elem_data *This = impl_from_IUIAutomationElement(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static const IUIAutomationElementVtbl uia_elem_vtbl = {
    uia_elem_QueryInterface,
    uia_elem_AddRef,
    uia_elem_Release,
    uia_elem_SetFocus,
    uia_elem_GetRuntimeId,
    uia_elem_FindFirst,
    uia_elem_FindAll,
    uia_elem_FindFirstBuildCache,
    uia_elem_FindAllBuildCache,
    uia_elem_BuildUpdatedCache,
    uia_elem_GetCurrentPropertyValue,
    uia_elem_GetCurrentPropertyValueEx,
    uia_elem_GetCachedPropertyValue,
    uia_elem_GetCachedPropertyValueEx,
    uia_elem_GetCurrentPatternAs,
    uia_elem_GetCachedPatternAs,
    uia_elem_GetCurrentPattern,
    uia_elem_GetCachedPattern,
    uia_elem_GetCachedParent,
    uia_elem_GetCachedChildren,
    uia_elem_get_CurrentProcessId,
    uia_elem_get_CurrentControlType,
    uia_elem_get_CurrentLocalizedControlType,
    uia_elem_get_CurrentName,
    uia_elem_get_CurrentAcceleratorKey,
    uia_elem_get_CurrentAccessKey,
    uia_elem_get_CurrentHasKeyboardFocus,
    uia_elem_get_CurrentIsKeyboardFocusable,
    uia_elem_get_CurrentIsEnabled,
    uia_elem_get_CurrentAutomationId,
    uia_elem_get_CurrentClassName,
    uia_elem_get_CurrentHelpText,
    uia_elem_get_CurrentCulture,
    uia_elem_get_CurrentIsControlElement,
    uia_elem_get_CurrentIsContentElement,
    uia_elem_get_CurrentIsPassword,
    uia_elem_get_CurrentNativeWindowHandle,
    uia_elem_get_CurrentItemType,
    uia_elem_get_CurrentIsOffscreen,
    uia_elem_get_CurrentOrientation,
    uia_elem_get_CurrentFrameworkId,
    uia_elem_get_CurrentIsRequiredForForm,
    uia_elem_get_CurrentItemStatus,
    uia_elem_get_CurrentBoundingRectangle,
    uia_elem_get_CurrentLabeledBy,
    uia_elem_get_CurrentAriaRole,
    uia_elem_get_CurrentAriaProperties,
    uia_elem_get_CurrentIsDataValidForForm,
    uia_elem_get_CurrentControllerFor,
    uia_elem_get_CurrentDescribedBy,
    uia_elem_get_CurrentFlowsTo,
    uia_elem_get_CurrentProviderDescription,
    uia_elem_get_CachedProcessId,
    uia_elem_get_CachedControlType,
    uia_elem_get_CachedLocalizedControlType,
    uia_elem_get_CachedName,
    uia_elem_get_CachedAcceleratorKey,
    uia_elem_get_CachedAccessKey,
    uia_elem_get_CachedHasKeyboardFocus,
    uia_elem_get_CachedIsKeyboardFocusable,
    uia_elem_get_CachedIsEnabled,
    uia_elem_get_CachedAutomationId,
    uia_elem_get_CachedClassName,
    uia_elem_get_CachedHelpText,
    uia_elem_get_CachedCulture,
    uia_elem_get_CachedIsControlElement,
    uia_elem_get_CachedIsContentElement,
    uia_elem_get_CachedIsPassword,
    uia_elem_get_CachedNativeWindowHandle,
    uia_elem_get_CachedItemType,
    uia_elem_get_CachedIsOffscreen,
    uia_elem_get_CachedOrientation,
    uia_elem_get_CachedFrameworkId,
    uia_elem_get_CachedIsRequiredForForm,
    uia_elem_get_CachedItemStatus,
    uia_elem_get_CachedBoundingRectangle,
    uia_elem_get_CachedLabeledBy,
    uia_elem_get_CachedAriaRole,
    uia_elem_get_CachedAriaProperties,
    uia_elem_get_CachedIsDataValidForForm,
    uia_elem_get_CachedControllerFor,
    uia_elem_get_CachedDescribedBy,
    uia_elem_get_CachedFlowsTo,
    uia_elem_get_CachedProviderDescription,
    uia_elem_GetClickablePoint,
};

static HRESULT create_uia_elem_iface(IUIAutomationElement **iface)
{
    struct uia_elem_data *uia;

    uia = heap_alloc_zero(sizeof(*uia));
    if (!uia)
        return E_OUTOFMEMORY;

    uia->IUIAutomationElement_iface.lpVtbl = &uia_elem_vtbl;
    uia->ref = 1;
    *iface = &uia->IUIAutomationElement_iface;

    return S_OK;
}

HRESULT create_uia_elem_from_raw_provider(IUIAutomationElement **iface,
        IRawElementProviderSimple *wrap)
{
    struct uia_elem_data *uia;
    HRESULT hr;

    hr = create_uia_elem_iface(iface);
    if (FAILED(hr))
        return hr;

    uia = impl_from_IUIAutomationElement(*iface);
    uia->elem_prov = wrap;
    uia->acc = NULL;

    return S_OK;
}

HRESULT create_uia_elem_from_msaa_acc(IUIAutomationElement **iface,
        IAccessible *wrap, INT child_id)
{
    struct uia_elem_data *uia;
    HRESULT hr;

    hr = create_uia_elem_iface(iface);
    if (FAILED(hr))
        return hr;

    uia = impl_from_IUIAutomationElement(*iface);
    uia->elem_prov = NULL;
    uia->acc = wrap;
    V_VT(&uia->child_id) = VT_I4;
    V_I4(&uia->child_id) = child_id;

    return S_OK;
}
