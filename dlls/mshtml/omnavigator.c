/*
 * Copyright 2008,2009 Jacek Caban for CodeWeavers
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
#include "mshtmdid.h"

#include "wine/debug.h"

#include "mshtml_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(mshtml);

static document_type_t document_type_from_content_type(const WCHAR *content_type)
{
    static const struct {
        const WCHAR *content_type;
        document_type_t doc_type;
    } table[] = {
        { L"application/xhtml+xml", DOCTYPE_XHTML },
        { L"application/xml",       DOCTYPE_XML },
        { L"image/svg+xml",         DOCTYPE_SVG },
        { L"text/html",             DOCTYPE_HTML },
        { L"text/xml",              DOCTYPE_XML },
    };
    unsigned int i, a = 0, b = ARRAY_SIZE(table);
    int c;

    while(a < b) {
        i = (a + b) / 2;
        c = wcsicmp(table[i].content_type, content_type);
        if(!c) return table[i].doc_type;
        if(c > 0) b = i;
        else      a = i + 1;
    }
    return DOCTYPE_INVALID;
}

typedef struct HTMLPluginsCollection HTMLPluginsCollection;
typedef struct HTMLMimeTypesCollection HTMLMimeTypesCollection;

typedef struct {
    DispatchEx dispex;
    IOmNavigator IOmNavigator_iface;

    LONG ref;

    HTMLInnerWindow *window;
    HTMLPluginsCollection *plugins;
    HTMLMimeTypesCollection *mime_types;
} OmNavigator;

typedef struct {
    DispatchEx dispex;
    IHTMLDOMImplementation IHTMLDOMImplementation_iface;
    IHTMLDOMImplementation2 IHTMLDOMImplementation2_iface;

    LONG ref;

    nsIDOMDOMImplementation *implementation;
    GeckoBrowser *browser;
} HTMLDOMImplementation;

static inline HTMLDOMImplementation *impl_from_IHTMLDOMImplementation(IHTMLDOMImplementation *iface)
{
    return CONTAINING_RECORD(iface, HTMLDOMImplementation, IHTMLDOMImplementation_iface);
}

static HRESULT WINAPI HTMLDOMImplementation_QueryInterface(IHTMLDOMImplementation *iface, REFIID riid, void **ppv)
{
    HTMLDOMImplementation *This = impl_from_IHTMLDOMImplementation(iface);

    TRACE("(%p)->(%s %p)\n", This, debugstr_mshtml_guid(riid), ppv);

    if(IsEqualGUID(&IID_IUnknown, riid) || IsEqualGUID(&IID_IHTMLDOMImplementation, riid)) {
        *ppv = &This->IHTMLDOMImplementation_iface;
    }else if(IsEqualGUID(&IID_IHTMLDOMImplementation2, riid)) {
        *ppv = &This->IHTMLDOMImplementation2_iface;
    }else if(dispex_query_interface(&This->dispex, riid, ppv)) {
        return *ppv ? S_OK : E_NOINTERFACE;
    }else {
        WARN("Unsupported interface %s\n", debugstr_mshtml_guid(riid));
        *ppv = NULL;
        return E_NOINTERFACE;
    }

    IUnknown_AddRef((IUnknown*)*ppv);
    return S_OK;
}

static ULONG WINAPI HTMLDOMImplementation_AddRef(IHTMLDOMImplementation *iface)
{
    HTMLDOMImplementation *This = impl_from_IHTMLDOMImplementation(iface);
    LONG ref = InterlockedIncrement(&This->ref);

    TRACE("(%p) ref=%ld\n", This, ref);

    return ref;
}

static ULONG WINAPI HTMLDOMImplementation_Release(IHTMLDOMImplementation *iface)
{
    HTMLDOMImplementation *This = impl_from_IHTMLDOMImplementation(iface);
    LONG ref = InterlockedDecrement(&This->ref);

    TRACE("(%p) ref=%ld\n", This, ref);

    if(!ref) {
        assert(!This->browser);
        if(This->implementation)
            nsIDOMDOMImplementation_Release(This->implementation);
        release_dispex(&This->dispex);
        free(This);
    }

    return ref;
}

static HRESULT WINAPI HTMLDOMImplementation_GetTypeInfoCount(IHTMLDOMImplementation *iface, UINT *pctinfo)
{
    HTMLDOMImplementation *This = impl_from_IHTMLDOMImplementation(iface);

    return IDispatchEx_GetTypeInfoCount(&This->dispex.IDispatchEx_iface, pctinfo);
}

static HRESULT WINAPI HTMLDOMImplementation_GetTypeInfo(IHTMLDOMImplementation *iface, UINT iTInfo,
        LCID lcid, ITypeInfo **ppTInfo)
{
    HTMLDOMImplementation *This = impl_from_IHTMLDOMImplementation(iface);

    return IDispatchEx_GetTypeInfo(&This->dispex.IDispatchEx_iface, iTInfo, lcid, ppTInfo);
}

static HRESULT WINAPI HTMLDOMImplementation_GetIDsOfNames(IHTMLDOMImplementation *iface, REFIID riid,
        LPOLESTR *rgszNames, UINT cNames, LCID lcid, DISPID *rgDispId)
{
    HTMLDOMImplementation *This = impl_from_IHTMLDOMImplementation(iface);

    return IDispatchEx_GetIDsOfNames(&This->dispex.IDispatchEx_iface, riid, rgszNames,
            cNames, lcid, rgDispId);
}

static HRESULT WINAPI HTMLDOMImplementation_Invoke(IHTMLDOMImplementation *iface, DISPID dispIdMember,
        REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS *pDispParams, VARIANT *pVarResult,
        EXCEPINFO *pExcepInfo, UINT *puArgErr)
{
    HTMLDOMImplementation *This = impl_from_IHTMLDOMImplementation(iface);

    return IDispatchEx_Invoke(&This->dispex.IDispatchEx_iface, dispIdMember, riid,
            lcid, wFlags, pDispParams, pVarResult, pExcepInfo, puArgErr);
}

static HRESULT WINAPI HTMLDOMImplementation_hasFeature(IHTMLDOMImplementation *iface, BSTR feature,
        VARIANT version, VARIANT_BOOL *pfHasFeature)
{
    HTMLDOMImplementation *This = impl_from_IHTMLDOMImplementation(iface);

    FIXME("(%p)->(%s %s %p) returning false\n", This, debugstr_w(feature), debugstr_variant(&version), pfHasFeature);

    *pfHasFeature = VARIANT_FALSE;
    return S_OK;
}

static const IHTMLDOMImplementationVtbl HTMLDOMImplementationVtbl = {
    HTMLDOMImplementation_QueryInterface,
    HTMLDOMImplementation_AddRef,
    HTMLDOMImplementation_Release,
    HTMLDOMImplementation_GetTypeInfoCount,
    HTMLDOMImplementation_GetTypeInfo,
    HTMLDOMImplementation_GetIDsOfNames,
    HTMLDOMImplementation_Invoke,
    HTMLDOMImplementation_hasFeature
};

static inline HTMLDOMImplementation *impl_from_IHTMLDOMImplementation2(IHTMLDOMImplementation2 *iface)
{
    return CONTAINING_RECORD(iface, HTMLDOMImplementation, IHTMLDOMImplementation2_iface);
}

static HRESULT WINAPI HTMLDOMImplementation2_QueryInterface(IHTMLDOMImplementation2 *iface, REFIID riid, void **ppv)
{
    HTMLDOMImplementation *This = impl_from_IHTMLDOMImplementation2(iface);
    return IHTMLDOMImplementation_QueryInterface(&This->IHTMLDOMImplementation_iface, riid, ppv);
}

static ULONG WINAPI HTMLDOMImplementation2_AddRef(IHTMLDOMImplementation2 *iface)
{
    HTMLDOMImplementation *This = impl_from_IHTMLDOMImplementation2(iface);
    return IHTMLDOMImplementation_AddRef(&This->IHTMLDOMImplementation_iface);
}

static ULONG WINAPI HTMLDOMImplementation2_Release(IHTMLDOMImplementation2 *iface)
{
    HTMLDOMImplementation *This = impl_from_IHTMLDOMImplementation2(iface);
    return IHTMLDOMImplementation_Release(&This->IHTMLDOMImplementation_iface);
}

static HRESULT WINAPI HTMLDOMImplementation2_GetTypeInfoCount(IHTMLDOMImplementation2 *iface, UINT *pctinfo)
{
    HTMLDOMImplementation *This = impl_from_IHTMLDOMImplementation2(iface);
    return IDispatchEx_GetTypeInfoCount(&This->dispex.IDispatchEx_iface, pctinfo);
}

static HRESULT WINAPI HTMLDOMImplementation2_GetTypeInfo(IHTMLDOMImplementation2 *iface, UINT iTInfo,
        LCID lcid, ITypeInfo **ppTInfo)
{
    HTMLDOMImplementation *This = impl_from_IHTMLDOMImplementation2(iface);
    return IDispatchEx_GetTypeInfo(&This->dispex.IDispatchEx_iface, iTInfo, lcid, ppTInfo);
}

static HRESULT WINAPI HTMLDOMImplementation2_GetIDsOfNames(IHTMLDOMImplementation2 *iface, REFIID riid,
        LPOLESTR *rgszNames, UINT cNames, LCID lcid, DISPID *rgDispId)
{
    HTMLDOMImplementation *This = impl_from_IHTMLDOMImplementation2(iface);
    return IDispatchEx_GetIDsOfNames(&This->dispex.IDispatchEx_iface, riid, rgszNames,
            cNames, lcid, rgDispId);
}

static HRESULT WINAPI HTMLDOMImplementation2_Invoke(IHTMLDOMImplementation2 *iface, DISPID dispIdMember,
        REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS *pDispParams, VARIANT *pVarResult,
        EXCEPINFO *pExcepInfo, UINT *puArgErr)
{
    HTMLDOMImplementation *This = impl_from_IHTMLDOMImplementation2(iface);
    return IDispatchEx_Invoke(&This->dispex.IDispatchEx_iface, dispIdMember, riid,
            lcid, wFlags, pDispParams, pVarResult, pExcepInfo, puArgErr);
}

static HRESULT WINAPI HTMLDOMImplementation2_createDocumentType(IHTMLDOMImplementation2 *iface, BSTR name,
        VARIANT *public_id, VARIANT *system_id, IDOMDocumentType **new_type)
{
    HTMLDOMImplementation *This = impl_from_IHTMLDOMImplementation2(iface);
    FIXME("(%p)->(%s %s %s %p)\n", This, debugstr_w(name), debugstr_variant(public_id),
          debugstr_variant(system_id), new_type);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDOMImplementation2_createDocument(IHTMLDOMImplementation2 *iface, VARIANT *ns,
        VARIANT *tag_name, IDOMDocumentType *document_type, IHTMLDocument7 **new_document)
{
    HTMLDOMImplementation *This = impl_from_IHTMLDOMImplementation2(iface);
    FIXME("(%p)->(%s %s %p %p)\n", This, debugstr_variant(ns), debugstr_variant(tag_name),
          document_type, new_document);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDOMImplementation2_createHTMLDocument(IHTMLDOMImplementation2 *iface, BSTR title,
        IHTMLDocument7 **new_document)
{
    HTMLDOMImplementation *This = impl_from_IHTMLDOMImplementation2(iface);
    HTMLDocumentNode *new_document_node;
    nsIDOMDocument *doc;
    nsAString title_str;
    nsresult nsres;
    HRESULT hres;

    FIXME("(%p)->(%s %p)\n", This, debugstr_w(title), new_document);

    if(!This->browser)
        return E_UNEXPECTED;

    nsAString_InitDepend(&title_str, title);
    nsres = nsIDOMDOMImplementation_CreateHTMLDocument(This->implementation, &title_str, &doc);
    nsAString_Finish(&title_str);
    if(NS_FAILED(nsres)) {
        ERR("CreateHTMLDocument failed: %08lx\n", nsres);
        return E_FAIL;
    }

    hres = create_document_node(doc, This->browser, NULL, DOCTYPE_HTML, dispex_compat_mode(&This->dispex), &new_document_node);
    nsIDOMDocument_Release(doc);
    if(FAILED(hres))
        return hres;

    *new_document = &new_document_node->IHTMLDocument7_iface;
    return S_OK;
}

static HRESULT WINAPI HTMLDOMImplementation2_hasFeature(IHTMLDOMImplementation2 *iface, BSTR feature,
        VARIANT version, VARIANT_BOOL *pfHasFeature)
{
    HTMLDOMImplementation *This = impl_from_IHTMLDOMImplementation2(iface);

    FIXME("(%p)->(%s %s %p) returning false\n", This, debugstr_w(feature), debugstr_variant(&version), pfHasFeature);

    *pfHasFeature = VARIANT_FALSE;
    return S_OK;
}

static const IHTMLDOMImplementation2Vtbl HTMLDOMImplementation2Vtbl = {
    HTMLDOMImplementation2_QueryInterface,
    HTMLDOMImplementation2_AddRef,
    HTMLDOMImplementation2_Release,
    HTMLDOMImplementation2_GetTypeInfoCount,
    HTMLDOMImplementation2_GetTypeInfo,
    HTMLDOMImplementation2_GetIDsOfNames,
    HTMLDOMImplementation2_Invoke,
    HTMLDOMImplementation2_createDocumentType,
    HTMLDOMImplementation2_createDocument,
    HTMLDOMImplementation2_createHTMLDocument,
    HTMLDOMImplementation2_hasFeature
};

static void HTMLDOMImplementation_init_dispex_info(dispex_data_t *info, compat_mode_t compat_mode)
{
    if(compat_mode >= COMPAT_MODE_IE9)
        dispex_info_add_interface(info, IHTMLDOMImplementation2_tid, NULL);
}

static const tid_t HTMLDOMImplementation_iface_tids[] = {
    IHTMLDOMImplementation_tid,
    0
};
dispex_static_data_t HTMLDOMImplementation_dispex = {
    "DOMImplementation",
    NULL,
    PROTO_ID_HTMLDOMImplementation,
    DispHTMLDOMImplementation_tid,
    HTMLDOMImplementation_iface_tids,
    HTMLDOMImplementation_init_dispex_info
};

HRESULT create_dom_implementation(HTMLDocumentNode *doc_node, IHTMLDOMImplementation **ret)
{
    HTMLDOMImplementation *dom_implementation;
    nsresult nsres;

    if(!doc_node->browser)
        return E_UNEXPECTED;

    dom_implementation = calloc(1, sizeof(*dom_implementation));
    if(!dom_implementation)
        return E_OUTOFMEMORY;

    dom_implementation->IHTMLDOMImplementation_iface.lpVtbl = &HTMLDOMImplementationVtbl;
    dom_implementation->IHTMLDOMImplementation2_iface.lpVtbl = &HTMLDOMImplementation2Vtbl;
    dom_implementation->ref = 1;
    dom_implementation->browser = doc_node->browser;

    init_dispatch(&dom_implementation->dispex, (IUnknown*)&dom_implementation->IHTMLDOMImplementation_iface,
                  &HTMLDOMImplementation_dispex, get_inner_window(doc_node), doc_node->document_mode);

    nsres = nsIDOMDocument_GetImplementation(doc_node->dom_document, &dom_implementation->implementation);
    if(NS_FAILED(nsres)) {
        ERR("GetDOMImplementation failed: %08lx\n", nsres);
        IHTMLDOMImplementation_Release(&dom_implementation->IHTMLDOMImplementation_iface);
        return E_FAIL;
    }

    *ret = &dom_implementation->IHTMLDOMImplementation_iface;
    return S_OK;
}

void detach_dom_implementation(IHTMLDOMImplementation *iface)
{
    HTMLDOMImplementation *dom_implementation = impl_from_IHTMLDOMImplementation(iface);
    dom_implementation->browser = NULL;
}

struct dom_parser {
    IDOMParser IDOMParser_iface;
    DispatchEx dispex;
    LONG ref;

    HTMLDocumentNode *doc;
};

static inline struct dom_parser *impl_from_IDOMParser(IDOMParser *iface)
{
    return CONTAINING_RECORD(iface, struct dom_parser, IDOMParser_iface);
}

static HRESULT WINAPI DOMParser_QueryInterface(IDOMParser *iface, REFIID riid, void **ppv)
{
    struct dom_parser *This = impl_from_IDOMParser(iface);

    TRACE("(%p)->(%s %p)\n", This, debugstr_mshtml_guid(riid), ppv);

    if(IsEqualGUID(&IID_IUnknown, riid) || IsEqualGUID(&IID_IDOMParser, riid)) {
        *ppv = &This->IDOMParser_iface;
    }else if(dispex_query_interface(&This->dispex, riid, ppv)) {
        return *ppv ? S_OK : E_NOINTERFACE;
    }else {
        *ppv = NULL;
        return E_NOINTERFACE;
    }

    IUnknown_AddRef((IUnknown*)*ppv);
    return S_OK;
}

static ULONG WINAPI DOMParser_AddRef(IDOMParser *iface)
{
    struct dom_parser *This = impl_from_IDOMParser(iface);
    LONG ref = InterlockedIncrement(&This->ref);

    TRACE("(%p) ref=%ld\n", This, ref);

    return ref;
}

static ULONG WINAPI DOMParser_Release(IDOMParser *iface)
{
    struct dom_parser *This = impl_from_IDOMParser(iface);
    LONG ref = InterlockedDecrement(&This->ref);

    TRACE("(%p) ref=%ld\n", This, ref);

    if(!ref) {
        IHTMLDOMNode_Release(&This->doc->node.IHTMLDOMNode_iface);
        release_dispex(&This->dispex);
        free(This);
    }

    return ref;
}

static HRESULT WINAPI DOMParser_GetTypeInfoCount(IDOMParser *iface, UINT *pctinfo)
{
    struct dom_parser *This = impl_from_IDOMParser(iface);

    return IDispatchEx_GetTypeInfoCount(&This->dispex.IDispatchEx_iface, pctinfo);
}

static HRESULT WINAPI DOMParser_GetTypeInfo(IDOMParser *iface, UINT iTInfo,
        LCID lcid, ITypeInfo **ppTInfo)
{
    struct dom_parser *This = impl_from_IDOMParser(iface);

    return IDispatchEx_GetTypeInfo(&This->dispex.IDispatchEx_iface, iTInfo, lcid, ppTInfo);
}

static HRESULT WINAPI DOMParser_GetIDsOfNames(IDOMParser *iface, REFIID riid,
        LPOLESTR *rgszNames, UINT cNames, LCID lcid, DISPID *rgDispId)
{
    struct dom_parser *This = impl_from_IDOMParser(iface);

    return IDispatchEx_GetIDsOfNames(&This->dispex.IDispatchEx_iface, riid, rgszNames, cNames,
                                     lcid, rgDispId);
}

static HRESULT WINAPI DOMParser_Invoke(IDOMParser *iface, DISPID dispIdMember,
        REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS *pDispParams,
        VARIANT *pVarResult, EXCEPINFO *pExcepInfo, UINT *puArgErr)
{
    struct dom_parser *This = impl_from_IDOMParser(iface);

    return IDispatchEx_Invoke(&This->dispex.IDispatchEx_iface, dispIdMember, riid, lcid, wFlags,
                              pDispParams, pVarResult, pExcepInfo, puArgErr);
}

static HRESULT WINAPI DOMParser_parseFromString(IDOMParser *iface, BSTR string, BSTR mimeType, IHTMLDocument2 **ppNode)
{
    struct dom_parser *This = impl_from_IDOMParser(iface);
    nsIDOMDocument *nsdoc = NULL;
    HTMLDocumentNode *xml_doc;
    document_type_t doc_type;
    nsAString errns, errtag;
    nsIDOMNodeList *nodes;
    nsIDOMParser *parser;
    nsresult nsres;
    HRESULT hres;

    TRACE("(%p)->(%s %s %p)\n", This, debugstr_w(string), debugstr_w(mimeType), ppNode);

    if(!string || !mimeType || (doc_type = document_type_from_content_type(mimeType)) == DOCTYPE_INVALID)
        return E_INVALIDARG;

    if(doc_type == DOCTYPE_HTML) {
        IHTMLDOMImplementation *impl_iface;
        HTMLDOMImplementation *impl;
        IHTMLDocument7 *html_doc;
        IHTMLElement *html_elem;
        HTMLDocumentNode *doc;

        hres = IHTMLDocument5_get_implementation(&This->doc->IHTMLDocument5_iface, &impl_iface);
        if(FAILED(hres))
            return hres;

        impl = impl_from_IHTMLDOMImplementation(impl_iface);
        hres = HTMLDOMImplementation2_createHTMLDocument(&impl->IHTMLDOMImplementation2_iface, NULL, &html_doc);
        HTMLDOMImplementation_Release(impl_iface);
        if(FAILED(hres))
            return hres;
        doc = CONTAINING_RECORD(html_doc, HTMLDocumentNode, IHTMLDocument7_iface);

        hres = IHTMLDocument3_get_documentElement(&doc->IHTMLDocument3_iface, &html_elem);
        if(FAILED(hres)) {
            IHTMLDocument7_Release(html_doc);
            return hres;
        }

        hres = IHTMLElement_put_innerHTML(html_elem, string);
        IHTMLElement_Release(html_elem);
        if(FAILED(hres)) {
            IHTMLDocument7_Release(html_doc);
            return hres;
        }

        *ppNode = &doc->IHTMLDocument2_iface;
        return hres;
    }

    if(!(parser = create_nsdomparser(This->doc)))
        return E_FAIL;
    nsres = nsIDOMParser_ParseFromString(parser, string ? string : L"",
            doc_type == DOCTYPE_SVG   ? "image/svg+xml" :
            doc_type == DOCTYPE_XHTML ? "application/xhtml+xml" :
                                        "text/xml", &nsdoc);
    nsIDOMParser_Release(parser);
    if(NS_FAILED(nsres) || !nsdoc) {
        ERR("ParseFromString failed: 0x%08lx\n", nsres);
        return NS_FAILED(nsres) ? map_nsresult(nsres) : E_FAIL;
    }

    nsAString_InitDepend(&errns, L"http://www.mozilla.org/newlayout/xml/parsererror.xml");
    nsAString_InitDepend(&errtag, L"parsererror");
    nsres = nsIDOMDocument_GetElementsByTagNameNS(nsdoc, &errns, &errtag, &nodes);
    nsAString_Finish(&errtag);
    nsAString_Finish(&errns);
    if(NS_SUCCEEDED(nsres)) {
        UINT32 length;
        nsres = nsIDOMNodeList_GetLength(nodes, &length);
        nsIDOMNodeList_Release(nodes);
        if(NS_SUCCEEDED(nsres) && length) {
            nsIDOMDocument_Release(nsdoc);
            return MSHTML_E_SYNTAX;
        }
    }

    hres = create_document_node(nsdoc, This->doc->browser, NULL, doc_type, This->doc->document_mode, &xml_doc);
    nsIDOMDocument_Release(nsdoc);
    if(FAILED(hres))
        return hres;

    *ppNode = &xml_doc->IHTMLDocument2_iface;
    return hres;
}

static const IDOMParserVtbl DOMParserVtbl = {
    DOMParser_QueryInterface,
    DOMParser_AddRef,
    DOMParser_Release,
    DOMParser_GetTypeInfoCount,
    DOMParser_GetTypeInfo,
    DOMParser_GetIDsOfNames,
    DOMParser_Invoke,
    DOMParser_parseFromString
};

static const tid_t DOMParser_iface_tids[] = {
    IDOMParser_tid,
    0
};

dispex_static_data_t DOMParser_dispex = {
    "DOMParser",
    NULL,
    PROTO_ID_DOMParser,
    DispDOMParser_tid,
    DOMParser_iface_tids
};

static HRESULT DOMParserCtor_value(DispatchEx *iface, LCID lcid, WORD flags, DISPPARAMS *params,
        VARIANT *res, EXCEPINFO *ei, IServiceProvider *caller)
{
    struct legacy_ctor *This = CONTAINING_RECORD(iface, struct legacy_ctor, dispex);
    struct dom_parser *ret;

    TRACE("\n");

    switch(flags) {
    case DISPATCH_METHOD|DISPATCH_PROPERTYGET:
        if(!res)
            return E_INVALIDARG;
        /* fall through */
    case DISPATCH_METHOD:
    case DISPATCH_CONSTRUCT:
        break;
    default:
        return legacy_ctor_value(iface, lcid, flags, params, res, ei, caller);
    }

    if(!(ret = calloc(1, sizeof(*ret))))
        return E_OUTOFMEMORY;

    ret->IDOMParser_iface.lpVtbl = &DOMParserVtbl;
    ret->ref = 1;
    ret->doc = This->window->doc;
    IHTMLDOMNode_AddRef(&ret->doc->node.IHTMLDOMNode_iface);

    init_dispatch(&ret->dispex, (IUnknown*)&ret->IDOMParser_iface, &DOMParser_dispex,
                  This->window, dispex_compat_mode(&This->dispex));

    V_VT(res) = VT_DISPATCH;
    V_DISPATCH(res) = (IDispatch*)&ret->IDOMParser_iface;
    return S_OK;
}

