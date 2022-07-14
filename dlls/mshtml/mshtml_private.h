/*
 * Copyright 2005-2009 Jacek Caban for CodeWeavers
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

#include "wingdi.h"
#include "docobj.h"
#include "docobjectservice.h"
#include "comcat.h"
#include "mshtml.h"
#include "mshtmhst.h"
#include "hlink.h"
#include "perhist.h"
#include "dispex.h"
#include "activscp.h"
#include "objsafe.h"
#include "htiframe.h"
#include "tlogstg.h"
#include "shdeprecated.h"

#include "wine/heap.h"
#include "wine/list.h"
#include "wine/rbtree.h"

#ifdef INIT_GUID
#include "initguid.h"
#endif

#include "nsiface.h"

#include "mshtml_private_iface.h"

#include <assert.h>

/* NOTE: Keep in sync with jscript.h in jscript.dll */
DEFINE_GUID(IID_IWineDispatchProxyPrivate, 0xd359f2fe,0x5531,0x741b,0xa4,0x1a,0x5c,0xf9,0x2e,0xdc,0x97,0x1b);
typedef struct _IWineDispatchProxyPrivate IWineDispatchProxyPrivate;
typedef struct _IWineDispatchProxyCbPrivate IWineDispatchProxyCbPrivate;

struct proxy_prototypes
{
    unsigned int num;
    struct {
        IDispatch *prototype;
        IDispatch *ctor;
    } disp[];
};

struct proxy_func_invoker
{
    HRESULT (STDMETHODCALLTYPE *invoke)(IDispatch*,void*,DISPPARAMS*,VARIANT*,EXCEPINFO*,IServiceProvider*);
    void *context;
    const WCHAR *name;
};

typedef struct {
    IDispatchExVtbl dispex;
    IWineDispatchProxyCbPrivate** (STDMETHODCALLTYPE *GetProxyFieldRef)(IWineDispatchProxyPrivate *This);
    IDispatch* (STDMETHODCALLTYPE *GetDefaultPrototype)(IWineDispatchProxyPrivate *This, struct proxy_prototypes **prots_ref);
    IDispatch* (STDMETHODCALLTYPE *GetDefaultConstructor)(IWineDispatchProxyPrivate *This, struct proxy_prototypes *prots);
    HRESULT (STDMETHODCALLTYPE *DefineConstructors)(IWineDispatchProxyPrivate *This, struct proxy_prototypes **prots_ref);
    BOOL (STDMETHODCALLTYPE *IsPrototype)(IWineDispatchProxyPrivate *This);
    BOOL (STDMETHODCALLTYPE *IsConstructor)(IWineDispatchProxyPrivate *This);
    DWORD (STDMETHODCALLTYPE *PropFlags)(IWineDispatchProxyPrivate *This, DISPID id);
    HRESULT (STDMETHODCALLTYPE *PropGetID)(IWineDispatchProxyPrivate *This, WCHAR *name, DISPID *id);
    HRESULT (STDMETHODCALLTYPE *PropInvoke)(IWineDispatchProxyPrivate *This, IDispatch *this_obj, DISPID id, LCID lcid,
                                            DWORD flags, DISPPARAMS *dp, VARIANT *ret, EXCEPINFO *ei, IServiceProvider *caller);
    HRESULT (STDMETHODCALLTYPE *PropDelete)(IWineDispatchProxyPrivate *This, DISPID id);
    HRESULT (STDMETHODCALLTYPE *FuncInfo)(IWineDispatchProxyPrivate *This, DISPID id, struct proxy_func_invoker *ret);
    HRESULT (STDMETHODCALLTYPE *AccessorInfo)(IWineDispatchProxyPrivate *This, DISPID id, struct proxy_func_invoker *ret);
    HRESULT (STDMETHODCALLTYPE *ToString)(IWineDispatchProxyPrivate *This, BSTR *string);
    BOOL (STDMETHODCALLTYPE *CanGC)(IWineDispatchProxyPrivate *This);
} IWineDispatchProxyPrivateVtbl;

typedef struct {
    IDispatchExVtbl dispex;
    void (STDMETHODCALLTYPE *Unlinked)(IWineDispatchProxyCbPrivate *This);
    void (STDMETHODCALLTYPE *Relinked)(IWineDispatchProxyCbPrivate *This, IWineDispatchProxyPrivate *proxy);
    HRESULT (STDMETHODCALLTYPE *HostUpdated)(IWineDispatchProxyCbPrivate *This, IActiveScript *script);
    DISPID (STDMETHODCALLTYPE *GetUnderlyingDispID)(IWineDispatchProxyCbPrivate *This, DISPID id);
    IDispatch* (STDMETHODCALLTYPE *CreateConstructor)(IWineDispatchProxyCbPrivate *This, DISPID id, const WCHAR *name);
    HRESULT (STDMETHODCALLTYPE *DefineConstructor)(IWineDispatchProxyCbPrivate *This, const WCHAR *name, IDispatch *prot, DISPID);
    HRESULT (STDMETHODCALLTYPE *GetRandomValues)(IDispatch *typedarr);
    void (STDMETHODCALLTYPE *Traverse)(IWineDispatchProxyCbPrivate *This,
                                       void (STDMETHODCALLTYPE *note_cc_edge)(IDispatch*,void*), void *cb);
} IWineDispatchProxyCbPrivateVtbl;

struct _IWineDispatchProxyPrivate {
    const IWineDispatchProxyPrivateVtbl *lpVtbl;
};

struct _IWineDispatchProxyCbPrivate {
    const IWineDispatchProxyCbPrivateVtbl *lpVtbl;
};

#define PROPF_ARGMASK       0x00ff
#define PROPF_METHOD        0x0100
#define PROPF_CONSTR        0x0200

#define PROPF_ENUMERABLE    0x0400
#define PROPF_WRITABLE      0x0800
#define PROPF_CONFIGURABLE  0x1000
#define PROPF_ALL           (PROPF_ENUMERABLE | PROPF_WRITABLE | PROPF_CONFIGURABLE)

#define PROPF_PROXY_ACCESSOR 0x8000



#define NS_ERROR_GENERATE_FAILURE(module,code) \
    ((nsresult) (((UINT32)(1u<<31)) | ((UINT32)(module+0x45)<<16) | ((UINT32)(code))))
#define NS_ERROR_GENERATE_SUCCESS(module,code) \
    ((nsresult) (((UINT32)(module+0x45)<<16) | ((UINT32)(code))))

#define NS_OK                     ((nsresult)0x00000000L)
#define NS_ERROR_FAILURE          ((nsresult)0x80004005L)
#define NS_ERROR_OUT_OF_MEMORY    ((nsresult)0x8007000EL)
#define NS_ERROR_NOT_IMPLEMENTED  ((nsresult)0x80004001L)
#define NS_NOINTERFACE            ((nsresult)0x80004002L)
#define NS_ERROR_INVALID_POINTER  ((nsresult)0x80004003L)
#define NS_ERROR_NULL_POINTER     NS_ERROR_INVALID_POINTER
#define NS_ERROR_NOT_AVAILABLE    ((nsresult)0x80040111L)
#define NS_ERROR_INVALID_ARG      ((nsresult)0x80070057L) 
#define NS_ERROR_UNEXPECTED       ((nsresult)0x8000ffffL)
#define NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR ((nsresult)0x80530007)

#define NS_ERROR_MODULE_NETWORK    6

#define NS_BINDING_ABORTED         NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_NETWORK, 2)
#define NS_ERROR_UNKNOWN_PROTOCOL  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_NETWORK, 18)
#define NS_SUCCESS_DEFAULT_ACTION  NS_ERROR_GENERATE_SUCCESS(NS_ERROR_MODULE_NETWORK, 66)

#define NS_FAILED(res) ((res) & 0x80000000)
#define NS_SUCCEEDED(res) (!NS_FAILED(res))

#define NSAPI WINAPI

#define MSHTML_E_INVALID_PROPERTY 0x800a01b6
#define MSHTML_E_INVALID_ACTION   0x800a01bd
#define MSHTML_E_NODOC            0x800a025c
#define MSHTML_E_SYNTAX           0x800a03ea

typedef struct HTMLDOMNode HTMLDOMNode;
typedef struct ConnectionPoint ConnectionPoint;
typedef struct BSCallback BSCallback;
typedef struct EventTarget EventTarget;

