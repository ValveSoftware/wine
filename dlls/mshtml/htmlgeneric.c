/*
 * Copyright 2008 Jacek Caban for CodeWeavers
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


#include <stdarg.h>
#include <assert.h>

#define COBJMACROS

#include "windef.h"
#include "winbase.h"
#include "winuser.h"
#include "ole2.h"

#include "wine/debug.h"

#include "mshtml_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(mshtml);

typedef struct domtokenlist
{
    DispatchEx dispex;
    IDOMTokenList IDOMTokenList_iface;
    IHTMLElement *element;

    LONG ref;
} tokenlist;

static inline tokenlist *impl_from_IDOMTokenList(IDOMTokenList *iface)
{
    return CONTAINING_RECORD(iface, tokenlist, IDOMTokenList_iface);
}

struct HTMLGenericElement {
    HTMLElement element;

    IHTMLGenericElement IHTMLGenericElement_iface;
};

static inline HTMLGenericElement *impl_from_IHTMLGenericElement(IHTMLGenericElement *iface)
{
    return CONTAINING_RECORD(iface, HTMLGenericElement, IHTMLGenericElement_iface);
}

static HRESULT WINAPI HTMLGenericElement_QueryInterface(IHTMLGenericElement *iface, REFIID riid, void **ppv)
{
    HTMLGenericElement *This = impl_from_IHTMLGenericElement(iface);

    return IHTMLDOMNode_QueryInterface(&This->element.node.IHTMLDOMNode_iface, riid, ppv);
}

static ULONG WINAPI HTMLGenericElement_AddRef(IHTMLGenericElement *iface)
{
    HTMLGenericElement *This = impl_from_IHTMLGenericElement(iface);

    return IHTMLDOMNode_AddRef(&This->element.node.IHTMLDOMNode_iface);
}

static ULONG WINAPI HTMLGenericElement_Release(IHTMLGenericElement *iface)
{
    HTMLGenericElement *This = impl_from_IHTMLGenericElement(iface);

    return IHTMLDOMNode_Release(&This->element.node.IHTMLDOMNode_iface);
}

static HRESULT WINAPI HTMLGenericElement_GetTypeInfoCount(IHTMLGenericElement *iface, UINT *pctinfo)
{
    HTMLGenericElement *This = impl_from_IHTMLGenericElement(iface);
    return IDispatchEx_GetTypeInfoCount(&This->element.node.event_target.dispex.IDispatchEx_iface, pctinfo);
}

static HRESULT WINAPI HTMLGenericElement_GetTypeInfo(IHTMLGenericElement *iface, UINT iTInfo,
                                              LCID lcid, ITypeInfo **ppTInfo)
{
    HTMLGenericElement *This = impl_from_IHTMLGenericElement(iface);
    return IDispatchEx_GetTypeInfo(&This->element.node.event_target.dispex.IDispatchEx_iface, iTInfo, lcid,
            ppTInfo);
}

static HRESULT WINAPI HTMLGenericElement_GetIDsOfNames(IHTMLGenericElement *iface, REFIID riid,
        LPOLESTR *rgszNames, UINT cNames, LCID lcid, DISPID *rgDispId)
{
    HTMLGenericElement *This = impl_from_IHTMLGenericElement(iface);
    return IDispatchEx_GetIDsOfNames(&This->element.node.event_target.dispex.IDispatchEx_iface, riid, rgszNames,
            cNames, lcid, rgDispId);
}

static HRESULT WINAPI HTMLGenericElement_Invoke(IHTMLGenericElement *iface, DISPID dispIdMember,
        REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS *pDispParams,
        VARIANT *pVarResult, EXCEPINFO *pExcepInfo, UINT *puArgErr)
{
    HTMLGenericElement *This = impl_from_IHTMLGenericElement(iface);
    return IDispatchEx_Invoke(&This->element.node.event_target.dispex.IDispatchEx_iface, dispIdMember, riid,
            lcid, wFlags, pDispParams, pVarResult, pExcepInfo, puArgErr);
}

static HRESULT WINAPI HTMLGenericElement_get_recordset(IHTMLGenericElement *iface, IDispatch **p)
{
    HTMLGenericElement *This = impl_from_IHTMLGenericElement(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLGenericElement_namedRecordset(IHTMLGenericElement *iface,
        BSTR dataMember, VARIANT *hierarchy, IDispatch **ppRecordset)
{
    HTMLGenericElement *This = impl_from_IHTMLGenericElement(iface);
    FIXME("(%p)->(%s %p %p)\n", This, debugstr_w(dataMember), hierarchy, ppRecordset);
    return E_NOTIMPL;
}

static const IHTMLGenericElementVtbl HTMLGenericElementVtbl = {
    HTMLGenericElement_QueryInterface,
    HTMLGenericElement_AddRef,
    HTMLGenericElement_Release,
    HTMLGenericElement_GetTypeInfoCount,
    HTMLGenericElement_GetTypeInfo,
    HTMLGenericElement_GetIDsOfNames,
    HTMLGenericElement_Invoke,
    HTMLGenericElement_get_recordset,
    HTMLGenericElement_namedRecordset
};

static inline HTMLGenericElement *impl_from_HTMLDOMNode(HTMLDOMNode *iface)
{
    return CONTAINING_RECORD(iface, HTMLGenericElement, element.node);
}

static HRESULT HTMLGenericElement_QI(HTMLDOMNode *iface, REFIID riid, void **ppv)
{
    HTMLGenericElement *This = impl_from_HTMLDOMNode(iface);

    *ppv = NULL;

    if(IsEqualGUID(&IID_IHTMLGenericElement, riid)) {
        TRACE("(%p)->(IID_IHTMLGenericElement %p)\n", This, ppv);
        *ppv = &This->IHTMLGenericElement_iface;
    }else {
        return HTMLElement_QI(&This->element.node, riid, ppv);
    }

    IUnknown_AddRef((IUnknown*)*ppv);
    return S_OK;
}

static void HTMLGenericElement_destructor(HTMLDOMNode *iface)
{
    HTMLGenericElement *This = impl_from_HTMLDOMNode(iface);
    tokenlist *class_list;
    VARIANT *class_list_prop;

    if (SUCCEEDED(dispex_get_dprop_ref(&This->element.node.event_target.dispex, L"classList", FALSE, &class_list_prop))
            && V_VT(class_list_prop) == VT_DISPATCH)
    {
        class_list = impl_from_IDOMTokenList((IDOMTokenList *)V_DISPATCH(class_list_prop));
        if (class_list)
            class_list->element = NULL;
    }

    HTMLElement_destructor(&This->element.node);
}

static const NodeImplVtbl HTMLGenericElementImplVtbl = {
    &CLSID_HTMLGenericElement,
    HTMLGenericElement_QI,
    HTMLGenericElement_destructor,
    HTMLElement_cpc,
    HTMLElement_clone,
    HTMLElement_handle_event,
    HTMLElement_get_attr_col
};

static const tid_t HTMLGenericElement_iface_tids[] = {
    HTMLELEMENT_TIDS,
    IHTMLGenericElement_tid,
    0
};

static dispex_static_data_t HTMLGenericElement_dispex = {
    NULL,
    DispHTMLGenericElement_tid,
    HTMLGenericElement_iface_tids,
    HTMLElement_init_dispex_info
};

static HRESULT WINAPI tokenlist_QueryInterface(IDOMTokenList *iface, REFIID riid, void **ppv)
{
    tokenlist *This = impl_from_IDOMTokenList(iface);

    TRACE("(%p)->(%s %p)\n", This, debugstr_mshtml_guid(riid), ppv);

    if(IsEqualGUID(&IID_IUnknown, riid)) {
        *ppv = &This->IDOMTokenList_iface;
    }else if(IsEqualGUID(&IID_IDOMTokenList, riid)) {
        *ppv = &This->IDOMTokenList_iface;
    }else if(dispex_query_interface(&This->dispex, riid, ppv)) {
        return *ppv ? S_OK : E_NOINTERFACE;
    }else {
        FIXME("(%p)->(%s %p)\n", This, debugstr_mshtml_guid(riid), ppv);
        *ppv = NULL;
        return E_NOINTERFACE;
    }

    IUnknown_AddRef((IUnknown*)*ppv);
    return S_OK;
}

static ULONG WINAPI tokenlist_AddRef(IDOMTokenList *iface)
{
    tokenlist *This = impl_from_IDOMTokenList(iface);
    LONG ref = InterlockedIncrement(&This->ref);

    TRACE("(%p) ref=%d\n", This, ref);

    return ref;
}

static ULONG WINAPI tokenlist_Release(IDOMTokenList *iface)
{
    tokenlist *This = impl_from_IDOMTokenList(iface);
    LONG ref = InterlockedDecrement(&This->ref);

    TRACE("(%p) ref=%d\n", This, ref);

    if(!ref) {
        release_dispex(&This->dispex);
        heap_free(This);
    }

    return ref;
}

static HRESULT WINAPI tokenlist_GetTypeInfoCount(IDOMTokenList *iface, UINT *pctinfo)
{
    tokenlist *This = impl_from_IDOMTokenList(iface);
    FIXME("(%p)->(%p)\n", This, pctinfo);
    return E_NOTIMPL;
}

static HRESULT WINAPI tokenlist_GetTypeInfo(IDOMTokenList *iface, UINT iTInfo,
        LCID lcid, ITypeInfo **ppTInfo)
{
    tokenlist *This = impl_from_IDOMTokenList(iface);

    return IDispatchEx_GetTypeInfo(&This->dispex.IDispatchEx_iface, iTInfo, lcid, ppTInfo);
}

static HRESULT WINAPI tokenlist_GetIDsOfNames(IDOMTokenList *iface, REFIID riid,
        LPOLESTR *rgszNames, UINT cNames, LCID lcid, DISPID *rgDispId)
{
    tokenlist *This = impl_from_IDOMTokenList(iface);

    return IDispatchEx_GetIDsOfNames(&This->dispex.IDispatchEx_iface, riid, rgszNames, cNames,
            lcid, rgDispId);
}

static HRESULT WINAPI tokenlist_Invoke(IDOMTokenList *iface, DISPID dispIdMember,
        REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS *pDispParams,
        VARIANT *pVarResult, EXCEPINFO *pExcepInfo, UINT *puArgErr)
{
    tokenlist *This = impl_from_IDOMTokenList(iface);

    return IDispatchEx_Invoke(&This->dispex.IDispatchEx_iface, dispIdMember, riid, lcid, wFlags,
            pDispParams, pVarResult, pExcepInfo, puArgErr);
}

static const WCHAR *token_present(const WCHAR *list, const WCHAR *token, unsigned int token_len)
{
    const WCHAR *ptr, *next;

    if (!list || !token)
        return NULL;

    ptr = list;
    while (*ptr)
    {
        while (iswspace(*ptr))
            ++ptr;
        if (!*ptr)
            break;
        next = ptr + 1;
        while (*next && !iswspace(*next))
            ++next;

        if (next - ptr == token_len && !wcsncmp(ptr, token, token_len))
            return ptr;
        ptr = next;
    }
    return NULL;
}

static HRESULT WINAPI tokenlist_add(IDOMTokenList *iface, VARIANT *token)
{
    tokenlist *list = impl_from_IDOMTokenList(iface);
    const WCHAR *ptr, *next;
    unsigned int new_pos, alloc_len;
    WCHAR *old, *new;
    HRESULT hr;

    TRACE("iface %p, token %p (%s).\n", iface, token,
            token && V_VT(token) == VT_BSTR ? debugstr_w(V_BSTR(token)) : "");

    if (!list->element)
        return E_FAIL;

    if (V_VT(token) != VT_BSTR)
    {
        FIXME("Unexpected token type %u.\n", V_VT(token));
        return E_INVALIDARG;
    }

    if (FAILED(hr = IHTMLElement_get_className(list->element, &old)))
        return hr;

    TRACE("old %s.\n", debugstr_w(old));

    ptr = V_BSTR(token);
    new_pos = old ? lstrlenW(old) : 0;
    alloc_len = new_pos + lstrlenW(ptr) + 2;
    new = heap_alloc(sizeof(*new) * (new_pos + lstrlenW(ptr) + 2));
    memcpy(new, old, sizeof(*new) * new_pos);
    while (*ptr)
    {
        while (iswspace(*ptr))
            ++ptr;
        if (!*ptr)
            break;
        next = ptr + 1;
        while (*next && !iswspace(*next))
            ++next;

        if (!token_present(old, ptr, next - ptr))
        {
            if (new_pos)
                new[new_pos++] = L' ';
            memcpy(new + new_pos, ptr, sizeof(*new) * (next - ptr));
            new_pos += next - ptr;
            assert(new_pos < alloc_len);
        }
        ptr = next;
    }
    new[new_pos] = 0;

    SysFreeString(old);
    TRACE("new %s.\n", debugstr_w(new));

    hr = IHTMLElement_put_className(list->element, new);
    heap_free(new);
    return hr;
}

static HRESULT WINAPI tokenlist_remove(IDOMTokenList *iface, VARIANT *token)
{
    tokenlist *list = impl_from_IDOMTokenList(iface);
    const WCHAR *ptr, *next;
    unsigned int new_pos;
    WCHAR *old, *new;
    HRESULT hr;

    TRACE("iface %p, token %p (%s).\n", iface, token,
            token && V_VT(token) == VT_BSTR ? debugstr_w(V_BSTR(token)) : "");

    if (!list->element)
        return E_FAIL;

    if (V_VT(token) != VT_BSTR)
    {
        FIXME("Unexpected token type %u.\n", V_VT(token));
        return E_INVALIDARG;
    }

    if (FAILED(hr = IHTMLElement_get_className(list->element, &old)))
        return hr;

    if (!old)
        return S_OK;

    TRACE("old %s.\n", debugstr_w(old));

    ptr = old;
    new_pos = 0;
    new = heap_alloc(sizeof(*new) * (lstrlenW(old) + 1));
    while (*ptr)
    {
        while (iswspace(*ptr))
            ++ptr;
        if (!*ptr)
            break;
        next = ptr + 1;
        while (*next && !iswspace(*next))
            ++next;

        if (!token_present(V_BSTR(token), ptr, next - ptr))
        {
            if (new_pos)
                new[new_pos++] = L' ';
            memcpy(new + new_pos, ptr, sizeof(*new) * (next - ptr));
            new_pos += next - ptr;
        }
        ptr = next;
    }
    new[new_pos] = 0;

    SysFreeString(old);
    TRACE("new %s, new_pos %d\n", debugstr_w(new), new_pos);

    hr = IHTMLElement_put_className(list->element, new);
    heap_free(new);
    return hr;
}

static const IDOMTokenListVtbl DOMTokenListVtbl = {
    tokenlist_QueryInterface,
    tokenlist_AddRef,
    tokenlist_Release,
    tokenlist_GetTypeInfoCount,
    tokenlist_GetTypeInfo,
    tokenlist_GetIDsOfNames,
    tokenlist_Invoke,
    tokenlist_add,
    tokenlist_remove,
};

static const tid_t domtokenlist_iface_tids[] = {
    IDOMTokenList_tid,
    0
};
static dispex_static_data_t domtokenlist_dispex = {
    NULL,
    IDOMTokenList_tid,
    domtokenlist_iface_tids
};

static HRESULT create_domtokenlist(IHTMLElement *element, IDOMTokenList **ret)
{
    tokenlist *obj;

    obj = heap_alloc_zero(sizeof(*obj));
    if(!obj)
        return E_OUTOFMEMORY;

    obj->IDOMTokenList_iface.lpVtbl = &DOMTokenListVtbl;
    obj->ref = 1;
    obj->element = element;
    init_dispex(&obj->dispex, (IUnknown*)&obj->IDOMTokenList_iface, &domtokenlist_dispex);

    *ret = &obj->IDOMTokenList_iface;
    return S_OK;
}

HRESULT HTMLGenericElement_Create(HTMLDocumentNode *doc, nsIDOMElement *nselem, HTMLElement **elem)
{
    IDOMTokenList *class_list;
    VARIANT *class_list_prop;
    HTMLGenericElement *ret;

    ret = heap_alloc_zero(sizeof(HTMLGenericElement));
    if(!ret)
        return E_OUTOFMEMORY;

    ret->IHTMLGenericElement_iface.lpVtbl = &HTMLGenericElementVtbl;
    ret->element.node.vtbl = &HTMLGenericElementImplVtbl;

    HTMLElement_Init(&ret->element, doc, nselem, &HTMLGenericElement_dispex);

    if (FAILED(create_domtokenlist(&ret->element.IHTMLElement_iface, &class_list)))
    {
        ERR("Could not create class_list.\n");
    }
    else if (FAILED(dispex_get_dprop_ref(&ret->element.node.event_target.dispex, L"classList", TRUE, &class_list_prop)))
    {
        IDOMTokenList_Release(class_list);
        ERR("Could not add classList property.\n");
    }
    else
    {
        V_VT(class_list_prop) = VT_DISPATCH;
        V_DISPATCH(class_list_prop) = (IDispatch *)class_list;
    }
    *elem = &ret->element;
    return S_OK;
}
