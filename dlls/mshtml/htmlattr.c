/*
 * Copyright 2011 Jacek Caban for CodeWeavers
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

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(mshtml);

static inline HTMLDOMAttribute *impl_from_IHTMLDOMAttribute(IHTMLDOMAttribute *iface)
{
    return CONTAINING_RECORD(iface, HTMLDOMAttribute, IHTMLDOMAttribute_iface);
}

static HRESULT WINAPI HTMLDOMAttribute_QueryInterface(IHTMLDOMAttribute *iface,
                                                 REFIID riid, void **ppv)
{
    HTMLDOMAttribute *This = impl_from_IHTMLDOMAttribute(iface);

    if(This->node.nsnode)
        return IHTMLDOMNode_QueryInterface(&This->node.IHTMLDOMNode_iface, riid, ppv);

    TRACE("(%p)->(%s %p)\n", This, debugstr_mshtml_guid(riid), ppv);

    if(IsEqualGUID(&IID_IUnknown, riid)) {
        *ppv = &This->IHTMLDOMAttribute_iface;
    }else if(IsEqualGUID(&IID_IHTMLDOMAttribute, riid)) {
        *ppv = &This->IHTMLDOMAttribute_iface;
    }else if(IsEqualGUID(&IID_IHTMLDOMAttribute2, riid)) {
        *ppv = &This->IHTMLDOMAttribute2_iface;
    }else if(dispex_query_interface(&This->node.event_target.dispex, riid, ppv)) {
        return *ppv ? S_OK : E_NOINTERFACE;
    }else {
        WARN("%s not supported\n", debugstr_mshtml_guid(riid));
        *ppv =  NULL;
        return E_NOINTERFACE;
    }

    IUnknown_AddRef((IUnknown*)*ppv);
    return S_OK;
}

static ULONG WINAPI HTMLDOMAttribute_AddRef(IHTMLDOMAttribute *iface)
{
    HTMLDOMAttribute *This = impl_from_IHTMLDOMAttribute(iface);
    LONG ref;

    if(This->node.nsnode)
        return IHTMLDOMNode_AddRef(&This->node.IHTMLDOMNode_iface);
    ref = InterlockedIncrement(&This->ref);

    TRACE("(%p) ref=%ld\n", This, ref);

    return ref;
}

static ULONG WINAPI HTMLDOMAttribute_Release(IHTMLDOMAttribute *iface)
{
    HTMLDOMAttribute *This = impl_from_IHTMLDOMAttribute(iface);
    LONG ref;

    if(This->node.nsnode)
        return IHTMLDOMNode_Release(&This->node.IHTMLDOMNode_iface);
    ref = InterlockedDecrement(&This->ref);

    TRACE("(%p) ref=%ld\n", This, ref);

    if(!ref) {
        assert(!This->elem);
        release_dispex(&This->node.event_target.dispex);
        VariantClear(&This->value);
        free(This->name);
        free(This);
    }

    return ref;
}

static HRESULT WINAPI HTMLDOMAttribute_GetTypeInfoCount(IHTMLDOMAttribute *iface, UINT *pctinfo)
{
    HTMLDOMAttribute *This = impl_from_IHTMLDOMAttribute(iface);
    return IDispatchEx_GetTypeInfoCount(&This->node.event_target.dispex.IDispatchEx_iface, pctinfo);
}

static HRESULT WINAPI HTMLDOMAttribute_GetTypeInfo(IHTMLDOMAttribute *iface, UINT iTInfo,
                                              LCID lcid, ITypeInfo **ppTInfo)
{
    HTMLDOMAttribute *This = impl_from_IHTMLDOMAttribute(iface);
    return IDispatchEx_GetTypeInfo(&This->node.event_target.dispex.IDispatchEx_iface, iTInfo, lcid, ppTInfo);
}

static HRESULT WINAPI HTMLDOMAttribute_GetIDsOfNames(IHTMLDOMAttribute *iface, REFIID riid,
                                                LPOLESTR *rgszNames, UINT cNames,
                                                LCID lcid, DISPID *rgDispId)
{
    HTMLDOMAttribute *This = impl_from_IHTMLDOMAttribute(iface);
    return IDispatchEx_GetIDsOfNames(&This->node.event_target.dispex.IDispatchEx_iface, riid, rgszNames, cNames,
            lcid, rgDispId);
}

static HRESULT WINAPI HTMLDOMAttribute_Invoke(IHTMLDOMAttribute *iface, DISPID dispIdMember,
                            REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS *pDispParams,
                            VARIANT *pVarResult, EXCEPINFO *pExcepInfo, UINT *puArgErr)
{
    HTMLDOMAttribute *This = impl_from_IHTMLDOMAttribute(iface);
    return IDispatchEx_Invoke(&This->node.event_target.dispex.IDispatchEx_iface, dispIdMember, riid, lcid,
            wFlags, pDispParams, pVarResult, pExcepInfo, puArgErr);
}

static HRESULT WINAPI HTMLDOMAttribute_get_nodeName(IHTMLDOMAttribute *iface, BSTR *p)
{
    HTMLDOMAttribute *This = impl_from_IHTMLDOMAttribute(iface);

    if(This->node.nsnode)
        return IHTMLDOMNode_get_nodeName(&This->node.IHTMLDOMNode_iface, p);

    TRACE("(%p)->(%p)\n", This, p);

    if(!This->elem) {
        if(!This->name) {
            FIXME("No name available\n");
            return E_FAIL;
        }

        *p = SysAllocString(This->name);
        return *p ? S_OK : E_OUTOFMEMORY;
    }

    return IDispatchEx_GetMemberName(&This->elem->node.event_target.dispex.IDispatchEx_iface, This->dispid, p);
}

static HRESULT WINAPI HTMLDOMAttribute_put_nodeValue(IHTMLDOMAttribute *iface, VARIANT v)
{
    HTMLDOMAttribute *This = impl_from_IHTMLDOMAttribute(iface);
    DISPID dispidNamed = DISPID_PROPERTYPUT;
    DISPPARAMS dp = {&v, &dispidNamed, 1, 1};
    EXCEPINFO ei;
    VARIANT ret;

    if(This->node.nsnode)
        return IHTMLDOMNode_put_nodeValue(&This->node.IHTMLDOMNode_iface, v);

    TRACE("(%p)->(%s)\n", This, debugstr_variant(&v));

    if(!This->elem)
        return VariantCopy(&This->value, &v);

    memset(&ei, 0, sizeof(ei));

    return IDispatchEx_InvokeEx(&This->elem->node.event_target.dispex.IDispatchEx_iface, This->dispid, LOCALE_SYSTEM_DEFAULT,
            DISPATCH_PROPERTYPUT, &dp, &ret, &ei, NULL);
}

static HRESULT WINAPI HTMLDOMAttribute_get_nodeValue(IHTMLDOMAttribute *iface, VARIANT *p)
{
    HTMLDOMAttribute *This = impl_from_IHTMLDOMAttribute(iface);

    if(This->node.nsnode)
        return IHTMLDOMNode_get_nodeValue(&This->node.IHTMLDOMNode_iface, p);

    TRACE("(%p)->(%p)\n", This, p);

    if(!This->elem)
        return VariantCopy(p, &This->value);

    return get_elem_attr_value_by_dispid(This->elem, This->dispid, p);
}

static HRESULT WINAPI HTMLDOMAttribute_get_specified(IHTMLDOMAttribute *iface, VARIANT_BOOL *p)
{
    HTMLDOMAttribute *This = impl_from_IHTMLDOMAttribute(iface);
    nsIDOMAttr *nsattr;
    nsAString nsname;
    BSTR name;
    nsresult nsres;
    HRESULT hres;
    cpp_bool b;

    TRACE("(%p)->(%p)\n", This, p);

    if(This->node.nsnode) {
        nsres = nsIDOMAttr_GetSpecified((nsIDOMAttr*)This->node.nsnode, &b);
        *p = variant_bool(NS_SUCCEEDED(nsres) && b);
        return S_OK;
    }

    if(!This->elem || !This->elem->dom_element) {
        *p = VARIANT_FALSE;
        return S_OK;
    }

    if(!dispex_is_builtin_value(&This->elem->node.event_target.dispex, This->dispid)) {
        *p = VARIANT_TRUE;
        return S_OK;
    }

    hres = IDispatchEx_GetMemberName(&This->elem->node.event_target.dispex.IDispatchEx_iface, This->dispid, &name);
    if(FAILED(hres))
        return hres;

    /* FIXME: This is not exactly right, we have some attributes that don't map directly to Gecko attributes. */
    nsAString_InitDepend(&nsname, name);
    nsres = nsIDOMElement_GetAttributeNode(This->elem->dom_element, &nsname, &nsattr);
    nsAString_Finish(&nsname);
    SysFreeString(name);
    if(NS_FAILED(nsres))
        return E_FAIL;

    /* If the Gecko attribute node can be found, we know that the attribute is specified.
       There is no point in calling GetSpecified */
    if(nsattr) {
        nsIDOMAttr_Release(nsattr);
        *p = VARIANT_TRUE;
    }else {
        *p = VARIANT_FALSE;
    }
    return S_OK;
}