#define TID_LIST \
    XIID(NULL) \
    XDIID(DispCEventObj) \
    XDIID(DispCPlugins) \
    XDIID(DispDOMChildrenCollection) \
    XDIID(DispDOMCustomEvent) \
    XDIID(DispDOMEvent) \
    XDIID(DispDOMKeyboardEvent) \
    XDIID(DispDOMMessageEvent) \
    XDIID(DispDOMMouseEvent) \
    XDIID(DispDOMUIEvent) \
    XDIID(DispDOMParser) \
    XDIID(DispHTMLAnchorElement) \
    XDIID(DispHTMLAreaElement) \
    XDIID(DispHTMLAttributeCollection) \
    XDIID(DispHTMLBody) \
    XDIID(DispHTMLButtonElement) \
    XDIID(DispHTMLCommentElement) \
    XDIID(DispHTMLCurrentStyle) \
    XDIID(DispHTMLDocument) \
    XDIID(DispHTMLDOMAttribute) \
    XDIID(DispHTMLDOMImplementation) \
    XDIID(DispHTMLDOMRange) \
    XDIID(DispHTMLDOMTextNode) \
    XDIID(DispHTMLElementCollection) \
    XDIID(DispHTMLEmbed) \
    XDIID(DispHTMLFormElement) \
    XDIID(DispHTMLGenericElement) \
    XDIID(DispHTMLFrameElement) \
    XDIID(DispHTMLHeadElement) \
    XDIID(DispHTMLHtmlElement) \
    XDIID(DispHTMLHistory) \
    XDIID(DispHTMLIFrame) \
    XDIID(DispHTMLImg) \
    XDIID(DispHTMLInputElement) \
    XDIID(DispHTMLLabelElement) \
    XDIID(DispHTMLLinkElement) \
    XDIID(DispHTMLLocation) \
    XDIID(DispHTMLMetaElement) \
    XDIID(DispHTMLNamespaceCollection) \
    XDIID(DispHTMLNavigator) \
    XDIID(DispHTMLObjectElement) \
    XDIID(DispHTMLOptionElement) \
    XDIID(DispHTMLScreen) \
    XDIID(DispHTMLScriptElement) \
    XDIID(DispHTMLSelectElement) \
    XDIID(DispHTMLStyle) \
    XDIID(DispHTMLStyleElement) \
    XDIID(DispHTMLStyleSheet) \
    XDIID(DispHTMLStyleSheetRule) \
    XDIID(DispHTMLStyleSheetRulesCollection) \
    XDIID(DispHTMLStyleSheetsCollection) \
    XDIID(DispHTMLTable) \
    XDIID(DispHTMLTableCell) \
    XDIID(DispHTMLTableRow) \
    XDIID(DispHTMLTextAreaElement) \
    XDIID(DispHTMLTitleElement) \
    XDIID(DispHTMLUnknownElement) \
    XDIID(DispHTMLW3CComputedStyle) \
    XDIID(DispHTMLWindow2) \
    XDIID(DispHTMLXMLHttpRequest) \
    XDIID(DispSVGCircleElement) \
    XDIID(DispSVGSVGElement) \
    XDIID(DispSVGTSpanElement) \
    XDIID(HTMLDocumentEvents) \
    XDIID(HTMLDocumentEvents2) \
    XDIID(HTMLElementEvents2) \
    XIID(IDOMCustomEvent) \
    XIID(IDOMEvent) \
    XIID(IDOMKeyboardEvent) \
    XIID(IDOMMessageEvent) \
    XIID(IDOMMouseEvent) \
    XIID(IDOMUIEvent) \
    XIID(IDOMParser) \
    XIID(IDocumentEvent) \
    XIID(IDocumentRange) \
    XIID(IDocumentSelector) \
    XIID(IElementSelector) \
    XIID(IElementTraversal) \
    XIID(IEventTarget) \
    XIID(IHTMLAnchorElement) \
    XIID(IHTMLAreaElement) \
    XIID(IHTMLAttributeCollection) \
    XIID(IHTMLAttributeCollection2) \
    XIID(IHTMLAttributeCollection3) \
    XIID(IHTMLBodyElement) \
    XIID(IHTMLBodyElement2) \
    XIID(IHTMLButtonElement) \
    XIID(IHTMLCSSStyleDeclaration) \
    XIID(IHTMLCSSStyleDeclaration2) \
    XIID(IHTMLCommentElement) \
    XIID(IHTMLCurrentStyle) \
    XIID(IHTMLCurrentStyle2) \
    XIID(IHTMLCurrentStyle3) \
    XIID(IHTMLCurrentStyle4) \
    XIID(IHTMLDocument2) \
    XIID(IHTMLDocument3) \
    XIID(IHTMLDocument4) \
    XIID(IHTMLDocument5) \
    XIID(IHTMLDocument6) \
    XIID(IHTMLDocument7) \
    XIID(IHTMLDOMAttribute) \
    XIID(IHTMLDOMAttribute2) \
    XIID(IHTMLDOMChildrenCollection) \
    XIID(IHTMLDOMImplementation) \
    XIID(IHTMLDOMImplementation2) \
    XIID(IHTMLDOMNode) \
    XIID(IHTMLDOMNode2) \
    XIID(IHTMLDOMNode3) \
    XIID(IHTMLDOMRange) \
    XIID(IHTMLDOMTextNode) \
    XIID(IHTMLDOMTextNode2) \
    XIID(IHTMLElement) \
    XIID(IHTMLElement2) \
    XIID(IHTMLElement3) \
    XIID(IHTMLElement4) \
    XIID(IHTMLElement6) \
    XIID(IHTMLElement7) \
    XIID(IHTMLElementCollection) \
    XIID(IHTMLEmbedElement) \
    XIID(IHTMLEventObj) \
    XIID(IHTMLFiltersCollection) \
    XIID(IHTMLFormElement) \
    XIID(IHTMLFrameBase) \
    XIID(IHTMLFrameBase2) \
    XIID(IHTMLFrameElement3) \
    XIID(IHTMLGenericElement) \
    XIID(IHTMLHeadElement) \
    XIID(IHTMLHtmlElement) \
    XIID(IHTMLIFrameElement) \
    XIID(IHTMLIFrameElement2) \
    XIID(IHTMLIFrameElement3) \
    XIID(IHTMLImageElementFactory) \
    XIID(IHTMLImgElement) \
    XIID(IHTMLInputElement) \
    XIID(IHTMLInputTextElement2) \
    XIID(IHTMLLabelElement) \
    XIID(IHTMLLinkElement) \
    XIID(IHTMLLocation) \
    XIID(IHTMLMetaElement) \
    XIID(IHTMLMimeTypesCollection) \
    XIID(IHTMLNamespaceCollection) \
    XIID(IHTMLObjectElement) \
    XIID(IHTMLObjectElement2) \
    XIID(IHTMLOptionElement) \
    XIID(IHTMLOptionElementFactory) \
    XIID(IHTMLPerformance) \
    XIID(IHTMLPerformanceNavigation) \
    XIID(IHTMLPerformanceTiming) \
    XIID(IHTMLPluginsCollection) \
    XIID(IHTMLRect) \
    XIID(IHTMLRectCollection) \
    XIID(IHTMLScreen) \
    XIID(IHTMLScriptElement) \
    XIID(IHTMLSelectElement) \
    XIID(IHTMLSelectionObject) \
    XIID(IHTMLSelectionObject2) \
    XIID(IHTMLStorage) \
    XIID(IHTMLStyle) \
    XIID(IHTMLStyle2) \
    XIID(IHTMLStyle3) \
    XIID(IHTMLStyle4) \
    XIID(IHTMLStyle5) \
    XIID(IHTMLStyle6) \
    XIID(IHTMLStyleElement) \
    XIID(IHTMLStyleElement2) \
    XIID(IHTMLStyleSheet) \
    XIID(IHTMLStyleSheet4) \
    XIID(IHTMLStyleSheetRule) \
    XIID(IHTMLCSSRule) \
    XIID(IHTMLStyleSheetRulesCollection) \
    XIID(IHTMLStyleSheetsCollection) \
    XIID(IHTMLTable) \
    XIID(IHTMLTable2) \
    XIID(IHTMLTable3) \
    XIID(IHTMLTableCell) \
    XIID(IHTMLTableRow) \
    XIID(IHTMLTextAreaElement) \
    XIID(IHTMLTextContainer) \
    XIID(IHTMLTitleElement) \
    XIID(IHTMLTxtRange) \
    XIID(IHTMLUniqueName) \
    XIID(IHTMLWindow2) \
    XIID(IHTMLWindow3) \
    XIID(IHTMLWindow4) \
    XIID(IHTMLWindow5) \
    XIID(IHTMLWindow6) \
    XIID(IHTMLWindow7) \
    XIID(IHTMLXMLHttpRequest) \
    XIID(IHTMLXMLHttpRequestFactory) \
    XIID(IOmHistory) \
    XIID(IOmNavigator) \
    XIID(ISVGCircleElement) \
    XIID(ISVGElement) \
    XIID(ISVGSVGElement) \
    XIID(ISVGTSpanElement) \
    XIID(ISVGTextContentElement)

#define PRIVATE_TID_LIST \
    XIID(IWineDOMTokenList) \
    XIID(IWineHTMLElementPrivate) \
    XIID(IWineHTMLInputPrivate) \
    XIID(IWineHTMLFormPrivate) \
    XIID(IWineHTMLParentFormPrivate) \
    XIID(IWineHTMLWindowPrivate) \
    XIID(IWineHTMLWindowCompatPrivate) \
    XIID(IWineMSHTMLConsole) \
    XIID(IWineMSHTMLCrypto) \
    XIID(IWineMSHTMLSubtleCrypto)

typedef enum {
#define XIID(iface) iface ## _tid,
#define XDIID(iface) iface ## _tid,
TID_LIST
    LAST_public_tid,
PRIVATE_TID_LIST
#undef XIID
#undef XDIID
    LAST_tid
} tid_t;

extern const tid_t no_iface_tids[1];

#define COMPAT_ONLY_PROTOTYPE_LIST \
    X(HTMLLocation,                   "Location",                     HTMLLocation_compat_dispex,             NULL) \
    X(HTMLUnknownElement,             "HTMLUnknownElement",           HTMLUnknownElement_dispex,              NULL)

#define COMPAT_PROTOTYPE_LIST \
    X(DOMParser,                      "DOMParser",                    DOMParser_dispex,                       Object) \
    X(History,                        "History",                      OmHistory_dispex,                       Object) \
    X(Navigator,                      "Navigator",                    OmNavigator_dispex,                     Object) \
    X(HTMLDOMAttribute,               "Attr",                         HTMLDOMAttribute_dispex,                HTMLDOMNode) \
    X(HTMLDOMChildrenCollection,      "NodeList",                     HTMLDOMChildrenCollection_dispex,       Object) \
    X(HTMLDOMImplementation,          "DOMImplementation",            HTMLDOMImplementation_dispex,           Object) \
    X(HTMLDOMTextNode,                "Text",                         HTMLDOMTextNode_dispex,                 DOMCharacterData) \
    X(HTMLDocument,                   "HTMLDocument",                 HTMLDocumentNode_dispex,                Document) \
    X(HTMLWindow,                     "Window",                       HTMLWindow_dispex,                      Object) \
    X(HTMLAttributeCollection,        "NamedNodeMap",                 HTMLAttributeCollection_dispex,         Object) \
    X(HTMLElementCollection,          "HTMLCollection",               HTMLElementCollection_dispex,           Object) \
    X(HTMLNamespaceCollection,        "MSNamespaceInfoCollection",    HTMLNamespaceCollection_dispex,         Object) \
    X(HTMLPluginsCollection,          "PluginArray",                  HTMLPluginsCollection_dispex,           Object) \
    X(HTMLRectCollection,             "ClientRectList",               HTMLRectCollection_dispex,              Object) \
    X(HTMLStyleSheetsCollection,      "StyleSheetList",               HTMLStyleSheetsCollection_dispex,       Object) \
    X(HTMLStyleSheetRulesCollection,  "MSCSSRuleList",                HTMLStyleSheetRulesCollection_dispex,   Object) \
    X(HTMLEventObj,                   "MSEventObj",                   HTMLEventObj_dispex,                    Object) \
    X(HTMLRect,                       "ClientRect",                   HTMLRect_dispex,                        Object) \
    X(HTMLScreen,                     "Screen",                       HTMLScreen_dispex,                      Object) \
    X(HTMLSelectionObject,            "MSSelection",                  HTMLSelectionObject_dispex,             Object) \
    X(HTMLStorage,                    "Storage",                      HTMLStorage_dispex,                     Object) \
    X(HTMLTextRange,                  "TextRange",                    HTMLTxtRange_dispex,                    Object) \
    X(HTMLXMLHttpRequest,             "XMLHttpRequest",               HTMLXMLHttpRequest_dispex,              Object) \
    X(HTMLCurrentStyle,               "MSCurrentStyleCSSProperties",  HTMLCurrentStyle_dispex,                HTMLCSSProperties) \
    X(HTMLW3CComputedStyle,           "CSSStyleDeclaration",          HTMLW3CComputedStyle_dispex,            Object) \
    X(HTMLStyleSheet,                 "CSSStyleSheet",                HTMLStyleSheet_dispex,                  StyleSheet) \
    X(HTMLStyleSheetRule,             "CSSStyleRule",                 HTMLStyleSheetRule_dispex,              CSSRule) \
    X(HTMLElement,                    "HTMLElement",                  HTMLElement_dispex,                     DOMElement) \
    X(HTMLGenericElement,             "HTMLUnknownElement",           HTMLGenericElement_dispex,              HTMLElement) \
    X(HTMLAnchorElement,              "HTMLAnchorElement",            HTMLAnchorElement_dispex,               HTMLElement) \
    X(HTMLAreaElement,                "HTMLAreaElement",              HTMLAreaElement_dispex,                 HTMLElement) \
    X(HTMLBodyElement,                "HTMLBodyElement",              HTMLBodyElement_dispex,                 HTMLElement) \
    X(HTMLButtonElement,              "HTMLButtonElement",            HTMLButtonElement_dispex,               HTMLElement) \
    X(HTMLCommentElement,             "Comment",                      HTMLCommentElement_dispex,              DOMCharacterData) \
    X(HTMLEmbedElement,               "HTMLEmbedElement",             HTMLEmbedElement_dispex,                HTMLElement) \
    X(HTMLFormElement,                "HTMLFormElement",              HTMLFormElement_dispex,                 HTMLElement) \
    X(HTMLFrameElement,               "HTMLFrameElement",             HTMLFrameElement_dispex,                HTMLElement) \
    X(HTMLHeadElement,                "HTMLHeadElement",              HTMLHeadElement_dispex,                 HTMLElement) \
    X(HTMLHtmlElement,                "HTMLHtmlElement",              HTMLHtmlElement_dispex,                 HTMLElement) \
    X(HTMLIFrameElement,              "HTMLIFrameElement",            HTMLIFrame_dispex,                      HTMLElement) \
    X(HTMLImgElement,                 "HTMLImageElement",             HTMLImgElement_dispex,                  HTMLElement) \
    X(HTMLInputElement,               "HTMLInputElement",             HTMLInputElement_dispex,                HTMLElement) \
    X(HTMLLabelElement,               "HTMLLabelElement",             HTMLLabelElement_dispex,                HTMLElement) \
    X(HTMLLinkElement,                "HTMLLinkElement",              HTMLLinkElement_dispex,                 HTMLElement) \
    X(HTMLMetaElement,                "HTMLMetaElement",              HTMLMetaElement_dispex,                 HTMLElement) \
    X(HTMLObjectElement,              "HTMLObjectElement",            HTMLObjectElement_dispex,               HTMLElement) \
    X(HTMLOptionElement,              "HTMLOptionElement",            HTMLOptionElement_dispex,               HTMLElement) \
    X(HTMLScriptElement,              "HTMLScriptElement",            HTMLScriptElement_dispex,               HTMLElement) \
    X(HTMLSelectElement,              "HTMLSelectElement",            HTMLSelectElement_dispex,               HTMLElement) \
    X(HTMLStyleElement,               "HTMLStyleElement",             HTMLStyleElement_dispex,                HTMLElement) \
    X(HTMLTableElement,               "HTMLTableElement",             HTMLTable_dispex,                       HTMLElement) \
    X(HTMLTableCellElement,           "HTMLTableDataCellElement",     HTMLTableCell_dispex,                   HTMLTableCellProt) \
    X(HTMLTableRowElement,            "HTMLTableRowElement",          HTMLTableRow_dispex,                    HTMLElement) \
    X(HTMLTextAreaElement,            "HTMLTextAreaElement",          HTMLTextAreaElement_dispex,             HTMLElement) \
    X(HTMLTitleElement,               "HTMLTitleElement",             HTMLTitleElement_dispex,                HTMLElement)