static const dispex_static_data_vtbl_t DOMParserCtor_dispex_vtbl = {
    .value            = DOMParserCtor_value,
    .get_dispid       = legacy_ctor_get_dispid,
    .get_name         = legacy_ctor_get_name,
    .invoke           = legacy_ctor_invoke,
    .delete           = legacy_ctor_delete
};

dispex_static_data_t DOMParserCtor_dispex = {
    "DOMParser",
    &DOMParserCtor_dispex_vtbl,
    PROTO_ID_NULL,
    NULL_tid,
    no_iface_tids
};

typedef struct {
    DispatchEx dispex;
    IHTMLScreen IHTMLScreen_iface;

    LONG ref;
} HTMLScreen;

static inline HTMLScreen *impl_from_IHTMLScreen(IHTMLScreen *iface)
{
    return CONTAINING_RECORD(iface, HTMLScreen, IHTMLScreen_iface);
}

static HRESULT WINAPI HTMLScreen_QueryInterface(IHTMLScreen *iface, REFIID riid, void **ppv)
{
    HTMLScreen *This = impl_from_IHTMLScreen(iface);

    TRACE("(%p)->(%s %p)\n", This, debugstr_mshtml_guid(riid), ppv);

    if(IsEqualGUID(&IID_IUnknown, riid)) {
        *ppv = &This->IHTMLScreen_iface;
    }else if(IsEqualGUID(&IID_IHTMLScreen, riid)) {
        *ppv = &This->IHTMLScreen_iface;
    }else if(dispex_query_interface(&This->dispex, riid, ppv)) {
        return *ppv ? S_OK : E_NOINTERFACE;
    }else {
        *ppv = NULL;
        WARN("(%p)->(%s %p)\n", This, debugstr_mshtml_guid(riid), ppv);
        return E_NOINTERFACE;
    }

    IUnknown_AddRef((IUnknown*)*ppv);
    return S_OK;
}

static ULONG WINAPI HTMLScreen_AddRef(IHTMLScreen *iface)
{
    HTMLScreen *This = impl_from_IHTMLScreen(iface);
    LONG ref = InterlockedIncrement(&This->ref);

    TRACE("(%p) ref=%ld\n", This, ref);

    return ref;
}

static ULONG WINAPI HTMLScreen_Release(IHTMLScreen *iface)
{
    HTMLScreen *This = impl_from_IHTMLScreen(iface);
    LONG ref = InterlockedDecrement(&This->ref);

    TRACE("(%p) ref=%ld\n", This, ref);

    if(!ref) {
        release_dispex(&This->dispex);
        free(This);
    }

    return ref;
}

static HRESULT WINAPI HTMLScreen_GetTypeInfoCount(IHTMLScreen *iface, UINT *pctinfo)
{
    HTMLScreen *This = impl_from_IHTMLScreen(iface);
    return IDispatchEx_GetTypeInfoCount(&This->dispex.IDispatchEx_iface, pctinfo);
}

static HRESULT WINAPI HTMLScreen_GetTypeInfo(IHTMLScreen *iface, UINT iTInfo,
        LCID lcid, ITypeInfo **ppTInfo)
{
    HTMLScreen *This = impl_from_IHTMLScreen(iface);
    return IDispatchEx_GetTypeInfo(&This->dispex.IDispatchEx_iface, iTInfo, lcid, ppTInfo);
}

static HRESULT WINAPI HTMLScreen_GetIDsOfNames(IHTMLScreen *iface, REFIID riid,
        LPOLESTR *rgszNames, UINT cNames, LCID lcid, DISPID *rgDispId)
{
    HTMLScreen *This = impl_from_IHTMLScreen(iface);
    return IDispatchEx_GetIDsOfNames(&This->dispex.IDispatchEx_iface, riid, rgszNames, cNames,
            lcid, rgDispId);
}

static HRESULT WINAPI HTMLScreen_Invoke(IHTMLScreen *iface, DISPID dispIdMember,
        REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS *pDispParams,
        VARIANT *pVarResult, EXCEPINFO *pExcepInfo, UINT *puArgErr)
{
    HTMLScreen *This = impl_from_IHTMLScreen(iface);
    return IDispatchEx_Invoke(&This->dispex.IDispatchEx_iface, dispIdMember, riid, lcid, wFlags,
            pDispParams, pVarResult, pExcepInfo, puArgErr);
}

static HRESULT WINAPI HTMLScreen_get_colorDepth(IHTMLScreen *iface, LONG *p)
{
    HTMLScreen *This = impl_from_IHTMLScreen(iface);
    HDC hdc = GetDC(0);

    TRACE("(%p)->(%p)\n", This, p);

    *p = GetDeviceCaps(hdc, BITSPIXEL);
    ReleaseDC(0, hdc);
    return S_OK;
}