static const IHTMLDOMAttributeVtbl HTMLDOMAttributeVtbl = {
    HTMLDOMAttribute_QueryInterface,
    HTMLDOMAttribute_AddRef,
    HTMLDOMAttribute_Release,
    HTMLDOMAttribute_GetTypeInfoCount,
    HTMLDOMAttribute_GetTypeInfo,
    HTMLDOMAttribute_GetIDsOfNames,
    HTMLDOMAttribute_Invoke,
    HTMLDOMAttribute_get_nodeName,
    HTMLDOMAttribute_put_nodeValue,
    HTMLDOMAttribute_get_nodeValue,
    HTMLDOMAttribute_get_specified
};

static inline HTMLDOMAttribute *impl_from_IHTMLDOMAttribute2(IHTMLDOMAttribute2 *iface)
{
    return CONTAINING_RECORD(iface, HTMLDOMAttribute, IHTMLDOMAttribute2_iface);
}

static HRESULT WINAPI HTMLDOMAttribute2_QueryInterface(IHTMLDOMAttribute2 *iface, REFIID riid, void **ppv)
{
    HTMLDOMAttribute *This = impl_from_IHTMLDOMAttribute2(iface);
    return IHTMLDOMAttribute_QueryInterface(&This->IHTMLDOMAttribute_iface, riid, ppv);
}