#define PROXY_PROTOTYPE_LIST \
    X(Console,                        "Console",                      console_dispex,                         Object) \
    X(Crypto,                         "Crypto",                       crypto_dispex,                          Object) \
    X(SubtleCrypto,                   "SubtleCrypto",                 crypto_subtle_dispex,                   Object) \
    X(DOMEvent,                       "Event",                        DOMEvent_dispex,                        Object) \
    X(DOMCustomEvent,                 "CustomEvent",                  DOMCustomEvent_dispex,                  DOMEvent) \
    X(DOMKeyboardEvent,               "KeyboardEvent",                DOMKeyboardEvent_dispex,                DOMUIEvent) \
    X(DOMMessageEvent,                "MessageEvent",                 DOMMessageEvent_dispex,                 DOMEvent) \
    X(DOMMouseEvent,                  "MouseEvent",                   DOMMouseEvent_dispex,                   DOMUIEvent) \
    X(DOMUIEvent,                     "UIEvent",                      DOMUIEvent_dispex,                      DOMEvent) \
    X(DOMCharacterData,               "CharacterData",                DOMCharacterData_dispex,                HTMLDOMNode) \
    X(Document,                       "Document",                     DocumentNode_dispex,                    HTMLDOMNode) \
    X(XMLDocument,                    "XMLDocument",                  XMLDocumentNode_dispex,                 Document) \
    X(DOMElement,                     "Element",                      DOMElement_dispex,                      HTMLDOMNode) \
    X(CSSRule,                        "CSSRule",                      CSSRule_dispex,                         Object) \
    X(StyleSheet,                     "StyleSheet",                   StyleSheet_dispex,                      Object) \
    X(DOMTokenList,                   "DOMTokenList",                 DOMTokenList_dispex,                    Object) \
    X(HTMLDOMNode,                    "Node",                         HTMLDOMNode_dispex,                     Object) \
    X(HTMLDOMRange,                   "Range",                        HTMLDOMRange_dispex,                    Object) \
    X(HTMLFiltersCollection,          "FiltersCollection",            HTMLFiltersCollection_dispex,           Object) \
    X(HTMLMimeTypesCollection,        "MimeTypeArray",                HTMLMimeTypesCollection_dispex,         Object) \
    X(HTMLPerformance,                "Performance",                  HTMLPerformance_dispex,                 Object) \
    X(HTMLPerformanceNavigation,      "PerformanceNavigation",        HTMLPerformanceNavigation_dispex,       Object) \
    X(HTMLPerformanceTiming,          "PerformanceTiming",            HTMLPerformanceTiming_dispex,           Object) \
    X(HTMLCSSProperties,              "MSCSSProperties",              HTMLCSSProperties_dispex,               HTMLW3CComputedStyle) \
    X(HTMLStyle,                      "MSStyleCSSProperties",         HTMLStyle_dispex,                       HTMLCSSProperties) \
    X(HTMLTableCellProt,              "HTMLTableCellElement",         HTMLTableCellProt_dispex,               HTMLElement)

typedef enum {
    PROTO_ID_NULL = -2,
    PROTO_ID_Object = -1,  /* jscript Object.prototype */
#define X(id, name, dispex, proto_id) PROTO_ID_ ## id,
COMPAT_ONLY_PROTOTYPE_LIST
    COMPAT_ONLY_PROTOTYPE_COUNT,
    PROTO_ID_LAST_COMPAT_ONLY = COMPAT_ONLY_PROTOTYPE_COUNT - 1,
COMPAT_PROTOTYPE_LIST
    COMPAT_PROTOTYPE_COUNT,
    PROTO_ID_LAST_COMPAT = COMPAT_PROTOTYPE_COUNT - 1,
PROXY_PROTOTYPE_LIST
#undef X
} prototype_id_t;

typedef enum {
#define X(id, name, dispex, proto_id) COMPAT_CTOR_ID_ ## id,
COMPAT_ONLY_PROTOTYPE_LIST
COMPAT_PROTOTYPE_LIST
#undef X
    /* extra ctors that share prototypes */
    COMPAT_CTOR_ID_Image,
    COMPAT_CTOR_ID_Option,

    COMPAT_CTOR_ID_Image_builtin,
    COMPAT_CTOR_ID_Option_builtin,
    COMPAT_CTOR_ID_HTMLXMLHttpRequest_builtin,

    COMPAT_CTOR_COUNT
} compat_ctor_id_t;

typedef enum {
    COMPAT_MODE_INVALID = -1,
    COMPAT_MODE_QUIRKS,
    COMPAT_MODE_IE5,
    COMPAT_MODE_IE7,
    COMPAT_MODE_IE8,
    COMPAT_MODE_IE9,
    COMPAT_MODE_IE10,
    COMPAT_MODE_IE11
} compat_mode_t;

#define COMPAT_MODE_CNT (COMPAT_MODE_IE11+1)
#define COMPAT_MODE_NONE COMPAT_MODE_QUIRKS

typedef struct {
    unsigned document_mode;
    unsigned ie_version;
} compat_mode_info_t;

extern const compat_mode_info_t compat_mode_info[COMPAT_MODE_CNT] DECLSPEC_HIDDEN;

typedef struct dispex_data_t dispex_data_t;
typedef struct dispex_dynamic_data_t dispex_dynamic_data_t;

#define MSHTML_DISPID_CUSTOM_MIN 0x60000000
#define MSHTML_DISPID_CUSTOM_MAX 0x6fffffff
#define MSHTML_CUSTOM_DISPID_CNT (MSHTML_DISPID_CUSTOM_MAX-MSHTML_DISPID_CUSTOM_MIN)

typedef struct HTMLDocumentNode HTMLDocumentNode;
typedef struct DispatchEx DispatchEx;
struct compat_prototype;

typedef struct {
    HRESULT (*value)(DispatchEx*,LCID,WORD,DISPPARAMS*,VARIANT*,EXCEPINFO*,IServiceProvider*);
    HRESULT (*get_dispid)(DispatchEx*,BSTR,DWORD,DISPID*);
    HRESULT (*invoke)(DispatchEx*,IDispatch*,DISPID,LCID,WORD,DISPPARAMS*,VARIANT*,EXCEPINFO*,IServiceProvider*);
    HRESULT (*delete)(DispatchEx*,DISPID);
    HRESULT (*get_static_dispid)(compat_mode_t,BSTR,DWORD,DISPID*);
    compat_mode_t (*get_compat_mode)(DispatchEx*);
    HRESULT (*populate_props)(DispatchEx*);
} dispex_static_data_vtbl_t;

typedef struct {
    const WCHAR *name;
    const dispex_static_data_vtbl_t *vtbl;
    const prototype_id_t prototype_id;
    const tid_t disp_tid;
    const tid_t* const iface_tids;
    void (*init_info)(dispex_data_t*,compat_mode_t);
    dispex_data_t *info_cache[COMPAT_MODE_CNT];
    dispex_data_t *delayed_init_info;
} dispex_static_data_t;

typedef HRESULT (*dispex_hook_invoke_t)(DispatchEx*,WORD,DISPPARAMS*,VARIANT*,
                                        EXCEPINFO*,IServiceProvider*);

typedef struct {
    DISPID dispid;
    dispex_hook_invoke_t invoke;
} dispex_hook_t;

struct DispatchEx {
    IDispatchEx IDispatchEx_iface;

    IUnknown *outer;
    IWineDispatchProxyCbPrivate *proxy;
    struct compat_prototype *prototype;

    dispex_data_t *info;
    dispex_dynamic_data_t *dynamic_data;
};

typedef struct {
    UINT_PTR x;
} nsCycleCollectingAutoRefCnt;

typedef struct {
    void *vtbl;
    int ref_flags;
    void *callbacks;
} ExternalCycleCollectionParticipant;

typedef struct nsCycleCollectionTraversalCallback nsCycleCollectionTraversalCallback;

typedef struct {
    nsresult (NSAPI *traverse)(void*,void*,nsCycleCollectionTraversalCallback*);
    nsresult (NSAPI *unlink)(void*);
    void (NSAPI *delete_cycle_collectable)(void*);
} CCObjCallback;

DEFINE_GUID(IID_nsXPCOMCycleCollectionParticipant, 0x9674489b,0x1f6f,0x4550,0xa7,0x30, 0xcc,0xae,0xdd,0x10,0x4c,0xf9);

extern nsrefcnt (__cdecl *ccref_incr)(nsCycleCollectingAutoRefCnt*,nsISupports*) DECLSPEC_HIDDEN;
extern nsrefcnt (__cdecl *ccref_decr)(nsCycleCollectingAutoRefCnt*,nsISupports*,ExternalCycleCollectionParticipant*) DECLSPEC_HIDDEN;
extern void (__cdecl *ccref_init)(nsCycleCollectingAutoRefCnt*,nsrefcnt) DECLSPEC_HIDDEN;
extern void (__cdecl *ccp_init)(ExternalCycleCollectionParticipant*,const CCObjCallback*) DECLSPEC_HIDDEN;
extern void (__cdecl *describe_cc_node)(nsCycleCollectingAutoRefCnt*,const char*,nsCycleCollectionTraversalCallback*) DECLSPEC_HIDDEN;
extern void (__cdecl *note_cc_edge)(nsISupports*,const char*,nsCycleCollectionTraversalCallback*) DECLSPEC_HIDDEN;

