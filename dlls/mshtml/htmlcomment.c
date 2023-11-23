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

#define COBJMACROS

#include "windef.h"
#include "winbase.h"
#include "winuser.h"
#include "ole2.h"

#include "mshtml_private.h"
#include "htmlevent.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(mshtml);

struct HTMLCommentElement {
    HTMLElement element;
    IHTMLCommentElement IHTMLCommentElement_iface;
    IHTMLDOMTextNode IHTMLDOMTextNode_iface;
    IHTMLDOMTextNode2 IHTMLDOMTextNode2_iface;
};

static inline HTMLCommentElement *impl_from_IHTMLCommentElement(IHTMLCommentElement *iface)
{
    return CONTAINING_RECORD(iface, HTMLCommentElement, IHTMLCommentElement_iface);
}

static HRESULT WINAPI HTMLCommentElement_QueryInterface(IHTMLCommentElement *iface,
        REFIID riid, void **ppv)
{
    HTMLCommentElement *This = impl_from_IHTMLCommentElement(iface);

    return IHTMLDOMNode_QueryInterface(&This->element.node.IHTMLDOMNode_iface, riid, ppv);
}

static ULONG WINAPI HTMLCommentElement_AddRef(IHTMLCommentElement *iface)
{
    HTMLCommentElement *This = impl_from_IHTMLCommentElement(iface);

    return IHTMLDOMNode_AddRef(&This->element.node.IHTMLDOMNode_iface);
}

static ULONG WINAPI HTMLCommentElement_Release(IHTMLCommentElement *iface)
{
    HTMLCommentElement *This = impl_from_IHTMLCommentElement(iface);

    return IHTMLDOMNode_Release(&This->element.node.IHTMLDOMNode_iface);
}

static HRESULT WINAPI HTMLCommentElement_GetTypeInfoCount(IHTMLCommentElement *iface, UINT *pctinfo)
{
    HTMLCommentElement *This = impl_from_IHTMLCommentElement(iface);
    return IDispatchEx_GetTypeInfoCount(&This->element.node.event_target.dispex.IDispatchEx_iface, pctinfo);
}

static HRESULT WINAPI HTMLCommentElement_GetTypeInfo(IHTMLCommentElement *iface, UINT iTInfo,
        LCID lcid, ITypeInfo **ppTInfo)
{
    HTMLCommentElement *This = impl_from_IHTMLCommentElement(iface);
    return IDispatchEx_GetTypeInfo(&This->element.node.event_target.dispex.IDispatchEx_iface, iTInfo, lcid,
            ppTInfo);
}

static HRESULT WINAPI HTMLCommentElement_GetIDsOfNames(IHTMLCommentElement *iface, REFIID riid,
                                                LPOLESTR *rgszNames, UINT cNames,
                                                LCID lcid, DISPID *rgDispId)
{
    HTMLCommentElement *This = impl_from_IHTMLCommentElement(iface);
    return IDispatchEx_GetIDsOfNames(&This->element.node.event_target.dispex.IDispatchEx_iface, riid, rgszNames,
            cNames, lcid, rgDispId);
}

static HRESULT WINAPI HTMLCommentElement_Invoke(IHTMLCommentElement *iface, DISPID dispIdMember,
                            REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS *pDispParams,
                            VARIANT *pVarResult, EXCEPINFO *pExcepInfo, UINT *puArgErr)
{
    HTMLCommentElement *This = impl_from_IHTMLCommentElement(iface);
    return IDispatchEx_Invoke(&This->element.node.event_target.dispex.IDispatchEx_iface, dispIdMember, riid,
            lcid, wFlags, pDispParams, pVarResult, pExcepInfo, puArgErr);
}