static ULONG WINAPI HTMLDOMAttribute2_AddRef(IHTMLDOMAttribute2 *iface)
{
    HTMLDOMAttribute *This = impl_from_IHTMLDOMAttribute2(iface);
    return IHTMLDOMAttribute_AddRef(&This->IHTMLDOMAttribute_iface);
}

static ULONG WINAPI HTMLDOMAttribute2_Release(IHTMLDOMAttribute2 *iface)
{
    HTMLDOMAttribute *This = impl_from_IHTMLDOMAttribute2(iface);
    return IHTMLDOMAttribute_Release(&This->IHTMLDOMAttribute_iface);
}

static HRESULT WINAPI HTMLDOMAttribute2_GetTypeInfoCount(IHTMLDOMAttribute2 *iface, UINT *pctinfo)
{
    HTMLDOMAttribute *This = impl_from_IHTMLDOMAttribute2(iface);
    return IDispatchEx_GetTypeInfoCount(&This->node.event_target.dispex.IDispatchEx_iface, pctinfo);
}

static HRESULT WINAPI HTMLDOMAttribute2_GetTypeInfo(IHTMLDOMAttribute2 *iface, UINT iTInfo,
        LCID lcid, ITypeInfo **ppTInfo)
{
    HTMLDOMAttribute *This = impl_from_IHTMLDOMAttribute2(iface);
    return IDispatchEx_GetTypeInfo(&This->node.event_target.dispex.IDispatchEx_iface, iTInfo, lcid, ppTInfo);
}