void init_dispatch(DispatchEx*,IUnknown*,dispex_static_data_t*,HTMLDocumentNode*,compat_mode_t) DECLSPEC_HIDDEN;
void release_dispex(DispatchEx*) DECLSPEC_HIDDEN;
void update_dispex(DispatchEx*,dispex_static_data_t*,HTMLDocumentNode*,compat_mode_t) DECLSPEC_HIDDEN;
BOOL dispex_query_interface(DispatchEx*,REFIID,void**) DECLSPEC_HIDDEN;
HRESULT change_type(VARIANT*,VARIANT*,VARTYPE,IServiceProvider*) DECLSPEC_HIDDEN;
HRESULT dispex_get_dprop_ref(DispatchEx*,const WCHAR*,BOOL,VARIANT**) DECLSPEC_HIDDEN;
HRESULT get_dispids(tid_t,DWORD*,DISPID**) DECLSPEC_HIDDEN;
BOOL is_custom_attribute(DispatchEx*,const WCHAR*) DECLSPEC_HIDDEN;
HRESULT remove_attribute(DispatchEx*,DISPID,VARIANT_BOOL*) DECLSPEC_HIDDEN;
HRESULT dispex_get_dynid(DispatchEx*,const WCHAR*,DISPID*) DECLSPEC_HIDDEN;
HRESULT dispex_invoke(DispatchEx*,IDispatch*,DISPID,LCID,WORD,DISPPARAMS*,VARIANT*,EXCEPINFO*,IServiceProvider*) DECLSPEC_HIDDEN;
HRESULT dispex_delete_prop(DispatchEx*,DISPID) DECLSPEC_HIDDEN;
void dispex_traverse(DispatchEx*,nsCycleCollectionTraversalCallback*) DECLSPEC_HIDDEN;
void dispex_unlink(DispatchEx*) DECLSPEC_HIDDEN;
void release_typelib(void) DECLSPEC_HIDDEN;
HRESULT get_class_typeinfo(const CLSID*,ITypeInfo**) DECLSPEC_HIDDEN;
const void *dispex_get_vtbl(DispatchEx*) DECLSPEC_HIDDEN;
void dispex_info_add_interface(dispex_data_t*,tid_t,const dispex_hook_t*) DECLSPEC_HIDDEN;
compat_mode_t dispex_compat_mode(DispatchEx*) DECLSPEC_HIDDEN;
HRESULT dispex_to_string(DispatchEx*,BSTR*) DECLSPEC_HIDDEN;
HRESULT dispex_call_builtin(DispatchEx *dispex, DISPID id, DISPPARAMS *dp,
                            VARIANT *res, EXCEPINFO *ei, IServiceProvider *caller) DECLSPEC_HIDDEN;
BOOL dispex_is_builtin_attribute(DispatchEx*,DISPID); DECLSPEC_HIDDEN;
BOOL dispex_is_builtin_method(DispatchEx*,DISPID) DECLSPEC_HIDDEN;
BOOL dispex_is_builtin_value(DispatchEx*,DISPID) DECLSPEC_HIDDEN;

typedef enum {
    DISPEXPROP_CUSTOM,
    DISPEXPROP_DYNAMIC,
    DISPEXPROP_BUILTIN
} dispex_prop_type_t;

dispex_prop_type_t get_dispid_type(DISPID) DECLSPEC_HIDDEN;

typedef struct HTMLWindow HTMLWindow;
typedef struct HTMLInnerWindow HTMLInnerWindow;
typedef struct HTMLOuterWindow HTMLOuterWindow;
typedef struct HTMLDocumentObj HTMLDocumentObj;
typedef struct HTMLFrameBase HTMLFrameBase;
typedef struct GeckoBrowser GeckoBrowser;
typedef struct HTMLAttributeCollection HTMLAttributeCollection;

typedef struct ScriptHost ScriptHost;

struct compat_ctor {
    DispatchEx dispex;
    union {
        IUnknown IUnknown_iface;
        IHTMLOptionElementFactory IHTMLOptionElementFactory_iface;
        IHTMLImageElementFactory IHTMLImageElementFactory_iface;
        IHTMLXMLHttpRequestFactory IHTMLXMLHttpRequestFactory_iface;
    };

    LONG ref;

    prototype_id_t prot_id;
    HTMLInnerWindow *window;
};

struct compat_prototype {
    IUnknown IUnknown_iface;
    DispatchEx dispex;
    LONG ref;

    HTMLInnerWindow *window;
};

typedef enum {
    GLOBAL_SCRIPTVAR,
    GLOBAL_ELEMENTVAR,
    GLOBAL_DISPEXVAR,
    GLOBAL_FRAMEVAR
} global_prop_type_t;

typedef struct {
    global_prop_type_t type;
    WCHAR *name;
    ScriptHost *script_host;
    DISPID id;
} global_prop_t;

struct EventTarget {
    DispatchEx dispex;
    IEventTarget IEventTarget_iface;
    struct wine_rb_tree handler_map;
};

struct HTMLLocation {
    DispatchEx dispex;
    IHTMLLocation IHTMLLocation_iface;

    LONG ref;

    HTMLInnerWindow *window;
};

typedef struct {
    DispatchEx dispex;
    IOmHistory IOmHistory_iface;

    LONG ref;

    HTMLInnerWindow *window;
} OmHistory;

typedef struct nsChannelBSC nsChannelBSC;

struct HTMLWindow {
    IHTMLWindow2       IHTMLWindow2_iface;
    IHTMLWindow3       IHTMLWindow3_iface;
    IHTMLWindow4       IHTMLWindow4_iface;
    IHTMLWindow5       IHTMLWindow5_iface;
    IHTMLWindow6       IHTMLWindow6_iface;
    IHTMLWindow7       IHTMLWindow7_iface;
    IHTMLPrivateWindow IHTMLPrivateWindow_iface;
    IDispatchEx        IDispatchEx_iface;
    IServiceProvider   IServiceProvider_iface;
    ITravelLogClient   ITravelLogClient_iface;
    IObjectIdentity    IObjectIdentity_iface;
    IProvideMultipleClassInfo IProvideMultipleClassInfo_iface;
    IWineHTMLWindowPrivate IWineHTMLWindowPrivate_iface;
    IWineHTMLWindowCompatPrivate IWineHTMLWindowCompatPrivate_iface;

    IWineMSHTMLConsole *console;
    IWineMSHTMLCrypto *crypto;

    LONG ref;

    HTMLInnerWindow *inner_window;
    HTMLOuterWindow *outer_window;
};

struct HTMLOuterWindow {
    HTMLWindow base;

    LONG task_magic;

    nsIDOMWindow *nswindow;
    mozIDOMWindowProxy *window_proxy;
    HTMLOuterWindow *parent;
    HTMLFrameBase *frame_element;
    IWineDispatchProxyCbPrivate *saved_proxy;

    GeckoBrowser *browser;
    struct list browser_entry;

    READYSTATE readystate;
    BOOL readystate_locked;
    unsigned readystate_pending;

    HTMLInnerWindow *pending_window;
    IMoniker *mon;
    IUri *uri;
    IUri *uri_nofrag;
    BSTR url;
    DWORD load_flags;

    struct list sibling_entry;
    struct wine_rb_entry entry;
};

struct HTMLInnerWindow {
    HTMLWindow base;
    EventTarget event_target;

    HTMLDocumentNode *doc;

    struct list children;
    struct list script_hosts;

    IHTMLEventObj *event;

    IHTMLScreen *screen;
    OmHistory *history;
    IOmNavigator *navigator;
    IHTMLStorage *session_storage;
    IHTMLStorage *local_storage;

    BOOL performance_initialized;
    VARIANT performance;

    unsigned parser_callback_cnt;
    struct list script_queue;

    global_prop_t *global_props;
    DWORD global_prop_cnt;
    DWORD global_prop_size;

    LONG task_magic;

    HTMLLocation *location;

    IMoniker *mon;
    nsChannelBSC *bscallback;
    struct list bindings;

    struct compat_ctor *compat_ctors[COMPAT_CTOR_COUNT];
    struct compat_prototype *compat_prototypes[COMPAT_PROTOTYPE_COUNT];
};

typedef enum {
    UNKNOWN_USERMODE,
    BROWSEMODE,
    EDITMODE        
} USERMODE;

typedef struct _cp_static_data_t {
    tid_t tid;
    void (*on_advise)(IUnknown*,struct _cp_static_data_t*);
    BOOL pass_event_arg;
    DWORD id_cnt;
    DISPID *ids;
} cp_static_data_t;

typedef struct {
    const IID *riid;
    cp_static_data_t *desc;
} cpc_entry_t;

typedef struct ConnectionPointContainer {
    IConnectionPointContainer IConnectionPointContainer_iface;

    ConnectionPoint *cps;
    const cpc_entry_t *cp_entries;
    IUnknown *outer;
    struct ConnectionPointContainer *forward_container;
} ConnectionPointContainer;

struct  ConnectionPoint {
    IConnectionPoint IConnectionPoint_iface;

    ConnectionPointContainer *container;

    union {
        IUnknown *unk;
        IDispatch *disp;
        IPropertyNotifySink *propnotif;
    } *sinks;
    DWORD sinks_size;

    const IID *iid;
    cp_static_data_t *data;
};

typedef enum {
    DOCTYPE_INVALID = -1,
    DOCTYPE_HTML,
    DOCTYPE_XHTML,
    DOCTYPE_XML,
    DOCTYPE_SVG,
    DOCTYPE_COUNT
} document_type_t;

extern dispex_static_data_t *const dispex_from_document_type[DOCTYPE_COUNT] DECLSPEC_HIDDEN;
extern const WCHAR *const content_type_from_document_type[DOCTYPE_COUNT] DECLSPEC_HIDDEN;

struct HTMLDocument {
    IHTMLDocument2              IHTMLDocument2_iface;
    IHTMLDocument3              IHTMLDocument3_iface;
    IHTMLDocument4              IHTMLDocument4_iface;
    IHTMLDocument5              IHTMLDocument5_iface;
    IHTMLDocument6              IHTMLDocument6_iface;
    IHTMLDocument7              IHTMLDocument7_iface;
    IDocumentSelector           IDocumentSelector_iface;
    IDocumentEvent              IDocumentEvent_iface;
    IPersistMoniker             IPersistMoniker_iface;
    IPersistFile                IPersistFile_iface;
    IPersistHistory             IPersistHistory_iface;
    IMonikerProp                IMonikerProp_iface;
    IOleObject                  IOleObject_iface;
    IOleDocument                IOleDocument_iface;
    IOleInPlaceActiveObject     IOleInPlaceActiveObject_iface;
    IOleInPlaceObjectWindowless IOleInPlaceObjectWindowless_iface;
    IServiceProvider            IServiceProvider_iface;
    IOleCommandTarget           IOleCommandTarget_iface;
    IOleControl                 IOleControl_iface;
    IHlinkTarget                IHlinkTarget_iface;
    IPersistStreamInit          IPersistStreamInit_iface;
    IDispatchEx                 IDispatchEx_iface;
    ISupportErrorInfo           ISupportErrorInfo_iface;
    IObjectWithSite             IObjectWithSite_iface;
    IOleContainer               IOleContainer_iface;
    IObjectSafety               IObjectSafety_iface;
    IProvideMultipleClassInfo   IProvideMultipleClassInfo_iface;
    IMarkupServices             IMarkupServices_iface;
    IMarkupContainer            IMarkupContainer_iface;
    IDisplayServices            IDisplayServices_iface;
    IDocumentRange              IDocumentRange_iface;