static HRESULT WINAPI HTMLScreen_put_bufferDepth(IHTMLScreen *iface, LONG v)
{
    HTMLScreen *This = impl_from_IHTMLScreen(iface);
    FIXME("(%p)->(%ld)\n", This, v);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLScreen_get_bufferDepth(IHTMLScreen *iface, LONG *p)
{
    HTMLScreen *This = impl_from_IHTMLScreen(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLScreen_get_width(IHTMLScreen *iface, LONG *p)
{
    HTMLScreen *This = impl_from_IHTMLScreen(iface);

    TRACE("(%p)->(%p)\n", This, p);

    *p = GetSystemMetrics(SM_CXSCREEN);
    return S_OK;
}

static HRESULT WINAPI HTMLScreen_get_height(IHTMLScreen *iface, LONG *p)
{
    HTMLScreen *This = impl_from_IHTMLScreen(iface);

    TRACE("(%p)->(%p)\n", This, p);

    *p = GetSystemMetrics(SM_CYSCREEN);
    return S_OK;
}

static HRESULT WINAPI HTMLScreen_put_updateInterval(IHTMLScreen *iface, LONG v)
{
    HTMLScreen *This = impl_from_IHTMLScreen(iface);
    FIXME("(%p)->(%ld)\n", This, v);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLScreen_get_updateInterval(IHTMLScreen *iface, LONG *p)
{
    HTMLScreen *This = impl_from_IHTMLScreen(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLScreen_get_availHeight(IHTMLScreen *iface, LONG *p)
{
    HTMLScreen *This = impl_from_IHTMLScreen(iface);
    RECT work_area;

    TRACE("(%p)->(%p)\n", This, p);

    if(!SystemParametersInfoW(SPI_GETWORKAREA, 0, &work_area, 0))
        return E_FAIL;

    *p = work_area.bottom-work_area.top;
    return S_OK;
}

static HRESULT WINAPI HTMLScreen_get_availWidth(IHTMLScreen *iface, LONG *p)
{
    HTMLScreen *This = impl_from_IHTMLScreen(iface);
    RECT work_area;

    TRACE("(%p)->(%p)\n", This, p);

    if(!SystemParametersInfoW(SPI_GETWORKAREA, 0, &work_area, 0))
        return E_FAIL;

    *p = work_area.right-work_area.left;
    return S_OK;
}

static HRESULT WINAPI HTMLScreen_get_fontSmoothingEnabled(IHTMLScreen *iface, VARIANT_BOOL *p)
{
    HTMLScreen *This = impl_from_IHTMLScreen(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static const IHTMLScreenVtbl HTMLSreenVtbl = {
    HTMLScreen_QueryInterface,
    HTMLScreen_AddRef,
    HTMLScreen_Release,
    HTMLScreen_GetTypeInfoCount,
    HTMLScreen_GetTypeInfo,
    HTMLScreen_GetIDsOfNames,
    HTMLScreen_Invoke,
    HTMLScreen_get_colorDepth,
    HTMLScreen_put_bufferDepth,
    HTMLScreen_get_bufferDepth,
    HTMLScreen_get_width,
    HTMLScreen_get_height,
    HTMLScreen_put_updateInterval,
    HTMLScreen_get_updateInterval,
    HTMLScreen_get_availHeight,
    HTMLScreen_get_availWidth,
    HTMLScreen_get_fontSmoothingEnabled
};

static const tid_t HTMLScreen_iface_tids[] = {
    IHTMLScreen_tid,
    0
};
dispex_static_data_t HTMLScreen_dispex = {
    "Screen",
    NULL,
    PROTO_ID_HTMLScreen,
    DispHTMLScreen_tid,
    HTMLScreen_iface_tids
};

HRESULT create_html_screen(HTMLInnerWindow *window, IHTMLScreen **ret)
{
    HTMLScreen *screen;

    screen = calloc(1, sizeof(HTMLScreen));
    if(!screen)
        return E_OUTOFMEMORY;

    screen->IHTMLScreen_iface.lpVtbl = &HTMLSreenVtbl;
    screen->ref = 1;

    init_dispatch(&screen->dispex, (IUnknown*)&screen->IHTMLScreen_iface, &HTMLScreen_dispex,
                  window, dispex_compat_mode(&window->event_target.dispex));

    *ret = &screen->IHTMLScreen_iface;
    return S_OK;
}

static inline OmHistory *impl_from_IOmHistory(IOmHistory *iface)
{
    return CONTAINING_RECORD(iface, OmHistory, IOmHistory_iface);
}

static HRESULT WINAPI OmHistory_QueryInterface(IOmHistory *iface, REFIID riid, void **ppv)
{
    OmHistory *This = impl_from_IOmHistory(iface);

    TRACE("(%p)->(%s %p)\n", This, debugstr_mshtml_guid(riid), ppv);

    if(IsEqualGUID(&IID_IUnknown, riid)) {
        *ppv = &This->IOmHistory_iface;
    }else if(IsEqualGUID(&IID_IOmHistory, riid)) {
        *ppv = &This->IOmHistory_iface;
    }else if(dispex_query_interface(&This->dispex, riid, ppv)) {
        return *ppv ? S_OK : E_NOINTERFACE;
    }else {
        WARN("Unsupported interface %s\n", debugstr_mshtml_guid(riid));
        *ppv = NULL;
        return E_NOINTERFACE;
    }

    IUnknown_AddRef((IUnknown*)*ppv);
    return S_OK;
}

static ULONG WINAPI OmHistory_AddRef(IOmHistory *iface)
{
    OmHistory *This = impl_from_IOmHistory(iface);
    LONG ref = InterlockedIncrement(&This->ref);

    TRACE("(%p) ref=%ld\n", This, ref);

    return ref;
}

static ULONG WINAPI OmHistory_Release(IOmHistory *iface)
{
    OmHistory *This = impl_from_IOmHistory(iface);
    LONG ref = InterlockedDecrement(&This->ref);

    TRACE("(%p) ref=%ld\n", This, ref);

    if(!ref) {
        release_dispex(&This->dispex);
        free(This);
    }

    return ref;
}

static HRESULT WINAPI OmHistory_GetTypeInfoCount(IOmHistory *iface, UINT *pctinfo)
{
    OmHistory *This = impl_from_IOmHistory(iface);
    FIXME("(%p)->(%p)\n", This, pctinfo);
    return E_NOTIMPL;
}

static HRESULT WINAPI OmHistory_GetTypeInfo(IOmHistory *iface, UINT iTInfo,
        LCID lcid, ITypeInfo **ppTInfo)
{
    OmHistory *This = impl_from_IOmHistory(iface);

    return IDispatchEx_GetTypeInfo(&This->dispex.IDispatchEx_iface, iTInfo, lcid, ppTInfo);
}

static HRESULT WINAPI OmHistory_GetIDsOfNames(IOmHistory *iface, REFIID riid, LPOLESTR *rgszNames, UINT cNames,
        LCID lcid, DISPID *rgDispId)
{
    OmHistory *This = impl_from_IOmHistory(iface);

    return IDispatchEx_GetIDsOfNames(&This->dispex.IDispatchEx_iface, riid, rgszNames, cNames,
            lcid, rgDispId);
}

static HRESULT WINAPI OmHistory_Invoke(IOmHistory *iface, DISPID dispIdMember, REFIID riid, LCID lcid,
        WORD wFlags, DISPPARAMS *pDispParams, VARIANT *pVarResult, EXCEPINFO *pExcepInfo, UINT *puArgErr)
{
    OmHistory *This = impl_from_IOmHistory(iface);

    return IDispatchEx_Invoke(&This->dispex.IDispatchEx_iface, dispIdMember, riid, lcid, wFlags,
            pDispParams, pVarResult, pExcepInfo, puArgErr);
}

static HRESULT WINAPI OmHistory_get_length(IOmHistory *iface, short *p)
{
    OmHistory *This = impl_from_IOmHistory(iface);
    GeckoBrowser *browser = NULL;

    TRACE("(%p)->(%p)\n", This, p);

    if(This->window)
        browser = This->window->base.outer_window->browser;

    *p = browser && browser->doc->travel_log
        ? ITravelLog_CountEntries(browser->doc->travel_log, browser->doc->browser_service)
        : 0;
    return S_OK;
}

static HRESULT WINAPI OmHistory_back(IOmHistory *iface, VARIANT *pvargdistance)
{
    OmHistory *This = impl_from_IOmHistory(iface);
    FIXME("(%p)->(%s)\n", This, debugstr_variant(pvargdistance));
    return E_NOTIMPL;
}

static HRESULT WINAPI OmHistory_forward(IOmHistory *iface, VARIANT *pvargdistance)
{
    OmHistory *This = impl_from_IOmHistory(iface);
    FIXME("(%p)->(%s)\n", This, debugstr_variant(pvargdistance));
    return E_NOTIMPL;
}

static HRESULT WINAPI OmHistory_go(IOmHistory *iface, VARIANT *pvargdistance)
{
    OmHistory *This = impl_from_IOmHistory(iface);
    FIXME("(%p)->(%s)\n", This, debugstr_variant(pvargdistance));
    return E_NOTIMPL;
}

static const IOmHistoryVtbl OmHistoryVtbl = {
    OmHistory_QueryInterface,
    OmHistory_AddRef,
    OmHistory_Release,
    OmHistory_GetTypeInfoCount,
    OmHistory_GetTypeInfo,
    OmHistory_GetIDsOfNames,
    OmHistory_Invoke,
    OmHistory_get_length,
    OmHistory_back,
    OmHistory_forward,
    OmHistory_go
};

static const tid_t OmHistory_iface_tids[] = {
    IOmHistory_tid,
    0
};
dispex_static_data_t OmHistory_dispex = {
    "History",
    NULL,
    PROTO_ID_History,
    DispHTMLHistory_tid,
    OmHistory_iface_tids
};


HRESULT create_history(HTMLInnerWindow *window, OmHistory **ret)
{
    OmHistory *history;

    history = calloc(1, sizeof(*history));
    if(!history)
        return E_OUTOFMEMORY;

    history->IOmHistory_iface.lpVtbl = &OmHistoryVtbl;
    history->ref = 1;

    init_dispatch(&history->dispex, (IUnknown*)&history->IOmHistory_iface, &OmHistory_dispex,
                  window, dispex_compat_mode(&window->event_target.dispex));

    history->window = window;

    *ret = history;
    return S_OK;
}

struct HTMLPluginsCollection {
    DispatchEx dispex;
    IHTMLPluginsCollection IHTMLPluginsCollection_iface;

    LONG ref;

    OmNavigator *navigator;
};

static inline HTMLPluginsCollection *impl_from_IHTMLPluginsCollection(IHTMLPluginsCollection *iface)
{
    return CONTAINING_RECORD(iface, HTMLPluginsCollection, IHTMLPluginsCollection_iface);
}

static HRESULT WINAPI HTMLPluginsCollection_QueryInterface(IHTMLPluginsCollection *iface, REFIID riid, void **ppv)
{
    HTMLPluginsCollection *This = impl_from_IHTMLPluginsCollection(iface);

    TRACE("(%p)->(%s %p)\n", This, debugstr_mshtml_guid(riid), ppv);

    if(IsEqualGUID(&IID_IUnknown, riid)) {
        *ppv = &This->IHTMLPluginsCollection_iface;
    }else if(IsEqualGUID(&IID_IHTMLPluginsCollection, riid)) {
        *ppv = &This->IHTMLPluginsCollection_iface;
    }else if(dispex_query_interface(&This->dispex, riid, ppv)) {
        return *ppv ? S_OK : E_NOINTERFACE;
    }else {
        *ppv = NULL;
        WARN("Unsupported interface %s\n", debugstr_mshtml_guid(riid));
        return E_NOINTERFACE;
    }

    IUnknown_AddRef((IUnknown*)*ppv);
    return S_OK;
}

static ULONG WINAPI HTMLPluginsCollection_AddRef(IHTMLPluginsCollection *iface)
{
    HTMLPluginsCollection *This = impl_from_IHTMLPluginsCollection(iface);
    LONG ref = InterlockedIncrement(&This->ref);

    TRACE("(%p) ref=%ld\n", This, ref);

    return ref;
}

static ULONG WINAPI HTMLPluginsCollection_Release(IHTMLPluginsCollection *iface)
{
    HTMLPluginsCollection *This = impl_from_IHTMLPluginsCollection(iface);
    LONG ref = InterlockedDecrement(&This->ref);

    TRACE("(%p) ref=%ld\n", This, ref);

    if(!ref) {
        if(This->navigator)
            This->navigator->plugins = NULL;
        release_dispex(&This->dispex);
        free(This);
    }

    return ref;
}

static HRESULT WINAPI HTMLPluginsCollection_GetTypeInfoCount(IHTMLPluginsCollection *iface, UINT *pctinfo)
{
    HTMLPluginsCollection *This = impl_from_IHTMLPluginsCollection(iface);
    return IDispatchEx_GetTypeInfoCount(&This->dispex.IDispatchEx_iface, pctinfo);
}

static HRESULT WINAPI HTMLPluginsCollection_GetTypeInfo(IHTMLPluginsCollection *iface, UINT iTInfo,
                                              LCID lcid, ITypeInfo **ppTInfo)
{
    HTMLPluginsCollection *This = impl_from_IHTMLPluginsCollection(iface);
    return IDispatchEx_GetTypeInfo(&This->dispex.IDispatchEx_iface, iTInfo, lcid, ppTInfo);
}

static HRESULT WINAPI HTMLPluginsCollection_GetIDsOfNames(IHTMLPluginsCollection *iface, REFIID riid,
        LPOLESTR *rgszNames, UINT cNames, LCID lcid, DISPID *rgDispId)
{
    HTMLPluginsCollection *This = impl_from_IHTMLPluginsCollection(iface);
    return IDispatchEx_GetIDsOfNames(&This->dispex.IDispatchEx_iface, riid, rgszNames, cNames,
            lcid, rgDispId);
}

static HRESULT WINAPI HTMLPluginsCollection_Invoke(IHTMLPluginsCollection *iface, DISPID dispIdMember,
        REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS *pDispParams, VARIANT *pVarResult,
        EXCEPINFO *pExcepInfo, UINT *puArgErr)
{
    HTMLPluginsCollection *This = impl_from_IHTMLPluginsCollection(iface);
    return IDispatchEx_Invoke(&This->dispex.IDispatchEx_iface, dispIdMember, riid, lcid,
            wFlags, pDispParams, pVarResult, pExcepInfo, puArgErr);
}

static HRESULT WINAPI HTMLPluginsCollection_get_length(IHTMLPluginsCollection *iface, LONG *p)
{
    HTMLPluginsCollection *This = impl_from_IHTMLPluginsCollection(iface);

    TRACE("(%p)->(%p)\n", This, p);

    /* IE always returns 0 here */
    *p = 0;
    return S_OK;
}

static HRESULT WINAPI HTMLPluginsCollection_refresh(IHTMLPluginsCollection *iface, VARIANT_BOOL reload)
{
    HTMLPluginsCollection *This = impl_from_IHTMLPluginsCollection(iface);

    TRACE("(%p)->(%x)\n", This, reload);

    /* Nothing to do here. */
    return S_OK;
}

static const IHTMLPluginsCollectionVtbl HTMLPluginsCollectionVtbl = {
    HTMLPluginsCollection_QueryInterface,
    HTMLPluginsCollection_AddRef,
    HTMLPluginsCollection_Release,
    HTMLPluginsCollection_GetTypeInfoCount,
    HTMLPluginsCollection_GetTypeInfo,
    HTMLPluginsCollection_GetIDsOfNames,
    HTMLPluginsCollection_Invoke,
    HTMLPluginsCollection_get_length,
    HTMLPluginsCollection_refresh
};

static const tid_t HTMLPluginsCollection_iface_tids[] = {
    IHTMLPluginsCollection_tid,
    0
};
dispex_static_data_t HTMLPluginsCollection_dispex = {
    "PluginArray",
    NULL,
    PROTO_ID_HTMLPluginsCollection,
    DispCPlugins_tid,
    HTMLPluginsCollection_iface_tids
};

static HRESULT create_plugins_collection(OmNavigator *navigator, HTMLPluginsCollection **ret)
{
    HTMLPluginsCollection *col;

    col = calloc(1, sizeof(*col));
    if(!col)
        return E_OUTOFMEMORY;

    col->IHTMLPluginsCollection_iface.lpVtbl = &HTMLPluginsCollectionVtbl;
    col->ref = 1;
    col->navigator = navigator;

    init_dispatch(&col->dispex, (IUnknown*)&col->IHTMLPluginsCollection_iface,
                  &HTMLPluginsCollection_dispex, navigator->window, dispex_compat_mode(&navigator->dispex));

    *ret = col;
    return S_OK;
}

struct HTMLMimeTypesCollection {
    DispatchEx dispex;
    IHTMLMimeTypesCollection IHTMLMimeTypesCollection_iface;

    LONG ref;

    OmNavigator *navigator;
};

static inline HTMLMimeTypesCollection *impl_from_IHTMLMimeTypesCollection(IHTMLMimeTypesCollection *iface)
{
    return CONTAINING_RECORD(iface, HTMLMimeTypesCollection, IHTMLMimeTypesCollection_iface);
}

static HRESULT WINAPI HTMLMimeTypesCollection_QueryInterface(IHTMLMimeTypesCollection *iface, REFIID riid, void **ppv)
{
    HTMLMimeTypesCollection *This = impl_from_IHTMLMimeTypesCollection(iface);

    TRACE("(%p)->(%s %p)\n", This, debugstr_mshtml_guid(riid), ppv);

    if(IsEqualGUID(&IID_IUnknown, riid)) {
        *ppv = &This->IHTMLMimeTypesCollection_iface;
    }else if(IsEqualGUID(&IID_IHTMLMimeTypesCollection, riid)) {
        *ppv = &This->IHTMLMimeTypesCollection_iface;
    }else if(dispex_query_interface(&This->dispex, riid, ppv)) {
        return *ppv ? S_OK : E_NOINTERFACE;
    }else {
        WARN("Unsupported interface %s\n", debugstr_mshtml_guid(riid));
        *ppv = NULL;
        return E_NOINTERFACE;
    }

    IUnknown_AddRef((IUnknown*)*ppv);
    return S_OK;
}

static ULONG WINAPI HTMLMimeTypesCollection_AddRef(IHTMLMimeTypesCollection *iface)
{
    HTMLMimeTypesCollection *This = impl_from_IHTMLMimeTypesCollection(iface);
    LONG ref = InterlockedIncrement(&This->ref);

    TRACE("(%p) ref=%ld\n", This, ref);

    return ref;
}

static ULONG WINAPI HTMLMimeTypesCollection_Release(IHTMLMimeTypesCollection *iface)
{
    HTMLMimeTypesCollection *This = impl_from_IHTMLMimeTypesCollection(iface);
    LONG ref = InterlockedDecrement(&This->ref);

    TRACE("(%p) ref=%ld\n", This, ref);

    if(!ref) {
        if(This->navigator)
            This->navigator->mime_types = NULL;
        release_dispex(&This->dispex);
        free(This);
    }

    return ref;
}

static HRESULT WINAPI HTMLMimeTypesCollection_GetTypeInfoCount(IHTMLMimeTypesCollection *iface, UINT *pctinfo)
{
    HTMLMimeTypesCollection *This = impl_from_IHTMLMimeTypesCollection(iface);
    return IDispatchEx_GetTypeInfoCount(&This->dispex.IDispatchEx_iface, pctinfo);
}

static HRESULT WINAPI HTMLMimeTypesCollection_GetTypeInfo(IHTMLMimeTypesCollection *iface, UINT iTInfo,
                                              LCID lcid, ITypeInfo **ppTInfo)
{
    HTMLMimeTypesCollection *This = impl_from_IHTMLMimeTypesCollection(iface);
    return IDispatchEx_GetTypeInfo(&This->dispex.IDispatchEx_iface, iTInfo, lcid, ppTInfo);
}

static HRESULT WINAPI HTMLMimeTypesCollection_GetIDsOfNames(IHTMLMimeTypesCollection *iface, REFIID riid,
        LPOLESTR *rgszNames, UINT cNames, LCID lcid, DISPID *rgDispId)
{
    HTMLMimeTypesCollection *This = impl_from_IHTMLMimeTypesCollection(iface);
    return IDispatchEx_GetIDsOfNames(&This->dispex.IDispatchEx_iface, riid, rgszNames, cNames,
            lcid, rgDispId);
}

static HRESULT WINAPI HTMLMimeTypesCollection_Invoke(IHTMLMimeTypesCollection *iface, DISPID dispIdMember,
        REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS *pDispParams, VARIANT *pVarResult,
        EXCEPINFO *pExcepInfo, UINT *puArgErr)
{
    HTMLMimeTypesCollection *This = impl_from_IHTMLMimeTypesCollection(iface);
    return IDispatchEx_Invoke(&This->dispex.IDispatchEx_iface, dispIdMember, riid, lcid,
            wFlags, pDispParams, pVarResult, pExcepInfo, puArgErr);
}

static HRESULT WINAPI HTMLMimeTypesCollection_get_length(IHTMLMimeTypesCollection *iface, LONG *p)
{
    HTMLMimeTypesCollection *This = impl_from_IHTMLMimeTypesCollection(iface);

    TRACE("(%p)->(%p)\n", This, p);

    /* This is just a stub for compatibility with other browser in IE */
    *p = 0;
    return S_OK;
}

static const IHTMLMimeTypesCollectionVtbl HTMLMimeTypesCollectionVtbl = {
    HTMLMimeTypesCollection_QueryInterface,
    HTMLMimeTypesCollection_AddRef,
    HTMLMimeTypesCollection_Release,
    HTMLMimeTypesCollection_GetTypeInfoCount,
    HTMLMimeTypesCollection_GetTypeInfo,
    HTMLMimeTypesCollection_GetIDsOfNames,
    HTMLMimeTypesCollection_Invoke,
    HTMLMimeTypesCollection_get_length
};

static const tid_t HTMLMimeTypesCollection_iface_tids[] = {
    IHTMLMimeTypesCollection_tid,
    0
};
dispex_static_data_t HTMLMimeTypesCollection_dispex = {
    "MimeTypeArray",
    NULL,
    PROTO_ID_HTMLMimeTypesCollection,
    IHTMLMimeTypesCollection_tid,
    HTMLMimeTypesCollection_iface_tids
};

static HRESULT create_mime_types_collection(OmNavigator *navigator, HTMLMimeTypesCollection **ret)
{
    HTMLMimeTypesCollection *col;

    col = calloc(1, sizeof(*col));
    if(!col)
        return E_OUTOFMEMORY;

    col->IHTMLMimeTypesCollection_iface.lpVtbl = &HTMLMimeTypesCollectionVtbl;
    col->ref = 1;
    col->navigator = navigator;

    init_dispatch(&col->dispex, (IUnknown*)&col->IHTMLMimeTypesCollection_iface,
                  &HTMLMimeTypesCollection_dispex, navigator->window, dispex_compat_mode(&navigator->dispex));

    *ret = col;
    return S_OK;
}

static inline OmNavigator *impl_from_IOmNavigator(IOmNavigator *iface)
{
    return CONTAINING_RECORD(iface, OmNavigator, IOmNavigator_iface);
}

static HRESULT WINAPI OmNavigator_QueryInterface(IOmNavigator *iface, REFIID riid, void **ppv)
{
    OmNavigator *This = impl_from_IOmNavigator(iface);

    TRACE("(%p)->(%s %p)\n", This, debugstr_mshtml_guid(riid), ppv);

    if(IsEqualGUID(&IID_IUnknown, riid)) {
        *ppv = &This->IOmNavigator_iface;
    }else if(IsEqualGUID(&IID_IOmNavigator, riid)) {
        *ppv = &This->IOmNavigator_iface;
    }else if(dispex_query_interface(&This->dispex, riid, ppv)) {
        return *ppv ? S_OK : E_NOINTERFACE;
    }else {
        WARN("Unsupported interface %s\n", debugstr_mshtml_guid(riid));
        *ppv = NULL;
        return E_NOINTERFACE;
    }

    IUnknown_AddRef((IUnknown*)*ppv);
    return S_OK;
}

static ULONG WINAPI OmNavigator_AddRef(IOmNavigator *iface)
{
    OmNavigator *This = impl_from_IOmNavigator(iface);
    LONG ref = InterlockedIncrement(&This->ref);

    TRACE("(%p) ref=%ld\n", This, ref);

    return ref;
}

static ULONG WINAPI OmNavigator_Release(IOmNavigator *iface)
{
    OmNavigator *This = impl_from_IOmNavigator(iface);
    LONG ref = InterlockedDecrement(&This->ref);

    TRACE("(%p) ref=%ld\n", This, ref);

    if(!ref) {
        if(This->plugins)
            This->plugins->navigator = NULL;
        if(This->mime_types)
            This->mime_types->navigator = NULL;
        release_dispex(&This->dispex);
        free(This);
    }

    return ref;
}

static HRESULT WINAPI OmNavigator_GetTypeInfoCount(IOmNavigator *iface, UINT *pctinfo)
{
    OmNavigator *This = impl_from_IOmNavigator(iface);
    FIXME("(%p)->(%p)\n", This, pctinfo);
    return E_NOTIMPL;
}

static HRESULT WINAPI OmNavigator_GetTypeInfo(IOmNavigator *iface, UINT iTInfo,
                                              LCID lcid, ITypeInfo **ppTInfo)
{
    OmNavigator *This = impl_from_IOmNavigator(iface);

    return IDispatchEx_GetTypeInfo(&This->dispex.IDispatchEx_iface, iTInfo, lcid, ppTInfo);
}

static HRESULT WINAPI OmNavigator_GetIDsOfNames(IOmNavigator *iface, REFIID riid,
                                                LPOLESTR *rgszNames, UINT cNames,
                                                LCID lcid, DISPID *rgDispId)
{
    OmNavigator *This = impl_from_IOmNavigator(iface);

    return IDispatchEx_GetIDsOfNames(&This->dispex.IDispatchEx_iface, riid, rgszNames, cNames,
            lcid, rgDispId);
}

static HRESULT WINAPI OmNavigator_Invoke(IOmNavigator *iface, DISPID dispIdMember,
                            REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS *pDispParams,
                            VARIANT *pVarResult, EXCEPINFO *pExcepInfo, UINT *puArgErr)
{
    OmNavigator *This = impl_from_IOmNavigator(iface);

    return IDispatchEx_Invoke(&This->dispex.IDispatchEx_iface, dispIdMember, riid, lcid, wFlags,
            pDispParams, pVarResult, pExcepInfo, puArgErr);
}

static HRESULT WINAPI OmNavigator_get_appCodeName(IOmNavigator *iface, BSTR *p)
{
    OmNavigator *This = impl_from_IOmNavigator(iface);

    TRACE("(%p)->(%p)\n", This, p);

    *p = SysAllocString(L"Mozilla");
    return S_OK;
}

static HRESULT WINAPI OmNavigator_get_appName(IOmNavigator *iface, BSTR *p)
{
    OmNavigator *This = impl_from_IOmNavigator(iface);

    TRACE("(%p)->(%p)\n", This, p);

    *p = SysAllocString(dispex_compat_mode(&This->dispex) == COMPAT_MODE_IE11
                        ? L"Netscape" : L"Microsoft Internet Explorer");
    if(!*p)
        return E_OUTOFMEMORY;

    return S_OK;
}

/* undocumented, added in IE8 */
extern HRESULT WINAPI MapBrowserEmulationModeToUserAgent(const void*,WCHAR**);

/* Retrieves allocated user agent via CoTaskMemAlloc */
static HRESULT get_user_agent(OmNavigator *navigator, WCHAR **user_agent)
{
    DWORD version = get_compat_mode_version(dispex_compat_mode(&navigator->dispex));

    return MapBrowserEmulationModeToUserAgent(&version, user_agent);
}

static HRESULT WINAPI OmNavigator_get_appVersion(IOmNavigator *iface, BSTR *p)
{
    OmNavigator *This = impl_from_IOmNavigator(iface);
    WCHAR *user_agent;
    unsigned len;
    HRESULT hres;
    const unsigned skip_prefix = 8; /* strlen("Mozilla/") */

    TRACE("(%p)->(%p)\n", This, p);

    hres = get_user_agent(This, &user_agent);
    if(FAILED(hres))
        return hres;
    len = wcslen(user_agent);

    if(len < skip_prefix) {
        CoTaskMemFree(user_agent);
        *p = NULL;
        return S_OK;
    }

    *p = SysAllocStringLen(user_agent + skip_prefix, len - skip_prefix);
    CoTaskMemFree(user_agent);
    return *p ? S_OK : E_OUTOFMEMORY;
}

static HRESULT WINAPI OmNavigator_get_userAgent(IOmNavigator *iface, BSTR *p)
{
    OmNavigator *This = impl_from_IOmNavigator(iface);
    WCHAR *user_agent;
    HRESULT hres;

    TRACE("(%p)->(%p)\n", This, p);

    hres = get_user_agent(This, &user_agent);
    if(FAILED(hres))
        return hres;

    *p = SysAllocString(user_agent);
    CoTaskMemFree(user_agent);
    return *p ? S_OK : E_OUTOFMEMORY;
}

static HRESULT WINAPI OmNavigator_javaEnabled(IOmNavigator *iface, VARIANT_BOOL *enabled)
{
    OmNavigator *This = impl_from_IOmNavigator(iface);

    FIXME("(%p)->(%p) semi-stub\n", This, enabled);

    *enabled = VARIANT_TRUE;
    return S_OK;
}

static HRESULT WINAPI OmNavigator_taintEnabled(IOmNavigator *iface, VARIANT_BOOL *enabled)
{
    OmNavigator *This = impl_from_IOmNavigator(iface);
    FIXME("(%p)->(%p)\n", This, enabled);
    return E_NOTIMPL;
}

static HRESULT WINAPI OmNavigator_get_mimeTypes(IOmNavigator *iface, IHTMLMimeTypesCollection **p)
{
    OmNavigator *This = impl_from_IOmNavigator(iface);

    TRACE("(%p)->(%p)\n", This, p);

    if(!This->mime_types) {
        HRESULT hres;

        hres = create_mime_types_collection(This, &This->mime_types);
        if(FAILED(hres))
            return hres;
    }else {
        IHTMLMimeTypesCollection_AddRef(&This->mime_types->IHTMLMimeTypesCollection_iface);
    }

    *p = &This->mime_types->IHTMLMimeTypesCollection_iface;
    return S_OK;
}

static HRESULT WINAPI OmNavigator_get_plugins(IOmNavigator *iface, IHTMLPluginsCollection **p)
{
    OmNavigator *This = impl_from_IOmNavigator(iface);

    TRACE("(%p)->(%p)\n", This, p);

    if(!This->plugins) {
        HRESULT hres;

        hres = create_plugins_collection(This, &This->plugins);
        if(FAILED(hres))
            return hres;
    }else {
        IHTMLPluginsCollection_AddRef(&This->plugins->IHTMLPluginsCollection_iface);
    }

    *p = &This->plugins->IHTMLPluginsCollection_iface;
    return S_OK;
}

static HRESULT WINAPI OmNavigator_get_cookieEnabled(IOmNavigator *iface, VARIANT_BOOL *p)
{
    OmNavigator *This = impl_from_IOmNavigator(iface);

    WARN("(%p)->(%p) semi-stub\n", This, p);

    *p = VARIANT_TRUE;
    return S_OK;
}

static HRESULT WINAPI OmNavigator_get_opsProfile(IOmNavigator *iface, IHTMLOpsProfile **p)
{
    OmNavigator *This = impl_from_IOmNavigator(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI OmNavigator_toString(IOmNavigator *iface, BSTR *String)
{
    OmNavigator *This = impl_from_IOmNavigator(iface);

    TRACE("(%p)->(%p)\n", This, String);

    return dispex_to_string(&This->dispex, String);
}

static HRESULT WINAPI OmNavigator_get_cpuClass(IOmNavigator *iface, BSTR *p)
{
    OmNavigator *This = impl_from_IOmNavigator(iface);

    TRACE("(%p)->(%p)\n", This, p);

#ifdef _WIN64
    *p = SysAllocString(L"x64");
#else
    *p = SysAllocString(L"x86");
#endif
    return *p ? S_OK : E_OUTOFMEMORY;
}

static HRESULT get_language_string(LCID lcid, BSTR *p)
{
    BSTR ret;
    int len;

    len = LCIDToLocaleName(lcid, NULL, 0, 0);
    if(!len) {
        WARN("LCIDToLocaleName failed: %lu\n", GetLastError());
        return E_FAIL;
    }

    ret = SysAllocStringLen(NULL, len-1);
    if(!ret)
        return E_OUTOFMEMORY;

    len = LCIDToLocaleName(lcid, ret, len, 0);
    if(!len) {
        WARN("LCIDToLocaleName failed: %lu\n", GetLastError());
        SysFreeString(ret);
        return E_FAIL;
    }

    *p = ret;
    return S_OK;
}

static HRESULT WINAPI OmNavigator_get_systemLanguage(IOmNavigator *iface, BSTR *p)
{
    OmNavigator *This = impl_from_IOmNavigator(iface);

    TRACE("(%p)->(%p)\n", This, p);

    return get_language_string(LOCALE_SYSTEM_DEFAULT, p);
}

static HRESULT WINAPI OmNavigator_get_browserLanguage(IOmNavigator *iface, BSTR *p)
{
    OmNavigator *This = impl_from_IOmNavigator(iface);

    TRACE("(%p)->(%p)\n", This, p);

    return get_language_string(GetUserDefaultUILanguage(), p);
}

static HRESULT WINAPI OmNavigator_get_userLanguage(IOmNavigator *iface, BSTR *p)
{
    OmNavigator *This = impl_from_IOmNavigator(iface);

    TRACE("(%p)->(%p)\n", This, p);

    return get_language_string(LOCALE_USER_DEFAULT, p);
}

static HRESULT WINAPI OmNavigator_get_platform(IOmNavigator *iface, BSTR *p)
{
    OmNavigator *This = impl_from_IOmNavigator(iface);

    TRACE("(%p)->(%p)\n", This, p);

#ifdef _WIN64
    *p = SysAllocString(L"Win64");
#else
    *p = SysAllocString(L"Win32");
#endif
    return S_OK;
}

static HRESULT WINAPI OmNavigator_get_appMinorVersion(IOmNavigator *iface, BSTR *p)
{
    OmNavigator *This = impl_from_IOmNavigator(iface);

    TRACE("(%p)->(%p)\n", This, p);

    /* NOTE: MSIE returns "0" or values like ";SP2;". Returning "0" should be enough. */
    *p = SysAllocString(L"0");
    return S_OK;
}

static HRESULT WINAPI OmNavigator_get_connectionSpeed(IOmNavigator *iface, LONG *p)
{
    OmNavigator *This = impl_from_IOmNavigator(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI OmNavigator_get_onLine(IOmNavigator *iface, VARIANT_BOOL *p)
{
    OmNavigator *This = impl_from_IOmNavigator(iface);

    WARN("(%p)->(%p) semi-stub, returning true\n", This, p);

    *p = VARIANT_TRUE;
    return S_OK;
}

static HRESULT WINAPI OmNavigator_get_userProfile(IOmNavigator *iface, IHTMLOpsProfile **p)
{
    OmNavigator *This = impl_from_IOmNavigator(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static const IOmNavigatorVtbl OmNavigatorVtbl = {
    OmNavigator_QueryInterface,
    OmNavigator_AddRef,
    OmNavigator_Release,
    OmNavigator_GetTypeInfoCount,
    OmNavigator_GetTypeInfo,
    OmNavigator_GetIDsOfNames,
    OmNavigator_Invoke,
    OmNavigator_get_appCodeName,
    OmNavigator_get_appName,
    OmNavigator_get_appVersion,
    OmNavigator_get_userAgent,
    OmNavigator_javaEnabled,
    OmNavigator_taintEnabled,
    OmNavigator_get_mimeTypes,
    OmNavigator_get_plugins,
    OmNavigator_get_cookieEnabled,
    OmNavigator_get_opsProfile,
    OmNavigator_toString,
    OmNavigator_get_cpuClass,
    OmNavigator_get_systemLanguage,
    OmNavigator_get_browserLanguage,
    OmNavigator_get_userLanguage,
    OmNavigator_get_platform,
    OmNavigator_get_appMinorVersion,
    OmNavigator_get_connectionSpeed,
    OmNavigator_get_onLine,
    OmNavigator_get_userProfile
};

static const tid_t OmNavigator_iface_tids[] = {
    IOmNavigator_tid,
    0
};
dispex_static_data_t OmNavigator_dispex = {
    "Navigator",
    NULL,
    PROTO_ID_Navigator,
    DispHTMLNavigator_tid,
    OmNavigator_iface_tids
};

HRESULT create_navigator(HTMLInnerWindow *window, IOmNavigator **navigator)
{
    OmNavigator *ret;

    ret = calloc(1, sizeof(*ret));
    if(!ret)
        return E_OUTOFMEMORY;

    ret->IOmNavigator_iface.lpVtbl = &OmNavigatorVtbl;
    ret->ref = 1;
    ret->window = window;

    init_dispatch(&ret->dispex, (IUnknown*)&ret->IOmNavigator_iface, &OmNavigator_dispex,
                  window, dispex_compat_mode(&window->event_target.dispex));

    *navigator = &ret->IOmNavigator_iface;
    return S_OK;
}

void detach_navigator(IOmNavigator *iface)
{
    OmNavigator *navigator = impl_from_IOmNavigator(iface);
    navigator->window = NULL;
}

static inline HTMLPerformanceTiming *impl_from_IHTMLPerformanceTiming(IHTMLPerformanceTiming *iface)
{
    return CONTAINING_RECORD(iface, HTMLPerformanceTiming, IHTMLPerformanceTiming_iface);
}

static HRESULT WINAPI HTMLPerformanceTiming_QueryInterface(IHTMLPerformanceTiming *iface, REFIID riid, void **ppv)
{
    HTMLPerformanceTiming *This = impl_from_IHTMLPerformanceTiming(iface);

    TRACE("(%p)->(%s %p)\n", This, debugstr_mshtml_guid(riid), ppv);

    if(IsEqualGUID(&IID_IUnknown, riid)) {
        *ppv = &This->IHTMLPerformanceTiming_iface;
    }else if(IsEqualGUID(&IID_IHTMLPerformanceTiming, riid)) {
        *ppv = &This->IHTMLPerformanceTiming_iface;
    }else if(dispex_query_interface(&This->dispex, riid, ppv)) {
        return *ppv ? S_OK : E_NOINTERFACE;
    }else {
        WARN("Unsupported interface %s\n", debugstr_mshtml_guid(riid));
        *ppv = NULL;
        return E_NOINTERFACE;
    }

    IUnknown_AddRef((IUnknown*)*ppv);
    return S_OK;
}

static ULONG WINAPI HTMLPerformanceTiming_AddRef(IHTMLPerformanceTiming *iface)
{
    HTMLPerformanceTiming *This = impl_from_IHTMLPerformanceTiming(iface);
    LONG ref = InterlockedIncrement(&This->ref);

    TRACE("(%p) ref=%ld\n", This, ref);

    return ref;
}

static ULONG WINAPI HTMLPerformanceTiming_Release(IHTMLPerformanceTiming *iface)
{
    HTMLPerformanceTiming *This = impl_from_IHTMLPerformanceTiming(iface);
    LONG ref = InterlockedDecrement(&This->ref);

    TRACE("(%p) ref=%ld\n", This, ref);

    if(!ref) {
        if(This->dispex.outer)
            release_dispex(&This->dispex);
        free(This);
    }

    return ref;
}

static HRESULT WINAPI HTMLPerformanceTiming_GetTypeInfoCount(IHTMLPerformanceTiming *iface, UINT *pctinfo)
{
    HTMLPerformanceTiming *This = impl_from_IHTMLPerformanceTiming(iface);
    FIXME("(%p)->(%p)\n", This, pctinfo);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLPerformanceTiming_GetTypeInfo(IHTMLPerformanceTiming *iface, UINT iTInfo,
                                              LCID lcid, ITypeInfo **ppTInfo)
{
    HTMLPerformanceTiming *This = impl_from_IHTMLPerformanceTiming(iface);

    return IDispatchEx_GetTypeInfo(&This->dispex.IDispatchEx_iface, iTInfo, lcid, ppTInfo);
}

static HRESULT WINAPI HTMLPerformanceTiming_GetIDsOfNames(IHTMLPerformanceTiming *iface, REFIID riid,
                                                          LPOLESTR *rgszNames, UINT cNames,
                                                          LCID lcid, DISPID *rgDispId)
{
    HTMLPerformanceTiming *This = impl_from_IHTMLPerformanceTiming(iface);

    return IDispatchEx_GetIDsOfNames(&This->dispex.IDispatchEx_iface, riid, rgszNames, cNames,
            lcid, rgDispId);
}

static HRESULT WINAPI HTMLPerformanceTiming_Invoke(IHTMLPerformanceTiming *iface, DISPID dispIdMember,
                            REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS *pDispParams,
                            VARIANT *pVarResult, EXCEPINFO *pExcepInfo, UINT *puArgErr)
{
    HTMLPerformanceTiming *This = impl_from_IHTMLPerformanceTiming(iface);

    return IDispatchEx_Invoke(&This->dispex.IDispatchEx_iface, dispIdMember, riid, lcid, wFlags,
            pDispParams, pVarResult, pExcepInfo, puArgErr);
}

static ULONGLONG get_fetch_time(HTMLPerformanceTiming *This)
{
    /* If there's no prior doc unloaded and no redirects, fetch time == navigationStart time */
    if(!This->unload_event_end_time && !This->redirect_time)
        return This->navigation_start_time;

    if(This->dns_lookup_time)
        return This->dns_lookup_time;
    if(This->connect_time)
        return This->connect_time;
    if(This->request_time)
        return This->request_time;
    if(This->unload_event_end_time)
        return This->unload_event_end_time;

    return This->redirect_time;
}

static HRESULT WINAPI HTMLPerformanceTiming_get_navigationStart(IHTMLPerformanceTiming *iface, ULONGLONG *p)
{
    HTMLPerformanceTiming *This = impl_from_IHTMLPerformanceTiming(iface);

    TRACE("(%p)->(%p)\n", This, p);

    *p = This->navigation_start_time;
    return S_OK;
}

static HRESULT WINAPI HTMLPerformanceTiming_get_unloadEventStart(IHTMLPerformanceTiming *iface, ULONGLONG *p)
{
    HTMLPerformanceTiming *This = impl_from_IHTMLPerformanceTiming(iface);

    TRACE("(%p)->(%p)\n", This, p);

    *p = This->unload_event_start_time;
    return S_OK;
}

static HRESULT WINAPI HTMLPerformanceTiming_get_unloadEventEnd(IHTMLPerformanceTiming *iface, ULONGLONG *p)
{
    HTMLPerformanceTiming *This = impl_from_IHTMLPerformanceTiming(iface);

    TRACE("(%p)->(%p)\n", This, p);

    *p = This->unload_event_end_time;
    return S_OK;
}

static HRESULT WINAPI HTMLPerformanceTiming_get_redirectStart(IHTMLPerformanceTiming *iface, ULONGLONG *p)
{
    HTMLPerformanceTiming *This = impl_from_IHTMLPerformanceTiming(iface);

    TRACE("(%p)->(%p)\n", This, p);

    *p = This->redirect_time;
    return S_OK;
}

static HRESULT WINAPI HTMLPerformanceTiming_get_redirectEnd(IHTMLPerformanceTiming *iface, ULONGLONG *p)
{
    HTMLPerformanceTiming *This = impl_from_IHTMLPerformanceTiming(iface);

    TRACE("(%p)->(%p)\n", This, p);

    *p = This->redirect_time ? get_fetch_time(This) : 0;
    return S_OK;
}

static HRESULT WINAPI HTMLPerformanceTiming_get_fetchStart(IHTMLPerformanceTiming *iface, ULONGLONG *p)
{
    HTMLPerformanceTiming *This = impl_from_IHTMLPerformanceTiming(iface);

    TRACE("(%p)->(%p)\n", This, p);

    *p = get_fetch_time(This);
    return S_OK;
}

static HRESULT WINAPI HTMLPerformanceTiming_get_domainLookupStart(IHTMLPerformanceTiming *iface, ULONGLONG *p)
{
    HTMLPerformanceTiming *This = impl_from_IHTMLPerformanceTiming(iface);

    TRACE("(%p)->(%p)\n", This, p);

    *p = This->dns_lookup_time ? This->dns_lookup_time : get_fetch_time(This);
    return S_OK;
}

static HRESULT WINAPI HTMLPerformanceTiming_get_domainLookupEnd(IHTMLPerformanceTiming *iface, ULONGLONG *p)
{
    HTMLPerformanceTiming *This = impl_from_IHTMLPerformanceTiming(iface);

    TRACE("(%p)->(%p)\n", This, p);

    *p = This->connect_time    ? This->connect_time    :
         This->dns_lookup_time ? This->dns_lookup_time : get_fetch_time(This);
    return S_OK;
}

static HRESULT WINAPI HTMLPerformanceTiming_get_connectStart(IHTMLPerformanceTiming *iface, ULONGLONG *p)
{
    HTMLPerformanceTiming *This = impl_from_IHTMLPerformanceTiming(iface);

    TRACE("(%p)->(%p)\n", This, p);

    *p = This->connect_time    ? This->connect_time    :
         This->dns_lookup_time ? This->dns_lookup_time : get_fetch_time(This);
    return S_OK;
}

static HRESULT WINAPI HTMLPerformanceTiming_get_connectEnd(IHTMLPerformanceTiming *iface, ULONGLONG *p)
{
    HTMLPerformanceTiming *This = impl_from_IHTMLPerformanceTiming(iface);

    TRACE("(%p)->(%p)\n", This, p);

    *p = This->request_time    ? This->request_time    :
         This->connect_time    ? This->connect_time    :
         This->dns_lookup_time ? This->dns_lookup_time : get_fetch_time(This);
    return S_OK;
}

static HRESULT WINAPI HTMLPerformanceTiming_get_requestStart(IHTMLPerformanceTiming *iface, ULONGLONG *p)
{
    HTMLPerformanceTiming *This = impl_from_IHTMLPerformanceTiming(iface);

    TRACE("(%p)->(%p)\n", This, p);

    *p = This->request_time    ? This->request_time    :
         This->connect_time    ? This->connect_time    :
         This->dns_lookup_time ? This->dns_lookup_time : get_fetch_time(This);
    return S_OK;
}

static HRESULT WINAPI HTMLPerformanceTiming_get_responseStart(IHTMLPerformanceTiming *iface, ULONGLONG *p)
{
    HTMLPerformanceTiming *This = impl_from_IHTMLPerformanceTiming(iface);

    TRACE("(%p)->(%p)\n", This, p);

    *p = This->response_start_time;
    return S_OK;
}

static HRESULT WINAPI HTMLPerformanceTiming_get_responseEnd(IHTMLPerformanceTiming *iface, ULONGLONG *p)
{
    HTMLPerformanceTiming *This = impl_from_IHTMLPerformanceTiming(iface);

    TRACE("(%p)->(%p)\n", This, p);

    *p = This->response_end_time;
    return S_OK;
}

static HRESULT WINAPI HTMLPerformanceTiming_get_domLoading(IHTMLPerformanceTiming *iface, ULONGLONG *p)
{
    HTMLPerformanceTiming *This = impl_from_IHTMLPerformanceTiming(iface);

    TRACE("(%p)->(%p)\n", This, p);

    /* Make sure this is after responseEnd, when the Gecko parser starts */
    *p = This->response_end_time;
    return S_OK;
}

static HRESULT WINAPI HTMLPerformanceTiming_get_domInteractive(IHTMLPerformanceTiming *iface, ULONGLONG *p)
{
    HTMLPerformanceTiming *This = impl_from_IHTMLPerformanceTiming(iface);

    TRACE("(%p)->(%p)\n", This, p);

    *p = This->dom_interactive_time;
    return S_OK;
}

static HRESULT WINAPI HTMLPerformanceTiming_get_domContentLoadedEventStart(IHTMLPerformanceTiming *iface, ULONGLONG *p)
{
    HTMLPerformanceTiming *This = impl_from_IHTMLPerformanceTiming(iface);

    TRACE("(%p)->(%p)\n", This, p);

    *p = This->dom_content_loaded_event_start_time;
    return S_OK;
}

static HRESULT WINAPI HTMLPerformanceTiming_get_domContentLoadedEventEnd(IHTMLPerformanceTiming *iface, ULONGLONG *p)
{
    HTMLPerformanceTiming *This = impl_from_IHTMLPerformanceTiming(iface);

    TRACE("(%p)->(%p)\n", This, p);

    *p = This->dom_content_loaded_event_end_time;
    return S_OK;
}

static HRESULT WINAPI HTMLPerformanceTiming_get_domComplete(IHTMLPerformanceTiming *iface, ULONGLONG *p)
{
    HTMLPerformanceTiming *This = impl_from_IHTMLPerformanceTiming(iface);

    TRACE("(%p)->(%p)\n", This, p);

    *p = This->dom_complete_time;
    return S_OK;
}

static HRESULT WINAPI HTMLPerformanceTiming_get_loadEventStart(IHTMLPerformanceTiming *iface, ULONGLONG *p)
{
    HTMLPerformanceTiming *This = impl_from_IHTMLPerformanceTiming(iface);

    TRACE("(%p)->(%p)\n", This, p);

    *p = This->load_event_start_time;
    return S_OK;
}

static HRESULT WINAPI HTMLPerformanceTiming_get_loadEventEnd(IHTMLPerformanceTiming *iface, ULONGLONG *p)
{
    HTMLPerformanceTiming *This = impl_from_IHTMLPerformanceTiming(iface);

    TRACE("(%p)->(%p)\n", This, p);

    *p = This->load_event_end_time;
    return S_OK;
}

static HRESULT WINAPI HTMLPerformanceTiming_get_msFirstPaint(IHTMLPerformanceTiming *iface, ULONGLONG *p)
{
    HTMLPerformanceTiming *This = impl_from_IHTMLPerformanceTiming(iface);

    TRACE("(%p)->(%p)\n", This, p);

    *p = This->first_paint_time;
    return S_OK;
}

static HRESULT WINAPI HTMLPerformanceTiming_toString(IHTMLPerformanceTiming *iface, BSTR *string)
{
    HTMLPerformanceTiming *This = impl_from_IHTMLPerformanceTiming(iface);

    TRACE("(%p)->(%p)\n", This, string);

    return dispex_to_string(&This->dispex, string);
}

static HRESULT WINAPI HTMLPerformanceTiming_toJSON(IHTMLPerformanceTiming *iface, VARIANT *p)
{
    HTMLPerformanceTiming *This = impl_from_IHTMLPerformanceTiming(iface);

    TRACE("(%p)->(%p)\n", This, p);

    return dispex_builtin_props_to_json(&This->dispex, p);
}

static const IHTMLPerformanceTimingVtbl HTMLPerformanceTimingVtbl = {
    HTMLPerformanceTiming_QueryInterface,
    HTMLPerformanceTiming_AddRef,
    HTMLPerformanceTiming_Release,
    HTMLPerformanceTiming_GetTypeInfoCount,
    HTMLPerformanceTiming_GetTypeInfo,
    HTMLPerformanceTiming_GetIDsOfNames,
    HTMLPerformanceTiming_Invoke,
    HTMLPerformanceTiming_get_navigationStart,
    HTMLPerformanceTiming_get_unloadEventStart,
    HTMLPerformanceTiming_get_unloadEventEnd,
    HTMLPerformanceTiming_get_redirectStart,
    HTMLPerformanceTiming_get_redirectEnd,
    HTMLPerformanceTiming_get_fetchStart,
    HTMLPerformanceTiming_get_domainLookupStart,
    HTMLPerformanceTiming_get_domainLookupEnd,
    HTMLPerformanceTiming_get_connectStart,
    HTMLPerformanceTiming_get_connectEnd,
    HTMLPerformanceTiming_get_requestStart,
    HTMLPerformanceTiming_get_responseStart,
    HTMLPerformanceTiming_get_responseEnd,
    HTMLPerformanceTiming_get_domLoading,
    HTMLPerformanceTiming_get_domInteractive,
    HTMLPerformanceTiming_get_domContentLoadedEventStart,
    HTMLPerformanceTiming_get_domContentLoadedEventEnd,
    HTMLPerformanceTiming_get_domComplete,
    HTMLPerformanceTiming_get_loadEventStart,
    HTMLPerformanceTiming_get_loadEventEnd,
    HTMLPerformanceTiming_get_msFirstPaint,
    HTMLPerformanceTiming_toString,
    HTMLPerformanceTiming_toJSON
};

static void HTMLPerformanceTiming_init_dispex_info(dispex_data_t *info, compat_mode_t mode)
{
    static const dispex_hook_t hooks[] = {
        {DISPID_IHTMLPERFORMANCETIMING_TOJSON},
        {DISPID_UNKNOWN}
    };
    dispex_info_add_interface(info, IHTMLPerformanceTiming_tid, mode < COMPAT_MODE_IE9 ? hooks : NULL);
}

dispex_static_data_t HTMLPerformanceTiming_dispex = {
    "PerformanceTiming",
    NULL,
    PROTO_ID_HTMLPerformanceTiming,
    IHTMLPerformanceTiming_tid,
    no_iface_tids,
    HTMLPerformanceTiming_init_dispex_info
};

HRESULT create_performance_timing(HTMLPerformanceTiming **ret)
{
    HTMLPerformanceTiming *timing;

    timing = calloc(1, sizeof(*timing));
    if(!timing)
        return E_OUTOFMEMORY;

    timing->IHTMLPerformanceTiming_iface.lpVtbl = &HTMLPerformanceTimingVtbl;
    timing->ref = 1;

    /* Defer initializing the dispex until it's actually needed (for compat mode) */
    *ret = timing;
    return S_OK;
}

typedef struct {
    DispatchEx dispex;
    IHTMLPerformanceNavigation IHTMLPerformanceNavigation_iface;

    LONG ref;
    HTMLPerformanceTiming *timing;
} HTMLPerformanceNavigation;

static inline HTMLPerformanceNavigation *impl_from_IHTMLPerformanceNavigation(IHTMLPerformanceNavigation *iface)
{
    return CONTAINING_RECORD(iface, HTMLPerformanceNavigation, IHTMLPerformanceNavigation_iface);
}

static HRESULT WINAPI HTMLPerformanceNavigation_QueryInterface(IHTMLPerformanceNavigation *iface, REFIID riid, void **ppv)
{
    HTMLPerformanceNavigation *This = impl_from_IHTMLPerformanceNavigation(iface);

    TRACE("(%p)->(%s %p)\n", This, debugstr_mshtml_guid(riid), ppv);

    if(IsEqualGUID(&IID_IUnknown, riid)) {
        *ppv = &This->IHTMLPerformanceNavigation_iface;
    }else if(IsEqualGUID(&IID_IHTMLPerformanceNavigation, riid)) {
        *ppv = &This->IHTMLPerformanceNavigation_iface;
    }else if(dispex_query_interface(&This->dispex, riid, ppv)) {
        return *ppv ? S_OK : E_NOINTERFACE;
    }else {
        WARN("Unsupported interface %s\n", debugstr_mshtml_guid(riid));
        *ppv = NULL;
        return E_NOINTERFACE;
    }

    IUnknown_AddRef((IUnknown*)*ppv);
    return S_OK;
}

static ULONG WINAPI HTMLPerformanceNavigation_AddRef(IHTMLPerformanceNavigation *iface)
{
    HTMLPerformanceNavigation *This = impl_from_IHTMLPerformanceNavigation(iface);
    LONG ref = InterlockedIncrement(&This->ref);

    TRACE("(%p) ref=%ld\n", This, ref);

    return ref;
}

static ULONG WINAPI HTMLPerformanceNavigation_Release(IHTMLPerformanceNavigation *iface)
{
    HTMLPerformanceNavigation *This = impl_from_IHTMLPerformanceNavigation(iface);
    LONG ref = InterlockedDecrement(&This->ref);

    TRACE("(%p) ref=%ld\n", This, ref);

    if(!ref) {
        IHTMLPerformanceTiming_Release(&This->timing->IHTMLPerformanceTiming_iface);
        release_dispex(&This->dispex);
        free(This);
    }

    return ref;
}

static HRESULT WINAPI HTMLPerformanceNavigation_GetTypeInfoCount(IHTMLPerformanceNavigation *iface, UINT *pctinfo)
{
    HTMLPerformanceNavigation *This = impl_from_IHTMLPerformanceNavigation(iface);
    FIXME("(%p)->(%p)\n", This, pctinfo);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLPerformanceNavigation_GetTypeInfo(IHTMLPerformanceNavigation *iface, UINT iTInfo,
                                              LCID lcid, ITypeInfo **ppTInfo)
{
    HTMLPerformanceNavigation *This = impl_from_IHTMLPerformanceNavigation(iface);

    return IDispatchEx_GetTypeInfo(&This->dispex.IDispatchEx_iface, iTInfo, lcid, ppTInfo);
}

static HRESULT WINAPI HTMLPerformanceNavigation_GetIDsOfNames(IHTMLPerformanceNavigation *iface, REFIID riid,
                                                          LPOLESTR *rgszNames, UINT cNames,
                                                          LCID lcid, DISPID *rgDispId)
{
    HTMLPerformanceNavigation *This = impl_from_IHTMLPerformanceNavigation(iface);

    return IDispatchEx_GetIDsOfNames(&This->dispex.IDispatchEx_iface, riid, rgszNames, cNames,
            lcid, rgDispId);
}

static HRESULT WINAPI HTMLPerformanceNavigation_Invoke(IHTMLPerformanceNavigation *iface, DISPID dispIdMember,
                            REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS *pDispParams,
                            VARIANT *pVarResult, EXCEPINFO *pExcepInfo, UINT *puArgErr)
{
    HTMLPerformanceNavigation *This = impl_from_IHTMLPerformanceNavigation(iface);

    return IDispatchEx_Invoke(&This->dispex.IDispatchEx_iface, dispIdMember, riid, lcid, wFlags,
            pDispParams, pVarResult, pExcepInfo, puArgErr);
}

static HRESULT WINAPI HTMLPerformanceNavigation_get_type(IHTMLPerformanceNavigation *iface, ULONG *p)
{
    HTMLPerformanceNavigation *This = impl_from_IHTMLPerformanceNavigation(iface);

    TRACE("(%p)->(%p)\n", This, p);

    *p = This->timing->navigation_type;
    return S_OK;
}

static HRESULT WINAPI HTMLPerformanceNavigation_get_redirectCount(IHTMLPerformanceNavigation *iface, ULONG *p)
{
    HTMLPerformanceNavigation *This = impl_from_IHTMLPerformanceNavigation(iface);

    TRACE("(%p)->(%p)\n", This, p);

    *p = This->timing->redirect_count;
    return S_OK;
}

static HRESULT WINAPI HTMLPerformanceNavigation_toString(IHTMLPerformanceNavigation *iface, BSTR *string)
{
    HTMLPerformanceNavigation *This = impl_from_IHTMLPerformanceNavigation(iface);

    TRACE("(%p)->(%p)\n", This, string);

    return dispex_to_string(&This->dispex, string);
}

static HRESULT WINAPI HTMLPerformanceNavigation_toJSON(IHTMLPerformanceNavigation *iface, VARIANT *p)
{
    HTMLPerformanceNavigation *This = impl_from_IHTMLPerformanceNavigation(iface);

    TRACE("(%p)->(%p)\n", This, p);

    return dispex_builtin_props_to_json(&This->dispex, p);
}

static const IHTMLPerformanceNavigationVtbl HTMLPerformanceNavigationVtbl = {
    HTMLPerformanceNavigation_QueryInterface,
    HTMLPerformanceNavigation_AddRef,
    HTMLPerformanceNavigation_Release,
    HTMLPerformanceNavigation_GetTypeInfoCount,
    HTMLPerformanceNavigation_GetTypeInfo,
    HTMLPerformanceNavigation_GetIDsOfNames,
    HTMLPerformanceNavigation_Invoke,
    HTMLPerformanceNavigation_get_type,
    HTMLPerformanceNavigation_get_redirectCount,
    HTMLPerformanceNavigation_toString,
    HTMLPerformanceNavigation_toJSON
};

static void HTMLPerformanceNavigation_init_dispex_info(dispex_data_t *info, compat_mode_t mode)
{
    static const dispex_hook_t hooks[] = {
        {DISPID_IHTMLPERFORMANCENAVIGATION_TOJSON},
        {DISPID_UNKNOWN}
    };
    dispex_info_add_interface(info, IHTMLPerformanceNavigation_tid, mode < COMPAT_MODE_IE9 ? hooks : NULL);
}

dispex_static_data_t HTMLPerformanceNavigation_dispex = {
    "PerformanceNavigation",
    NULL,
    PROTO_ID_HTMLPerformanceNavigation,
    IHTMLPerformanceNavigation_tid,
    no_iface_tids,
    HTMLPerformanceNavigation_init_dispex_info
};

typedef struct {
    DispatchEx dispex;
    IHTMLPerformance IHTMLPerformance_iface;

    LONG ref;

    IHTMLPerformanceNavigation *navigation;
    HTMLPerformanceTiming *timing;
} HTMLPerformance;

static inline HTMLPerformance *impl_from_IHTMLPerformance(IHTMLPerformance *iface)
{
    return CONTAINING_RECORD(iface, HTMLPerformance, IHTMLPerformance_iface);
}

static HRESULT WINAPI HTMLPerformance_QueryInterface(IHTMLPerformance *iface, REFIID riid, void **ppv)
{
    HTMLPerformance *This = impl_from_IHTMLPerformance(iface);

    TRACE("(%p)->(%s %p)\n", This, debugstr_mshtml_guid(riid), ppv);

    if(IsEqualGUID(&IID_IUnknown, riid)) {
        *ppv = &This->IHTMLPerformance_iface;
    }else if(IsEqualGUID(&IID_IHTMLPerformance, riid)) {
        *ppv = &This->IHTMLPerformance_iface;
    }else if(dispex_query_interface(&This->dispex, riid, ppv)) {
        return *ppv ? S_OK : E_NOINTERFACE;
    }else {
        WARN("Unsupported interface %s\n", debugstr_mshtml_guid(riid));
        *ppv = NULL;
        return E_NOINTERFACE;
    }

    IUnknown_AddRef((IUnknown*)*ppv);
    return S_OK;
}

static ULONG WINAPI HTMLPerformance_AddRef(IHTMLPerformance *iface)
{
    HTMLPerformance *This = impl_from_IHTMLPerformance(iface);
    LONG ref = InterlockedIncrement(&This->ref);

    TRACE("(%p) ref=%ld\n", This, ref);

    return ref;
}

static ULONG WINAPI HTMLPerformance_Release(IHTMLPerformance *iface)
{
    HTMLPerformance *This = impl_from_IHTMLPerformance(iface);
    LONG ref = InterlockedDecrement(&This->ref);

    TRACE("(%p) ref=%ld\n", This, ref);

    if(!ref) {
        IHTMLPerformanceTiming_Release(&This->timing->IHTMLPerformanceTiming_iface);
        IHTMLPerformanceNavigation_Release(This->navigation);
        release_dispex(&This->dispex);
        free(This);
    }

    return ref;
}

static HRESULT WINAPI HTMLPerformance_GetTypeInfoCount(IHTMLPerformance *iface, UINT *pctinfo)
{
    HTMLPerformance *This = impl_from_IHTMLPerformance(iface);
    FIXME("(%p)->(%p)\n", This, pctinfo);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLPerformance_GetTypeInfo(IHTMLPerformance *iface, UINT iTInfo,
                                                  LCID lcid, ITypeInfo **ppTInfo)
{
    HTMLPerformance *This = impl_from_IHTMLPerformance(iface);

    return IDispatchEx_GetTypeInfo(&This->dispex.IDispatchEx_iface, iTInfo, lcid, ppTInfo);
}

static HRESULT WINAPI HTMLPerformance_GetIDsOfNames(IHTMLPerformance *iface, REFIID riid,
                                                    LPOLESTR *rgszNames, UINT cNames,
                                                    LCID lcid, DISPID *rgDispId)
{
    HTMLPerformance *This = impl_from_IHTMLPerformance(iface);

    return IDispatchEx_GetIDsOfNames(&This->dispex.IDispatchEx_iface, riid, rgszNames, cNames,
            lcid, rgDispId);
}

static HRESULT WINAPI HTMLPerformance_Invoke(IHTMLPerformance *iface, DISPID dispIdMember,
                            REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS *pDispParams,
                            VARIANT *pVarResult, EXCEPINFO *pExcepInfo, UINT *puArgErr)
{
    HTMLPerformance *This = impl_from_IHTMLPerformance(iface);

    return IDispatchEx_Invoke(&This->dispex.IDispatchEx_iface, dispIdMember, riid, lcid, wFlags,
            pDispParams, pVarResult, pExcepInfo, puArgErr);
}

static HRESULT WINAPI HTMLPerformance_get_navigation(IHTMLPerformance *iface,
                                                     IHTMLPerformanceNavigation **p)
{
    HTMLPerformance *This = impl_from_IHTMLPerformance(iface);

    TRACE("(%p)->(%p)\n", This, p);

    IHTMLPerformanceNavigation_AddRef(*p = This->navigation);
    return S_OK;
}

static HRESULT WINAPI HTMLPerformance_get_timing(IHTMLPerformance *iface, IHTMLPerformanceTiming **p)
{
    HTMLPerformance *This = impl_from_IHTMLPerformance(iface);

    TRACE("(%p)->(%p)\n", This, p);

    IHTMLPerformanceTiming_AddRef(*p = &This->timing->IHTMLPerformanceTiming_iface);
    return S_OK;
}

static HRESULT WINAPI HTMLPerformance_toString(IHTMLPerformance *iface, BSTR *string)
{
    HTMLPerformance *This = impl_from_IHTMLPerformance(iface);

    TRACE("(%p)->(%p)\n", This, string);

    return dispex_to_string(&This->dispex, string);
}

static HRESULT WINAPI HTMLPerformance_toJSON(IHTMLPerformance *iface, VARIANT *var)
{
    HTMLPerformance *This = impl_from_IHTMLPerformance(iface);

    TRACE("(%p)->(%p)\n", This, var);

    return dispex_builtin_props_to_json(&This->dispex, var);
}

static const IHTMLPerformanceVtbl HTMLPerformanceVtbl = {
    HTMLPerformance_QueryInterface,
    HTMLPerformance_AddRef,
    HTMLPerformance_Release,
    HTMLPerformance_GetTypeInfoCount,
    HTMLPerformance_GetTypeInfo,
    HTMLPerformance_GetIDsOfNames,
    HTMLPerformance_Invoke,
    HTMLPerformance_get_navigation,
    HTMLPerformance_get_timing,
    HTMLPerformance_toString,
    HTMLPerformance_toJSON
};

static void HTMLPerformance_init_dispex_info(dispex_data_t *info, compat_mode_t mode)
{
    static const dispex_hook_t hooks[] = {
        {DISPID_IHTMLPERFORMANCE_TOJSON},
        {DISPID_UNKNOWN}
    };
    dispex_info_add_interface(info, IHTMLPerformance_tid, mode < COMPAT_MODE_IE9 ? hooks : NULL);
}

dispex_static_data_t HTMLPerformance_dispex = {
    "Performance",
    NULL,
    PROTO_ID_HTMLPerformance,
    IHTMLPerformance_tid,
    no_iface_tids,
    HTMLPerformance_init_dispex_info
};

HRESULT create_performance(HTMLInnerWindow *window, IHTMLPerformance **ret)
{
    compat_mode_t compat_mode = dispex_compat_mode(&window->event_target.dispex);
    HTMLPerformanceNavigation *navigation;
    HTMLPerformance *performance;

    performance = calloc(1, sizeof(*performance));
    if(!performance)
        return E_OUTOFMEMORY;

    navigation = calloc(1, sizeof(*navigation));
    if(!navigation) {
        free(performance);
        return E_OUTOFMEMORY;
    }

    performance->IHTMLPerformance_iface.lpVtbl = &HTMLPerformanceVtbl;
    performance->navigation = &navigation->IHTMLPerformanceNavigation_iface;
    performance->timing = window->performance_timing;
    performance->ref = 1;
    IHTMLPerformanceTiming_AddRef(&performance->timing->IHTMLPerformanceTiming_iface);

    init_dispatch(&performance->dispex, (IUnknown*)&performance->IHTMLPerformance_iface,
                  &HTMLPerformance_dispex, window, compat_mode);

    navigation->IHTMLPerformanceNavigation_iface.lpVtbl = &HTMLPerformanceNavigationVtbl;
    navigation->ref = 1;
    navigation->timing = window->performance_timing;
    IHTMLPerformanceTiming_AddRef(&navigation->timing->IHTMLPerformanceTiming_iface);

    init_dispatch(&navigation->dispex, (IUnknown*)&navigation->IHTMLPerformanceNavigation_iface,
                  &HTMLPerformanceNavigation_dispex, window, compat_mode);

    init_dispatch(&performance->timing->dispex, (IUnknown*)&performance->timing->IHTMLPerformanceTiming_iface,
                  &HTMLPerformanceTiming_dispex, window, compat_mode);

    *ret = &performance->IHTMLPerformance_iface;
    return S_OK;
}

typedef struct {
    DispatchEx dispex;
    IHTMLNamespaceCollection IHTMLNamespaceCollection_iface;

    LONG ref;
} HTMLNamespaceCollection;

static inline HTMLNamespaceCollection *impl_from_IHTMLNamespaceCollection(IHTMLNamespaceCollection *iface)
{
    return CONTAINING_RECORD(iface, HTMLNamespaceCollection, IHTMLNamespaceCollection_iface);
}

static HRESULT WINAPI HTMLNamespaceCollection_QueryInterface(IHTMLNamespaceCollection *iface, REFIID riid, void **ppv)
{
    HTMLNamespaceCollection *This = impl_from_IHTMLNamespaceCollection(iface);

    TRACE("(%p)->(%s %p)\n", This, debugstr_mshtml_guid(riid), ppv);

    if(IsEqualGUID(&IID_IUnknown, riid)) {
        *ppv = &This->IHTMLNamespaceCollection_iface;
    }else if(IsEqualGUID(&IID_IHTMLNamespaceCollection, riid)) {
        *ppv = &This->IHTMLNamespaceCollection_iface;
    }else if(dispex_query_interface(&This->dispex, riid, ppv)) {
        return *ppv ? S_OK : E_NOINTERFACE;
    }else {
        WARN("Unsupported interface %s\n", debugstr_mshtml_guid(riid));
        *ppv = NULL;
        return E_NOINTERFACE;
    }

    IUnknown_AddRef((IUnknown*)*ppv);
    return S_OK;
}

static ULONG WINAPI HTMLNamespaceCollection_AddRef(IHTMLNamespaceCollection *iface)
{
    HTMLNamespaceCollection *This = impl_from_IHTMLNamespaceCollection(iface);
    LONG ref = InterlockedIncrement(&This->ref);

    TRACE("(%p) ref=%ld\n", This, ref);

    return ref;
}

static ULONG WINAPI HTMLNamespaceCollection_Release(IHTMLNamespaceCollection *iface)
{
    HTMLNamespaceCollection *This = impl_from_IHTMLNamespaceCollection(iface);
    LONG ref = InterlockedDecrement(&This->ref);

    TRACE("(%p) ref=%ld\n", This, ref);

    if(!ref) {
        release_dispex(&This->dispex);
        free(This);
    }

    return ref;
}

static HRESULT WINAPI HTMLNamespaceCollection_GetTypeInfoCount(IHTMLNamespaceCollection *iface, UINT *pctinfo)
{
    HTMLNamespaceCollection *This = impl_from_IHTMLNamespaceCollection(iface);
    FIXME("(%p)->(%p)\n", This, pctinfo);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLNamespaceCollection_GetTypeInfo(IHTMLNamespaceCollection *iface, UINT iTInfo,
                                                  LCID lcid, ITypeInfo **ppTInfo)
{
    HTMLNamespaceCollection *This = impl_from_IHTMLNamespaceCollection(iface);

    return IDispatchEx_GetTypeInfo(&This->dispex.IDispatchEx_iface, iTInfo, lcid, ppTInfo);
}

static HRESULT WINAPI HTMLNamespaceCollection_GetIDsOfNames(IHTMLNamespaceCollection *iface, REFIID riid,
                                                    LPOLESTR *rgszNames, UINT cNames,
                                                    LCID lcid, DISPID *rgDispId)
{
    HTMLNamespaceCollection *This = impl_from_IHTMLNamespaceCollection(iface);

    return IDispatchEx_GetIDsOfNames(&This->dispex.IDispatchEx_iface, riid, rgszNames, cNames,
            lcid, rgDispId);
}

static HRESULT WINAPI HTMLNamespaceCollection_Invoke(IHTMLNamespaceCollection *iface, DISPID dispIdMember,
                            REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS *pDispParams,
                            VARIANT *pVarResult, EXCEPINFO *pExcepInfo, UINT *puArgErr)
{
    HTMLNamespaceCollection *This = impl_from_IHTMLNamespaceCollection(iface);

    return IDispatchEx_Invoke(&This->dispex.IDispatchEx_iface, dispIdMember, riid, lcid, wFlags,
            pDispParams, pVarResult, pExcepInfo, puArgErr);
}

static HRESULT WINAPI HTMLNamespaceCollection_get_length(IHTMLNamespaceCollection *iface, LONG *p)
{
    HTMLNamespaceCollection *This = impl_from_IHTMLNamespaceCollection(iface);
    FIXME("(%p)->(%p) returning 0\n", This, p);
    *p = 0;
    return S_OK;
}

static HRESULT WINAPI HTMLNamespaceCollection_item(IHTMLNamespaceCollection *iface, VARIANT index, IDispatch **p)
{
    HTMLNamespaceCollection *This = impl_from_IHTMLNamespaceCollection(iface);
    FIXME("(%p)->(%s %p)\n", This, debugstr_variant(&index), p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLNamespaceCollection_add(IHTMLNamespaceCollection *iface, BSTR namespace, BSTR urn,
                                                  VARIANT implementation_url, IDispatch **p)
{
    HTMLNamespaceCollection *This = impl_from_IHTMLNamespaceCollection(iface);
    FIXME("(%p)->(%s %s %s %p)\n", This, debugstr_w(namespace), debugstr_w(urn), debugstr_variant(&implementation_url), p);
    return E_NOTIMPL;
}

static const IHTMLNamespaceCollectionVtbl HTMLNamespaceCollectionVtbl = {
    HTMLNamespaceCollection_QueryInterface,
    HTMLNamespaceCollection_AddRef,
    HTMLNamespaceCollection_Release,
    HTMLNamespaceCollection_GetTypeInfoCount,
    HTMLNamespaceCollection_GetTypeInfo,
    HTMLNamespaceCollection_GetIDsOfNames,
    HTMLNamespaceCollection_Invoke,
    HTMLNamespaceCollection_get_length,
    HTMLNamespaceCollection_item,
    HTMLNamespaceCollection_add
};

static const tid_t HTMLNamespaceCollection_iface_tids[] = {
    IHTMLNamespaceCollection_tid,
    0
};
dispex_static_data_t HTMLNamespaceCollection_dispex = {
    "MSNamespaceInfoCollection",
    NULL,
    PROTO_ID_HTMLNamespaceCollection,
    DispHTMLNamespaceCollection_tid,
    HTMLNamespaceCollection_iface_tids
};

HRESULT create_namespace_collection(HTMLDocumentNode *doc_node, IHTMLNamespaceCollection **ret)
{
    HTMLNamespaceCollection *namespaces;

    if (!(namespaces = calloc(1, sizeof(*namespaces))))
        return E_OUTOFMEMORY;

    namespaces->IHTMLNamespaceCollection_iface.lpVtbl = &HTMLNamespaceCollectionVtbl;
    namespaces->ref = 1;
    init_dispatch(&namespaces->dispex, (IUnknown*)&namespaces->IHTMLNamespaceCollection_iface,
                  &HTMLNamespaceCollection_dispex, get_inner_window(doc_node),
                  dispex_compat_mode(&doc_node->node.event_target.dispex));
    *ret = &namespaces->IHTMLNamespaceCollection_iface;
    return S_OK;
}

struct console {
    DispatchEx dispex;
    IWineMSHTMLConsole IWineMSHTMLConsole_iface;
    LONG ref;
};

static inline struct console *impl_from_IWineMSHTMLConsole(IWineMSHTMLConsole *iface)
{
    return CONTAINING_RECORD(iface, struct console, IWineMSHTMLConsole_iface);
}

static HRESULT WINAPI console_QueryInterface(IWineMSHTMLConsole *iface, REFIID riid, void **ppv)
{
    struct console *console = impl_from_IWineMSHTMLConsole(iface);

    TRACE("(%p)->(%s %p)\n", console, debugstr_mshtml_guid(riid), ppv);

    if(IsEqualGUID(&IID_IUnknown, riid)) {
        *ppv = &console->IWineMSHTMLConsole_iface;
    }else if(IsEqualGUID(&IID_IWineMSHTMLConsole, riid)) {
        *ppv = &console->IWineMSHTMLConsole_iface;
    }else if(dispex_query_interface(&console->dispex, riid, ppv)) {
        return *ppv ? S_OK : E_NOINTERFACE;
    }else {
        WARN("(%p)->(%s %p)\n", console, debugstr_mshtml_guid(riid), ppv);
        *ppv = NULL;
        return E_NOINTERFACE;
    }

    IUnknown_AddRef((IUnknown*)*ppv);
    return S_OK;
}

static ULONG WINAPI console_AddRef(IWineMSHTMLConsole *iface)
{
    struct console *console = impl_from_IWineMSHTMLConsole(iface);
    LONG ref = InterlockedIncrement(&console->ref);

    TRACE("(%p) ref=%ld\n", console, ref);

    return ref;
}

static ULONG WINAPI console_Release(IWineMSHTMLConsole *iface)
{
    struct console *console = impl_from_IWineMSHTMLConsole(iface);
    LONG ref = InterlockedDecrement(&console->ref);

    TRACE("(%p) ref=%ld\n", console, ref);

    if(!ref) {
        release_dispex(&console->dispex);
        free(console);
    }

    return ref;
}

static HRESULT WINAPI console_GetTypeInfoCount(IWineMSHTMLConsole *iface, UINT *pctinfo)
{
    struct console *console = impl_from_IWineMSHTMLConsole(iface);
    FIXME("(%p)->(%p)\n", console, pctinfo);
    return E_NOTIMPL;
}

static HRESULT WINAPI console_GetTypeInfo(IWineMSHTMLConsole *iface, UINT iTInfo,
        LCID lcid, ITypeInfo **ppTInfo)
{
    struct console *console = impl_from_IWineMSHTMLConsole(iface);

    return IDispatchEx_GetTypeInfo(&console->dispex.IDispatchEx_iface, iTInfo, lcid, ppTInfo);
}

static HRESULT WINAPI console_GetIDsOfNames(IWineMSHTMLConsole *iface, REFIID riid,
        LPOLESTR *rgszNames, UINT cNames, LCID lcid, DISPID *rgDispId)
{
    struct console *console = impl_from_IWineMSHTMLConsole(iface);

    return IDispatchEx_GetIDsOfNames(&console->dispex.IDispatchEx_iface, riid, rgszNames, cNames,
            lcid, rgDispId);
}

static HRESULT WINAPI console_Invoke(IWineMSHTMLConsole *iface, DISPID dispIdMember,
        REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS *pDispParams,
        VARIANT *pVarResult, EXCEPINFO *pExcepInfo, UINT *puArgErr)
{
    struct console *console = impl_from_IWineMSHTMLConsole(iface);

    return IDispatchEx_Invoke(&console->dispex.IDispatchEx_iface, dispIdMember, riid, lcid, wFlags,
            pDispParams, pVarResult, pExcepInfo, puArgErr);
}

static HRESULT WINAPI console_assert(IWineMSHTMLConsole *iface, VARIANT_BOOL *assertion, VARIANT *vararg_start)
{
    FIXME("iface %p, assertion %p, vararg_start %p stub.\n", iface, assertion, vararg_start);

    return S_OK;
}

static HRESULT WINAPI console_clear(IWineMSHTMLConsole *iface)
{
    FIXME("iface %p stub.\n", iface);

    return S_OK;
}

static HRESULT WINAPI console_count(IWineMSHTMLConsole *iface, VARIANT *label)
{
    FIXME("iface %p, label %p stub.\n", iface, label);

    return S_OK;
}

static HRESULT WINAPI console_debug(IWineMSHTMLConsole *iface, VARIANT *vararg_start)
{
    FIXME("iface %p, vararg_start %p stub.\n", iface, vararg_start);

    return S_OK;
}

static HRESULT WINAPI console_dir(IWineMSHTMLConsole *iface, VARIANT *object)
{
    FIXME("iface %p, object %p stub.\n", iface, object);

    return S_OK;
}

static HRESULT WINAPI console_dirxml(IWineMSHTMLConsole *iface, VARIANT *object)
{
    FIXME("iface %p, object %p stub.\n", iface, object);

    return S_OK;
}

static HRESULT WINAPI console_error(IWineMSHTMLConsole *iface, VARIANT *vararg_start)
{
    FIXME("iface %p, vararg_start %p stub.\n", iface, vararg_start);

    return S_OK;
}

static HRESULT WINAPI console_group(IWineMSHTMLConsole *iface, VARIANT *label)
{
    FIXME("iface %p, label %p stub.\n", iface, label);

    return S_OK;
}

static HRESULT WINAPI console_group_collapsed(IWineMSHTMLConsole *iface, VARIANT *label)
{
    FIXME("iface %p, label %p stub.\n", iface, label);

    return S_OK;
}

static HRESULT WINAPI console_group_end(IWineMSHTMLConsole *iface)
{
    FIXME("iface %p, stub.\n", iface);

    return S_OK;
}

static HRESULT WINAPI console_info(IWineMSHTMLConsole *iface, VARIANT *vararg_start)
{
    FIXME("iface %p, vararg_start %p stub.\n", iface, vararg_start);

    return S_OK;
}

static HRESULT WINAPI console_log(IWineMSHTMLConsole *iface, VARIANT *vararg_start)
{
    FIXME("iface %p, vararg_start %p stub.\n", iface, vararg_start);

    return S_OK;
}

static HRESULT WINAPI console_time(IWineMSHTMLConsole *iface, VARIANT *label)
{
    FIXME("iface %p, label %p stub.\n", iface, label);

    return S_OK;
}

static HRESULT WINAPI console_time_end(IWineMSHTMLConsole *iface, VARIANT *label)
{
    FIXME("iface %p, label %p stub.\n", iface, label);

    return S_OK;
}

static HRESULT WINAPI console_trace(IWineMSHTMLConsole *iface, VARIANT *vararg_start)
{
    FIXME("iface %p, vararg_start %p stub.\n", iface, vararg_start);

    return S_OK;
}

static HRESULT WINAPI console_warn(IWineMSHTMLConsole *iface, VARIANT *vararg_start)
{
    FIXME("iface %p, vararg_start %p stub.\n", iface, vararg_start);

    return S_OK;
}

static const IWineMSHTMLConsoleVtbl WineMSHTMLConsoleVtbl = {
    console_QueryInterface,
    console_AddRef,
    console_Release,
    console_GetTypeInfoCount,
    console_GetTypeInfo,
    console_GetIDsOfNames,
    console_Invoke,
    console_assert,
    console_clear,
    console_count,
    console_debug,
    console_dir,
    console_dirxml,
    console_error,
    console_group,
    console_group_collapsed,
    console_group_end,
    console_info,
    console_log,
    console_time,
    console_time_end,
    console_trace,
    console_warn,
};

static const tid_t console_iface_tids[] = {
    IWineMSHTMLConsole_tid,
    0
};
dispex_static_data_t console_dispex = {
    "Console",
    NULL,
    PROTO_ID_Console,
    IWineMSHTMLConsole_tid,
    console_iface_tids
};

void create_console(HTMLInnerWindow *window, IWineMSHTMLConsole **ret)
{
    struct console *obj;

    obj = calloc(1, sizeof(*obj));
    if(!obj)
    {
        ERR("No memory.\n");
        return;
    }

    obj->IWineMSHTMLConsole_iface.lpVtbl = &WineMSHTMLConsoleVtbl;
    obj->ref = 1;
    init_dispatch(&obj->dispex, (IUnknown*)&obj->IWineMSHTMLConsole_iface, &console_dispex,
                  window, dispex_compat_mode(&window->event_target.dispex));

    *ret = &obj->IWineMSHTMLConsole_iface;
}

struct media_query_list_listener {
    struct list entry;
    IDispatch *function;
};

struct media_query_list_callback;
struct media_query_list {
    DispatchEx dispex;
    IWineMSHTMLMediaQueryList IWineMSHTMLMediaQueryList_iface;
    LONG ref;
    nsIDOMMediaQueryList *nsquerylist;
    struct media_query_list_callback *callback;
    struct list listeners;
};

struct media_query_list_callback {
    nsIDOMMediaQueryListListener nsIDOMMediaQueryListListener_iface;
    struct media_query_list *media_query_list;
    LONG ref;
};

static inline struct media_query_list *impl_from_IWineMSHTMLMediaQueryList(IWineMSHTMLMediaQueryList *iface)
{
    return CONTAINING_RECORD(iface, struct media_query_list, IWineMSHTMLMediaQueryList_iface);
}

static HRESULT WINAPI media_query_list_QueryInterface(IWineMSHTMLMediaQueryList *iface, REFIID riid, void **ppv)
{
    struct media_query_list *media_query_list = impl_from_IWineMSHTMLMediaQueryList(iface);

    TRACE("(%p)->(%s %p)\n", media_query_list, debugstr_mshtml_guid(riid), ppv);

    if(IsEqualGUID(&IID_IUnknown, riid) || IsEqualGUID(&IID_IWineMSHTMLMediaQueryList, riid)) {
        *ppv = &media_query_list->IWineMSHTMLMediaQueryList_iface;
    }else if(dispex_query_interface(&media_query_list->dispex, riid, ppv)) {
        return *ppv ? S_OK : E_NOINTERFACE;
    }else {
        *ppv = NULL;
        return E_NOINTERFACE;
    }

    IUnknown_AddRef((IUnknown*)*ppv);
    return S_OK;
}

static ULONG WINAPI media_query_list_AddRef(IWineMSHTMLMediaQueryList *iface)
{
    struct media_query_list *media_query_list = impl_from_IWineMSHTMLMediaQueryList(iface);
    LONG ref = InterlockedIncrement(&media_query_list->ref);

    TRACE("(%p) ref=%ld\n", media_query_list, ref);

    return ref;
}

static ULONG WINAPI media_query_list_Release(IWineMSHTMLMediaQueryList *iface)
{
    struct media_query_list *media_query_list = impl_from_IWineMSHTMLMediaQueryList(iface);
    LONG ref = InterlockedDecrement(&media_query_list->ref);
    struct media_query_list_listener *listener, *listener2;

    TRACE("(%p) ref=%ld\n", media_query_list, ref);

    if(!ref) {
        media_query_list->callback->media_query_list = NULL;
        LIST_FOR_EACH_ENTRY_SAFE(listener, listener2, &media_query_list->listeners, struct media_query_list_listener, entry) {
            IDispatch_Release(listener->function);
            free(listener);
        }
        nsIDOMMediaQueryListListener_Release(&media_query_list->callback->nsIDOMMediaQueryListListener_iface);
        nsIDOMMediaQueryList_Release(media_query_list->nsquerylist);
        release_dispex(&media_query_list->dispex);
        free(media_query_list);
    }

    return ref;
}

static HRESULT WINAPI media_query_list_GetTypeInfoCount(IWineMSHTMLMediaQueryList *iface, UINT *pctinfo)
{
    struct media_query_list *media_query_list = impl_from_IWineMSHTMLMediaQueryList(iface);

    TRACE("(%p)->(%p)\n", media_query_list, pctinfo);

    return IDispatchEx_GetTypeInfoCount(&media_query_list->dispex.IDispatchEx_iface, pctinfo);
}

static HRESULT WINAPI media_query_list_GetTypeInfo(IWineMSHTMLMediaQueryList *iface, UINT iTInfo,
        LCID lcid, ITypeInfo **ppTInfo)
{
    struct media_query_list *media_query_list = impl_from_IWineMSHTMLMediaQueryList(iface);

    return IDispatchEx_GetTypeInfo(&media_query_list->dispex.IDispatchEx_iface, iTInfo, lcid, ppTInfo);
}

static HRESULT WINAPI media_query_list_GetIDsOfNames(IWineMSHTMLMediaQueryList *iface, REFIID riid,
        LPOLESTR *rgszNames, UINT cNames, LCID lcid, DISPID *rgDispId)
{
    struct media_query_list *media_query_list = impl_from_IWineMSHTMLMediaQueryList(iface);

    return IDispatchEx_GetIDsOfNames(&media_query_list->dispex.IDispatchEx_iface, riid, rgszNames, cNames,
            lcid, rgDispId);
}

static HRESULT WINAPI media_query_list_Invoke(IWineMSHTMLMediaQueryList *iface, DISPID dispIdMember,
        REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS *pDispParams,
        VARIANT *pVarResult, EXCEPINFO *pExcepInfo, UINT *puArgErr)
{
    struct media_query_list *media_query_list = impl_from_IWineMSHTMLMediaQueryList(iface);

    return IDispatchEx_Invoke(&media_query_list->dispex.IDispatchEx_iface, dispIdMember, riid, lcid, wFlags,
            pDispParams, pVarResult, pExcepInfo, puArgErr);
}

static HRESULT WINAPI media_query_list_get_media(IWineMSHTMLMediaQueryList *iface, BSTR *p)
{
    struct media_query_list *media_query_list = impl_from_IWineMSHTMLMediaQueryList(iface);
    nsAString nsstr;

    TRACE("(%p)->(%p)\n", media_query_list, p);

    nsAString_InitDepend(&nsstr, NULL);
    return return_nsstr(nsIDOMMediaQueryList_GetMedia(media_query_list->nsquerylist, &nsstr), &nsstr, p);
}

static HRESULT WINAPI media_query_list_get_matches(IWineMSHTMLMediaQueryList *iface, VARIANT_BOOL *p)
{
    struct media_query_list *media_query_list = impl_from_IWineMSHTMLMediaQueryList(iface);
    nsresult nsres;
    cpp_bool b;

    TRACE("(%p)->(%p)\n", media_query_list, p);

    nsres = nsIDOMMediaQueryList_GetMatches(media_query_list->nsquerylist, &b);
    if(NS_FAILED(nsres))
        return map_nsresult(nsres);
    *p = b ? VARIANT_TRUE : VARIANT_FALSE;
    return S_OK;
}

static HRESULT WINAPI media_query_list_addListener(IWineMSHTMLMediaQueryList *iface, VARIANT *listener)
{
    struct media_query_list *media_query_list = impl_from_IWineMSHTMLMediaQueryList(iface);
    struct media_query_list_listener *entry;

    TRACE("(%p)->(%s)\n", media_query_list, debugstr_variant(listener));

    if(V_VT(listener) != VT_DISPATCH || !V_DISPATCH(listener))
        return S_OK;

    LIST_FOR_EACH_ENTRY(entry, &media_query_list->listeners, struct media_query_list_listener, entry)
        if(entry->function == V_DISPATCH(listener))
            return S_OK;

    if(!(entry = malloc(sizeof(*entry))))
        return E_OUTOFMEMORY;
    entry->function = V_DISPATCH(listener);
    IDispatch_AddRef(V_DISPATCH(listener));

    list_add_tail(&media_query_list->listeners, &entry->entry);
    return S_OK;
}

static HRESULT WINAPI media_query_list_removeListener(IWineMSHTMLMediaQueryList *iface, VARIANT *listener)
{
    struct media_query_list *media_query_list = impl_from_IWineMSHTMLMediaQueryList(iface);
    struct media_query_list_listener *entry;

    TRACE("(%p)->(%s)\n", media_query_list, debugstr_variant(listener));

    if(V_VT(listener) != VT_DISPATCH || !V_DISPATCH(listener))
        return S_OK;

    LIST_FOR_EACH_ENTRY(entry, &media_query_list->listeners, struct media_query_list_listener, entry) {
        if(entry->function == V_DISPATCH(listener)) {
            list_remove(&entry->entry);
            IDispatch_Release(entry->function);
            free(entry);
            break;
        }
    }

    return S_OK;
}

static const IWineMSHTMLMediaQueryListVtbl media_query_list_vtbl = {
    media_query_list_QueryInterface,
    media_query_list_AddRef,
    media_query_list_Release,
    media_query_list_GetTypeInfoCount,
    media_query_list_GetTypeInfo,
    media_query_list_GetIDsOfNames,
    media_query_list_Invoke,
    media_query_list_get_media,
    media_query_list_get_matches,
    media_query_list_addListener,
    media_query_list_removeListener
};

static inline struct media_query_list_callback *impl_from_nsIDOMMediaQueryListListener(nsIDOMMediaQueryListListener *iface)
{
    return CONTAINING_RECORD(iface, struct media_query_list_callback, nsIDOMMediaQueryListListener_iface);
}

static nsresult NSAPI media_query_list_callback_QueryInterface(nsIDOMMediaQueryListListener *iface,
        nsIIDRef riid, void **result)
{
    struct media_query_list_callback *callback = impl_from_nsIDOMMediaQueryListListener(iface);

    if(IsEqualGUID(&IID_nsISupports, riid) || IsEqualGUID(&IID_nsIDOMMediaQueryListListener, riid)) {
        *result = &callback->nsIDOMMediaQueryListListener_iface;
    }else {
        *result = NULL;
        return NS_NOINTERFACE;
    }

    nsIDOMMediaQueryListListener_AddRef(&callback->nsIDOMMediaQueryListListener_iface);
    return NS_OK;
}

static nsrefcnt NSAPI media_query_list_callback_AddRef(nsIDOMMediaQueryListListener *iface)
{
    struct media_query_list_callback *callback = impl_from_nsIDOMMediaQueryListListener(iface);
    LONG ref = InterlockedIncrement(&callback->ref);

    TRACE("(%p) ref=%ld\n", callback, ref);

    return ref;
}

static nsrefcnt NSAPI media_query_list_callback_Release(nsIDOMMediaQueryListListener *iface)
{
    struct media_query_list_callback *callback = impl_from_nsIDOMMediaQueryListListener(iface);
    LONG ref = InterlockedDecrement(&callback->ref);

    TRACE("(%p) ref=%ld\n", callback, ref);

    if(!ref)
        free(callback);
    return ref;
}

static nsresult NSAPI media_query_list_callback_HandleChange(nsIDOMMediaQueryListListener *iface, nsIDOMMediaQueryList *mql)
{
    struct media_query_list_callback *callback = impl_from_nsIDOMMediaQueryListListener(iface);
    IDispatch *listener_funcs_buf[4], **listener_funcs = listener_funcs_buf;
    struct media_query_list *media_query_list = callback->media_query_list;
    struct media_query_list_listener *listener;
    unsigned cnt, i = 0;
    VARIANT args[1], v;
    HRESULT hres;

    if(!media_query_list)
        return NS_OK;

    cnt = list_count(&media_query_list->listeners);
    if(cnt > ARRAY_SIZE(listener_funcs_buf) && !(listener_funcs = malloc(cnt * sizeof(*listener_funcs))))
        return NS_ERROR_OUT_OF_MEMORY;

    LIST_FOR_EACH_ENTRY(listener, &media_query_list->listeners, struct media_query_list_listener, entry) {
        listener_funcs[i] = listener->function;
        IDispatch_AddRef(listener_funcs[i++]);
    }

    for(i = 0; i < cnt; i++) {
        DISPPARAMS dp = { args, NULL, 1, 0 };

        V_VT(args) = VT_DISPATCH;
        V_DISPATCH(args) = (IDispatch*)&media_query_list->dispex.IDispatchEx_iface;
        V_VT(&v) = VT_EMPTY;

        TRACE("%p >>>\n", media_query_list);
        hres = call_disp_func(listener_funcs[i], &dp, &v);
        if(hres == S_OK) {
            TRACE("%p <<< %s\n", media_query_list, debugstr_variant(&v));
            VariantClear(&v);
        }else {
            WARN("%p <<< %08lx\n", media_query_list, hres);
        }
        IDispatch_Release(listener_funcs[i]);
    }

    if(listener_funcs != listener_funcs_buf)
        free(listener_funcs);
    return NS_OK;
}

static const nsIDOMMediaQueryListListenerVtbl media_query_list_callback_vtbl = {
    media_query_list_callback_QueryInterface,
    media_query_list_callback_AddRef,
    media_query_list_callback_Release,
    media_query_list_callback_HandleChange
};

static const tid_t media_query_list_iface_tids[] = {
    IWineMSHTMLMediaQueryList_tid,
    0
};
dispex_static_data_t media_query_list_dispex = {
    "MediaQueryList",
    NULL,
    PROTO_ID_MediaQueryList,
    IWineMSHTMLMediaQueryList_tid,
    media_query_list_iface_tids
};

HRESULT create_media_query_list(HTMLWindow *window, BSTR media_query, IDispatch **ret)
{
    struct media_query_list *media_query_list;
    nsISupports *nsunk;
    nsAString nsstr;
    nsresult nsres;

    if(!media_query || !media_query[0])
        return E_INVALIDARG;

    if(!(media_query_list = malloc(sizeof(*media_query_list))))
        return E_OUTOFMEMORY;

    if(!(media_query_list->callback = malloc(sizeof(*media_query_list->callback)))) {
        free(media_query_list);
        return E_OUTOFMEMORY;
    }
    media_query_list->callback->nsIDOMMediaQueryListListener_iface.lpVtbl = &media_query_list_callback_vtbl;
    media_query_list->callback->media_query_list = media_query_list;
    media_query_list->callback->ref = 1;

    nsAString_InitDepend(&nsstr, media_query);
    nsres = nsIDOMWindow_MatchMedia(window->outer_window->nswindow, &nsstr, &nsunk);
    nsAString_Finish(&nsstr);
    if(NS_FAILED(nsres)) {
        free(media_query_list->callback);
        free(media_query_list);
        return map_nsresult(nsres);
    }
    nsres = nsISupports_QueryInterface(nsunk, &IID_nsIDOMMediaQueryList, (void**)&media_query_list->nsquerylist);
    assert(NS_SUCCEEDED(nsres));
    nsISupports_Release(nsunk);

    nsres = nsIDOMMediaQueryList_SetListener(media_query_list->nsquerylist, &media_query_list->callback->nsIDOMMediaQueryListListener_iface);
    assert(NS_SUCCEEDED(nsres));

    media_query_list->IWineMSHTMLMediaQueryList_iface.lpVtbl = &media_query_list_vtbl;
    media_query_list->ref = 1;
    list_init(&media_query_list->listeners);
    init_dispatch(&media_query_list->dispex, (IUnknown*)&media_query_list->IWineMSHTMLMediaQueryList_iface,
                  &media_query_list_dispex, window->inner_window, dispex_compat_mode(&window->inner_window->event_target.dispex));

    *ret = (IDispatch*)&media_query_list->IWineMSHTMLMediaQueryList_iface;
    return S_OK;
}

struct crypto_subtle {
    DispatchEx dispex;
    IWineMSHTMLSubtleCrypto IWineMSHTMLSubtleCrypto_iface;
    LONG ref;
};

static inline struct crypto_subtle *impl_from_IWineMSHTMLSubtleCrypto(IWineMSHTMLSubtleCrypto *iface)
{
    return CONTAINING_RECORD(iface, struct crypto_subtle, IWineMSHTMLSubtleCrypto_iface);
}

static HRESULT WINAPI crypto_subtle_QueryInterface(IWineMSHTMLSubtleCrypto *iface, REFIID riid, void **ppv)
{
    struct crypto_subtle *subtle = impl_from_IWineMSHTMLSubtleCrypto(iface);

    TRACE("(%p)->(%s %p)\n", subtle, debugstr_mshtml_guid(riid), ppv);

    if(IsEqualGUID(&IID_IUnknown, riid)) {
        *ppv = &subtle->IWineMSHTMLSubtleCrypto_iface;
    }else if(IsEqualGUID(&IID_IWineMSHTMLSubtleCrypto, riid)) {
        *ppv = &subtle->IWineMSHTMLSubtleCrypto_iface;
    }else if(dispex_query_interface(&subtle->dispex, riid, ppv)) {
        return *ppv ? S_OK : E_NOINTERFACE;
    }else {
        WARN("(%p)->(%s %p)\n", subtle, debugstr_mshtml_guid(riid), ppv);
        *ppv = NULL;
        return E_NOINTERFACE;
    }

    IUnknown_AddRef((IUnknown*)*ppv);
    return S_OK;
}

static ULONG WINAPI crypto_subtle_AddRef(IWineMSHTMLSubtleCrypto *iface)
{
    struct crypto_subtle *subtle = impl_from_IWineMSHTMLSubtleCrypto(iface);
    LONG ref = InterlockedIncrement(&subtle->ref);

    TRACE("(%p) ref=%ld\n", subtle, ref);

    return ref;
}

static ULONG WINAPI crypto_subtle_Release(IWineMSHTMLSubtleCrypto *iface)
{
    struct crypto_subtle *subtle = impl_from_IWineMSHTMLSubtleCrypto(iface);
    LONG ref = InterlockedDecrement(&subtle->ref);

    TRACE("(%p) ref=%ld\n", subtle, ref);

    if(!ref) {
        release_dispex(&subtle->dispex);
        free(subtle);
    }

    return ref;
}

static HRESULT WINAPI crypto_subtle_GetTypeInfoCount(IWineMSHTMLSubtleCrypto *iface, UINT *pctinfo)
{
    struct crypto_subtle *subtle = impl_from_IWineMSHTMLSubtleCrypto(iface);

    TRACE("(%p)->(%p)\n", subtle, pctinfo);

    return IDispatchEx_GetTypeInfoCount(&subtle->dispex.IDispatchEx_iface, pctinfo);
}

static HRESULT WINAPI crypto_subtle_GetTypeInfo(IWineMSHTMLSubtleCrypto *iface, UINT iTInfo,
        LCID lcid, ITypeInfo **ppTInfo)
{
    struct crypto_subtle *subtle = impl_from_IWineMSHTMLSubtleCrypto(iface);

    return IDispatchEx_GetTypeInfo(&subtle->dispex.IDispatchEx_iface, iTInfo, lcid, ppTInfo);
}

static HRESULT WINAPI crypto_subtle_GetIDsOfNames(IWineMSHTMLSubtleCrypto *iface, REFIID riid,
        LPOLESTR *rgszNames, UINT cNames, LCID lcid, DISPID *rgDispId)
{
    struct crypto_subtle *subtle = impl_from_IWineMSHTMLSubtleCrypto(iface);

    return IDispatchEx_GetIDsOfNames(&subtle->dispex.IDispatchEx_iface, riid, rgszNames, cNames,
            lcid, rgDispId);
}

static HRESULT WINAPI crypto_subtle_Invoke(IWineMSHTMLSubtleCrypto *iface, DISPID dispIdMember,
        REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS *pDispParams,
        VARIANT *pVarResult, EXCEPINFO *pExcepInfo, UINT *puArgErr)
{
    struct crypto_subtle *subtle = impl_from_IWineMSHTMLSubtleCrypto(iface);

    return IDispatchEx_Invoke(&subtle->dispex.IDispatchEx_iface, dispIdMember, riid, lcid, wFlags,
            pDispParams, pVarResult, pExcepInfo, puArgErr);
}

static HRESULT WINAPI crypto_subtle_encrypt(IWineMSHTMLSubtleCrypto *iface, VARIANT *algorithm, VARIANT *key,
        VARIANT *data, IDispatch **result)
{
    struct crypto_subtle *subtle = impl_from_IWineMSHTMLSubtleCrypto(iface);

    FIXME("(%p)->(%p %p %p %p)\n", subtle, algorithm, key, data, result);

    return E_NOTIMPL;
}

static HRESULT WINAPI crypto_subtle_decrypt(IWineMSHTMLSubtleCrypto *iface, VARIANT *algorithm, VARIANT *key,
        VARIANT *data, IDispatch **result)
{
    struct crypto_subtle *subtle = impl_from_IWineMSHTMLSubtleCrypto(iface);

    FIXME("(%p)->(%p %p %p %p)\n", subtle, algorithm, key, data, result);

    return E_NOTIMPL;
}

static HRESULT WINAPI crypto_subtle_sign(IWineMSHTMLSubtleCrypto *iface, VARIANT *algorithm, VARIANT *key,
        VARIANT *data, IDispatch **signature)
{
    struct crypto_subtle *subtle = impl_from_IWineMSHTMLSubtleCrypto(iface);

    FIXME("(%p)->(%p %p %p %p)\n", subtle, algorithm, key, data, signature);

    return E_NOTIMPL;
}

static HRESULT WINAPI crypto_subtle_verify(IWineMSHTMLSubtleCrypto *iface, VARIANT *algorithm, VARIANT *key,
        VARIANT *signature, VARIANT *data, IDispatch **result)
{
    struct crypto_subtle *subtle = impl_from_IWineMSHTMLSubtleCrypto(iface);

    FIXME("(%p)->(%p %p %p %p %p)\n", subtle, algorithm, key, signature, data, result);

    return E_NOTIMPL;
}

static HRESULT WINAPI crypto_subtle_digest(IWineMSHTMLSubtleCrypto *iface, VARIANT *algorithm, VARIANT *data,
        IDispatch **digest)
{
    struct crypto_subtle *subtle = impl_from_IWineMSHTMLSubtleCrypto(iface);

    FIXME("(%p)->(%p %p %p)\n", subtle, algorithm, data, digest);

    return E_NOTIMPL;
}

static HRESULT WINAPI crypto_subtle_generateKey(IWineMSHTMLSubtleCrypto *iface, VARIANT *algorithm,
        VARIANT_BOOL extractable, VARIANT *keyUsages, IDispatch **result)
{
    struct crypto_subtle *subtle = impl_from_IWineMSHTMLSubtleCrypto(iface);

    FIXME("(%p)->(%p %x %p %p)\n", subtle, algorithm, extractable, keyUsages, result);

    return E_NOTIMPL;
}

static HRESULT WINAPI crypto_subtle_deriveKey(IWineMSHTMLSubtleCrypto *iface, VARIANT *algorithm, VARIANT *baseKey,
        VARIANT *derivedKeyAlgorithm, VARIANT_BOOL extractable, VARIANT *keyUsages, IDispatch **result)
{
    struct crypto_subtle *subtle = impl_from_IWineMSHTMLSubtleCrypto(iface);

    FIXME("(%p)->(%p %p %p %x %p %p)\n", subtle, algorithm, baseKey, derivedKeyAlgorithm, extractable,
          keyUsages, result);

    return E_NOTIMPL;
}

static HRESULT WINAPI crypto_subtle_importKey(IWineMSHTMLSubtleCrypto *iface, BSTR format, VARIANT *keyData,
        VARIANT *algorithm, VARIANT_BOOL extractable, VARIANT *keyUsages, IDispatch **result)
{
    struct crypto_subtle *subtle = impl_from_IWineMSHTMLSubtleCrypto(iface);

    FIXME("(%p)->(%s %p %p %x %p %p)\n", subtle, debugstr_w(format), keyData, algorithm, extractable,
          keyUsages, result);

    return E_NOTIMPL;
}

static HRESULT WINAPI crypto_subtle_exportKey(IWineMSHTMLSubtleCrypto *iface, BSTR format, VARIANT *key,
        IDispatch **result)
{
    struct crypto_subtle *subtle = impl_from_IWineMSHTMLSubtleCrypto(iface);

    FIXME("(%p)->(%s %p %p)\n", subtle, debugstr_w(format), key, result);

    return E_NOTIMPL;
}

static HRESULT WINAPI crypto_subtle_wrapKey(IWineMSHTMLSubtleCrypto *iface, BSTR format, VARIANT *key,
        VARIANT *wrappingKey, VARIANT *wrapAlgo, IDispatch **result)
{
    struct crypto_subtle *subtle = impl_from_IWineMSHTMLSubtleCrypto(iface);

    FIXME("(%p)->(%s %p %p %p %p)\n", subtle, debugstr_w(format), key, wrappingKey, wrapAlgo, result);

    return E_NOTIMPL;
}

static HRESULT WINAPI crypto_subtle_unwrapKey(IWineMSHTMLSubtleCrypto *iface, BSTR format, VARIANT *wrappedKey,
        VARIANT *unwrappingKey, VARIANT *unwrapAlgo, VARIANT *unwrappedKeyAlgo, VARIANT_BOOL extractable,
        VARIANT *keyUsages, IDispatch **result)
{
    struct crypto_subtle *subtle = impl_from_IWineMSHTMLSubtleCrypto(iface);

    FIXME("(%p)->(%s %p %p %p %p %x %p %p)\n", subtle, debugstr_w(format), wrappedKey, unwrappingKey, unwrapAlgo,
          unwrappedKeyAlgo, extractable, keyUsages, result);

    return E_NOTIMPL;
}

static const IWineMSHTMLSubtleCryptoVtbl WineMSHTMLSubtleCryptoVtbl = {
    crypto_subtle_QueryInterface,
    crypto_subtle_AddRef,
    crypto_subtle_Release,
    crypto_subtle_GetTypeInfoCount,
    crypto_subtle_GetTypeInfo,
    crypto_subtle_GetIDsOfNames,
    crypto_subtle_Invoke,
    crypto_subtle_encrypt,
    crypto_subtle_decrypt,
    crypto_subtle_sign,
    crypto_subtle_verify,
    crypto_subtle_digest,
    crypto_subtle_generateKey,
    crypto_subtle_deriveKey,
    crypto_subtle_importKey,
    crypto_subtle_exportKey,
    crypto_subtle_wrapKey,
    crypto_subtle_unwrapKey
};

static const tid_t crypto_subtle_iface_tids[] = {
    IWineMSHTMLSubtleCrypto_tid,
    0
};
dispex_static_data_t crypto_subtle_dispex = {
    "SubtleCrypto",
    NULL,
    PROTO_ID_SubtleCrypto,
    IWineMSHTMLSubtleCrypto_tid,
    crypto_subtle_iface_tids
};

struct crypto {
    DispatchEx dispex;
    IWineMSHTMLCrypto IWineMSHTMLCrypto_iface;
    struct crypto_subtle *subtle;
    LONG ref;
};

static inline struct crypto *impl_from_IWineMSHTMLCrypto(IWineMSHTMLCrypto *iface)
{
    return CONTAINING_RECORD(iface, struct crypto, IWineMSHTMLCrypto_iface);
}

static HRESULT WINAPI crypto_QueryInterface(IWineMSHTMLCrypto *iface, REFIID riid, void **ppv)
{
    struct crypto *crypto = impl_from_IWineMSHTMLCrypto(iface);

    TRACE("(%p)->(%s %p)\n", crypto, debugstr_mshtml_guid(riid), ppv);

    if(IsEqualGUID(&IID_IUnknown, riid)) {
        *ppv = &crypto->IWineMSHTMLCrypto_iface;
    }else if(IsEqualGUID(&IID_IWineMSHTMLCrypto, riid)) {
        *ppv = &crypto->IWineMSHTMLCrypto_iface;
    }else if(dispex_query_interface(&crypto->dispex, riid, ppv)) {
        return *ppv ? S_OK : E_NOINTERFACE;
    }else {
        WARN("(%p)->(%s %p)\n", crypto, debugstr_mshtml_guid(riid), ppv);
        *ppv = NULL;
        return E_NOINTERFACE;
    }

    IUnknown_AddRef((IUnknown*)*ppv);
    return S_OK;
}

static ULONG WINAPI crypto_AddRef(IWineMSHTMLCrypto *iface)
{
    struct crypto *crypto = impl_from_IWineMSHTMLCrypto(iface);
    LONG ref = InterlockedIncrement(&crypto->ref);

    TRACE("(%p) ref=%ld\n", crypto, ref);

    return ref;
}

static ULONG WINAPI crypto_Release(IWineMSHTMLCrypto *iface)
{
    struct crypto *crypto = impl_from_IWineMSHTMLCrypto(iface);
    LONG ref = InterlockedDecrement(&crypto->ref);

    TRACE("(%p) ref=%ld\n", crypto, ref);

    if(!ref) {
        IWineMSHTMLSubtleCrypto_Release(&crypto->subtle->IWineMSHTMLSubtleCrypto_iface);
        release_dispex(&crypto->dispex);
        free(crypto);
    }

    return ref;
}

static HRESULT WINAPI crypto_GetTypeInfoCount(IWineMSHTMLCrypto *iface, UINT *pctinfo)
{
    struct crypto *crypto = impl_from_IWineMSHTMLCrypto(iface);

    TRACE("(%p)->(%p)\n", crypto, pctinfo);

    return IDispatchEx_GetTypeInfoCount(&crypto->dispex.IDispatchEx_iface, pctinfo);
}

static HRESULT WINAPI crypto_GetTypeInfo(IWineMSHTMLCrypto *iface, UINT iTInfo,
        LCID lcid, ITypeInfo **ppTInfo)
{
    struct crypto *crypto = impl_from_IWineMSHTMLCrypto(iface);

    return IDispatchEx_GetTypeInfo(&crypto->dispex.IDispatchEx_iface, iTInfo, lcid, ppTInfo);
}

static HRESULT WINAPI crypto_GetIDsOfNames(IWineMSHTMLCrypto *iface, REFIID riid,
        LPOLESTR *rgszNames, UINT cNames, LCID lcid, DISPID *rgDispId)
{
    struct crypto *crypto = impl_from_IWineMSHTMLCrypto(iface);

    return IDispatchEx_GetIDsOfNames(&crypto->dispex.IDispatchEx_iface, riid, rgszNames, cNames,
            lcid, rgDispId);
}

static HRESULT WINAPI crypto_Invoke(IWineMSHTMLCrypto *iface, DISPID dispIdMember,
        REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS *pDispParams,
        VARIANT *pVarResult, EXCEPINFO *pExcepInfo, UINT *puArgErr)
{
    struct crypto *crypto = impl_from_IWineMSHTMLCrypto(iface);

    return IDispatchEx_Invoke(&crypto->dispex.IDispatchEx_iface, dispIdMember, riid, lcid, wFlags,
            pDispParams, pVarResult, pExcepInfo, puArgErr);
}

static HRESULT WINAPI crypto_get_subtle(IWineMSHTMLCrypto *iface, IDispatch **subtle)
{
    struct crypto *crypto = impl_from_IWineMSHTMLCrypto(iface);

    TRACE("(%p)->(%p)\n", crypto, subtle);

    *subtle = (IDispatch*)&crypto->subtle->dispex.IDispatchEx_iface;
    IWineMSHTMLSubtleCrypto_AddRef(&crypto->subtle->IWineMSHTMLSubtleCrypto_iface);
    return S_OK;
}

static HRESULT WINAPI crypto_getRandomValues(IWineMSHTMLCrypto *iface, VARIANT *typedArray, IDispatch **ret)
{
    struct crypto *crypto = impl_from_IWineMSHTMLCrypto(iface);
    IWineDispatchProxyCbPrivate *proxy;
    HRESULT hres;

    TRACE("(%p)->(%p %p)\n", crypto, typedArray, ret);

    if(V_VT(typedArray) != VT_DISPATCH || !V_DISPATCH(typedArray))
        return E_INVALIDARG;

    if(!(proxy = crypto->dispex.proxy)) {
        FIXME("No proxy\n");
        return E_NOTIMPL;
    }

    hres = proxy->lpVtbl->GetRandomValues(V_DISPATCH(typedArray));
    if(SUCCEEDED(hres) && ret) {
        *ret = V_DISPATCH(typedArray);
        IDispatch_AddRef(*ret);
    }
    return hres;
}

static const IWineMSHTMLCryptoVtbl WineMSHTMLCryptoVtbl = {
    crypto_QueryInterface,
    crypto_AddRef,
    crypto_Release,
    crypto_GetTypeInfoCount,
    crypto_GetTypeInfo,
    crypto_GetIDsOfNames,
    crypto_Invoke,
    crypto_get_subtle,
    crypto_getRandomValues
};

static const tid_t crypto_iface_tids[] = {
    IWineMSHTMLCrypto_tid,
    0
};
dispex_static_data_t crypto_dispex = {
    "Crypto",
    NULL,
    PROTO_ID_Crypto,
    IWineMSHTMLCrypto_tid,
    crypto_iface_tids
};

void create_crypto(HTMLInnerWindow *window, IWineMSHTMLCrypto **ret)
{
    compat_mode_t compat_mode = dispex_compat_mode(&window->event_target.dispex);
    struct crypto_subtle *subtle;
    struct crypto *obj;

    if(!(obj = calloc(1, sizeof(*obj))) || !(subtle = calloc(1, sizeof(*subtle)))) {
        ERR("No memory.\n");
        free(obj);
        return;
    }

    obj->IWineMSHTMLCrypto_iface.lpVtbl = &WineMSHTMLCryptoVtbl;
    obj->ref = 1;
    obj->subtle = subtle;

    init_dispatch(&obj->dispex, (IUnknown*)&obj->IWineMSHTMLCrypto_iface, &crypto_dispex, window, compat_mode);

    subtle->IWineMSHTMLSubtleCrypto_iface.lpVtbl = &WineMSHTMLSubtleCryptoVtbl;
    subtle->ref = 1;
    init_dispatch(&subtle->dispex, (IUnknown*)&subtle->IWineMSHTMLSubtleCrypto_iface, &crypto_subtle_dispex,
                  window, compat_mode);

    *ret = &obj->IWineMSHTMLCrypto_iface;
}