static HRESULT WINAPI HTMLDOMAttribute2_GetIDsOfNames(IHTMLDOMAttribute2 *iface, REFIID riid,
        LPOLESTR *rgszNames, UINT cNames, LCID lcid, DISPID *rgDispId)
{
    HTMLDOMAttribute *This = impl_from_IHTMLDOMAttribute2(iface);
    return IDispatchEx_GetIDsOfNames(&This->node.event_target.dispex.IDispatchEx_iface, riid, rgszNames, cNames,
            lcid, rgDispId);
}

static HRESULT WINAPI HTMLDOMAttribute2_Invoke(IHTMLDOMAttribute2 *iface, DISPID dispIdMember,
        REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS *pDispParams,
        VARIANT *pVarResult, EXCEPINFO *pExcepInfo, UINT *puArgErr)
{
    HTMLDOMAttribute *This = impl_from_IHTMLDOMAttribute2(iface);
    return IDispatchEx_Invoke(&This->node.event_target.dispex.IDispatchEx_iface, dispIdMember, riid, lcid,
            wFlags, pDispParams, pVarResult, pExcepInfo, puArgErr);
}

static HRESULT WINAPI HTMLDOMAttribute2_get_name(IHTMLDOMAttribute2 *iface, BSTR *p)
{
    HTMLDOMAttribute *This = impl_from_IHTMLDOMAttribute2(iface);
    nsAString nsstr;
    nsresult nsres;

    TRACE("(%p)->(%p)\n", This, p);

    if(This->node.nsnode) {
        nsAString_InitDepend(&nsstr, NULL);
        nsres = nsIDOMAttr_GetName((nsIDOMAttr*)This->node.nsnode, &nsstr);
        return return_nsstr(nsres, &nsstr, p);
    }

    return IHTMLDOMAttribute_get_nodeName(&This->IHTMLDOMAttribute_iface, p);
}

static HRESULT WINAPI HTMLDOMAttribute2_put_value(IHTMLDOMAttribute2 *iface, BSTR v)
{
    HTMLDOMAttribute *This = impl_from_IHTMLDOMAttribute2(iface);
    nsAString nsstr;
    nsresult nsres;
    VARIANT var;

    TRACE("(%p)->(%s)\n", This, debugstr_w(v));

    if(This->node.nsnode) {
        nsAString_InitDepend(&nsstr, v);
        nsres = nsIDOMAttr_SetValue((nsIDOMAttr*)This->node.nsnode, &nsstr);
        nsAString_Finish(&nsstr);
        return NS_SUCCEEDED(nsres) ? S_OK : E_FAIL;
    }

    V_VT(&var) = VT_BSTR;
    V_BSTR(&var) = v;
    return IHTMLDOMAttribute_put_nodeValue(&This->IHTMLDOMAttribute_iface, var);
}

static HRESULT WINAPI HTMLDOMAttribute2_get_value(IHTMLDOMAttribute2 *iface, BSTR *p)
{
    HTMLDOMAttribute *This = impl_from_IHTMLDOMAttribute2(iface);
    nsAString nsstr;
    nsresult nsres;
    VARIANT val;
    HRESULT hres;

    TRACE("(%p)->(%p)\n", This, p);

    if(This->node.nsnode) {
        nsAString_InitDepend(&nsstr, NULL);
        nsres = nsIDOMAttr_GetValue((nsIDOMAttr*)This->node.nsnode, &nsstr);
        return return_nsstr(nsres, &nsstr, p);
    }

    V_VT(&val) = VT_EMPTY;
    if(This->elem)
        hres = get_elem_attr_value_by_dispid(This->elem, This->dispid, &val);
    else
        hres = VariantCopy(&val, &This->value);
    if(SUCCEEDED(hres))
        hres = attr_value_to_string(&val);
    if(FAILED(hres))
        return hres;

    assert(V_VT(&val) == VT_BSTR);
    *p = V_BSTR(&val);
    if(!*p && !(*p = SysAllocStringLen(NULL, 0)))
        return E_OUTOFMEMORY;
    return S_OK;
}