    document_type_t doc_type;

    IUnknown *outer_unk;
    DispatchEx *dispex;

    HTMLDocumentObj *doc_obj;
    HTMLDocumentNode *doc_node;

    HTMLOuterWindow *window;

    ConnectionPointContainer cp_container;
};

static inline HRESULT htmldoc_query_interface(HTMLDocument *This, REFIID riid, void **ppv)
{
    return IUnknown_QueryInterface(This->outer_unk, riid, ppv);
}

static inline ULONG htmldoc_addref(HTMLDocument *This)
{
    return IUnknown_AddRef(This->outer_unk);
}

static inline ULONG htmldoc_release(HTMLDocument *This)
{
    return IUnknown_Release(This->outer_unk);
}

struct HTMLDocumentObj {
    HTMLDocument basedoc;
    DispatchEx dispex;
    IUnknown IUnknown_inner;
    ICustomDoc ICustomDoc_iface;
    IOleDocumentView IOleDocumentView_iface;
    IViewObjectEx IViewObjectEx_iface;
    ITargetContainer ITargetContainer_iface;

    IWindowForBindingUI IWindowForBindingUI_iface;

    LONG ref;

    GeckoBrowser *nscontainer;

    IOleClientSite *client;
    IDocHostUIHandler *hostui;
    IOleCommandTarget *client_cmdtrg;
    BOOL custom_hostui;
    IOleInPlaceSite *ipsite;
    IOleInPlaceFrame *frame;
    IOleInPlaceUIWindow *ip_window;
    IAdviseSink *view_sink;
    IDocObjectService *doc_object_service;
    IUnknown *webbrowser;
    ITravelLog *travel_log;
    IUnknown *browser_service;
    IOleAdviseHolder *advise_holder;

    DOCHOSTUIINFO hostinfo;

    IOleUndoManager *undomgr;
    IHTMLEditServices *editsvcs;

    HWND hwnd;
    HWND tooltips_hwnd;

    BOOL is_mhtml;
    BOOL request_uiactivate;
    BOOL in_place_active;
    BOOL ui_active;
    BOOL window_active;
    BOOL hostui_setup;
    BOOL container_locked;
    BOOL focus;
    BOOL has_popup;
    INT download_state;

    LPWSTR mime;

    DWORD update;
    LONG task_magic;
    SIZEL extent;
};

typedef struct nsWeakReference nsWeakReference;


typedef enum {
    SCRIPTMODE_GECKO,
    SCRIPTMODE_ACTIVESCRIPT
} SCRIPTMODE;

struct GeckoBrowser {
    nsIWebBrowserChrome      nsIWebBrowserChrome_iface;
    nsIContextMenuListener   nsIContextMenuListener_iface;
    nsIURIContentListener    nsIURIContentListener_iface;
    nsIEmbeddingSiteWindow   nsIEmbeddingSiteWindow_iface;
    nsITooltipListener       nsITooltipListener_iface;
    nsIInterfaceRequestor    nsIInterfaceRequestor_iface;
    nsISupportsWeakReference nsISupportsWeakReference_iface;

    nsIWebBrowser *webbrowser;
    nsIWebNavigation *navigation;
    nsIBaseWindow *window;
    nsIWebBrowserFocus *focus;

    HTMLOuterWindow *content_window;

    nsIEditor *editor;
    nsIController *editor_controller;

    LONG ref;

    nsWeakReference *weak_reference;

    HTMLDocumentObj *doc;

    nsIURIContentListener *content_listener;

    HWND hwnd;
    SCRIPTMODE script_mode;
    USERMODE usermode;

    struct list document_nodes;
    struct list outer_windows;
};

typedef struct {
    const CLSID *clsid;
    HRESULT (*qi)(HTMLDOMNode*,REFIID,void**);
    void (*destructor)(HTMLDOMNode*);
    const cpc_entry_t *cpc_entries;
    HRESULT (*clone)(HTMLDOMNode*,nsIDOMNode*,HTMLDOMNode**);
    HRESULT (*handle_event)(HTMLDOMNode*,DWORD,nsIDOMEvent*,BOOL*);
    HRESULT (*get_attr_col)(HTMLDOMNode*,HTMLAttributeCollection**);
    EventTarget *(*get_event_prop_target)(HTMLDOMNode*,int);
    HRESULT (*put_disabled)(HTMLDOMNode*,VARIANT_BOOL);
    HRESULT (*get_disabled)(HTMLDOMNode*,VARIANT_BOOL*);
    HRESULT (*get_document)(HTMLDOMNode*,IDispatch**);
    HRESULT (*get_readystate)(HTMLDOMNode*,BSTR*);
    HRESULT (*get_dispid)(HTMLDOMNode*,BSTR,DWORD,DISPID*);
    HRESULT (*invoke)(HTMLDOMNode*,IDispatch*,DISPID,LCID,WORD,DISPPARAMS*,VARIANT*,EXCEPINFO*,IServiceProvider*);
    HRESULT (*bind_to_tree)(HTMLDOMNode*);
    void (*traverse)(HTMLDOMNode*,nsCycleCollectionTraversalCallback*);
    void (*unlink)(HTMLDOMNode*);
    BOOL (*is_text_edit)(HTMLDOMNode*);
    BOOL (*is_settable)(HTMLDOMNode*,DISPID);
} NodeImplVtbl;

struct HTMLDOMNode {
    EventTarget   event_target;
    IHTMLDOMNode  IHTMLDOMNode_iface;
    IHTMLDOMNode2 IHTMLDOMNode2_iface;
    IHTMLDOMNode3 IHTMLDOMNode3_iface;
    const NodeImplVtbl *vtbl;

    nsCycleCollectingAutoRefCnt ccref;

    nsIDOMNode *nsnode;
    HTMLDocumentNode *doc;
};

HTMLDOMNode *unsafe_impl_from_IHTMLDOMNode(IHTMLDOMNode*) DECLSPEC_HIDDEN;

static inline void node_addref(HTMLDOMNode *node)
{
    IHTMLDOMNode_AddRef(&node->IHTMLDOMNode_iface);
}

static inline void node_release(HTMLDOMNode *node)
{
    IHTMLDOMNode_Release(&node->IHTMLDOMNode_iface);
}

typedef struct {
    HTMLDOMNode node;
    ConnectionPointContainer cp_container;

    IHTMLElement  IHTMLElement_iface;
    IHTMLElement2 IHTMLElement2_iface;
    IHTMLElement3 IHTMLElement3_iface;
    IHTMLElement4 IHTMLElement4_iface;
    IHTMLElement6 IHTMLElement6_iface;
    IHTMLElement7 IHTMLElement7_iface;
    IHTMLUniqueName IHTMLUniqueName_iface;
    IElementSelector IElementSelector_iface;
    IElementTraversal IElementTraversal_iface;
    IProvideMultipleClassInfo IProvideMultipleClassInfo_iface;
    IWineHTMLElementPrivate IWineHTMLElementPrivate_iface;

    nsIDOMElement *dom_element;       /* NULL for legacy comments represented as HTML elements */
    nsIDOMHTMLElement *html_element;  /* NULL for non-HTML elements (like SVG elements) */
    HTMLStyle *style;
    HTMLStyle *runtime_style;
    HTMLAttributeCollection *attrs;
    WCHAR *filter;
    unsigned unique_id;
} HTMLElement;

#define HTMLELEMENT_TIDS    \
    IHTMLDOMNode_tid,       \
    IHTMLDOMNode2_tid,      \
    IHTMLElement_tid,       \
    IHTMLElement3_tid,      \
    IHTMLElement4_tid,      \
    IHTMLUniqueName_tid

extern cp_static_data_t HTMLElementEvents2_data DECLSPEC_HIDDEN;
#define HTMLELEMENT_CPC {&DIID_HTMLElementEvents2, &HTMLElementEvents2_data}
extern const cpc_entry_t HTMLElement_cpc[] DECLSPEC_HIDDEN;

struct HTMLFrameBase {
    HTMLElement element;

    IHTMLFrameBase  IHTMLFrameBase_iface;
    IHTMLFrameBase2 IHTMLFrameBase2_iface;

    HTMLOuterWindow *content_window;

    nsIDOMHTMLFrameElement *nsframe;
    nsIDOMHTMLIFrameElement *nsiframe;
};

typedef struct nsDocumentEventListener nsDocumentEventListener;

struct HTMLDocumentNode {
    HTMLDOMNode node;
    HTMLDocument basedoc;

    IInternetHostSecurityManager IInternetHostSecurityManager_iface;

    nsIDocumentObserver          nsIDocumentObserver_iface;

    LONG ref;

    HTMLInnerWindow *window;

    GeckoBrowser *browser;
    struct list browser_entry;

    compat_mode_t document_mode;
    BOOL document_mode_locked;

    nsIDOMDocument *nsdoc;
    nsIDOMHTMLDocument *nshtmldoc;
    BOOL content_ready;

    IHTMLDOMImplementation *dom_implementation;
    IHTMLNamespaceCollection *namespaces;

    ICatInformation *catmgr;
    nsDocumentEventListener *nsevent_listener;
    BOOL *event_vector;

    WCHAR **elem_vars;
    unsigned elem_vars_size;
    unsigned elem_vars_cnt;

    BOOL skip_mutation_notif;

    UINT charset;

    unsigned unique_id;

    struct list selection_list;
    struct list range_list;
    struct list plugin_hosts;
};

HRESULT HTMLDocument_Create(IUnknown*,REFIID,void**) DECLSPEC_HIDDEN;
HRESULT MHTMLDocument_Create(IUnknown*,REFIID,void**) DECLSPEC_HIDDEN;
HRESULT HTMLLoadOptions_Create(IUnknown*,REFIID,void**) DECLSPEC_HIDDEN;
HRESULT create_document_node(nsIDOMDocument*,GeckoBrowser*,HTMLInnerWindow*,
                             document_type_t,compat_mode_t,HTMLDocumentNode**) DECLSPEC_HIDDEN;
HRESULT create_xml_document(BSTR,HTMLDocumentNode*,document_type_t,BOOL,IDispatch**) DECLSPEC_HIDDEN;
HRESULT create_marshaled_doc(HWND,REFIID,void**) DECLSPEC_HIDDEN;