static HRESULT WINAPI HTMLCommentElement_put_text(IHTMLCommentElement *iface, BSTR v)
{
    HTMLCommentElement *This = impl_from_IHTMLCommentElement(iface);
    FIXME("(%p)->(%s)\n", This, debugstr_w(v));
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCommentElement_get_text(IHTMLCommentElement *iface, BSTR *p)
{
    HTMLCommentElement *This = impl_from_IHTMLCommentElement(iface);

    TRACE("(%p)->(%p)\n", This, p);

    return IHTMLElement_get_outerHTML(&This->element.IHTMLElement_iface, p);
}

static HRESULT WINAPI HTMLCommentElement_put_atomic(IHTMLCommentElement *iface, LONG v)
{
    HTMLCommentElement *This = impl_from_IHTMLCommentElement(iface);
    FIXME("(%p)->(%ld)\n", This, v);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCommentElement_get_atomic(IHTMLCommentElement *iface, LONG *p)
{
    HTMLCommentElement *This = impl_from_IHTMLCommentElement(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static const IHTMLCommentElementVtbl HTMLCommentElementVtbl = {
    HTMLCommentElement_QueryInterface,
    HTMLCommentElement_AddRef,
    HTMLCommentElement_Release,
    HTMLCommentElement_GetTypeInfoCount,
    HTMLCommentElement_GetTypeInfo,
    HTMLCommentElement_GetIDsOfNames,
    HTMLCommentElement_Invoke,
    HTMLCommentElement_put_text,
    HTMLCommentElement_get_text,
    HTMLCommentElement_put_atomic,
    HTMLCommentElement_get_atomic
};

static inline HTMLCommentElement *impl_from_IHTMLDOMTextNode(IHTMLDOMTextNode *iface)
{
    return CONTAINING_RECORD(iface, HTMLCommentElement, IHTMLDOMTextNode_iface);
}

static HRESULT WINAPI HTMLCommentElement_TextNode_QueryInterface(IHTMLDOMTextNode *iface,
                                                 REFIID riid, void **ppv)
{
    HTMLCommentElement *This = impl_from_IHTMLDOMTextNode(iface);

    return IHTMLDOMNode_QueryInterface(&This->element.node.IHTMLDOMNode_iface, riid, ppv);
}

static ULONG WINAPI HTMLCommentElement_TextNode_AddRef(IHTMLDOMTextNode *iface)
{
    HTMLCommentElement *This = impl_from_IHTMLDOMTextNode(iface);

    return IHTMLDOMNode_AddRef(&This->element.node.IHTMLDOMNode_iface);
}

static ULONG WINAPI HTMLCommentElement_TextNode_Release(IHTMLDOMTextNode *iface)
{
    HTMLCommentElement *This = impl_from_IHTMLDOMTextNode(iface);

    return IHTMLDOMNode_Release(&This->element.node.IHTMLDOMNode_iface);
}

static HRESULT WINAPI HTMLCommentElement_TextNode_GetTypeInfoCount(IHTMLDOMTextNode *iface, UINT *pctinfo)
{
    HTMLCommentElement *This = impl_from_IHTMLDOMTextNode(iface);
    return IDispatchEx_GetTypeInfoCount(&This->element.node.event_target.dispex.IDispatchEx_iface, pctinfo);
}

static HRESULT WINAPI HTMLCommentElement_TextNode_GetTypeInfo(IHTMLDOMTextNode *iface, UINT iTInfo,
                                              LCID lcid, ITypeInfo **ppTInfo)
{
    HTMLCommentElement *This = impl_from_IHTMLDOMTextNode(iface);
    return IDispatchEx_GetTypeInfo(&This->element.node.event_target.dispex.IDispatchEx_iface, iTInfo, lcid, ppTInfo);
}

static HRESULT WINAPI HTMLCommentElement_TextNode_GetIDsOfNames(IHTMLDOMTextNode *iface, REFIID riid,
                                                LPOLESTR *rgszNames, UINT cNames,
                                                LCID lcid, DISPID *rgDispId)
{
    HTMLCommentElement *This = impl_from_IHTMLDOMTextNode(iface);
    return IDispatchEx_GetIDsOfNames(&This->element.node.event_target.dispex.IDispatchEx_iface, riid, rgszNames, cNames,
            lcid, rgDispId);
}

static HRESULT WINAPI HTMLCommentElement_TextNode_Invoke(IHTMLDOMTextNode *iface, DISPID dispIdMember,
                            REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS *pDispParams,
                            VARIANT *pVarResult, EXCEPINFO *pExcepInfo, UINT *puArgErr)
{
    HTMLCommentElement *This = impl_from_IHTMLDOMTextNode(iface);
    return IDispatchEx_Invoke(&This->element.node.event_target.dispex.IDispatchEx_iface, dispIdMember, riid, lcid,
            wFlags, pDispParams, pVarResult, pExcepInfo, puArgErr);
}

static HRESULT WINAPI HTMLCommentElement_TextNode_put_data(IHTMLDOMTextNode *iface, BSTR v)
{
    HTMLCommentElement *This = impl_from_IHTMLDOMTextNode(iface);
    FIXME("(%p)->(%s)\n", This, debugstr_w(v));
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCommentElement_TextNode_get_data(IHTMLDOMTextNode *iface, BSTR *p)
{
    HTMLCommentElement *This = impl_from_IHTMLDOMTextNode(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCommentElement_TextNode_toString(IHTMLDOMTextNode *iface, BSTR *String)
{
    HTMLCommentElement *This = impl_from_IHTMLDOMTextNode(iface);
    WARN("(%p)->(%p)\n", This, String);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCommentElement_TextNode_get_length(IHTMLDOMTextNode *iface, LONG *p)
{
    HTMLCommentElement *This = impl_from_IHTMLDOMTextNode(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCommentElement_TextNode_splitText(IHTMLDOMTextNode *iface, LONG offset, IHTMLDOMNode **pRetNode)
{
    HTMLCommentElement *This = impl_from_IHTMLDOMTextNode(iface);
    WARN("(%p)->(%ld %p)\n", This, offset, pRetNode);
    return E_UNEXPECTED;
}

static const IHTMLDOMTextNodeVtbl HTMLCommentElement_TextNodeVtbl = {
    HTMLCommentElement_TextNode_QueryInterface,
    HTMLCommentElement_TextNode_AddRef,
    HTMLCommentElement_TextNode_Release,
    HTMLCommentElement_TextNode_GetTypeInfoCount,
    HTMLCommentElement_TextNode_GetTypeInfo,
    HTMLCommentElement_TextNode_GetIDsOfNames,
    HTMLCommentElement_TextNode_Invoke,
    HTMLCommentElement_TextNode_put_data,
    HTMLCommentElement_TextNode_get_data,
    HTMLCommentElement_TextNode_toString,
    HTMLCommentElement_TextNode_get_length,
    HTMLCommentElement_TextNode_splitText
};

static inline HTMLCommentElement *impl_from_IHTMLDOMTextNode2(IHTMLDOMTextNode2 *iface)
{
    return CONTAINING_RECORD(iface, HTMLCommentElement, IHTMLDOMTextNode2_iface);
}

static HRESULT WINAPI HTMLCommentElement_TextNode2_QueryInterface(IHTMLDOMTextNode2 *iface, REFIID riid, void **ppv)
{
    HTMLCommentElement *This = impl_from_IHTMLDOMTextNode2(iface);

    return IHTMLDOMNode_QueryInterface(&This->element.node.IHTMLDOMNode_iface, riid, ppv);
}

static ULONG WINAPI HTMLCommentElement_TextNode2_AddRef(IHTMLDOMTextNode2 *iface)
{
    HTMLCommentElement *This = impl_from_IHTMLDOMTextNode2(iface);

    return IHTMLDOMNode_AddRef(&This->element.node.IHTMLDOMNode_iface);
}

static ULONG WINAPI HTMLCommentElement_TextNode2_Release(IHTMLDOMTextNode2 *iface)
{
    HTMLCommentElement *This = impl_from_IHTMLDOMTextNode2(iface);

    return IHTMLDOMNode_Release(&This->element.node.IHTMLDOMNode_iface);
}

static HRESULT WINAPI HTMLCommentElement_TextNode2_GetTypeInfoCount(IHTMLDOMTextNode2 *iface, UINT *pctinfo)
{
    HTMLCommentElement *This = impl_from_IHTMLDOMTextNode2(iface);
    return IDispatchEx_GetTypeInfoCount(&This->element.node.event_target.dispex.IDispatchEx_iface, pctinfo);
}

static HRESULT WINAPI HTMLCommentElement_TextNode2_GetTypeInfo(IHTMLDOMTextNode2 *iface, UINT iTInfo,
        LCID lcid, ITypeInfo **ppTInfo)
{
    HTMLCommentElement *This = impl_from_IHTMLDOMTextNode2(iface);
    return IDispatchEx_GetTypeInfo(&This->element.node.event_target.dispex.IDispatchEx_iface, iTInfo, lcid, ppTInfo);
}

static HRESULT WINAPI HTMLCommentElement_TextNode2_GetIDsOfNames(IHTMLDOMTextNode2 *iface, REFIID riid,
        LPOLESTR *rgszNames, UINT cNames, LCID lcid, DISPID *rgDispId)
{
    HTMLCommentElement *This = impl_from_IHTMLDOMTextNode2(iface);
    return IDispatchEx_GetIDsOfNames(&This->element.node.event_target.dispex.IDispatchEx_iface, riid, rgszNames, cNames,
            lcid, rgDispId);
}

static HRESULT WINAPI HTMLCommentElement_TextNode2_Invoke(IHTMLDOMTextNode2 *iface, DISPID dispIdMember,
        REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS *pDispParams, VARIANT *pVarResult,
        EXCEPINFO *pExcepInfo, UINT *puArgErr)
{
    HTMLCommentElement *This = impl_from_IHTMLDOMTextNode2(iface);
    return IDispatchEx_Invoke(&This->element.node.event_target.dispex.IDispatchEx_iface, dispIdMember, riid, lcid,
            wFlags, pDispParams, pVarResult, pExcepInfo, puArgErr);
}

static HRESULT WINAPI HTMLCommentElement_TextNode2_substringData(IHTMLDOMTextNode2 *iface, LONG offset, LONG count, BSTR *string)
{
    HTMLCommentElement *This = impl_from_IHTMLDOMTextNode2(iface);
    FIXME("(%p)->(%ld %ld %p)\n", This, offset, count, string);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCommentElement_TextNode2_appendData(IHTMLDOMTextNode2 *iface, BSTR string)
{
    HTMLCommentElement *This = impl_from_IHTMLDOMTextNode2(iface);
    FIXME("(%p)->(%s)\n", This, debugstr_w(string));
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCommentElement_TextNode2_insertData(IHTMLDOMTextNode2 *iface, LONG offset, BSTR string)
{
    HTMLCommentElement *This = impl_from_IHTMLDOMTextNode2(iface);
    FIXME("(%p)->(%ld %s)\n", This, offset, debugstr_w(string));
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCommentElement_TextNode2_deleteData(IHTMLDOMTextNode2 *iface, LONG offset, LONG count)
{
    HTMLCommentElement *This = impl_from_IHTMLDOMTextNode2(iface);
    FIXME("(%p)->(%ld %ld)\n", This, offset, count);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCommentElement_TextNode2_replaceData(IHTMLDOMTextNode2 *iface, LONG offset, LONG count, BSTR string)
{
    HTMLCommentElement *This = impl_from_IHTMLDOMTextNode2(iface);
    FIXME("(%p)->(%ld %ld %s)\n", This, offset, count, debugstr_w(string));
    return E_NOTIMPL;
}

static const IHTMLDOMTextNode2Vtbl HTMLCommentElement_TextNode2Vtbl = {
    HTMLCommentElement_TextNode2_QueryInterface,
    HTMLCommentElement_TextNode2_AddRef,
    HTMLCommentElement_TextNode2_Release,
    HTMLCommentElement_TextNode2_GetTypeInfoCount,
    HTMLCommentElement_TextNode2_GetTypeInfo,
    HTMLCommentElement_TextNode2_GetIDsOfNames,
    HTMLCommentElement_TextNode2_Invoke,
    HTMLCommentElement_TextNode2_substringData,
    HTMLCommentElement_TextNode2_appendData,
    HTMLCommentElement_TextNode2_insertData,
    HTMLCommentElement_TextNode2_deleteData,
    HTMLCommentElement_TextNode2_replaceData
};

static inline HTMLCommentElement *impl_from_HTMLDOMNode(HTMLDOMNode *iface)
{
    return CONTAINING_RECORD(iface, HTMLCommentElement, element.node);
}

static HRESULT HTMLCommentElement_clone(HTMLDOMNode *iface, nsIDOMNode *nsnode, HTMLDOMNode **ret)
{
    HTMLCommentElement *This = impl_from_HTMLDOMNode(iface);
    HTMLElement *new_elem;
    HRESULT hres;

    hres = HTMLCommentElement_Create(This->element.node.doc, nsnode, &new_elem);
    if(FAILED(hres))
        return hres;

    *ret = &new_elem->node;
    return S_OK;
}

static inline HTMLCommentElement *impl_from_DispatchEx(DispatchEx *iface)
{
    return CONTAINING_RECORD(iface, HTMLCommentElement, element.node.event_target.dispex);
}

static void *HTMLCommentElement_query_interface(DispatchEx *dispex, REFIID riid)
{
    HTMLCommentElement *This = impl_from_DispatchEx(dispex);

    if(IsEqualGUID(&IID_IHTMLCommentElement, riid))
        return &This->IHTMLCommentElement_iface;
    if(IsEqualGUID(&IID_IHTMLDOMTextNode, riid))
        return &This->IHTMLDOMTextNode_iface;
    if(IsEqualGUID(&IID_IHTMLDOMTextNode2, riid))
        return &This->IHTMLDOMTextNode2_iface;

    return HTMLElement_query_interface(&This->element.node.event_target.dispex, riid);
}

static const NodeImplVtbl HTMLCommentElementImplVtbl = {
    .clsid                 = &CLSID_HTMLCommentElement,
    .cpc_entries           = HTMLElement_cpc,
    .clone                 = HTMLCommentElement_clone,
    .get_attr_col          = HTMLElement_get_attr_col
};

static const event_target_vtbl_t HTMLCommentElement_event_target_vtbl = {
    {
        HTMLELEMENT_DISPEX_VTBL_ENTRIES,
        .query_interface= HTMLCommentElement_query_interface,
        .destructor     = HTMLElement_destructor,
        .traverse       = HTMLElement_traverse,
        .unlink         = HTMLElement_unlink
    },
    HTMLELEMENT_EVENT_TARGET_VTBL_ENTRIES,
    .handle_event       = HTMLElement_handle_event
};

static const tid_t HTMLCommentElement_iface_tids[] = {
    HTMLELEMENT_TIDS,
    IHTMLCommentElement_tid,
    0
};
dispex_static_data_t HTMLCommentElement_dispex = {
    "Comment",
    &HTMLCommentElement_event_target_vtbl.dispex_vtbl,
    PROTO_ID_HTMLCommentElement,
    DispHTMLCommentElement_tid,
    HTMLCommentElement_iface_tids,
    HTMLElement_init_dispex_info
};

HRESULT HTMLCommentElement_Create(HTMLDocumentNode *doc, nsIDOMNode *nsnode, HTMLElement **elem)
{
    HTMLCommentElement *ret;

    ret = calloc(1, sizeof(*ret));
    if(!ret)
        return E_OUTOFMEMORY;

    ret->element.node.vtbl = &HTMLCommentElementImplVtbl;
    ret->IHTMLCommentElement_iface.lpVtbl = &HTMLCommentElementVtbl;
    ret->IHTMLDOMTextNode_iface.lpVtbl = &HTMLCommentElement_TextNodeVtbl;
    ret->IHTMLDOMTextNode2_iface.lpVtbl = &HTMLCommentElement_TextNode2Vtbl;

    HTMLElement_Init(&ret->element, doc, NULL, &HTMLCommentElement_dispex);
    HTMLDOMNode_Init(doc, &ret->element.node, nsnode, &HTMLCommentElement_dispex);

    *elem = &ret->element;
    return S_OK;
}