static HRESULT WINAPI HTMLDOMAttribute2_get_expando(IHTMLDOMAttribute2 *iface, VARIANT_BOOL *p)
{
    HTMLDOMAttribute *This = impl_from_IHTMLDOMAttribute2(iface);

    TRACE("(%p)->(%p)\n", This, p);

    *p = variant_bool(This->elem && !dispex_is_builtin_attribute(&This->elem->node.event_target.dispex, This->dispid));
    return S_OK;
}

static HRESULT WINAPI HTMLDOMAttribute2_get_nodeType(IHTMLDOMAttribute2 *iface, LONG *p)
{
    HTMLDOMAttribute *This = impl_from_IHTMLDOMAttribute2(iface);

    if(This->node.nsnode)
        return IHTMLDOMNode_get_nodeType(&This->node.IHTMLDOMNode_iface, p);

    FIXME("(%p)->(%p)\n", This, p);

    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDOMAttribute2_get_parentNode(IHTMLDOMAttribute2 *iface, IHTMLDOMNode **p)
{
    HTMLDOMAttribute *This = impl_from_IHTMLDOMAttribute2(iface);

    if(This->node.nsnode)
        return IHTMLDOMNode_get_parentNode(&This->node.IHTMLDOMNode_iface, p);

    TRACE("(%p)->(%p)\n", This, p);

    *p = NULL;
    return S_OK;
}

static HRESULT WINAPI HTMLDOMAttribute2_get_childNodes(IHTMLDOMAttribute2 *iface, IDispatch **p)
{
    HTMLDOMAttribute *This = impl_from_IHTMLDOMAttribute2(iface);

    if(This->node.nsnode)
        return IHTMLDOMNode_get_childNodes(&This->node.IHTMLDOMNode_iface, p);

    FIXME("(%p)->(%p)\n", This, p);

    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDOMAttribute2_get_firstChild(IHTMLDOMAttribute2 *iface, IHTMLDOMNode **p)
{
    HTMLDOMAttribute *This = impl_from_IHTMLDOMAttribute2(iface);

    if(This->node.nsnode)
        return IHTMLDOMNode_get_firstChild(&This->node.IHTMLDOMNode_iface, p);

    FIXME("(%p)->(%p)\n", This, p);

    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDOMAttribute2_get_lastChild(IHTMLDOMAttribute2 *iface, IHTMLDOMNode **p)
{
    HTMLDOMAttribute *This = impl_from_IHTMLDOMAttribute2(iface);

    if(This->node.nsnode)
        return IHTMLDOMNode_get_lastChild(&This->node.IHTMLDOMNode_iface, p);

    FIXME("(%p)->(%p)\n", This, p);

    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDOMAttribute2_get_previousSibling(IHTMLDOMAttribute2 *iface, IHTMLDOMNode **p)
{
    HTMLDOMAttribute *This = impl_from_IHTMLDOMAttribute2(iface);

    if(This->node.nsnode)
        return IHTMLDOMNode_get_previousSibling(&This->node.IHTMLDOMNode_iface, p);

    FIXME("(%p)->(%p)\n", This, p);

    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDOMAttribute2_get_nextSibling(IHTMLDOMAttribute2 *iface, IHTMLDOMNode **p)
{
    HTMLDOMAttribute *This = impl_from_IHTMLDOMAttribute2(iface);

    if(This->node.nsnode)
        return IHTMLDOMNode_get_nextSibling(&This->node.IHTMLDOMNode_iface, p);

    FIXME("(%p)->(%p)\n", This, p);

    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDOMAttribute2_get_attributes(IHTMLDOMAttribute2 *iface, IDispatch **p)
{
    HTMLDOMAttribute *This = impl_from_IHTMLDOMAttribute2(iface);

    if(This->node.nsnode)
        return IHTMLDOMNode_get_attributes(&This->node.IHTMLDOMNode_iface, p);

    FIXME("(%p)->(%p)\n", This, p);

    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDOMAttribute2_get_ownerDocument(IHTMLDOMAttribute2 *iface, IDispatch **p)
{
    HTMLDOMAttribute *This = impl_from_IHTMLDOMAttribute2(iface);

    if(This->node.nsnode)
        return IHTMLDOMNode2_get_ownerDocument(&This->node.IHTMLDOMNode2_iface, p);

    FIXME("(%p)->(%p)\n", This, p);

    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDOMAttribute2_insertBefore(IHTMLDOMAttribute2 *iface, IHTMLDOMNode *newChild,
        VARIANT refChild, IHTMLDOMNode **node)
{
    HTMLDOMAttribute *This = impl_from_IHTMLDOMAttribute2(iface);

    if(This->node.nsnode)
        return IHTMLDOMNode_insertBefore(&This->node.IHTMLDOMNode_iface, newChild, refChild, node);

    FIXME("(%p)->(%p %s %p)\n", This, newChild, debugstr_variant(&refChild), node);

    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDOMAttribute2_replaceChild(IHTMLDOMAttribute2 *iface, IHTMLDOMNode *newChild,
        IHTMLDOMNode *oldChild, IHTMLDOMNode **node)
{
    HTMLDOMAttribute *This = impl_from_IHTMLDOMAttribute2(iface);

    if(This->node.nsnode)
        return IHTMLDOMNode_replaceChild(&This->node.IHTMLDOMNode_iface, newChild, oldChild, node);

    FIXME("(%p)->(%p %p %p)\n", This, newChild, oldChild, node);

    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDOMAttribute2_removeChild(IHTMLDOMAttribute2 *iface, IHTMLDOMNode *oldChild,
        IHTMLDOMNode **node)
{
    HTMLDOMAttribute *This = impl_from_IHTMLDOMAttribute2(iface);

    if(This->node.nsnode)
        return IHTMLDOMNode_removeChild(&This->node.IHTMLDOMNode_iface, oldChild, node);

    FIXME("(%p)->(%p %p)\n", This, oldChild, node);

    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDOMAttribute2_appendChild(IHTMLDOMAttribute2 *iface, IHTMLDOMNode *newChild,
        IHTMLDOMNode **node)
{
    HTMLDOMAttribute *This = impl_from_IHTMLDOMAttribute2(iface);

    if(This->node.nsnode)
        return IHTMLDOMNode_appendChild(&This->node.IHTMLDOMNode_iface, newChild, node);

    FIXME("(%p)->(%p %p)\n", This, newChild, node);

    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDOMAttribute2_hasChildNodes(IHTMLDOMAttribute2 *iface, VARIANT_BOOL *fChildren)
{
    HTMLDOMAttribute *This = impl_from_IHTMLDOMAttribute2(iface);

    if(This->node.nsnode)
        return IHTMLDOMNode_hasChildNodes(&This->node.IHTMLDOMNode_iface, fChildren);

    FIXME("(%p)->(%p)\n", This, fChildren);

    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDOMAttribute2_cloneNode(IHTMLDOMAttribute2 *iface, VARIANT_BOOL fDeep,
        IHTMLDOMAttribute **clonedNode)
{
    HTMLDOMAttribute *This = impl_from_IHTMLDOMAttribute2(iface);
    HRESULT hres;

    if(This->node.nsnode) {
        IHTMLDOMNode *cloned;
        hres = IHTMLDOMNode_cloneNode(&This->node.IHTMLDOMNode_iface, fDeep, &cloned);
        if(SUCCEEDED(hres)) {
            hres = IHTMLDOMNode_QueryInterface(cloned, &IID_IHTMLDOMAttribute, (void**)clonedNode);
            IHTMLDOMNode_Release(cloned);
        }
        return hres;
    }

    FIXME("(%p)->(%x %p)\n", This, fDeep, clonedNode);

    return E_NOTIMPL;
}

static const IHTMLDOMAttribute2Vtbl HTMLDOMAttribute2Vtbl = {
    HTMLDOMAttribute2_QueryInterface,
    HTMLDOMAttribute2_AddRef,
    HTMLDOMAttribute2_Release,
    HTMLDOMAttribute2_GetTypeInfoCount,
    HTMLDOMAttribute2_GetTypeInfo,
    HTMLDOMAttribute2_GetIDsOfNames,
    HTMLDOMAttribute2_Invoke,
    HTMLDOMAttribute2_get_name,
    HTMLDOMAttribute2_put_value,
    HTMLDOMAttribute2_get_value,
    HTMLDOMAttribute2_get_expando,
    HTMLDOMAttribute2_get_nodeType,
    HTMLDOMAttribute2_get_parentNode,
    HTMLDOMAttribute2_get_childNodes,
    HTMLDOMAttribute2_get_firstChild,
    HTMLDOMAttribute2_get_lastChild,
    HTMLDOMAttribute2_get_previousSibling,
    HTMLDOMAttribute2_get_nextSibling,
    HTMLDOMAttribute2_get_attributes,
    HTMLDOMAttribute2_get_ownerDocument,
    HTMLDOMAttribute2_insertBefore,
    HTMLDOMAttribute2_replaceChild,
    HTMLDOMAttribute2_removeChild,
    HTMLDOMAttribute2_appendChild,
    HTMLDOMAttribute2_hasChildNodes,
    HTMLDOMAttribute2_cloneNode
};

static inline HTMLDOMAttribute *impl_from_HTMLDOMNode(HTMLDOMNode *iface)
{
    return CONTAINING_RECORD(iface, HTMLDOMAttribute, node);
}

static HRESULT HTMLDOMAttribute_QI(HTMLDOMNode *iface, REFIID riid, void **ppv)
{
    HTMLDOMAttribute *This = impl_from_HTMLDOMNode(iface);

    *ppv =  NULL;

    if(IsEqualGUID(&IID_IUnknown, riid) || IsEqualGUID(&IID_IHTMLDOMAttribute, riid)) {
        *ppv = &This->IHTMLDOMAttribute_iface;
    }else if(IsEqualGUID(&IID_IHTMLDOMAttribute2, riid)) {
        *ppv = &This->IHTMLDOMAttribute2_iface;
    }else {
        return HTMLDOMNode_QI(&This->node, riid, ppv);
    }

    IUnknown_AddRef((IUnknown*)*ppv);
    return S_OK;
}

static HRESULT HTMLDOMAttribute_clone(HTMLDOMNode *iface, nsIDOMNode *nsnode, HTMLDOMNode **ret)
{
    HTMLDOMAttribute *This = impl_from_HTMLDOMNode(iface);
    HTMLDOMAttribute *new_attr;
    nsIDOMAttr *nsattr;
    nsresult nsres;
    HRESULT hres;

    nsres = nsIDOMNode_QueryInterface(nsnode, &IID_nsIDOMAttr, (void**)&nsattr);
    if(NS_FAILED(nsres)) {
        ERR("no nsIDOMAttr iface\n");
        return E_FAIL;
    }

    hres = HTMLDOMAttribute_Create(NULL, This->node.doc, This->elem, This->dispid, nsattr,
                                   dispex_compat_mode(&This->node.event_target.dispex), &new_attr);
    nsIDOMAttr_Release(nsattr);
    if(FAILED(hres))
        return hres;

    *ret = &new_attr->node;
    return S_OK;
}

static const cpc_entry_t HTMLDOMAttribute_cpc[] = {{NULL}};

static const NodeImplVtbl HTMLDOMAttributeImplVtbl = {
    .qi                    = HTMLDOMAttribute_QI,
    .destructor            = HTMLDOMNode_destructor,
    .cpc_entries           = HTMLDOMAttribute_cpc,
    .clone                 = HTMLDOMAttribute_clone
};

static void HTMLDOMAttribute_init_dispex_info(dispex_data_t *info, compat_mode_t mode)
{
    HTMLDOMNode_init_dispex_info(info, mode);

    if(mode >= COMPAT_MODE_IE9) {
        dispex_info_add_interface(info, IHTMLDOMNode_tid, NULL);
        dispex_info_add_interface(info, IHTMLDOMNode2_tid, NULL);
    }
}

static const tid_t HTMLDOMAttribute_iface_tids[] = {
    IHTMLDOMAttribute_tid,
    IHTMLDOMAttribute2_tid,
    0
};
dispex_static_data_t HTMLDOMAttribute_dispex = {
    "Attr",
    NULL,
    PROTO_ID_HTMLDOMAttribute,
    DispHTMLDOMAttribute_tid,
    HTMLDOMAttribute_iface_tids,
    HTMLDOMAttribute_init_dispex_info
};

HTMLDOMAttribute *unsafe_impl_from_IHTMLDOMAttribute(IHTMLDOMAttribute *iface)
{
    return iface->lpVtbl == &HTMLDOMAttributeVtbl ? impl_from_IHTMLDOMAttribute(iface) : NULL;
}

HRESULT HTMLDOMAttribute_Create(const WCHAR *name, HTMLDocumentNode *doc, HTMLElement *elem, DISPID dispid,
        nsIDOMAttr *nsattr, compat_mode_t compat_mode, HTMLDOMAttribute **attr)
{
    HTMLAttributeCollection *col;
    HTMLDOMAttribute *ret;
    HRESULT hres;

    ret = calloc(1, sizeof(*ret));
    if(!ret)
        return E_OUTOFMEMORY;

    ret->IHTMLDOMAttribute_iface.lpVtbl = &HTMLDOMAttributeVtbl;
    ret->IHTMLDOMAttribute2_iface.lpVtbl = &HTMLDOMAttribute2Vtbl;
    ret->ref = 1;
    ret->dispid = dispid;
    ret->elem = elem;

    /* For attributes attached to an element, (elem,dispid) pair should
       be valid used for its operation if we don't have a proper node. */
    if(elem) {
        hres = HTMLElement_get_attr_col(&elem->node, &col);
        if(FAILED(hres)) {
            free(ret);
            return hres;
        }
        IHTMLAttributeCollection_Release(&col->IHTMLAttributeCollection_iface);

        list_add_tail(&elem->attrs->attrs, &ret->entry);
    }

    /* If we have a nsattr, use proper node */
    if(nsattr) {
        ret->node.vtbl = &HTMLDOMAttributeImplVtbl;
        HTMLDOMNode_Init(doc, &ret->node, (nsIDOMNode*)nsattr, &HTMLDOMAttribute_dispex);
        *attr = ret;
        return S_OK;
    }

    init_dispatch(&ret->node.event_target.dispex, (IUnknown*)&ret->IHTMLDOMAttribute_iface,
                  &HTMLDOMAttribute_dispex, get_inner_window(doc), compat_mode);

    /* For detached attributes we may still do most operations if we have its name available. */
    if(name) {
        ret->name = wcsdup(name);
        if(!ret->name) {
            IHTMLDOMAttribute_Release(&ret->IHTMLDOMAttribute_iface);
            return E_OUTOFMEMORY;
        }
    }

    *attr = ret;
    return S_OK;
}