HRESULT create_outer_window(GeckoBrowser*,mozIDOMWindowProxy*,HTMLOuterWindow*,HTMLOuterWindow**) DECLSPEC_HIDDEN;
HRESULT update_window_doc(HTMLInnerWindow*) DECLSPEC_HIDDEN;
HTMLOuterWindow *mozwindow_to_window(const mozIDOMWindowProxy*) DECLSPEC_HIDDEN;
void get_top_window(HTMLOuterWindow*,HTMLOuterWindow**) DECLSPEC_HIDDEN;
HRESULT compat_ctor_value(DispatchEx*,LCID,WORD,DISPPARAMS*,VARIANT*,EXCEPINFO*,IServiceProvider*);
HRESULT compat_ctor_get_dispid(DispatchEx*,BSTR,DWORD,DISPID*) DECLSPEC_HIDDEN;
HRESULT compat_ctor_invoke(DispatchEx*,IDispatch*,DISPID,LCID,WORD,DISPPARAMS*,VARIANT*,EXCEPINFO*,IServiceProvider*) DECLSPEC_HIDDEN;
HRESULT compat_ctor_delete(DispatchEx*,DISPID) DECLSPEC_HIDDEN;
HRESULT HTMLLocation_Create(HTMLInnerWindow*,HTMLLocation**) DECLSPEC_HIDDEN;
HRESULT create_navigator(HTMLDocumentNode*,compat_mode_t,IOmNavigator**) DECLSPEC_HIDDEN;
HRESULT create_html_screen(HTMLDocumentNode*,compat_mode_t,IHTMLScreen**) DECLSPEC_HIDDEN;
HRESULT create_performance(compat_mode_t,IHTMLPerformance**) DECLSPEC_HIDDEN;
HRESULT create_history(HTMLInnerWindow*,OmHistory**) DECLSPEC_HIDDEN;
HRESULT create_namespace_collection(HTMLDocumentNode*,IHTMLNamespaceCollection**) DECLSPEC_HIDDEN;
HRESULT create_dom_implementation(HTMLDocumentNode*,IHTMLDOMImplementation**) DECLSPEC_HIDDEN;
void detach_dom_implementation(IHTMLDOMImplementation*) DECLSPEC_HIDDEN;

HRESULT create_html_storage(compat_mode_t,HTMLDocumentNode*,IHTMLStorage**) DECLSPEC_HIDDEN;

void HTMLDocument_Persist_Init(HTMLDocument*) DECLSPEC_HIDDEN;
void HTMLDocument_OleCmd_Init(HTMLDocument*) DECLSPEC_HIDDEN;
void HTMLDocument_OleObj_Init(HTMLDocument*) DECLSPEC_HIDDEN;
void HTMLDocument_Service_Init(HTMLDocument*) DECLSPEC_HIDDEN;

void HTMLDocument_View_Init(HTMLDocumentObj*) DECLSPEC_HIDDEN;
void TargetContainer_Init(HTMLDocumentObj*) DECLSPEC_HIDDEN;

void HTMLDocumentNode_SecMgr_Init(HTMLDocumentNode*) DECLSPEC_HIDDEN;

HRESULT HTMLCurrentStyle_Create(HTMLElement*,IHTMLCurrentStyle**) DECLSPEC_HIDDEN;

void ConnectionPointContainer_Init(ConnectionPointContainer*,IUnknown*,const cpc_entry_t*) DECLSPEC_HIDDEN;
void ConnectionPointContainer_Destroy(ConnectionPointContainer*) DECLSPEC_HIDDEN;

HRESULT create_gecko_browser(HTMLDocumentObj*,GeckoBrowser**) DECLSPEC_HIDDEN;
void detach_gecko_browser(GeckoBrowser*) DECLSPEC_HIDDEN;

compat_mode_t lock_document_mode(HTMLDocumentNode*) DECLSPEC_HIDDEN;

void init_mutation(nsIComponentManager*) DECLSPEC_HIDDEN;
void init_document_mutation(HTMLDocumentNode*) DECLSPEC_HIDDEN;
void release_document_mutation(HTMLDocumentNode*) DECLSPEC_HIDDEN;
JSContext *get_context_from_document(nsIDOMDocument*) DECLSPEC_HIDDEN;

void HTMLDocument_LockContainer(HTMLDocumentObj*,BOOL) DECLSPEC_HIDDEN;
void show_context_menu(HTMLDocumentObj*,DWORD,POINT*,IDispatch*) DECLSPEC_HIDDEN;
void notif_focus(HTMLDocumentObj*) DECLSPEC_HIDDEN;

void show_tooltip(HTMLDocumentObj*,DWORD,DWORD,LPCWSTR) DECLSPEC_HIDDEN;
void hide_tooltip(HTMLDocumentObj*) DECLSPEC_HIDDEN;
HRESULT get_client_disp_property(IOleClientSite*,DISPID,VARIANT*) DECLSPEC_HIDDEN;

UINT get_document_charset(HTMLDocumentNode*) DECLSPEC_HIDDEN;

HRESULT ProtocolFactory_Create(REFCLSID,REFIID,void**) DECLSPEC_HIDDEN;

BOOL load_gecko(void) DECLSPEC_HIDDEN;
void close_gecko(void) DECLSPEC_HIDDEN;
void register_nsservice(nsIComponentRegistrar*,nsIServiceManager*) DECLSPEC_HIDDEN;
void init_nsio(nsIComponentManager*) DECLSPEC_HIDDEN;
void release_nsio(void) DECLSPEC_HIDDEN;
BOOL is_gecko_path(const char*) DECLSPEC_HIDDEN;
void set_viewer_zoom(GeckoBrowser*,float) DECLSPEC_HIDDEN;
float get_viewer_zoom(GeckoBrowser*) DECLSPEC_HIDDEN;

void init_node_cc(void) DECLSPEC_HIDDEN;

HRESULT nsuri_to_url(LPCWSTR,BOOL,BSTR*) DECLSPEC_HIDDEN;

void call_property_onchanged(ConnectionPointContainer*,DISPID) DECLSPEC_HIDDEN;
HRESULT call_set_active_object(IOleInPlaceUIWindow*,IOleInPlaceActiveObject*) DECLSPEC_HIDDEN;

void *nsalloc(size_t) __WINE_ALLOC_SIZE(1) DECLSPEC_HIDDEN;
void nsfree(void*) DECLSPEC_HIDDEN;

BOOL nsACString_Init(nsACString *str, const char *data) DECLSPEC_HIDDEN;
void nsACString_InitDepend(nsACString*,const char*) DECLSPEC_HIDDEN;
void nsACString_SetData(nsACString*,const char*) DECLSPEC_HIDDEN;
UINT32 nsACString_GetData(const nsACString*,const char**) DECLSPEC_HIDDEN;
void nsACString_Finish(nsACString*) DECLSPEC_HIDDEN;

BOOL nsAString_Init(nsAString*,const PRUnichar*) DECLSPEC_HIDDEN;
void nsAString_InitDepend(nsAString*,const PRUnichar*) DECLSPEC_HIDDEN;
void nsAString_SetData(nsAString*,const PRUnichar*) DECLSPEC_HIDDEN;
UINT32 nsAString_GetData(const nsAString*,const PRUnichar**) DECLSPEC_HIDDEN;
void nsAString_Finish(nsAString*) DECLSPEC_HIDDEN;

#define NSSTR_IMPLICIT_PX    0x01
#define NSSTR_COLOR          0x02

HRESULT map_nsresult(nsresult) DECLSPEC_HIDDEN;
HRESULT return_nsstr(nsresult,nsAString*,BSTR*) DECLSPEC_HIDDEN;
HRESULT return_nsstr_variant(nsresult,nsAString*,unsigned,VARIANT*) DECLSPEC_HIDDEN;
HRESULT variant_to_nsstr(VARIANT*,BOOL,nsAString*) DECLSPEC_HIDDEN;
HRESULT return_nsform(nsresult,nsIDOMHTMLFormElement*,IHTMLFormElement**) DECLSPEC_HIDDEN;

nsICommandParams *create_nscommand_params(void) DECLSPEC_HIDDEN;
HRESULT nsnode_to_nsstring(nsIDOMNode*,nsAString*) DECLSPEC_HIDDEN;
void setup_editor_controller(GeckoBrowser*) DECLSPEC_HIDDEN;
nsresult get_nsinterface(nsISupports*,REFIID,void**) DECLSPEC_HIDDEN;
nsIWritableVariant *create_nsvariant(void) DECLSPEC_HIDDEN;
nsIDOMParser *create_nsdomparser(HTMLDocumentNode*) DECLSPEC_HIDDEN;
nsIXMLHttpRequest *create_nsxhr(nsIDOMWindow *nswindow) DECLSPEC_HIDDEN;
nsresult create_nsfile(const PRUnichar*,nsIFile**) DECLSPEC_HIDDEN;
char *get_nscategory_entry(const char*,const char*) DECLSPEC_HIDDEN;

HRESULT create_pending_window(HTMLOuterWindow*,nsChannelBSC*) DECLSPEC_HIDDEN;
HRESULT start_binding(HTMLInnerWindow*,BSCallback*,IBindCtx*) DECLSPEC_HIDDEN;
HRESULT async_start_doc_binding(HTMLOuterWindow*,HTMLInnerWindow*) DECLSPEC_HIDDEN;
void abort_window_bindings(HTMLInnerWindow*) DECLSPEC_HIDDEN;
void set_download_state(HTMLDocumentObj*,int) DECLSPEC_HIDDEN;
void call_docview_84(HTMLDocumentObj*) DECLSPEC_HIDDEN;

void set_ready_state(HTMLOuterWindow*,READYSTATE) DECLSPEC_HIDDEN;
HRESULT get_readystate_string(READYSTATE,BSTR*) DECLSPEC_HIDDEN;

HRESULT HTMLSelectionObject_Create(HTMLDocumentNode*,nsISelection*,IHTMLSelectionObject**) DECLSPEC_HIDDEN;
HRESULT HTMLTxtRange_Create(HTMLDocumentNode*,nsIDOMRange*,IHTMLTxtRange**) DECLSPEC_HIDDEN;
HRESULT create_style_sheet(nsIDOMStyleSheet*,HTMLDocumentNode*,IHTMLStyleSheet**) DECLSPEC_HIDDEN;
HRESULT create_style_sheet_collection(nsIDOMStyleSheetList*,HTMLDocumentNode*,
                                      IHTMLStyleSheetsCollection**) DECLSPEC_HIDDEN;
HRESULT create_dom_range(nsIDOMRange*,compat_mode_t,IHTMLDOMRange**) DECLSPEC_HIDDEN;
HRESULT create_markup_pointer(IMarkupPointer**) DECLSPEC_HIDDEN;

void detach_document_node(HTMLDocumentNode*) DECLSPEC_HIDDEN;
void detach_selection(HTMLDocumentNode*) DECLSPEC_HIDDEN;
void detach_ranges(HTMLDocumentNode*) DECLSPEC_HIDDEN;
HRESULT get_node_text(HTMLDOMNode*,BSTR*) DECLSPEC_HIDDEN;
HRESULT replace_node_by_html(nsIDOMDocument*,nsIDOMNode*,const WCHAR*) DECLSPEC_HIDDEN;

HRESULT create_nselem(HTMLDocumentNode*,const WCHAR*,nsIDOMElement**) DECLSPEC_HIDDEN;
HRESULT create_element(HTMLDocumentNode*,const WCHAR*,HTMLElement**) DECLSPEC_HIDDEN;

HRESULT HTMLDOMTextNode_Create(HTMLDocumentNode*,nsIDOMNode*,HTMLDOMNode**) DECLSPEC_HIDDEN;

BOOL variant_to_nscolor(const VARIANT *v, nsAString *nsstr) DECLSPEC_HIDDEN;
HRESULT nscolor_to_str(LPCWSTR color, BSTR *ret) DECLSPEC_HIDDEN;

static inline BOOL is_main_content_window(HTMLOuterWindow *window)
{
    return window->browser && window == window->browser->content_window;
}

struct HTMLAttributeCollection {
    DispatchEx dispex;
    IHTMLAttributeCollection IHTMLAttributeCollection_iface;
    IHTMLAttributeCollection2 IHTMLAttributeCollection2_iface;
    IHTMLAttributeCollection3 IHTMLAttributeCollection3_iface;

    LONG ref;

    nsIDOMMozNamedAttrMap *nsattrs;
    HTMLElement *elem;
    struct list attrs;
};

typedef struct {
    /* valid only when attribute nodes are used (node.nsnode) */
    HTMLDOMNode node;

    IHTMLDOMAttribute IHTMLDOMAttribute_iface;
    IHTMLDOMAttribute2 IHTMLDOMAttribute2_iface;

    LONG ref;

    /* value is valid only for detached attributes (when elem == NULL). */
    VARIANT value;
    /* name must be valid for detached attributes */
    WCHAR *name;

    HTMLElement *elem;
    DISPID dispid;
    struct list entry;
} HTMLDOMAttribute;

HTMLDOMAttribute *unsafe_impl_from_IHTMLDOMAttribute(IHTMLDOMAttribute*) DECLSPEC_HIDDEN;

HRESULT HTMLDOMAttribute_Create(const WCHAR*,HTMLDocumentNode*,HTMLElement*,DISPID,nsIDOMAttr*,
                                compat_mode_t,HTMLDOMAttribute**) DECLSPEC_HIDDEN;

HRESULT HTMLElement_Create(HTMLDocumentNode*,nsIDOMNode*,BOOL,HTMLElement**) DECLSPEC_HIDDEN;
HRESULT HTMLCommentElement_Create(HTMLDocumentNode*,nsIDOMNode*,HTMLElement**) DECLSPEC_HIDDEN;
HRESULT HTMLAnchorElement_Create(HTMLDocumentNode*,nsIDOMElement*,HTMLElement**) DECLSPEC_HIDDEN;
HRESULT HTMLAreaElement_Create(HTMLDocumentNode*,nsIDOMElement*,HTMLElement**) DECLSPEC_HIDDEN;
HRESULT HTMLBodyElement_Create(HTMLDocumentNode*,nsIDOMElement*,HTMLElement**) DECLSPEC_HIDDEN;
HRESULT HTMLButtonElement_Create(HTMLDocumentNode*,nsIDOMElement*,HTMLElement**) DECLSPEC_HIDDEN;
HRESULT HTMLEmbedElement_Create(HTMLDocumentNode*,nsIDOMElement*,HTMLElement**) DECLSPEC_HIDDEN;
HRESULT HTMLFormElement_Create(HTMLDocumentNode*,nsIDOMElement*,HTMLElement**) DECLSPEC_HIDDEN;
HRESULT HTMLFrameElement_Create(HTMLDocumentNode*,nsIDOMElement*,HTMLElement**) DECLSPEC_HIDDEN;
HRESULT HTMLHeadElement_Create(HTMLDocumentNode*,nsIDOMElement*,HTMLElement**) DECLSPEC_HIDDEN;
HRESULT HTMLHtmlElement_Create(HTMLDocumentNode*,nsIDOMElement*,HTMLElement**) DECLSPEC_HIDDEN;
HRESULT HTMLIFrame_Create(HTMLDocumentNode*,nsIDOMElement*,HTMLElement**) DECLSPEC_HIDDEN;
HRESULT HTMLStyleElement_Create(HTMLDocumentNode*,nsIDOMElement*,HTMLElement**) DECLSPEC_HIDDEN;
HRESULT HTMLImgElement_Create(HTMLDocumentNode*,nsIDOMElement*,HTMLElement**) DECLSPEC_HIDDEN;
HRESULT HTMLInputElement_Create(HTMLDocumentNode*,nsIDOMElement*,HTMLElement**) DECLSPEC_HIDDEN;
HRESULT HTMLLabelElement_Create(HTMLDocumentNode*,nsIDOMElement*,HTMLElement**) DECLSPEC_HIDDEN;
HRESULT HTMLLinkElement_Create(HTMLDocumentNode*,nsIDOMElement*,HTMLElement**) DECLSPEC_HIDDEN;
HRESULT HTMLMetaElement_Create(HTMLDocumentNode*,nsIDOMElement*,HTMLElement**) DECLSPEC_HIDDEN;
HRESULT HTMLObjectElement_Create(HTMLDocumentNode*,nsIDOMElement*,HTMLElement**) DECLSPEC_HIDDEN;
HRESULT HTMLOptionElement_Create(HTMLDocumentNode*,nsIDOMElement*,HTMLElement**) DECLSPEC_HIDDEN;
HRESULT HTMLScriptElement_Create(HTMLDocumentNode*,nsIDOMElement*,HTMLElement**) DECLSPEC_HIDDEN;
HRESULT HTMLSelectElement_Create(HTMLDocumentNode*,nsIDOMElement*,HTMLElement**) DECLSPEC_HIDDEN;
HRESULT HTMLTable_Create(HTMLDocumentNode*,nsIDOMElement*,HTMLElement**) DECLSPEC_HIDDEN;
HRESULT HTMLTableCell_Create(HTMLDocumentNode*,nsIDOMElement*,HTMLElement**) DECLSPEC_HIDDEN;
HRESULT HTMLTableRow_Create(HTMLDocumentNode*,nsIDOMElement*,HTMLElement**) DECLSPEC_HIDDEN;
HRESULT HTMLTextAreaElement_Create(HTMLDocumentNode*,nsIDOMElement*,HTMLElement**) DECLSPEC_HIDDEN;
HRESULT HTMLTitleElement_Create(HTMLDocumentNode*,nsIDOMElement*,HTMLElement**) DECLSPEC_HIDDEN;
HRESULT HTMLGenericElement_Create(HTMLDocumentNode*,nsIDOMElement*,HTMLElement**) DECLSPEC_HIDDEN;

HRESULT create_svg_element(HTMLDocumentNode*,nsIDOMSVGElement*,const WCHAR*,HTMLElement**) DECLSPEC_HIDDEN;

void HTMLDOMNode_Init(HTMLDocumentNode*,HTMLDOMNode*,nsIDOMNode*,dispex_static_data_t*) DECLSPEC_HIDDEN;
void HTMLElement_Init(HTMLElement*,HTMLDocumentNode*,nsIDOMElement*,dispex_static_data_t*) DECLSPEC_HIDDEN;

void EventTarget_Init(EventTarget*,IUnknown*,dispex_static_data_t*,HTMLDocumentNode*) DECLSPEC_HIDDEN;
HRESULT EventTarget_QI(EventTarget*,REFIID,void**) DECLSPEC_HIDDEN;
void EventTarget_init_dispex_info(dispex_data_t*,compat_mode_t) DECLSPEC_HIDDEN;

HRESULT HTMLDOMNode_QI(HTMLDOMNode*,REFIID,void**) DECLSPEC_HIDDEN;
void HTMLDOMNode_destructor(HTMLDOMNode*) DECLSPEC_HIDDEN;
void HTMLDOMNode_init_dispex_info(dispex_data_t*,compat_mode_t) DECLSPEC_HIDDEN;

HRESULT HTMLElement_QI(HTMLDOMNode*,REFIID,void**) DECLSPEC_HIDDEN;
void HTMLElement_destructor(HTMLDOMNode*) DECLSPEC_HIDDEN;
HRESULT HTMLElement_clone(HTMLDOMNode*,nsIDOMNode*,HTMLDOMNode**) DECLSPEC_HIDDEN;
HRESULT HTMLElement_get_attr_col(HTMLDOMNode*,HTMLAttributeCollection**) DECLSPEC_HIDDEN;
HRESULT HTMLElement_handle_event(HTMLDOMNode*,DWORD,nsIDOMEvent*,BOOL*) DECLSPEC_HIDDEN;
void HTMLElement_init_dispex_info(dispex_data_t*,compat_mode_t) DECLSPEC_HIDDEN;

HRESULT get_node(nsIDOMNode*,BOOL,HTMLDOMNode**) DECLSPEC_HIDDEN;
HRESULT get_element(nsIDOMElement*,HTMLElement**) DECLSPEC_HIDDEN;
HRESULT get_document_node(nsIDOMDocument*,HTMLDocumentNode**) DECLSPEC_HIDDEN;

HTMLElement *unsafe_impl_from_IHTMLElement(IHTMLElement*) DECLSPEC_HIDDEN;

HRESULT search_window_props(HTMLInnerWindow*,BSTR,DWORD,DISPID*) DECLSPEC_HIDDEN;
HRESULT get_frame_by_name(HTMLOuterWindow*,const WCHAR*,BOOL,HTMLOuterWindow**) DECLSPEC_HIDDEN;
HRESULT get_doc_elem_by_id(HTMLDocumentNode*,const WCHAR*,HTMLElement**) DECLSPEC_HIDDEN;
HTMLOuterWindow *get_target_window(HTMLOuterWindow*,nsAString*,BOOL*) DECLSPEC_HIDDEN;
HRESULT handle_link_click_event(HTMLElement*,nsAString*,nsAString*,nsIDOMEvent*,BOOL*) DECLSPEC_HIDDEN;

HRESULT wrap_iface(IUnknown*,IUnknown*,IUnknown**) DECLSPEC_HIDDEN;

IHTMLElementCollection *create_all_collection(HTMLDOMNode*,BOOL) DECLSPEC_HIDDEN;
IHTMLElementCollection *create_collection_from_nodelist(nsIDOMNodeList*,HTMLDocumentNode*) DECLSPEC_HIDDEN;
IHTMLElementCollection *create_collection_from_htmlcol(nsIDOMHTMLCollection*,HTMLDocumentNode*) DECLSPEC_HIDDEN;
HRESULT create_child_collection(nsIDOMNodeList*,HTMLDocumentNode*,IHTMLDOMChildrenCollection**) DECLSPEC_HIDDEN;

HRESULT attr_value_to_string(VARIANT*) DECLSPEC_HIDDEN;
HRESULT get_elem_attr_value_by_dispid(HTMLElement*,DISPID,VARIANT*) DECLSPEC_HIDDEN;
HRESULT get_elem_source_index(HTMLElement*,LONG*) DECLSPEC_HIDDEN;

nsresult get_elem_attr_value(nsIDOMElement*,const WCHAR*,nsAString*,const PRUnichar**) DECLSPEC_HIDDEN;
HRESULT elem_string_attr_getter(HTMLElement*,const WCHAR*,BOOL,BSTR*) DECLSPEC_HIDDEN;
HRESULT elem_string_attr_setter(HTMLElement*,const WCHAR*,const WCHAR*) DECLSPEC_HIDDEN;

HRESULT elem_unique_id(unsigned id, BSTR *p) DECLSPEC_HIDDEN;

/* commands */
typedef struct {
    DWORD id;
    HRESULT (*query)(HTMLDocumentNode*,OLECMD*);
    HRESULT (*exec)(HTMLDocumentNode*,DWORD,VARIANT*,VARIANT*);
} cmdtable_t;

extern const cmdtable_t editmode_cmds[] DECLSPEC_HIDDEN;

void do_ns_command(HTMLDocumentNode*,const char*,nsICommandParams*) DECLSPEC_HIDDEN;

/* timer */
#define UPDATE_UI       0x0001
#define UPDATE_TITLE    0x0002

void update_doc(HTMLDocumentObj*,DWORD) DECLSPEC_HIDDEN;
void update_title(HTMLDocumentObj*) DECLSPEC_HIDDEN;
void set_document_navigation(HTMLDocumentObj*,BOOL) DECLSPEC_HIDDEN;

HRESULT do_query_service(IUnknown*,REFGUID,REFIID,void**) DECLSPEC_HIDDEN;

/* editor */
HRESULT setup_edit_mode(HTMLDocumentObj*) DECLSPEC_HIDDEN;
void init_editor(HTMLDocumentNode*) DECLSPEC_HIDDEN;
void handle_edit_event(HTMLDocumentNode*,nsIDOMEvent*) DECLSPEC_HIDDEN;
HRESULT editor_exec_copy(HTMLDocumentNode*,DWORD,VARIANT*,VARIANT*) DECLSPEC_HIDDEN;
HRESULT editor_exec_cut(HTMLDocumentNode*,DWORD,VARIANT*,VARIANT*) DECLSPEC_HIDDEN;
HRESULT editor_exec_paste(HTMLDocumentNode*,DWORD,VARIANT*,VARIANT*) DECLSPEC_HIDDEN;
HRESULT browser_is_dirty(GeckoBrowser*) DECLSPEC_HIDDEN;
void set_dirty(GeckoBrowser*,VARIANT_BOOL) DECLSPEC_HIDDEN;

extern DWORD mshtml_tls DECLSPEC_HIDDEN;

typedef struct task_t task_t;
typedef void (*task_proc_t)(task_t*);

struct task_t {
    LONG target_magic;
    task_proc_t proc;
    task_proc_t destr;
    struct list entry;
};

typedef struct {
    task_t header;
    HTMLDocumentObj *doc;
} docobj_task_t;

typedef struct {
    HWND thread_hwnd;
    struct list task_list;
    struct list timer_list;
} thread_data_t;

thread_data_t *get_thread_data(BOOL) DECLSPEC_HIDDEN;
HWND get_thread_hwnd(void) DECLSPEC_HIDDEN;

LONG get_task_target_magic(void) DECLSPEC_HIDDEN;
HRESULT push_task(task_t*,task_proc_t,task_proc_t,LONG) DECLSPEC_HIDDEN;
void remove_target_tasks(LONG) DECLSPEC_HIDDEN;
ULONGLONG get_time_stamp(void) DECLSPEC_HIDDEN;

enum timer_type {
    TIMER_TIMEOUT,
    TIMER_INTERVAL,
    TIMER_ANIMATION_FRAME,
};

HRESULT set_task_timer(HTMLInnerWindow*,LONG,enum timer_type,IDispatch*,LONG*) DECLSPEC_HIDDEN;
HRESULT clear_task_timer(HTMLInnerWindow*,DWORD) DECLSPEC_HIDDEN;
HRESULT clear_animation_timer(HTMLInnerWindow*,DWORD) DECLSPEC_HIDDEN;

const WCHAR *parse_compat_version(const WCHAR*,compat_mode_t*) DECLSPEC_HIDDEN;

const char *debugstr_mshtml_guid(const GUID*) DECLSPEC_HIDDEN;

DEFINE_GUID(CLSID_AboutProtocol, 0x3050F406, 0x98B5, 0x11CF, 0xBB,0x82, 0x00,0xAA,0x00,0xBD,0xCE,0x0B);
DEFINE_GUID(CLSID_JSProtocol, 0x3050F3B2, 0x98B5, 0x11CF, 0xBB,0x82, 0x00,0xAA,0x00,0xBD,0xCE,0x0B);
DEFINE_GUID(CLSID_MailtoProtocol, 0x3050F3DA, 0x98B5, 0x11CF, 0xBB,0x82, 0x00,0xAA,0x00,0xBD,0xCE,0x0B);
DEFINE_GUID(CLSID_ResProtocol, 0x3050F3BC, 0x98B5, 0x11CF, 0xBB,0x82, 0x00,0xAA,0x00,0xBD,0xCE,0x0B);
DEFINE_GUID(CLSID_SysimageProtocol, 0x76E67A63, 0x06E9, 0x11D2, 0xA8,0x40, 0x00,0x60,0x08,0x05,0x93,0x82);

DEFINE_GUID(CLSID_CMarkup,0x3050f4fb,0x98b5,0x11cf,0xbb,0x82,0x00,0xaa,0x00,0xbd,0xce,0x0b);

DEFINE_OLEGUID(CGID_DocHostCmdPriv, 0x000214D4L, 0, 0);

DEFINE_GUID(CLSID_JScript, 0xf414c260,0x6ac0,0x11cf, 0xb6,0xd1,0x00,0xaa,0x00,0xbb,0xbb,0x58);
DEFINE_GUID(CLSID_VBScript, 0xb54f3741,0x5b07,0x11cf, 0xa4,0xb0,0x00,0xaa,0x00,0x4a,0x55,0xe8);

DEFINE_GUID(IID_UndocumentedScriptIface,0x719c3050,0xf9d3,0x11cf,0xa4,0x93,0x00,0x40,0x05,0x23,0xa8,0xa0);
DEFINE_GUID(IID_IDispatchJS,0x719c3050,0xf9d3,0x11cf,0xa4,0x93,0x00,0x40,0x05,0x23,0xa8,0xa6);

/* memory allocation functions */

static inline void * __WINE_ALLOC_SIZE(2) heap_realloc_zero(void *mem, size_t len)
{
    return HeapReAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, mem, len);
}

static inline LPWSTR heap_strdupW(LPCWSTR str)
{
    LPWSTR ret = NULL;

    if(str) {
        DWORD size;

        size = (lstrlenW(str)+1)*sizeof(WCHAR);
        ret = heap_alloc(size);
        if(ret)
            memcpy(ret, str, size);
    }

    return ret;
}

static inline LPWSTR heap_strndupW(LPCWSTR str, unsigned len)
{
    LPWSTR ret = NULL;

    if(str) {
        ret = heap_alloc((len+1)*sizeof(WCHAR));
        if(ret)
        {
            memcpy(ret, str, len*sizeof(WCHAR));
            ret[len] = 0;
        }
    }

    return ret;
}

static inline char *heap_strdupA(const char *str)
{
    char *ret = NULL;

    if(str) {
        DWORD size;

        size = strlen(str)+1;
        ret = heap_alloc(size);
        if(ret)
            memcpy(ret, str, size);
    }

    return ret;
}

static inline WCHAR *heap_strdupAtoW(const char *str)
{
    LPWSTR ret = NULL;

    if(str) {
        DWORD len;

        len = MultiByteToWideChar(CP_ACP, 0, str, -1, NULL, 0);
        ret = heap_alloc(len*sizeof(WCHAR));
        if(ret)
            MultiByteToWideChar(CP_ACP, 0, str, -1, ret, len);
    }

    return ret;
}

static inline char *heap_strdupWtoA(LPCWSTR str)
{
    char *ret = NULL;

    if(str) {
        DWORD size = WideCharToMultiByte(CP_ACP, 0, str, -1, NULL, 0, NULL, NULL);
        ret = heap_alloc(size);
        if(ret)
            WideCharToMultiByte(CP_ACP, 0, str, -1, ret, size, NULL, NULL);
    }

    return ret;
}

static inline WCHAR *heap_strdupUtoW(const char *str)
{
    WCHAR *ret = NULL;

    if(str) {
        size_t len;

        len = MultiByteToWideChar(CP_UTF8, 0, str, -1, NULL, 0);
        ret = heap_alloc(len*sizeof(WCHAR));
        if(ret)
            MultiByteToWideChar(CP_UTF8, 0, str, -1, ret, len);
    }

    return ret;
}

static inline char *heap_strdupWtoU(const WCHAR *str)
{
    char *ret = NULL;

    if(str) {
        size_t size = WideCharToMultiByte(CP_UTF8, 0, str, -1, NULL, 0, NULL, NULL);
        ret = heap_alloc(size);
        if(ret)
            WideCharToMultiByte(CP_UTF8, 0, str, -1, ret, size, NULL, NULL);
    }

    return ret;
}

static inline char *heap_strndupWtoU(LPCWSTR str, unsigned len)
{
    char *ret = NULL;
    DWORD size;

    if(str) {
        size = len ? WideCharToMultiByte(CP_UTF8, 0, str, len, NULL, 0, NULL, NULL) : 0;
        ret = heap_alloc(size + 1);
        if(ret) {
            if(len) WideCharToMultiByte(CP_UTF8, 0, str, len, ret, size, NULL, NULL);
            ret[size] = '\0';
        }
    }

    return ret;
}

static inline VARIANT_BOOL variant_bool(BOOL b)
{
    return b ? VARIANT_TRUE : VARIANT_FALSE;
}

static inline BOOL is_digit(WCHAR c)
{
    return '0' <= c && c <= '9';
}

#ifdef __i386__
extern void *call_thiscall_func;
#endif

compat_mode_t get_max_compat_mode(IUri*) DECLSPEC_HIDDEN;
UINT cp_from_charset_string(BSTR) DECLSPEC_HIDDEN;
BSTR charset_string_from_cp(UINT) DECLSPEC_HIDDEN;
HRESULT get_mime_type_display_name(const WCHAR*,BSTR*) DECLSPEC_HIDDEN;
HINSTANCE get_shdoclc(void) DECLSPEC_HIDDEN;
void set_statustext(HTMLDocumentObj*,INT,LPCWSTR) DECLSPEC_HIDDEN;
IInternetSecurityManager *get_security_manager(void) DECLSPEC_HIDDEN;

extern HINSTANCE hInst DECLSPEC_HIDDEN;
void create_console(compat_mode_t compat_mode, IWineMSHTMLConsole **ret) DECLSPEC_HIDDEN;
void create_crypto(compat_mode_t compat_mode, IWineMSHTMLCrypto **ret) DECLSPEC_HIDDEN;

extern const IHTMLImageElementFactoryVtbl HTMLImageElementFactoryVtbl DECLSPEC_HIDDEN;
extern const IHTMLOptionElementFactoryVtbl HTMLOptionElementFactoryVtbl DECLSPEC_HIDDEN;
extern const IHTMLXMLHttpRequestFactoryVtbl HTMLXMLHttpRequestFactoryVtbl DECLSPEC_HIDDEN;
extern dispex_static_data_t HTMLImageElementFactory_dispex DECLSPEC_HIDDEN;
extern dispex_static_data_t HTMLOptionElementFactory_dispex DECLSPEC_HIDDEN;
extern dispex_static_data_t HTMLXMLHttpRequestFactory_dispex DECLSPEC_HIDDEN;
extern dispex_static_data_t HTMLImageCtor_dispex DECLSPEC_HIDDEN;
extern dispex_static_data_t HTMLOptionCtor_dispex DECLSPEC_HIDDEN;
extern dispex_static_data_t HTMLXMLHttpRequestCtor_dispex DECLSPEC_HIDDEN;
extern dispex_static_data_t DOMParserCtor_dispex DECLSPEC_HIDDEN;

#define X(id, name, dispex, proto_id) extern dispex_static_data_t dispex DECLSPEC_HIDDEN;
COMPAT_ONLY_PROTOTYPE_LIST
COMPAT_PROTOTYPE_LIST
PROXY_PROTOTYPE_LIST
#undef X
