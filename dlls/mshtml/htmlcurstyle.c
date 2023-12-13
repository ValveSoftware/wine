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
#include "htmlstyle.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(mshtml);

struct HTMLCurrentStyle {
    CSSStyle css_style;
    IHTMLCurrentStyle  IHTMLCurrentStyle_iface;
    IHTMLCurrentStyle2 IHTMLCurrentStyle2_iface;
    IHTMLCurrentStyle3 IHTMLCurrentStyle3_iface;
    IHTMLCurrentStyle4 IHTMLCurrentStyle4_iface;
    IHTMLStyle         IHTMLStyle_iface;
    IHTMLStyle2        IHTMLStyle2_iface;
    IHTMLStyle3        IHTMLStyle3_iface;
    IHTMLStyle5        IHTMLStyle5_iface;
    IHTMLStyle6        IHTMLStyle6_iface;

    HTMLElement *elem;
};

static inline HRESULT get_current_style_property(HTMLCurrentStyle *current_style, styleid_t sid, BSTR *p)
{
    return get_style_property(&current_style->css_style, sid, p);
}

static inline HRESULT get_current_style_property_var(HTMLCurrentStyle *This, styleid_t sid, VARIANT *v)
{
    return get_style_property_var(&This->css_style, sid, v);
}

static inline HTMLCurrentStyle *impl_from_IHTMLCurrentStyle(IHTMLCurrentStyle *iface)
{
    return CONTAINING_RECORD(iface, HTMLCurrentStyle, IHTMLCurrentStyle_iface);
}

static inline HTMLCurrentStyle *impl_from_IHTMLCurrentStyle2(IHTMLCurrentStyle2 *iface)
{
    return CONTAINING_RECORD(iface, HTMLCurrentStyle, IHTMLCurrentStyle2_iface);
}

static inline HTMLCurrentStyle *impl_from_IHTMLCurrentStyle3(IHTMLCurrentStyle3 *iface)
{
    return CONTAINING_RECORD(iface, HTMLCurrentStyle, IHTMLCurrentStyle3_iface);
}

static inline HTMLCurrentStyle *impl_from_IHTMLCurrentStyle4(IHTMLCurrentStyle4 *iface)
{
    return CONTAINING_RECORD(iface, HTMLCurrentStyle, IHTMLCurrentStyle4_iface);
}

static inline HTMLCurrentStyle *impl_from_IHTMLStyle(IHTMLStyle *iface)
{
    return CONTAINING_RECORD(iface, HTMLCurrentStyle, IHTMLStyle_iface);
}

static inline HTMLCurrentStyle *impl_from_IHTMLStyle2(IHTMLStyle2 *iface)
{
    return CONTAINING_RECORD(iface, HTMLCurrentStyle, IHTMLStyle2_iface);
}

static inline HTMLCurrentStyle *impl_from_IHTMLStyle3(IHTMLStyle3 *iface)
{
    return CONTAINING_RECORD(iface, HTMLCurrentStyle, IHTMLStyle3_iface);
}

static inline HTMLCurrentStyle *impl_from_IHTMLStyle5(IHTMLStyle5 *iface)
{
    return CONTAINING_RECORD(iface, HTMLCurrentStyle, IHTMLStyle5_iface);
}

static inline HTMLCurrentStyle *impl_from_IHTMLStyle6(IHTMLStyle6 *iface)
{
    return CONTAINING_RECORD(iface, HTMLCurrentStyle, IHTMLStyle6_iface);
}

static HRESULT WINAPI HTMLCurrentStyle_QueryInterface(IHTMLCurrentStyle *iface, REFIID riid, void **ppv)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    TRACE("(%p)->(%s %p)\n", This, debugstr_mshtml_guid(riid), ppv);
    return IHTMLCSSStyleDeclaration_QueryInterface(&This->css_style.IHTMLCSSStyleDeclaration_iface, riid, ppv);
}

static ULONG WINAPI HTMLCurrentStyle_AddRef(IHTMLCurrentStyle *iface)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    TRACE("(%p)\n", This);
    return IHTMLCSSStyleDeclaration_AddRef(&This->css_style.IHTMLCSSStyleDeclaration_iface);
}

static ULONG WINAPI HTMLCurrentStyle_Release(IHTMLCurrentStyle *iface)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    TRACE("(%p)\n", This);
    return IHTMLCSSStyleDeclaration_Release(&This->css_style.IHTMLCSSStyleDeclaration_iface);
}

static HRESULT WINAPI HTMLCurrentStyle_GetTypeInfoCount(IHTMLCurrentStyle *iface, UINT *pctinfo)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    return IDispatchEx_GetTypeInfoCount(&This->css_style.dispex.IDispatchEx_iface, pctinfo);
}

static HRESULT WINAPI HTMLCurrentStyle_GetTypeInfo(IHTMLCurrentStyle *iface, UINT iTInfo,
        LCID lcid, ITypeInfo **ppTInfo)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    return IDispatchEx_GetTypeInfo(&This->css_style.dispex.IDispatchEx_iface, iTInfo, lcid, ppTInfo);
}

static HRESULT WINAPI HTMLCurrentStyle_GetIDsOfNames(IHTMLCurrentStyle *iface, REFIID riid,
        LPOLESTR *rgszNames, UINT cNames, LCID lcid, DISPID *rgDispId)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    return IDispatchEx_GetIDsOfNames(&This->css_style.dispex.IDispatchEx_iface, riid, rgszNames, cNames,
            lcid, rgDispId);
}

static HRESULT WINAPI HTMLCurrentStyle_Invoke(IHTMLCurrentStyle *iface, DISPID dispIdMember,
        REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS *pDispParams,
        VARIANT *pVarResult, EXCEPINFO *pExcepInfo, UINT *puArgErr)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    return IDispatchEx_Invoke(&This->css_style.dispex.IDispatchEx_iface, dispIdMember, riid, lcid,
            wFlags, pDispParams, pVarResult, pExcepInfo, puArgErr);
}

static HRESULT WINAPI HTMLCurrentStyle_get_position(IHTMLCurrentStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);

    TRACE("(%p)->(%p)\n", This, p);

    return get_current_style_property(This, STYLEID_POSITION, p);
}

static HRESULT WINAPI HTMLCurrentStyle_get_styleFloat(IHTMLCurrentStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);

    TRACE("(%p)->(%p)\n", This, p);

    return get_current_style_property(This, STYLEID_FLOAT, p);
}

static HRESULT WINAPI HTMLCurrentStyle_get_color(IHTMLCurrentStyle *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    TRACE("(%p)->(%p)\n", This, p);
    return get_current_style_property_var(This, STYLEID_COLOR, p);
}

static HRESULT WINAPI HTMLCurrentStyle_get_backgroundColor(IHTMLCurrentStyle *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    TRACE("(%p)->(%p)\n", This, p);
    return get_current_style_property_var(This, STYLEID_BACKGROUND_COLOR, p);
}

static HRESULT WINAPI HTMLCurrentStyle_get_fontFamily(IHTMLCurrentStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);

    TRACE("(%p)->(%p)\n", This, p);

    return get_current_style_property(This, STYLEID_FONT_FAMILY, p);
}

static HRESULT WINAPI HTMLCurrentStyle_get_fontStyle(IHTMLCurrentStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    TRACE("(%p)->(%p)\n", This, p);
    return get_current_style_property(This, STYLEID_FONT_STYLE, p);
}

static HRESULT WINAPI HTMLCurrentStyle_get_fontVariant(IHTMLCurrentStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    TRACE("(%p)->(%p)\n", This, p);
    return get_current_style_property(This, STYLEID_FONT_VARIANT, p);
}

static HRESULT WINAPI HTMLCurrentStyle_get_fontWeight(IHTMLCurrentStyle *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    TRACE("(%p)->(%p)\n", This, p);
    return get_current_style_property_var(This, STYLEID_FONT_WEIGHT, p);
}

static HRESULT WINAPI HTMLCurrentStyle_get_fontSize(IHTMLCurrentStyle *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    TRACE("(%p)->(%p)\n", This, p);
    return get_current_style_property_var(This, STYLEID_FONT_SIZE, p);
}

static HRESULT WINAPI HTMLCurrentStyle_get_backgroundImage(IHTMLCurrentStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    TRACE("(%p)->(%p)\n", This, p);
    return get_current_style_property(This, STYLEID_BACKGROUND_IMAGE, p);
}

static HRESULT WINAPI HTMLCurrentStyle_get_backgroundPositionX(IHTMLCurrentStyle *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCurrentStyle_get_backgroundPositionY(IHTMLCurrentStyle *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCurrentStyle_get_backgroundRepeat(IHTMLCurrentStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    TRACE("(%p)->(%p)\n", This, p);
    return get_current_style_property(This, STYLEID_BACKGROUND_REPEAT, p);
}

static HRESULT WINAPI HTMLCurrentStyle_get_borderLeftColor(IHTMLCurrentStyle *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    TRACE("(%p)->(%p)\n", This, p);
    return get_current_style_property_var(This, STYLEID_BORDER_LEFT_COLOR, p);
}

static HRESULT WINAPI HTMLCurrentStyle_get_borderTopColor(IHTMLCurrentStyle *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    TRACE("(%p)->(%p)\n", This, p);
    return get_current_style_property_var(This, STYLEID_BORDER_TOP_COLOR, p);
}

static HRESULT WINAPI HTMLCurrentStyle_get_borderRightColor(IHTMLCurrentStyle *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    TRACE("(%p)->(%p)\n", This, p);
    return get_current_style_property_var(This, STYLEID_BORDER_RIGHT_COLOR, p);
}

static HRESULT WINAPI HTMLCurrentStyle_get_borderBottomColor(IHTMLCurrentStyle *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    TRACE("(%p)->(%p)\n", This, p);
    return get_current_style_property_var(This, STYLEID_BORDER_BOTTOM_COLOR, p);
}

static HRESULT WINAPI HTMLCurrentStyle_get_borderTopStyle(IHTMLCurrentStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    TRACE("(%p)->(%p)\n", This, p);
    return get_current_style_property(This, STYLEID_BORDER_TOP_STYLE, p);
}

static HRESULT WINAPI HTMLCurrentStyle_get_borderRightStyle(IHTMLCurrentStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    TRACE("(%p)->(%p)\n", This, p);
    return get_current_style_property(This, STYLEID_BORDER_RIGHT_STYLE, p);
}

static HRESULT WINAPI HTMLCurrentStyle_get_borderBottomStyle(IHTMLCurrentStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    TRACE("(%p)->(%p)\n", This, p);
    return get_current_style_property(This, STYLEID_BORDER_BOTTOM_STYLE, p);
}

static HRESULT WINAPI HTMLCurrentStyle_get_borderLeftStyle(IHTMLCurrentStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    TRACE("(%p)->(%p)\n", This, p);
    return get_current_style_property(This, STYLEID_BORDER_LEFT_STYLE, p);
}

static HRESULT WINAPI HTMLCurrentStyle_get_borderTopWidth(IHTMLCurrentStyle *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    TRACE("(%p)->(%p)\n", This, p);
    return get_current_style_property_var(This, STYLEID_BORDER_TOP_WIDTH, p);
}

static HRESULT WINAPI HTMLCurrentStyle_get_borderRightWidth(IHTMLCurrentStyle *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    TRACE("(%p)->(%p)\n", This, p);
    return get_current_style_property_var(This, STYLEID_BORDER_RIGHT_WIDTH, p);
}

static HRESULT WINAPI HTMLCurrentStyle_get_borderBottomWidth(IHTMLCurrentStyle *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    TRACE("(%p)->(%p)\n", This, p);
    return get_current_style_property_var(This, STYLEID_BORDER_BOTTOM_WIDTH, p);
}

static HRESULT WINAPI HTMLCurrentStyle_get_borderLeftWidth(IHTMLCurrentStyle *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    TRACE("(%p)->(%p)\n", This, p);
    return get_current_style_property_var(This, STYLEID_BORDER_LEFT_WIDTH, p);
}

static HRESULT WINAPI HTMLCurrentStyle_get_left(IHTMLCurrentStyle *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    TRACE("(%p)->(%p)\n", This, p);
    return get_current_style_property_var(This, STYLEID_LEFT, p);
}

static HRESULT WINAPI HTMLCurrentStyle_get_top(IHTMLCurrentStyle *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    TRACE("(%p)->(%p)\n", This, p);
    return get_current_style_property_var(This, STYLEID_TOP, p);
}

static HRESULT WINAPI HTMLCurrentStyle_get_width(IHTMLCurrentStyle *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    TRACE("(%p)->(%p)\n", This, p);
    return get_current_style_property_var(This, STYLEID_WIDTH, p);
}

static HRESULT WINAPI HTMLCurrentStyle_get_height(IHTMLCurrentStyle *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    TRACE("(%p)->(%p)\n", This, p);
    return get_current_style_property_var(This, STYLEID_HEIGHT, p);
}

static HRESULT WINAPI HTMLCurrentStyle_get_paddingLeft(IHTMLCurrentStyle *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    TRACE("(%p)->(%p)\n", This, p);
    return get_current_style_property_var(This, STYLEID_PADDING_LEFT, p);
}

static HRESULT WINAPI HTMLCurrentStyle_get_paddingTop(IHTMLCurrentStyle *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    TRACE("(%p)->(%p)\n", This, p);
    return get_current_style_property_var(This, STYLEID_PADDING_TOP, p);
}

static HRESULT WINAPI HTMLCurrentStyle_get_paddingRight(IHTMLCurrentStyle *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    TRACE("(%p)->(%p)\n", This, p);
    return get_current_style_property_var(This, STYLEID_PADDING_RIGHT, p);
}

static HRESULT WINAPI HTMLCurrentStyle_get_paddingBottom(IHTMLCurrentStyle *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    TRACE("(%p)->(%p)\n", This, p);
    return get_current_style_property_var(This, STYLEID_PADDING_BOTTOM, p);
}

static HRESULT WINAPI HTMLCurrentStyle_get_textAlign(IHTMLCurrentStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    TRACE("(%p)->(%p)\n", This, p);
    return get_current_style_property(This, STYLEID_TEXT_ALIGN, p);
}

static HRESULT WINAPI HTMLCurrentStyle_get_textDecoration(IHTMLCurrentStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    TRACE("(%p)->(%p)\n", This, p);
    return get_current_style_property(This, STYLEID_TEXT_DECORATION, p);
}

static HRESULT WINAPI HTMLCurrentStyle_get_display(IHTMLCurrentStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);

    TRACE("(%p)->(%p)\n", This, p);

    return get_current_style_property(This, STYLEID_DISPLAY, p);
}

static HRESULT WINAPI HTMLCurrentStyle_get_visibility(IHTMLCurrentStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);

    TRACE("(%p)->(%p)\n", This, p);

    return get_current_style_property(This, STYLEID_VISIBILITY, p);
}

static HRESULT WINAPI HTMLCurrentStyle_get_zIndex(IHTMLCurrentStyle *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    TRACE("(%p)->(%p)\n", This, p);
    return get_current_style_property_var(This, STYLEID_Z_INDEX, p);
}

static HRESULT WINAPI HTMLCurrentStyle_get_letterSpacing(IHTMLCurrentStyle *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    TRACE("(%p)->(%p)\n", This, p);
    return get_current_style_property_var(This, STYLEID_LETTER_SPACING, p);
}

static HRESULT WINAPI HTMLCurrentStyle_get_lineHeight(IHTMLCurrentStyle *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    TRACE("(%p)->(%p)\n", This, p);
    return get_current_style_property_var(This, STYLEID_LINE_HEIGHT, p);
}

static HRESULT WINAPI HTMLCurrentStyle_get_textIndent(IHTMLCurrentStyle *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    TRACE("(%p)->(%p)\n", This, p);
    return get_current_style_property_var(This, STYLEID_TEXT_INDENT, p);
}

static HRESULT WINAPI HTMLCurrentStyle_get_verticalAlign(IHTMLCurrentStyle *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    TRACE("(%p)->(%p)\n", This, p);
    return get_current_style_property_var(This, STYLEID_VERTICAL_ALIGN, p);
}

static HRESULT WINAPI HTMLCurrentStyle_get_backgroundAttachment(IHTMLCurrentStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCurrentStyle_get_marginTop(IHTMLCurrentStyle *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    TRACE("(%p)->(%p)\n", This, p);
    return get_current_style_property_var(This, STYLEID_MARGIN_TOP, p);
}

static HRESULT WINAPI HTMLCurrentStyle_get_marginRight(IHTMLCurrentStyle *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    TRACE("(%p)->(%p)\n", This, p);
    return get_current_style_property_var(This, STYLEID_MARGIN_RIGHT, p);
}

static HRESULT WINAPI HTMLCurrentStyle_get_marginBottom(IHTMLCurrentStyle *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    TRACE("(%p)->(%p)\n", This, p);
    return get_current_style_property_var(This, STYLEID_MARGIN_BOTTOM, p);
}

static HRESULT WINAPI HTMLCurrentStyle_get_marginLeft(IHTMLCurrentStyle *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    TRACE("(%p)->(%p)\n", This, p);
    return get_current_style_property_var(This, STYLEID_MARGIN_LEFT, p);
}

static HRESULT WINAPI HTMLCurrentStyle_get_clear(IHTMLCurrentStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCurrentStyle_get_listStyleType(IHTMLCurrentStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCurrentStyle_get_listStylePosition(IHTMLCurrentStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCurrentStyle_get_listStyleImage(IHTMLCurrentStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCurrentStyle_get_clipTop(IHTMLCurrentStyle *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCurrentStyle_get_clipRight(IHTMLCurrentStyle *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCurrentStyle_get_clipBottom(IHTMLCurrentStyle *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCurrentStyle_get_clipLeft(IHTMLCurrentStyle *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCurrentStyle_get_overflow(IHTMLCurrentStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    TRACE("(%p)->(%p)\n", This, p);
    return get_current_style_property(This, STYLEID_OVERFLOW, p);
}

static HRESULT WINAPI HTMLCurrentStyle_get_pageBreakBefore(IHTMLCurrentStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCurrentStyle_get_pageBreakAfter(IHTMLCurrentStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCurrentStyle_get_cursor(IHTMLCurrentStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    TRACE("(%p)->(%p)\n", This, p);
    return get_current_style_property(This, STYLEID_CURSOR, p);
}

static HRESULT WINAPI HTMLCurrentStyle_get_tableLayout(IHTMLCurrentStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCurrentStyle_get_borderCollapse(IHTMLCurrentStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCurrentStyle_get_direction(IHTMLCurrentStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    TRACE("(%p)->(%p)\n", This, p);
    return get_current_style_property(This, STYLEID_DIRECTION, p);
}

static HRESULT WINAPI HTMLCurrentStyle_get_behavior(IHTMLCurrentStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCurrentStyle_getAttribute(IHTMLCurrentStyle *iface, BSTR strAttributeName,
        LONG lFlags, VARIANT *AttributeValue)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    FIXME("(%p)->(%s %lx %p)\n", This, debugstr_w(strAttributeName), lFlags, AttributeValue);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCurrentStyle_get_unicodeBidi(IHTMLCurrentStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCurrentStyle_get_right(IHTMLCurrentStyle *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    TRACE("(%p)->(%p)\n", This, p);
    return get_current_style_property_var(This, STYLEID_RIGHT, p);
}

static HRESULT WINAPI HTMLCurrentStyle_get_bottom(IHTMLCurrentStyle *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    TRACE("(%p)->(%p)\n", This, p);
    return get_current_style_property_var(This, STYLEID_BOTTOM, p);
}

static HRESULT WINAPI HTMLCurrentStyle_get_imeMode(IHTMLCurrentStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCurrentStyle_get_rubyAlign(IHTMLCurrentStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCurrentStyle_get_rubyPosition(IHTMLCurrentStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCurrentStyle_get_rubyOverhang(IHTMLCurrentStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCurrentStyle_get_textAutospace(IHTMLCurrentStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCurrentStyle_get_lineBreak(IHTMLCurrentStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCurrentStyle_get_wordBreak(IHTMLCurrentStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCurrentStyle_get_textJustify(IHTMLCurrentStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCurrentStyle_get_textJustifyTrim(IHTMLCurrentStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCurrentStyle_get_textKashida(IHTMLCurrentStyle *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCurrentStyle_get_blockDirection(IHTMLCurrentStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCurrentStyle_get_layoutGridChar(IHTMLCurrentStyle *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCurrentStyle_get_layoutGridLine(IHTMLCurrentStyle *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCurrentStyle_get_layoutGridMode(IHTMLCurrentStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCurrentStyle_get_layoutGridType(IHTMLCurrentStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCurrentStyle_get_borderStyle(IHTMLCurrentStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    TRACE("(%p)->(%p)\n", This, p);
    return get_current_style_property(This, STYLEID_BORDER_STYLE, p);
}

static HRESULT WINAPI HTMLCurrentStyle_get_borderColor(IHTMLCurrentStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    TRACE("(%p)->(%p)\n", This, p);
    return get_current_style_property(This, STYLEID_BORDER_COLOR, p);
}

static HRESULT WINAPI HTMLCurrentStyle_get_borderWidth(IHTMLCurrentStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    TRACE("(%p)->(%p)\n", This, p);
    return get_current_style_property(This, STYLEID_BORDER_WIDTH, p);
}

static HRESULT WINAPI HTMLCurrentStyle_get_padding(IHTMLCurrentStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    TRACE("(%p)->(%p)\n", This, p);
    return get_current_style_property(This, STYLEID_PADDING, p);
}

static HRESULT WINAPI HTMLCurrentStyle_get_margin(IHTMLCurrentStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    TRACE("(%p)->(%p)\n", This, p);
    return get_current_style_property(This, STYLEID_MARGIN, p);
}

static HRESULT WINAPI HTMLCurrentStyle_get_accelerator(IHTMLCurrentStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCurrentStyle_get_overflowX(IHTMLCurrentStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    TRACE("(%p)->(%p)\n", This, p);
    return get_current_style_property(This, STYLEID_OVERFLOW_X, p);
}

static HRESULT WINAPI HTMLCurrentStyle_get_overflowY(IHTMLCurrentStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    TRACE("(%p)->(%p)\n", This, p);
    return get_current_style_property(This, STYLEID_OVERFLOW_Y, p);
}

static HRESULT WINAPI HTMLCurrentStyle_get_textTransform(IHTMLCurrentStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle(iface);
    TRACE("(%p)->(%p)\n", This, p);
    return get_current_style_property(This, STYLEID_TEXT_TRANSFORM, p);
}

static const IHTMLCurrentStyleVtbl HTMLCurrentStyleVtbl = {
    HTMLCurrentStyle_QueryInterface,
    HTMLCurrentStyle_AddRef,
    HTMLCurrentStyle_Release,
    HTMLCurrentStyle_GetTypeInfoCount,
    HTMLCurrentStyle_GetTypeInfo,
    HTMLCurrentStyle_GetIDsOfNames,
    HTMLCurrentStyle_Invoke,
    HTMLCurrentStyle_get_position,
    HTMLCurrentStyle_get_styleFloat,
    HTMLCurrentStyle_get_color,
    HTMLCurrentStyle_get_backgroundColor,
    HTMLCurrentStyle_get_fontFamily,
    HTMLCurrentStyle_get_fontStyle,
    HTMLCurrentStyle_get_fontVariant,
    HTMLCurrentStyle_get_fontWeight,
    HTMLCurrentStyle_get_fontSize,
    HTMLCurrentStyle_get_backgroundImage,
    HTMLCurrentStyle_get_backgroundPositionX,
    HTMLCurrentStyle_get_backgroundPositionY,
    HTMLCurrentStyle_get_backgroundRepeat,
    HTMLCurrentStyle_get_borderLeftColor,
    HTMLCurrentStyle_get_borderTopColor,
    HTMLCurrentStyle_get_borderRightColor,
    HTMLCurrentStyle_get_borderBottomColor,
    HTMLCurrentStyle_get_borderTopStyle,
    HTMLCurrentStyle_get_borderRightStyle,
    HTMLCurrentStyle_get_borderBottomStyle,
    HTMLCurrentStyle_get_borderLeftStyle,
    HTMLCurrentStyle_get_borderTopWidth,
    HTMLCurrentStyle_get_borderRightWidth,
    HTMLCurrentStyle_get_borderBottomWidth,
    HTMLCurrentStyle_get_borderLeftWidth,
    HTMLCurrentStyle_get_left,
    HTMLCurrentStyle_get_top,
    HTMLCurrentStyle_get_width,
    HTMLCurrentStyle_get_height,
    HTMLCurrentStyle_get_paddingLeft,
    HTMLCurrentStyle_get_paddingTop,
    HTMLCurrentStyle_get_paddingRight,
    HTMLCurrentStyle_get_paddingBottom,
    HTMLCurrentStyle_get_textAlign,
    HTMLCurrentStyle_get_textDecoration,
    HTMLCurrentStyle_get_display,
    HTMLCurrentStyle_get_visibility,
    HTMLCurrentStyle_get_zIndex,
    HTMLCurrentStyle_get_letterSpacing,
    HTMLCurrentStyle_get_lineHeight,
    HTMLCurrentStyle_get_textIndent,
    HTMLCurrentStyle_get_verticalAlign,
    HTMLCurrentStyle_get_backgroundAttachment,
    HTMLCurrentStyle_get_marginTop,
    HTMLCurrentStyle_get_marginRight,
    HTMLCurrentStyle_get_marginBottom,
    HTMLCurrentStyle_get_marginLeft,
    HTMLCurrentStyle_get_clear,
    HTMLCurrentStyle_get_listStyleType,
    HTMLCurrentStyle_get_listStylePosition,
    HTMLCurrentStyle_get_listStyleImage,
    HTMLCurrentStyle_get_clipTop,
    HTMLCurrentStyle_get_clipRight,
    HTMLCurrentStyle_get_clipBottom,
    HTMLCurrentStyle_get_clipLeft,
    HTMLCurrentStyle_get_overflow,
    HTMLCurrentStyle_get_pageBreakBefore,
    HTMLCurrentStyle_get_pageBreakAfter,
    HTMLCurrentStyle_get_cursor,
    HTMLCurrentStyle_get_tableLayout,
    HTMLCurrentStyle_get_borderCollapse,
    HTMLCurrentStyle_get_direction,
    HTMLCurrentStyle_get_behavior,
    HTMLCurrentStyle_getAttribute,
    HTMLCurrentStyle_get_unicodeBidi,
    HTMLCurrentStyle_get_right,
    HTMLCurrentStyle_get_bottom,
    HTMLCurrentStyle_get_imeMode,
    HTMLCurrentStyle_get_rubyAlign,
    HTMLCurrentStyle_get_rubyPosition,
    HTMLCurrentStyle_get_rubyOverhang,
    HTMLCurrentStyle_get_textAutospace,
    HTMLCurrentStyle_get_lineBreak,
    HTMLCurrentStyle_get_wordBreak,
    HTMLCurrentStyle_get_textJustify,
    HTMLCurrentStyle_get_textJustifyTrim,
    HTMLCurrentStyle_get_textKashida,
    HTMLCurrentStyle_get_blockDirection,
    HTMLCurrentStyle_get_layoutGridChar,
    HTMLCurrentStyle_get_layoutGridLine,
    HTMLCurrentStyle_get_layoutGridMode,
    HTMLCurrentStyle_get_layoutGridType,
    HTMLCurrentStyle_get_borderStyle,
    HTMLCurrentStyle_get_borderColor,
    HTMLCurrentStyle_get_borderWidth,
    HTMLCurrentStyle_get_padding,
    HTMLCurrentStyle_get_margin,
    HTMLCurrentStyle_get_accelerator,
    HTMLCurrentStyle_get_overflowX,
    HTMLCurrentStyle_get_overflowY,
    HTMLCurrentStyle_get_textTransform
};

/* IHTMLCurrentStyle2 */
static HRESULT WINAPI HTMLCurrentStyle2_QueryInterface(IHTMLCurrentStyle2 *iface, REFIID riid, void **ppv)
{
   HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle2(iface);

   return IHTMLCurrentStyle_QueryInterface(&This->IHTMLCurrentStyle_iface, riid, ppv);
}

static ULONG WINAPI HTMLCurrentStyle2_AddRef(IHTMLCurrentStyle2 *iface)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle2(iface);

    return IHTMLCurrentStyle_AddRef(&This->IHTMLCurrentStyle_iface);
}

static ULONG WINAPI HTMLCurrentStyle2_Release(IHTMLCurrentStyle2 *iface)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle2(iface);
    return IHTMLCurrentStyle_Release(&This->IHTMLCurrentStyle_iface);
}

static HRESULT WINAPI HTMLCurrentStyle2_GetTypeInfoCount(IHTMLCurrentStyle2 *iface, UINT *pctinfo)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle2(iface);
    return IDispatchEx_GetTypeInfoCount(&This->css_style.dispex.IDispatchEx_iface, pctinfo);
}

static HRESULT WINAPI HTMLCurrentStyle2_GetTypeInfo(IHTMLCurrentStyle2 *iface, UINT iTInfo,
        LCID lcid, ITypeInfo **ppTInfo)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle2(iface);
    return IDispatchEx_GetTypeInfo(&This->css_style.dispex.IDispatchEx_iface, iTInfo, lcid, ppTInfo);
}

static HRESULT WINAPI HTMLCurrentStyle2_GetIDsOfNames(IHTMLCurrentStyle2 *iface, REFIID riid,
        LPOLESTR *rgszNames, UINT cNames, LCID lcid, DISPID *rgDispId)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle2(iface);
    return IDispatchEx_GetIDsOfNames(&This->css_style.dispex.IDispatchEx_iface, riid, rgszNames, cNames,
            lcid, rgDispId);
}

static HRESULT WINAPI HTMLCurrentStyle2_Invoke(IHTMLCurrentStyle2 *iface, DISPID dispIdMember,
        REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS *pDispParams,
        VARIANT *pVarResult, EXCEPINFO *pExcepInfo, UINT *puArgErr)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle2(iface);
    return IDispatchEx_Invoke(&This->css_style.dispex.IDispatchEx_iface, dispIdMember, riid, lcid,
            wFlags, pDispParams, pVarResult, pExcepInfo, puArgErr);
}

static HRESULT WINAPI HTMLCurrentStyle2_get_layoutFlow(IHTMLCurrentStyle2 *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle2(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCurrentStyle2_get_wordWrap(IHTMLCurrentStyle2 *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle2(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCurrentStyle2_get_textUnderlinePosition(IHTMLCurrentStyle2 *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle2(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCurrentStyle2_get_hasLayout(IHTMLCurrentStyle2 *iface, VARIANT_BOOL *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle2(iface);

    FIXME("(%p)->(%p) returning true\n", This, p);

    *p = VARIANT_TRUE;
    return S_OK;
}

static HRESULT WINAPI HTMLCurrentStyle2_get_scrollbarBaseColor(IHTMLCurrentStyle2 *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle2(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCurrentStyle2_get_scrollbarFaceColor(IHTMLCurrentStyle2 *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle2(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCurrentStyle2_get_scrollbar3dLightColor(IHTMLCurrentStyle2 *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle2(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCurrentStyle2_get_scrollbarShadowColor(IHTMLCurrentStyle2 *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle2(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCurrentStyle2_get_scrollbarHighlightColor(IHTMLCurrentStyle2 *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle2(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCurrentStyle2_get_scrollbarDarkShadowColor(IHTMLCurrentStyle2 *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle2(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCurrentStyle2_get_scrollbarArrowColor(IHTMLCurrentStyle2 *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle2(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCurrentStyle2_get_scrollbarTrackColor(IHTMLCurrentStyle2 *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle2(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCurrentStyle2_get_writingMode(IHTMLCurrentStyle2 *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle2(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCurrentStyle2_get_zoom(IHTMLCurrentStyle2 *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle2(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCurrentStyle2_get_filter(IHTMLCurrentStyle2 *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle2(iface);

    TRACE("(%p)->(%p)\n", This, p);

    if(This->elem->filter) {
        *p = SysAllocString(This->elem->filter);
        if(!*p)
            return E_OUTOFMEMORY;
    }else {
        *p = NULL;
    }

    return S_OK;
}

static HRESULT WINAPI HTMLCurrentStyle2_get_textAlignLast(IHTMLCurrentStyle2 *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle2(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCurrentStyle2_get_textKashidaSpace(IHTMLCurrentStyle2 *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle2(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCurrentStyle2_get_isBlock(IHTMLCurrentStyle2 *iface, VARIANT_BOOL *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle2(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static const IHTMLCurrentStyle2Vtbl HTMLCurrentStyle2Vtbl = {
    HTMLCurrentStyle2_QueryInterface,
    HTMLCurrentStyle2_AddRef,
    HTMLCurrentStyle2_Release,
    HTMLCurrentStyle2_GetTypeInfoCount,
    HTMLCurrentStyle2_GetTypeInfo,
    HTMLCurrentStyle2_GetIDsOfNames,
    HTMLCurrentStyle2_Invoke,
    HTMLCurrentStyle2_get_layoutFlow,
    HTMLCurrentStyle2_get_wordWrap,
    HTMLCurrentStyle2_get_textUnderlinePosition,
    HTMLCurrentStyle2_get_hasLayout,
    HTMLCurrentStyle2_get_scrollbarBaseColor,
    HTMLCurrentStyle2_get_scrollbarFaceColor,
    HTMLCurrentStyle2_get_scrollbar3dLightColor,
    HTMLCurrentStyle2_get_scrollbarShadowColor,
    HTMLCurrentStyle2_get_scrollbarHighlightColor,
    HTMLCurrentStyle2_get_scrollbarDarkShadowColor,
    HTMLCurrentStyle2_get_scrollbarArrowColor,
    HTMLCurrentStyle2_get_scrollbarTrackColor,
    HTMLCurrentStyle2_get_writingMode,
    HTMLCurrentStyle2_get_zoom,
    HTMLCurrentStyle2_get_filter,
    HTMLCurrentStyle2_get_textAlignLast,
    HTMLCurrentStyle2_get_textKashidaSpace,
    HTMLCurrentStyle2_get_isBlock
};

/* IHTMLCurrentStyle3 */
static HRESULT WINAPI HTMLCurrentStyle3_QueryInterface(IHTMLCurrentStyle3 *iface, REFIID riid, void **ppv)
{
   HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle3(iface);

   return IHTMLCurrentStyle_QueryInterface(&This->IHTMLCurrentStyle_iface, riid, ppv);
}

static ULONG WINAPI HTMLCurrentStyle3_AddRef(IHTMLCurrentStyle3 *iface)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle3(iface);

    return IHTMLCurrentStyle_AddRef(&This->IHTMLCurrentStyle_iface);
}

static ULONG WINAPI HTMLCurrentStyle3_Release(IHTMLCurrentStyle3 *iface)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle3(iface);
    return IHTMLCurrentStyle_Release(&This->IHTMLCurrentStyle_iface);
}

static HRESULT WINAPI HTMLCurrentStyle3_GetTypeInfoCount(IHTMLCurrentStyle3 *iface, UINT *pctinfo)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle3(iface);
    return IDispatchEx_GetTypeInfoCount(&This->css_style.dispex.IDispatchEx_iface, pctinfo);
}

static HRESULT WINAPI HTMLCurrentStyle3_GetTypeInfo(IHTMLCurrentStyle3 *iface, UINT iTInfo,
        LCID lcid, ITypeInfo **ppTInfo)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle3(iface);
    return IDispatchEx_GetTypeInfo(&This->css_style.dispex.IDispatchEx_iface, iTInfo, lcid, ppTInfo);
}

static HRESULT WINAPI HTMLCurrentStyle3_GetIDsOfNames(IHTMLCurrentStyle3 *iface, REFIID riid,
        LPOLESTR *rgszNames, UINT cNames, LCID lcid, DISPID *rgDispId)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle3(iface);
    return IDispatchEx_GetIDsOfNames(&This->css_style.dispex.IDispatchEx_iface, riid, rgszNames, cNames,
            lcid, rgDispId);
}

static HRESULT WINAPI HTMLCurrentStyle3_Invoke(IHTMLCurrentStyle3 *iface, DISPID dispIdMember,
        REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS *pDispParams,
        VARIANT *pVarResult, EXCEPINFO *pExcepInfo, UINT *puArgErr)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle3(iface);
    return IDispatchEx_Invoke(&This->css_style.dispex.IDispatchEx_iface, dispIdMember, riid, lcid,
            wFlags, pDispParams, pVarResult, pExcepInfo, puArgErr);
}

static HRESULT WINAPI HTMLCurrentStyle3_get_textOverflow(IHTMLCurrentStyle3 *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle3(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCurrentStyle3_get_minHeight(IHTMLCurrentStyle3 *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle3(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCurrentStyle3_get_wordSpacing(IHTMLCurrentStyle3 *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle3(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCurrentStyle3_get_whiteSpace(IHTMLCurrentStyle3 *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle3(iface);

    TRACE("(%p)->(%p)\n", This, p);

    return get_current_style_property(This, STYLEID_WHITE_SPACE, p);
}

static const IHTMLCurrentStyle3Vtbl HTMLCurrentStyle3Vtbl = {
    HTMLCurrentStyle3_QueryInterface,
    HTMLCurrentStyle3_AddRef,
    HTMLCurrentStyle3_Release,
    HTMLCurrentStyle3_GetTypeInfoCount,
    HTMLCurrentStyle3_GetTypeInfo,
    HTMLCurrentStyle3_GetIDsOfNames,
    HTMLCurrentStyle3_Invoke,
    HTMLCurrentStyle3_get_textOverflow,
    HTMLCurrentStyle3_get_minHeight,
    HTMLCurrentStyle3_get_wordSpacing,
    HTMLCurrentStyle3_get_whiteSpace
};

/* IHTMLCurrentStyle4 */
static HRESULT WINAPI HTMLCurrentStyle4_QueryInterface(IHTMLCurrentStyle4 *iface, REFIID riid, void **ppv)
{
   HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle4(iface);

   return IHTMLCurrentStyle_QueryInterface(&This->IHTMLCurrentStyle_iface, riid, ppv);
}

static ULONG WINAPI HTMLCurrentStyle4_AddRef(IHTMLCurrentStyle4 *iface)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle4(iface);

    return IHTMLCurrentStyle_AddRef(&This->IHTMLCurrentStyle_iface);
}

static ULONG WINAPI HTMLCurrentStyle4_Release(IHTMLCurrentStyle4 *iface)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle4(iface);
    return IHTMLCurrentStyle_Release(&This->IHTMLCurrentStyle_iface);
}

static HRESULT WINAPI HTMLCurrentStyle4_GetTypeInfoCount(IHTMLCurrentStyle4 *iface, UINT *pctinfo)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle4(iface);
    return IDispatchEx_GetTypeInfoCount(&This->css_style.dispex.IDispatchEx_iface, pctinfo);
}

static HRESULT WINAPI HTMLCurrentStyle4_GetTypeInfo(IHTMLCurrentStyle4 *iface, UINT iTInfo,
        LCID lcid, ITypeInfo **ppTInfo)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle4(iface);
    return IDispatchEx_GetTypeInfo(&This->css_style.dispex.IDispatchEx_iface, iTInfo, lcid, ppTInfo);
}

static HRESULT WINAPI HTMLCurrentStyle4_GetIDsOfNames(IHTMLCurrentStyle4 *iface, REFIID riid,
        LPOLESTR *rgszNames, UINT cNames, LCID lcid, DISPID *rgDispId)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle4(iface);
    return IDispatchEx_GetIDsOfNames(&This->css_style.dispex.IDispatchEx_iface, riid, rgszNames, cNames,
            lcid, rgDispId);
}

static HRESULT WINAPI HTMLCurrentStyle4_Invoke(IHTMLCurrentStyle4 *iface, DISPID dispIdMember,
        REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS *pDispParams,
        VARIANT *pVarResult, EXCEPINFO *pExcepInfo, UINT *puArgErr)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle4(iface);
    return IDispatchEx_Invoke(&This->css_style.dispex.IDispatchEx_iface, dispIdMember, riid, lcid,
            wFlags, pDispParams, pVarResult, pExcepInfo, puArgErr);
}

static HRESULT WINAPI HTMLCurrentStyle4_msInterpolationMode(IHTMLCurrentStyle4 *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle4(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCurrentStyle4_get_maxHeight(IHTMLCurrentStyle4 *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle4(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCurrentStyle4_get_minWidth(IHTMLCurrentStyle4 *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle4(iface);
    TRACE("(%p)->(%p)\n", This, p);
    return get_current_style_property_var(This, STYLEID_MIN_WIDTH, p);
}

static HRESULT WINAPI HTMLCurrentStyle4_get_maxWidth(IHTMLCurrentStyle4 *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLCurrentStyle4(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static const IHTMLCurrentStyle4Vtbl HTMLCurrentStyle4Vtbl = {
    HTMLCurrentStyle4_QueryInterface,
    HTMLCurrentStyle4_AddRef,
    HTMLCurrentStyle4_Release,
    HTMLCurrentStyle4_GetTypeInfoCount,
    HTMLCurrentStyle4_GetTypeInfo,
    HTMLCurrentStyle4_GetIDsOfNames,
    HTMLCurrentStyle4_Invoke,
    HTMLCurrentStyle4_msInterpolationMode,
    HTMLCurrentStyle4_get_maxHeight,
    HTMLCurrentStyle4_get_minWidth,
    HTMLCurrentStyle4_get_maxWidth
};

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_QueryInterface(IHTMLStyle *iface, REFIID riid, void **ppv)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    return IHTMLCurrentStyle_QueryInterface(&This->IHTMLCurrentStyle_iface, riid, ppv);
}

static ULONG WINAPI HTMLCurrentStyle_HTMLStyle_AddRef(IHTMLStyle *iface)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    return IHTMLCurrentStyle_AddRef(&This->IHTMLCurrentStyle_iface);
}

static ULONG WINAPI HTMLCurrentStyle_HTMLStyle_Release(IHTMLStyle *iface)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    return IHTMLCurrentStyle_Release(&This->IHTMLCurrentStyle_iface);
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_GetTypeInfoCount(IHTMLStyle *iface, UINT *pctinfo)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    return IDispatchEx_GetTypeInfoCount(&This->css_style.dispex.IDispatchEx_iface, pctinfo);
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_GetTypeInfo(IHTMLStyle *iface, UINT iTInfo,
                                              LCID lcid, ITypeInfo **ppTInfo)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    return IDispatchEx_GetTypeInfo(&This->css_style.dispex.IDispatchEx_iface, iTInfo, lcid, ppTInfo);
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_GetIDsOfNames(IHTMLStyle *iface, REFIID riid,
                                                LPOLESTR *rgszNames, UINT cNames,
                                                LCID lcid, DISPID *rgDispId)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    return IDispatchEx_GetIDsOfNames(&This->css_style.dispex.IDispatchEx_iface, riid, rgszNames, cNames,
            lcid, rgDispId);
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_Invoke(IHTMLStyle *iface, DISPID dispIdMember,
                            REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS *pDispParams,
                            VARIANT *pVarResult, EXCEPINFO *pExcepInfo, UINT *puArgErr)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    return IDispatchEx_Invoke(&This->css_style.dispex.IDispatchEx_iface, dispIdMember, riid, lcid,
            wFlags, pDispParams, pVarResult, pExcepInfo, puArgErr);
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_put_fontFamily(IHTMLStyle *iface, BSTR v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%s)\n", This, debugstr_w(v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_fontFamily(IHTMLStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_put_fontStyle(IHTMLStyle *iface, BSTR v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%s)\n", This, debugstr_w(v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_fontStyle(IHTMLStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_put_fontVariant(IHTMLStyle *iface, BSTR v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%s)\n", This, debugstr_w(v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_fontVariant(IHTMLStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_put_fontWeight(IHTMLStyle *iface, BSTR v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%s)\n", This, debugstr_w(v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_fontWeight(IHTMLStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_put_fontSize(IHTMLStyle *iface, VARIANT v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%s)\n", This, debugstr_variant(&v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_fontSize(IHTMLStyle *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_put_font(IHTMLStyle *iface, BSTR v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%s)\n", This, debugstr_w(v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_font(IHTMLStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_put_color(IHTMLStyle *iface, VARIANT v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%s)\n", This, debugstr_variant(&v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_color(IHTMLStyle *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_put_background(IHTMLStyle *iface, BSTR v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%s)\n", This, debugstr_w(v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_background(IHTMLStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_put_backgroundColor(IHTMLStyle *iface, VARIANT v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%s)\n", This, debugstr_variant(&v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_backgroundColor(IHTMLStyle *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_put_backgroundImage(IHTMLStyle *iface, BSTR v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%s)\n", This, debugstr_w(v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_backgroundImage(IHTMLStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_put_backgroundRepeat(IHTMLStyle *iface, BSTR v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%s)\n", This, debugstr_w(v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_backgroundRepeat(IHTMLStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_put_backgroundAttachment(IHTMLStyle *iface, BSTR v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%s)\n", This, debugstr_w(v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_backgroundAttachment(IHTMLStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_put_backgroundPosition(IHTMLStyle *iface, BSTR v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%s)\n", This, debugstr_w(v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_backgroundPosition(IHTMLStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_put_backgroundPositionX(IHTMLStyle *iface, VARIANT v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    TRACE("(%p)->(%s)\n", This, debugstr_variant(&v));
    return E_FAIL;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_backgroundPositionX(IHTMLStyle *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    TRACE("(%p)->(%p)\n", This, p);
    return IHTMLCSSStyleDeclaration_get_backgroundPositionX(&This->css_style.IHTMLCSSStyleDeclaration_iface, p);
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_put_backgroundPositionY(IHTMLStyle *iface, VARIANT v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    TRACE("(%p)->(%s)\n", This, debugstr_variant(&v));
    return E_FAIL;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_backgroundPositionY(IHTMLStyle *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    TRACE("(%p)->(%p)\n", This, p);
    return IHTMLCSSStyleDeclaration_get_backgroundPositionY(&This->css_style.IHTMLCSSStyleDeclaration_iface, p);
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_put_wordSpacing(IHTMLStyle *iface, VARIANT v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%s)\n", This, debugstr_variant(&v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_wordSpacing(IHTMLStyle *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_put_letterSpacing(IHTMLStyle *iface, VARIANT v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%s)\n", This, debugstr_variant(&v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_letterSpacing(IHTMLStyle *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_put_textDecoration(IHTMLStyle *iface, BSTR v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%s)\n", This, debugstr_w(v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_textDecoration(IHTMLStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_put_textDecorationNone(IHTMLStyle *iface, VARIANT_BOOL v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%x)\n", This, v);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_textDecorationNone(IHTMLStyle *iface, VARIANT_BOOL *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_put_textDecorationUnderline(IHTMLStyle *iface, VARIANT_BOOL v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%x)\n", This, v);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_textDecorationUnderline(IHTMLStyle *iface, VARIANT_BOOL *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_put_textDecorationOverline(IHTMLStyle *iface, VARIANT_BOOL v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%x)\n", This, v);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_textDecorationOverline(IHTMLStyle *iface, VARIANT_BOOL *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_put_textDecorationLineThrough(IHTMLStyle *iface, VARIANT_BOOL v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%x)\n", This, v);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_textDecorationLineThrough(IHTMLStyle *iface, VARIANT_BOOL *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_put_textDecorationBlink(IHTMLStyle *iface, VARIANT_BOOL v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%x)\n", This, v);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_textDecorationBlink(IHTMLStyle *iface, VARIANT_BOOL *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_put_verticalAlign(IHTMLStyle *iface, VARIANT v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%s)\n", This, debugstr_variant(&v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_verticalAlign(IHTMLStyle *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_put_textTransform(IHTMLStyle *iface, BSTR v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%s)\n", This, debugstr_w(v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_textTransform(IHTMLStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_put_textAlign(IHTMLStyle *iface, BSTR v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%s)\n", This, debugstr_w(v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_textAlign(IHTMLStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_put_textIndent(IHTMLStyle *iface, VARIANT v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%s)\n", This, debugstr_variant(&v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_textIndent(IHTMLStyle *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_put_lineHeight(IHTMLStyle *iface, VARIANT v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%s)\n", This, debugstr_variant(&v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_lineHeight(IHTMLStyle *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_put_marginTop(IHTMLStyle *iface, VARIANT v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%s)\n", This, debugstr_variant(&v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_marginTop(IHTMLStyle *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_put_marginRight(IHTMLStyle *iface, VARIANT v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%s)\n", This, debugstr_variant(&v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_marginRight(IHTMLStyle *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_put_marginBottom(IHTMLStyle *iface, VARIANT v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%s)\n", This, debugstr_variant(&v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_marginBottom(IHTMLStyle *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_put_marginLeft(IHTMLStyle *iface, VARIANT v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%s)\n", This, debugstr_variant(&v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_put_margin(IHTMLStyle *iface, BSTR v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%s)\n", This, debugstr_w(v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_margin(IHTMLStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_marginLeft(IHTMLStyle *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_put_paddingTop(IHTMLStyle *iface, VARIANT v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%s)\n", This, debugstr_variant(&v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_paddingTop(IHTMLStyle *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_put_paddingRight(IHTMLStyle *iface, VARIANT v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%s)\n", This, debugstr_variant(&v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_paddingRight(IHTMLStyle *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_put_paddingBottom(IHTMLStyle *iface, VARIANT v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%s)\n", This, debugstr_variant(&v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_paddingBottom(IHTMLStyle *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_put_paddingLeft(IHTMLStyle *iface, VARIANT v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%s)\n", This, debugstr_variant(&v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_paddingLeft(IHTMLStyle *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_put_padding(IHTMLStyle *iface, BSTR v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%s)\n", This, debugstr_w(v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_padding(IHTMLStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_put_border(IHTMLStyle *iface, BSTR v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%s)\n", This, debugstr_w(v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_border(IHTMLStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_put_borderTop(IHTMLStyle *iface, BSTR v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%s)\n", This, debugstr_w(v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_borderTop(IHTMLStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_put_borderRight(IHTMLStyle *iface, BSTR v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%s)\n", This, debugstr_w(v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_borderRight(IHTMLStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_put_borderBottom(IHTMLStyle *iface, BSTR v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%s)\n", This, debugstr_w(v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_borderBottom(IHTMLStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_put_borderLeft(IHTMLStyle *iface, BSTR v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%s)\n", This, debugstr_w(v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_borderLeft(IHTMLStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_put_borderColor(IHTMLStyle *iface, BSTR v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%s)\n", This, debugstr_w(v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_borderColor(IHTMLStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_put_borderTopColor(IHTMLStyle *iface, VARIANT v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%s)\n", This, debugstr_variant(&v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_borderTopColor(IHTMLStyle *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_put_borderRightColor(IHTMLStyle *iface, VARIANT v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%s)\n", This, debugstr_variant(&v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_borderRightColor(IHTMLStyle *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_put_borderBottomColor(IHTMLStyle *iface, VARIANT v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%s)\n", This, debugstr_variant(&v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_borderBottomColor(IHTMLStyle *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_put_borderLeftColor(IHTMLStyle *iface, VARIANT v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%s)\n", This, debugstr_variant(&v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_borderLeftColor(IHTMLStyle *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_put_borderWidth(IHTMLStyle *iface, BSTR v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%s)\n", This, debugstr_w(v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_borderWidth(IHTMLStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_put_borderTopWidth(IHTMLStyle *iface, VARIANT v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%s)\n", This, debugstr_variant(&v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_borderTopWidth(IHTMLStyle *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_put_borderRightWidth(IHTMLStyle *iface, VARIANT v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%s)\n", This, debugstr_variant(&v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_borderRightWidth(IHTMLStyle *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_put_borderBottomWidth(IHTMLStyle *iface, VARIANT v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%s)\n", This, debugstr_variant(&v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_borderBottomWidth(IHTMLStyle *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_put_borderLeftWidth(IHTMLStyle *iface, VARIANT v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%s)\n", This, debugstr_variant(&v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_borderLeftWidth(IHTMLStyle *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_put_borderStyle(IHTMLStyle *iface, BSTR v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%s)\n", This, debugstr_w(v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_borderStyle(IHTMLStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_put_borderTopStyle(IHTMLStyle *iface, BSTR v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%s)\n", This, debugstr_w(v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_borderTopStyle(IHTMLStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_put_borderRightStyle(IHTMLStyle *iface, BSTR v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%s)\n", This, debugstr_w(v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_borderRightStyle(IHTMLStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_put_borderBottomStyle(IHTMLStyle *iface, BSTR v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%s)\n", This, debugstr_w(v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_borderBottomStyle(IHTMLStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_put_borderLeftStyle(IHTMLStyle *iface, BSTR v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%s)\n", This, debugstr_w(v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_borderLeftStyle(IHTMLStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_put_width(IHTMLStyle *iface, VARIANT v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%s)\n", This, debugstr_variant(&v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_width(IHTMLStyle *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_put_height(IHTMLStyle *iface, VARIANT v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%s)\n", This, debugstr_variant(&v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_height(IHTMLStyle *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_put_styleFloat(IHTMLStyle *iface, BSTR v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    TRACE("(%p)->(%s)\n", This, debugstr_w(v));
    return E_FAIL;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_styleFloat(IHTMLStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    TRACE("(%p)->(%p)\n", This, p);
    return get_current_style_property(This, STYLEID_FLOAT, p);
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_put_clear(IHTMLStyle *iface, BSTR v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%s)\n", This, debugstr_w(v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_clear(IHTMLStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_put_display(IHTMLStyle *iface, BSTR v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%s)\n", This, debugstr_w(v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_display(IHTMLStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_put_visibility(IHTMLStyle *iface, BSTR v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%s)\n", This, debugstr_w(v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_visibility(IHTMLStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_put_listStyleType(IHTMLStyle *iface, BSTR v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%s)\n", This, debugstr_w(v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_listStyleType(IHTMLStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_put_listStylePosition(IHTMLStyle *iface, BSTR v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%s)\n", This, debugstr_w(v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_listStylePosition(IHTMLStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_put_listStyleImage(IHTMLStyle *iface, BSTR v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%s)\n", This, debugstr_w(v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_listStyleImage(IHTMLStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_put_listStyle(IHTMLStyle *iface, BSTR v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%s)\n", This, debugstr_w(v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_listStyle(IHTMLStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_put_whiteSpace(IHTMLStyle *iface, BSTR v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%s)\n", This, debugstr_w(v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_whiteSpace(IHTMLStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_put_top(IHTMLStyle *iface, VARIANT v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%s)\n", This, debugstr_variant(&v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_top(IHTMLStyle *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_put_left(IHTMLStyle *iface, VARIANT v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%s)\n", This, debugstr_variant(&v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_left(IHTMLStyle *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_position(IHTMLStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_put_zIndex(IHTMLStyle *iface, VARIANT v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%s)\n", This, debugstr_variant(&v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_zIndex(IHTMLStyle *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_put_overflow(IHTMLStyle *iface, BSTR v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%s)\n", This, debugstr_w(v));
    return E_UNEXPECTED;
}


static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_overflow(IHTMLStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_put_pageBreakBefore(IHTMLStyle *iface, BSTR v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%s)\n", This, debugstr_w(v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_pageBreakBefore(IHTMLStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_put_pageBreakAfter(IHTMLStyle *iface, BSTR v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%s)\n", This, debugstr_w(v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_pageBreakAfter(IHTMLStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_put_cssText(IHTMLStyle *iface, BSTR v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%s)\n", This, debugstr_w(v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_cssText(IHTMLStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_put_pixelTop(IHTMLStyle *iface, LONG v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%ld)\n", This, v);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_pixelTop(IHTMLStyle *iface, LONG *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_put_pixelLeft(IHTMLStyle *iface, LONG v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%ld)\n", This, v);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_pixelLeft(IHTMLStyle *iface, LONG *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_put_pixelWidth(IHTMLStyle *iface, LONG v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->()\n", This);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_pixelWidth(IHTMLStyle *iface, LONG *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_put_pixelHeight(IHTMLStyle *iface, LONG v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%ld)\n", This, v);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_pixelHeight(IHTMLStyle *iface, LONG *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_put_posTop(IHTMLStyle *iface, float v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%f)\n", This, v);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_posTop(IHTMLStyle *iface, float *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_put_posLeft(IHTMLStyle *iface, float v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%f)\n", This, v);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_posLeft(IHTMLStyle *iface, float *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_put_posWidth(IHTMLStyle *iface, float v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%f)\n", This, v);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_posWidth(IHTMLStyle *iface, float *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_put_posHeight(IHTMLStyle *iface, float v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%f)\n", This, v);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_posHeight(IHTMLStyle *iface, float *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_put_cursor(IHTMLStyle *iface, BSTR v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%s)\n", This, debugstr_w(v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_cursor(IHTMLStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_put_clip(IHTMLStyle *iface, BSTR v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%s)\n", This, debugstr_w(v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_clip(IHTMLStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_put_filter(IHTMLStyle *iface, BSTR v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%s)\n", This, debugstr_w(v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_get_filter(IHTMLStyle *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_setAttribute(IHTMLStyle *iface, BSTR strAttributeName,
        VARIANT AttributeValue, LONG lFlags)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    FIXME("(%p)->(%s %s %08lx)\n", This, debugstr_w(strAttributeName),
          debugstr_variant(&AttributeValue), lFlags);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_getAttribute(IHTMLStyle *iface, BSTR strAttributeName,
        LONG lFlags, VARIANT *AttributeValue)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    FIXME("(%p)->(%s v%p %08lx)\n", This, debugstr_w(strAttributeName), AttributeValue, lFlags);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_removeAttribute(IHTMLStyle *iface, BSTR strAttributeName,
                                                LONG lFlags, VARIANT_BOOL *pfSuccess)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    FIXME("(%p)->(%s %08lx %p)\n", This, debugstr_w(strAttributeName), lFlags, pfSuccess);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle_toString(IHTMLStyle *iface, BSTR *String)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle(iface);
    WARN("(%p)->(%p)\n", This, String);
    return E_UNEXPECTED;
}

static const IHTMLStyleVtbl HTMLStyleVtbl = {
    HTMLCurrentStyle_HTMLStyle_QueryInterface,
    HTMLCurrentStyle_HTMLStyle_AddRef,
    HTMLCurrentStyle_HTMLStyle_Release,
    HTMLCurrentStyle_HTMLStyle_GetTypeInfoCount,
    HTMLCurrentStyle_HTMLStyle_GetTypeInfo,
    HTMLCurrentStyle_HTMLStyle_GetIDsOfNames,
    HTMLCurrentStyle_HTMLStyle_Invoke,
    HTMLCurrentStyle_HTMLStyle_put_fontFamily,
    HTMLCurrentStyle_HTMLStyle_get_fontFamily,
    HTMLCurrentStyle_HTMLStyle_put_fontStyle,
    HTMLCurrentStyle_HTMLStyle_get_fontStyle,
    HTMLCurrentStyle_HTMLStyle_put_fontVariant,
    HTMLCurrentStyle_HTMLStyle_get_fontVariant,
    HTMLCurrentStyle_HTMLStyle_put_fontWeight,
    HTMLCurrentStyle_HTMLStyle_get_fontWeight,
    HTMLCurrentStyle_HTMLStyle_put_fontSize,
    HTMLCurrentStyle_HTMLStyle_get_fontSize,
    HTMLCurrentStyle_HTMLStyle_put_font,
    HTMLCurrentStyle_HTMLStyle_get_font,
    HTMLCurrentStyle_HTMLStyle_put_color,
    HTMLCurrentStyle_HTMLStyle_get_color,
    HTMLCurrentStyle_HTMLStyle_put_background,
    HTMLCurrentStyle_HTMLStyle_get_background,
    HTMLCurrentStyle_HTMLStyle_put_backgroundColor,
    HTMLCurrentStyle_HTMLStyle_get_backgroundColor,
    HTMLCurrentStyle_HTMLStyle_put_backgroundImage,
    HTMLCurrentStyle_HTMLStyle_get_backgroundImage,
    HTMLCurrentStyle_HTMLStyle_put_backgroundRepeat,
    HTMLCurrentStyle_HTMLStyle_get_backgroundRepeat,
    HTMLCurrentStyle_HTMLStyle_put_backgroundAttachment,
    HTMLCurrentStyle_HTMLStyle_get_backgroundAttachment,
    HTMLCurrentStyle_HTMLStyle_put_backgroundPosition,
    HTMLCurrentStyle_HTMLStyle_get_backgroundPosition,
    HTMLCurrentStyle_HTMLStyle_put_backgroundPositionX,
    HTMLCurrentStyle_HTMLStyle_get_backgroundPositionX,
    HTMLCurrentStyle_HTMLStyle_put_backgroundPositionY,
    HTMLCurrentStyle_HTMLStyle_get_backgroundPositionY,
    HTMLCurrentStyle_HTMLStyle_put_wordSpacing,
    HTMLCurrentStyle_HTMLStyle_get_wordSpacing,
    HTMLCurrentStyle_HTMLStyle_put_letterSpacing,
    HTMLCurrentStyle_HTMLStyle_get_letterSpacing,
    HTMLCurrentStyle_HTMLStyle_put_textDecoration,
    HTMLCurrentStyle_HTMLStyle_get_textDecoration,
    HTMLCurrentStyle_HTMLStyle_put_textDecorationNone,
    HTMLCurrentStyle_HTMLStyle_get_textDecorationNone,
    HTMLCurrentStyle_HTMLStyle_put_textDecorationUnderline,
    HTMLCurrentStyle_HTMLStyle_get_textDecorationUnderline,
    HTMLCurrentStyle_HTMLStyle_put_textDecorationOverline,
    HTMLCurrentStyle_HTMLStyle_get_textDecorationOverline,
    HTMLCurrentStyle_HTMLStyle_put_textDecorationLineThrough,
    HTMLCurrentStyle_HTMLStyle_get_textDecorationLineThrough,
    HTMLCurrentStyle_HTMLStyle_put_textDecorationBlink,
    HTMLCurrentStyle_HTMLStyle_get_textDecorationBlink,
    HTMLCurrentStyle_HTMLStyle_put_verticalAlign,
    HTMLCurrentStyle_HTMLStyle_get_verticalAlign,
    HTMLCurrentStyle_HTMLStyle_put_textTransform,
    HTMLCurrentStyle_HTMLStyle_get_textTransform,
    HTMLCurrentStyle_HTMLStyle_put_textAlign,
    HTMLCurrentStyle_HTMLStyle_get_textAlign,
    HTMLCurrentStyle_HTMLStyle_put_textIndent,
    HTMLCurrentStyle_HTMLStyle_get_textIndent,
    HTMLCurrentStyle_HTMLStyle_put_lineHeight,
    HTMLCurrentStyle_HTMLStyle_get_lineHeight,
    HTMLCurrentStyle_HTMLStyle_put_marginTop,
    HTMLCurrentStyle_HTMLStyle_get_marginTop,
    HTMLCurrentStyle_HTMLStyle_put_marginRight,
    HTMLCurrentStyle_HTMLStyle_get_marginRight,
    HTMLCurrentStyle_HTMLStyle_put_marginBottom,
    HTMLCurrentStyle_HTMLStyle_get_marginBottom,
    HTMLCurrentStyle_HTMLStyle_put_marginLeft,
    HTMLCurrentStyle_HTMLStyle_get_marginLeft,
    HTMLCurrentStyle_HTMLStyle_put_margin,
    HTMLCurrentStyle_HTMLStyle_get_margin,
    HTMLCurrentStyle_HTMLStyle_put_paddingTop,
    HTMLCurrentStyle_HTMLStyle_get_paddingTop,
    HTMLCurrentStyle_HTMLStyle_put_paddingRight,
    HTMLCurrentStyle_HTMLStyle_get_paddingRight,
    HTMLCurrentStyle_HTMLStyle_put_paddingBottom,
    HTMLCurrentStyle_HTMLStyle_get_paddingBottom,
    HTMLCurrentStyle_HTMLStyle_put_paddingLeft,
    HTMLCurrentStyle_HTMLStyle_get_paddingLeft,
    HTMLCurrentStyle_HTMLStyle_put_padding,
    HTMLCurrentStyle_HTMLStyle_get_padding,
    HTMLCurrentStyle_HTMLStyle_put_border,
    HTMLCurrentStyle_HTMLStyle_get_border,
    HTMLCurrentStyle_HTMLStyle_put_borderTop,
    HTMLCurrentStyle_HTMLStyle_get_borderTop,
    HTMLCurrentStyle_HTMLStyle_put_borderRight,
    HTMLCurrentStyle_HTMLStyle_get_borderRight,
    HTMLCurrentStyle_HTMLStyle_put_borderBottom,
    HTMLCurrentStyle_HTMLStyle_get_borderBottom,
    HTMLCurrentStyle_HTMLStyle_put_borderLeft,
    HTMLCurrentStyle_HTMLStyle_get_borderLeft,
    HTMLCurrentStyle_HTMLStyle_put_borderColor,
    HTMLCurrentStyle_HTMLStyle_get_borderColor,
    HTMLCurrentStyle_HTMLStyle_put_borderTopColor,
    HTMLCurrentStyle_HTMLStyle_get_borderTopColor,
    HTMLCurrentStyle_HTMLStyle_put_borderRightColor,
    HTMLCurrentStyle_HTMLStyle_get_borderRightColor,
    HTMLCurrentStyle_HTMLStyle_put_borderBottomColor,
    HTMLCurrentStyle_HTMLStyle_get_borderBottomColor,
    HTMLCurrentStyle_HTMLStyle_put_borderLeftColor,
    HTMLCurrentStyle_HTMLStyle_get_borderLeftColor,
    HTMLCurrentStyle_HTMLStyle_put_borderWidth,
    HTMLCurrentStyle_HTMLStyle_get_borderWidth,
    HTMLCurrentStyle_HTMLStyle_put_borderTopWidth,
    HTMLCurrentStyle_HTMLStyle_get_borderTopWidth,
    HTMLCurrentStyle_HTMLStyle_put_borderRightWidth,
    HTMLCurrentStyle_HTMLStyle_get_borderRightWidth,
    HTMLCurrentStyle_HTMLStyle_put_borderBottomWidth,
    HTMLCurrentStyle_HTMLStyle_get_borderBottomWidth,
    HTMLCurrentStyle_HTMLStyle_put_borderLeftWidth,
    HTMLCurrentStyle_HTMLStyle_get_borderLeftWidth,
    HTMLCurrentStyle_HTMLStyle_put_borderStyle,
    HTMLCurrentStyle_HTMLStyle_get_borderStyle,
    HTMLCurrentStyle_HTMLStyle_put_borderTopStyle,
    HTMLCurrentStyle_HTMLStyle_get_borderTopStyle,
    HTMLCurrentStyle_HTMLStyle_put_borderRightStyle,
    HTMLCurrentStyle_HTMLStyle_get_borderRightStyle,
    HTMLCurrentStyle_HTMLStyle_put_borderBottomStyle,
    HTMLCurrentStyle_HTMLStyle_get_borderBottomStyle,
    HTMLCurrentStyle_HTMLStyle_put_borderLeftStyle,
    HTMLCurrentStyle_HTMLStyle_get_borderLeftStyle,
    HTMLCurrentStyle_HTMLStyle_put_width,
    HTMLCurrentStyle_HTMLStyle_get_width,
    HTMLCurrentStyle_HTMLStyle_put_height,
    HTMLCurrentStyle_HTMLStyle_get_height,
    HTMLCurrentStyle_HTMLStyle_put_styleFloat,
    HTMLCurrentStyle_HTMLStyle_get_styleFloat,
    HTMLCurrentStyle_HTMLStyle_put_clear,
    HTMLCurrentStyle_HTMLStyle_get_clear,
    HTMLCurrentStyle_HTMLStyle_put_display,
    HTMLCurrentStyle_HTMLStyle_get_display,
    HTMLCurrentStyle_HTMLStyle_put_visibility,
    HTMLCurrentStyle_HTMLStyle_get_visibility,
    HTMLCurrentStyle_HTMLStyle_put_listStyleType,
    HTMLCurrentStyle_HTMLStyle_get_listStyleType,
    HTMLCurrentStyle_HTMLStyle_put_listStylePosition,
    HTMLCurrentStyle_HTMLStyle_get_listStylePosition,
    HTMLCurrentStyle_HTMLStyle_put_listStyleImage,
    HTMLCurrentStyle_HTMLStyle_get_listStyleImage,
    HTMLCurrentStyle_HTMLStyle_put_listStyle,
    HTMLCurrentStyle_HTMLStyle_get_listStyle,
    HTMLCurrentStyle_HTMLStyle_put_whiteSpace,
    HTMLCurrentStyle_HTMLStyle_get_whiteSpace,
    HTMLCurrentStyle_HTMLStyle_put_top,
    HTMLCurrentStyle_HTMLStyle_get_top,
    HTMLCurrentStyle_HTMLStyle_put_left,
    HTMLCurrentStyle_HTMLStyle_get_left,
    HTMLCurrentStyle_HTMLStyle_get_position,
    HTMLCurrentStyle_HTMLStyle_put_zIndex,
    HTMLCurrentStyle_HTMLStyle_get_zIndex,
    HTMLCurrentStyle_HTMLStyle_put_overflow,
    HTMLCurrentStyle_HTMLStyle_get_overflow,
    HTMLCurrentStyle_HTMLStyle_put_pageBreakBefore,
    HTMLCurrentStyle_HTMLStyle_get_pageBreakBefore,
    HTMLCurrentStyle_HTMLStyle_put_pageBreakAfter,
    HTMLCurrentStyle_HTMLStyle_get_pageBreakAfter,
    HTMLCurrentStyle_HTMLStyle_put_cssText,
    HTMLCurrentStyle_HTMLStyle_get_cssText,
    HTMLCurrentStyle_HTMLStyle_put_pixelTop,
    HTMLCurrentStyle_HTMLStyle_get_pixelTop,
    HTMLCurrentStyle_HTMLStyle_put_pixelLeft,
    HTMLCurrentStyle_HTMLStyle_get_pixelLeft,
    HTMLCurrentStyle_HTMLStyle_put_pixelWidth,
    HTMLCurrentStyle_HTMLStyle_get_pixelWidth,
    HTMLCurrentStyle_HTMLStyle_put_pixelHeight,
    HTMLCurrentStyle_HTMLStyle_get_pixelHeight,
    HTMLCurrentStyle_HTMLStyle_put_posTop,
    HTMLCurrentStyle_HTMLStyle_get_posTop,
    HTMLCurrentStyle_HTMLStyle_put_posLeft,
    HTMLCurrentStyle_HTMLStyle_get_posLeft,
    HTMLCurrentStyle_HTMLStyle_put_posWidth,
    HTMLCurrentStyle_HTMLStyle_get_posWidth,
    HTMLCurrentStyle_HTMLStyle_put_posHeight,
    HTMLCurrentStyle_HTMLStyle_get_posHeight,
    HTMLCurrentStyle_HTMLStyle_put_cursor,
    HTMLCurrentStyle_HTMLStyle_get_cursor,
    HTMLCurrentStyle_HTMLStyle_put_clip,
    HTMLCurrentStyle_HTMLStyle_get_clip,
    HTMLCurrentStyle_HTMLStyle_put_filter,
    HTMLCurrentStyle_HTMLStyle_get_filter,
    HTMLCurrentStyle_HTMLStyle_setAttribute,
    HTMLCurrentStyle_HTMLStyle_getAttribute,
    HTMLCurrentStyle_HTMLStyle_removeAttribute,
    HTMLCurrentStyle_HTMLStyle_toString
};

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle2_QueryInterface(IHTMLStyle2 *iface, REFIID riid, void **ppv)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle2(iface);
    return IHTMLCurrentStyle_QueryInterface(&This->IHTMLCurrentStyle_iface, riid, ppv);
}

static ULONG WINAPI HTMLCurrentStyle_HTMLStyle2_AddRef(IHTMLStyle2 *iface)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle2(iface);
    return IHTMLCurrentStyle_AddRef(&This->IHTMLCurrentStyle_iface);
}

static ULONG WINAPI HTMLCurrentStyle_HTMLStyle2_Release(IHTMLStyle2 *iface)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle2(iface);
    return IHTMLCurrentStyle_Release(&This->IHTMLCurrentStyle_iface);
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle2_GetTypeInfoCount(IHTMLStyle2 *iface, UINT *pctinfo)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle2(iface);
    return IDispatchEx_GetTypeInfoCount(&This->css_style.dispex.IDispatchEx_iface, pctinfo);
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle2_GetTypeInfo(IHTMLStyle2 *iface, UINT iTInfo,
                                              LCID lcid, ITypeInfo **ppTInfo)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle2(iface);
    return IDispatchEx_GetTypeInfo(&This->css_style.dispex.IDispatchEx_iface, iTInfo, lcid, ppTInfo);
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle2_GetIDsOfNames(IHTMLStyle2 *iface, REFIID riid,
                                                LPOLESTR *rgszNames, UINT cNames,
                                                LCID lcid, DISPID *rgDispId)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle2(iface);
    return IDispatchEx_GetIDsOfNames(&This->css_style.dispex.IDispatchEx_iface, riid, rgszNames, cNames,
            lcid, rgDispId);
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle2_Invoke(IHTMLStyle2 *iface, DISPID dispIdMember,
                            REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS *pDispParams,
                            VARIANT *pVarResult, EXCEPINFO *pExcepInfo, UINT *puArgErr)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle2(iface);
    return IDispatchEx_Invoke(&This->css_style.dispex.IDispatchEx_iface, dispIdMember, riid, lcid,
            wFlags, pDispParams, pVarResult, pExcepInfo, puArgErr);
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle2_put_tableLayout(IHTMLStyle2 *iface, BSTR v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle2(iface);
    WARN("(%p)->(%s)\n", This, debugstr_w(v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle2_get_tableLayout(IHTMLStyle2 *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle2(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle2_put_borderCollapse(IHTMLStyle2 *iface, BSTR v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle2(iface);
    WARN("(%p)->(%s)\n", This, debugstr_w(v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle2_get_borderCollapse(IHTMLStyle2 *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle2(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle2_put_direction(IHTMLStyle2 *iface, BSTR v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle2(iface);
    WARN("(%p)->(%s)\n", This, debugstr_w(v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle2_get_direction(IHTMLStyle2 *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle2(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle2_put_behavior(IHTMLStyle2 *iface, BSTR v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle2(iface);
    WARN("(%p)->(%s)\n", This, debugstr_w(v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle2_get_behavior(IHTMLStyle2 *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle2(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle2_setExpression(IHTMLStyle2 *iface, BSTR propname, BSTR expression, BSTR language)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle2(iface);
    WARN("(%p)->(%s %s %s)\n", This, debugstr_w(propname), debugstr_w(expression), debugstr_w(language));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle2_getExpression(IHTMLStyle2 *iface, BSTR propname, VARIANT *expression)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle2(iface);
    WARN("(%p)->(%s %p)\n", This, debugstr_w(propname), expression);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle2_removeExpression(IHTMLStyle2 *iface, BSTR propname, VARIANT_BOOL *pfSuccess)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle2(iface);
    WARN("(%p)->(%s %p)\n", This, debugstr_w(propname), pfSuccess);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle2_put_position(IHTMLStyle2 *iface, BSTR v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle2(iface);
    WARN("(%p)->(%s)\n", This, debugstr_w(v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle2_get_position(IHTMLStyle2 *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle2(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle2_put_unicodeBidi(IHTMLStyle2 *iface, BSTR v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle2(iface);
    WARN("(%p)->(%s)\n", This, debugstr_w(v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle2_get_unicodeBidi(IHTMLStyle2 *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle2(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle2_put_bottom(IHTMLStyle2 *iface, VARIANT v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle2(iface);
    WARN("(%p)->(%s)\n", This, debugstr_variant(&v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle2_get_bottom(IHTMLStyle2 *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle2(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle2_put_right(IHTMLStyle2 *iface, VARIANT v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle2(iface);
    WARN("(%p)->(%s)\n", This, debugstr_variant(&v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle2_get_right(IHTMLStyle2 *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle2(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle2_put_pixelBottom(IHTMLStyle2 *iface, LONG v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle2(iface);
    WARN("(%p)->(%ld)\n", This, v);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle2_get_pixelBottom(IHTMLStyle2 *iface, LONG *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle2(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle2_put_pixelRight(IHTMLStyle2 *iface, LONG v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle2(iface);
    WARN("(%p)->(%ld)\n", This, v);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle2_get_pixelRight(IHTMLStyle2 *iface, LONG *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle2(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle2_put_posBottom(IHTMLStyle2 *iface, float v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle2(iface);
    WARN("(%p)->(%f)\n", This, v);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle2_get_posBottom(IHTMLStyle2 *iface, float *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle2(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle2_put_posRight(IHTMLStyle2 *iface, float v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle2(iface);
    WARN("(%p)->(%f)\n", This, v);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle2_get_posRight(IHTMLStyle2 *iface, float *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle2(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle2_put_imeMode(IHTMLStyle2 *iface, BSTR v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle2(iface);
    TRACE("(%p)->(%s)\n", This, debugstr_w(v));
    return E_FAIL;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle2_get_imeMode(IHTMLStyle2 *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle2(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle2_put_rubyAlign(IHTMLStyle2 *iface, BSTR v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle2(iface);
    WARN("(%p)->(%s)\n", This, debugstr_w(v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle2_get_rubyAlign(IHTMLStyle2 *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle2(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle2_put_rubyPosition(IHTMLStyle2 *iface, BSTR v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle2(iface);
    WARN("(%p)->(%s)\n", This, debugstr_w(v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle2_get_rubyPosition(IHTMLStyle2 *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle2(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle2_put_rubyOverhang(IHTMLStyle2 *iface, BSTR v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle2(iface);
    WARN("(%p)->(%s)\n", This, debugstr_w(v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle2_get_rubyOverhang(IHTMLStyle2 *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle2(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle2_put_layoutGridChar(IHTMLStyle2 *iface, VARIANT v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle2(iface);
    TRACE("(%p)->(%s)\n", This, debugstr_variant(&v));
    return E_FAIL;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle2_get_layoutGridChar(IHTMLStyle2 *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle2(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle2_put_layoutGridLine(IHTMLStyle2 *iface, VARIANT v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle2(iface);
    TRACE("(%p)->(%s)\n", This, debugstr_variant(&v));
    return E_FAIL;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle2_get_layoutGridLine(IHTMLStyle2 *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle2(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle2_put_layoutGridMode(IHTMLStyle2 *iface, BSTR v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle2(iface);
    TRACE("(%p)->(%s)\n", This, debugstr_w(v));
    return E_FAIL;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle2_get_layoutGridMode(IHTMLStyle2 *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle2(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle2_put_layoutGridType(IHTMLStyle2 *iface, BSTR v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle2(iface);
    TRACE("(%p)->(%s)\n", This, debugstr_w(v));
    return E_FAIL;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle2_get_layoutGridType(IHTMLStyle2 *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle2(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle2_put_layoutGrid(IHTMLStyle2 *iface, BSTR v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle2(iface);
    TRACE("(%p)->(%s)\n", This, debugstr_w(v));
    return E_FAIL;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle2_get_layoutGrid(IHTMLStyle2 *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle2(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle2_put_wordBreak(IHTMLStyle2 *iface, BSTR v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle2(iface);
    WARN("(%p)->(%s)\n", This, debugstr_w(v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle2_get_wordBreak(IHTMLStyle2 *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle2(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle2_put_lineBreak(IHTMLStyle2 *iface, BSTR v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle2(iface);
    TRACE("(%p)->(%s)\n", This, debugstr_w(v));
    return E_FAIL;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle2_get_lineBreak(IHTMLStyle2 *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle2(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle2_put_textJustify(IHTMLStyle2 *iface, BSTR v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle2(iface);
    WARN("(%p)->(%s)\n", This, debugstr_w(v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle2_get_textJustify(IHTMLStyle2 *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle2(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle2_put_textJustifyTrim(IHTMLStyle2 *iface, BSTR v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle2(iface);
    TRACE("(%p)->(%s)\n", This, debugstr_w(v));
    return E_FAIL;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle2_get_textJustifyTrim(IHTMLStyle2 *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle2(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle2_put_textKashida(IHTMLStyle2 *iface, VARIANT v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle2(iface);
    TRACE("(%p)->(%s)\n", This, debugstr_variant(&v));
    return E_FAIL;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle2_get_textKashida(IHTMLStyle2 *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle2(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle2_put_textAutospace(IHTMLStyle2 *iface, BSTR v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle2(iface);
    TRACE("(%p)->(%s)\n", This, debugstr_w(v));
    return E_FAIL;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle2_get_textAutospace(IHTMLStyle2 *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle2(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle2_put_overflowX(IHTMLStyle2 *iface, BSTR v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle2(iface);
    WARN("(%p)->(%s)\n", This, debugstr_w(v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle2_get_overflowX(IHTMLStyle2 *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle2(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle2_put_overflowY(IHTMLStyle2 *iface, BSTR v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle2(iface);
    WARN("(%p)->(%s)\n", This, debugstr_w(v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle2_get_overflowY(IHTMLStyle2 *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle2(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle2_put_accelerator(IHTMLStyle2 *iface, BSTR v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle2(iface);
    TRACE("(%p)->(%s)\n", This, debugstr_w(v));
    return E_FAIL;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle2_get_accelerator(IHTMLStyle2 *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle2(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static const IHTMLStyle2Vtbl HTMLStyle2Vtbl = {
    HTMLCurrentStyle_HTMLStyle2_QueryInterface,
    HTMLCurrentStyle_HTMLStyle2_AddRef,
    HTMLCurrentStyle_HTMLStyle2_Release,
    HTMLCurrentStyle_HTMLStyle2_GetTypeInfoCount,
    HTMLCurrentStyle_HTMLStyle2_GetTypeInfo,
    HTMLCurrentStyle_HTMLStyle2_GetIDsOfNames,
    HTMLCurrentStyle_HTMLStyle2_Invoke,
    HTMLCurrentStyle_HTMLStyle2_put_tableLayout,
    HTMLCurrentStyle_HTMLStyle2_get_tableLayout,
    HTMLCurrentStyle_HTMLStyle2_put_borderCollapse,
    HTMLCurrentStyle_HTMLStyle2_get_borderCollapse,
    HTMLCurrentStyle_HTMLStyle2_put_direction,
    HTMLCurrentStyle_HTMLStyle2_get_direction,
    HTMLCurrentStyle_HTMLStyle2_put_behavior,
    HTMLCurrentStyle_HTMLStyle2_get_behavior,
    HTMLCurrentStyle_HTMLStyle2_setExpression,
    HTMLCurrentStyle_HTMLStyle2_getExpression,
    HTMLCurrentStyle_HTMLStyle2_removeExpression,
    HTMLCurrentStyle_HTMLStyle2_put_position,
    HTMLCurrentStyle_HTMLStyle2_get_position,
    HTMLCurrentStyle_HTMLStyle2_put_unicodeBidi,
    HTMLCurrentStyle_HTMLStyle2_get_unicodeBidi,
    HTMLCurrentStyle_HTMLStyle2_put_bottom,
    HTMLCurrentStyle_HTMLStyle2_get_bottom,
    HTMLCurrentStyle_HTMLStyle2_put_right,
    HTMLCurrentStyle_HTMLStyle2_get_right,
    HTMLCurrentStyle_HTMLStyle2_put_pixelBottom,
    HTMLCurrentStyle_HTMLStyle2_get_pixelBottom,
    HTMLCurrentStyle_HTMLStyle2_put_pixelRight,
    HTMLCurrentStyle_HTMLStyle2_get_pixelRight,
    HTMLCurrentStyle_HTMLStyle2_put_posBottom,
    HTMLCurrentStyle_HTMLStyle2_get_posBottom,
    HTMLCurrentStyle_HTMLStyle2_put_posRight,
    HTMLCurrentStyle_HTMLStyle2_get_posRight,
    HTMLCurrentStyle_HTMLStyle2_put_imeMode,
    HTMLCurrentStyle_HTMLStyle2_get_imeMode,
    HTMLCurrentStyle_HTMLStyle2_put_rubyAlign,
    HTMLCurrentStyle_HTMLStyle2_get_rubyAlign,
    HTMLCurrentStyle_HTMLStyle2_put_rubyPosition,
    HTMLCurrentStyle_HTMLStyle2_get_rubyPosition,
    HTMLCurrentStyle_HTMLStyle2_put_rubyOverhang,
    HTMLCurrentStyle_HTMLStyle2_get_rubyOverhang,
    HTMLCurrentStyle_HTMLStyle2_put_layoutGridChar,
    HTMLCurrentStyle_HTMLStyle2_get_layoutGridChar,
    HTMLCurrentStyle_HTMLStyle2_put_layoutGridLine,
    HTMLCurrentStyle_HTMLStyle2_get_layoutGridLine,
    HTMLCurrentStyle_HTMLStyle2_put_layoutGridMode,
    HTMLCurrentStyle_HTMLStyle2_get_layoutGridMode,
    HTMLCurrentStyle_HTMLStyle2_put_layoutGridType,
    HTMLCurrentStyle_HTMLStyle2_get_layoutGridType,
    HTMLCurrentStyle_HTMLStyle2_put_layoutGrid,
    HTMLCurrentStyle_HTMLStyle2_get_layoutGrid,
    HTMLCurrentStyle_HTMLStyle2_put_wordBreak,
    HTMLCurrentStyle_HTMLStyle2_get_wordBreak,
    HTMLCurrentStyle_HTMLStyle2_put_lineBreak,
    HTMLCurrentStyle_HTMLStyle2_get_lineBreak,
    HTMLCurrentStyle_HTMLStyle2_put_textJustify,
    HTMLCurrentStyle_HTMLStyle2_get_textJustify,
    HTMLCurrentStyle_HTMLStyle2_put_textJustifyTrim,
    HTMLCurrentStyle_HTMLStyle2_get_textJustifyTrim,
    HTMLCurrentStyle_HTMLStyle2_put_textKashida,
    HTMLCurrentStyle_HTMLStyle2_get_textKashida,
    HTMLCurrentStyle_HTMLStyle2_put_textAutospace,
    HTMLCurrentStyle_HTMLStyle2_get_textAutospace,
    HTMLCurrentStyle_HTMLStyle2_put_overflowX,
    HTMLCurrentStyle_HTMLStyle2_get_overflowX,
    HTMLCurrentStyle_HTMLStyle2_put_overflowY,
    HTMLCurrentStyle_HTMLStyle2_get_overflowY,
    HTMLCurrentStyle_HTMLStyle2_put_accelerator,
    HTMLCurrentStyle_HTMLStyle2_get_accelerator
};

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle3_QueryInterface(IHTMLStyle3 *iface, REFIID riid, void **ppv)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle3(iface);
    return IHTMLCurrentStyle_QueryInterface(&This->IHTMLCurrentStyle_iface, riid, ppv);
}

static ULONG WINAPI HTMLCurrentStyle_HTMLStyle3_AddRef(IHTMLStyle3 *iface)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle3(iface);
    return IHTMLCurrentStyle_AddRef(&This->IHTMLCurrentStyle_iface);
}

static ULONG WINAPI HTMLCurrentStyle_HTMLStyle3_Release(IHTMLStyle3 *iface)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle3(iface);
    return IHTMLCurrentStyle_Release(&This->IHTMLCurrentStyle_iface);
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle3_GetTypeInfoCount(IHTMLStyle3 *iface, UINT *pctinfo)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle3(iface);
    return IDispatchEx_GetTypeInfoCount(&This->css_style.dispex.IDispatchEx_iface, pctinfo);
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle3_GetTypeInfo(IHTMLStyle3 *iface, UINT iTInfo,
                                              LCID lcid, ITypeInfo **ppTInfo)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle3(iface);
    return IDispatchEx_GetTypeInfo(&This->css_style.dispex.IDispatchEx_iface, iTInfo, lcid, ppTInfo);
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle3_GetIDsOfNames(IHTMLStyle3 *iface, REFIID riid,
                                                LPOLESTR *rgszNames, UINT cNames,
                                                LCID lcid, DISPID *rgDispId)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle3(iface);
    return IDispatchEx_GetIDsOfNames(&This->css_style.dispex.IDispatchEx_iface, riid, rgszNames, cNames,
            lcid, rgDispId);
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle3_Invoke(IHTMLStyle3 *iface, DISPID dispIdMember,
                            REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS *pDispParams,
                            VARIANT *pVarResult, EXCEPINFO *pExcepInfo, UINT *puArgErr)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle3(iface);
    return IDispatchEx_Invoke(&This->css_style.dispex.IDispatchEx_iface, dispIdMember, riid, lcid,
            wFlags, pDispParams, pVarResult, pExcepInfo, puArgErr);
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle3_put_layoutFlow(IHTMLStyle3 *iface, BSTR v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle3(iface);
    TRACE("(%p)->(%s)\n", This, debugstr_w(v));
    return E_FAIL;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle3_get_layoutFlow(IHTMLStyle3 *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle3(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle3_put_zoom(IHTMLStyle3 *iface, VARIANT v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle3(iface);
    TRACE("(%p)->(%s)\n", This, debugstr_variant(&v));
    return E_FAIL;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle3_get_zoom(IHTMLStyle3 *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle3(iface);
    TRACE("(%p)->(%p)\n", This, p);
    return IHTMLCSSStyleDeclaration_get_zoom(&This->css_style.IHTMLCSSStyleDeclaration_iface, p);
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle3_put_wordWrap(IHTMLStyle3 *iface, BSTR v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle3(iface);
    WARN("(%p)->(%s)\n", This, debugstr_w(v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle3_get_wordWrap(IHTMLStyle3 *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle3(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle3_put_textUnderlinePosition(IHTMLStyle3 *iface, BSTR v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle3(iface);
    WARN("(%p)->(%s)\n", This, debugstr_w(v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle3_get_textUnderlinePosition(IHTMLStyle3 *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle3(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle3_put_scrollbarBaseColor(IHTMLStyle3 *iface, VARIANT v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle3(iface);
    TRACE("(%p)->(%s)\n", This, debugstr_variant(&v));
    return E_FAIL;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle3_get_scrollbarBaseColor(IHTMLStyle3 *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle3(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle3_put_scrollbarFaceColor(IHTMLStyle3 *iface, VARIANT v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle3(iface);
    TRACE("(%p)->(%s)\n", This, debugstr_variant(&v));
    return E_FAIL;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle3_get_scrollbarFaceColor(IHTMLStyle3 *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle3(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle3_put_scrollbar3dLightColor(IHTMLStyle3 *iface, VARIANT v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle3(iface);
    TRACE("(%p)->(%s)\n", This, debugstr_variant(&v));
    return E_FAIL;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle3_get_scrollbar3dLightColor(IHTMLStyle3 *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle3(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle3_put_scrollbarShadowColor(IHTMLStyle3 *iface, VARIANT v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle3(iface);
    TRACE("(%p)->(%s)\n", This, debugstr_variant(&v));
    return E_FAIL;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle3_get_scrollbarShadowColor(IHTMLStyle3 *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle3(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle3_put_scrollbarHighlightColor(IHTMLStyle3 *iface, VARIANT v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle3(iface);
    TRACE("(%p)->(%s)\n", This, debugstr_variant(&v));
    return E_FAIL;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle3_get_scrollbarHighlightColor(IHTMLStyle3 *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle3(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle3_put_scrollbarDarkShadowColor(IHTMLStyle3 *iface, VARIANT v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle3(iface);
    TRACE("(%p)->(%s)\n", This, debugstr_variant(&v));
    return E_FAIL;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle3_get_scrollbarDarkShadowColor(IHTMLStyle3 *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle3(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle3_put_scrollbarArrowColor(IHTMLStyle3 *iface, VARIANT v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle3(iface);
    TRACE("(%p)->(%s)\n", This, debugstr_variant(&v));
    return E_FAIL;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle3_get_scrollbarArrowColor(IHTMLStyle3 *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle3(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle3_put_scrollbarTrackColor(IHTMLStyle3 *iface, VARIANT v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle3(iface);
    TRACE("(%p)->(%s)\n", This, debugstr_variant(&v));
    return E_FAIL;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle3_get_scrollbarTrackColor(IHTMLStyle3 *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle3(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle3_put_writingMode(IHTMLStyle3 *iface, BSTR v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle3(iface);
    TRACE("(%p)->(%s)\n", This, debugstr_w(v));
    return E_FAIL;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle3_get_writingMode(IHTMLStyle3 *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle3(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle3_put_textAlignLast(IHTMLStyle3 *iface, BSTR v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle3(iface);
    WARN("(%p)->(%s)\n", This, debugstr_w(v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle3_get_textAlignLast(IHTMLStyle3 *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle3(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle3_put_textKashidaSpace(IHTMLStyle3 *iface, VARIANT v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle3(iface);
    TRACE("(%p)->(%s)\n", This, debugstr_variant(&v));
    return E_FAIL;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle3_get_textKashidaSpace(IHTMLStyle3 *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle3(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static const IHTMLStyle3Vtbl HTMLStyle3Vtbl = {
    HTMLCurrentStyle_HTMLStyle3_QueryInterface,
    HTMLCurrentStyle_HTMLStyle3_AddRef,
    HTMLCurrentStyle_HTMLStyle3_Release,
    HTMLCurrentStyle_HTMLStyle3_GetTypeInfoCount,
    HTMLCurrentStyle_HTMLStyle3_GetTypeInfo,
    HTMLCurrentStyle_HTMLStyle3_GetIDsOfNames,
    HTMLCurrentStyle_HTMLStyle3_Invoke,
    HTMLCurrentStyle_HTMLStyle3_put_layoutFlow,
    HTMLCurrentStyle_HTMLStyle3_get_layoutFlow,
    HTMLCurrentStyle_HTMLStyle3_put_zoom,
    HTMLCurrentStyle_HTMLStyle3_get_zoom,
    HTMLCurrentStyle_HTMLStyle3_put_wordWrap,
    HTMLCurrentStyle_HTMLStyle3_get_wordWrap,
    HTMLCurrentStyle_HTMLStyle3_put_textUnderlinePosition,
    HTMLCurrentStyle_HTMLStyle3_get_textUnderlinePosition,
    HTMLCurrentStyle_HTMLStyle3_put_scrollbarBaseColor,
    HTMLCurrentStyle_HTMLStyle3_get_scrollbarBaseColor,
    HTMLCurrentStyle_HTMLStyle3_put_scrollbarFaceColor,
    HTMLCurrentStyle_HTMLStyle3_get_scrollbarFaceColor,
    HTMLCurrentStyle_HTMLStyle3_put_scrollbar3dLightColor,
    HTMLCurrentStyle_HTMLStyle3_get_scrollbar3dLightColor,
    HTMLCurrentStyle_HTMLStyle3_put_scrollbarShadowColor,
    HTMLCurrentStyle_HTMLStyle3_get_scrollbarShadowColor,
    HTMLCurrentStyle_HTMLStyle3_put_scrollbarHighlightColor,
    HTMLCurrentStyle_HTMLStyle3_get_scrollbarHighlightColor,
    HTMLCurrentStyle_HTMLStyle3_put_scrollbarDarkShadowColor,
    HTMLCurrentStyle_HTMLStyle3_get_scrollbarDarkShadowColor,
    HTMLCurrentStyle_HTMLStyle3_put_scrollbarArrowColor,
    HTMLCurrentStyle_HTMLStyle3_get_scrollbarArrowColor,
    HTMLCurrentStyle_HTMLStyle3_put_scrollbarTrackColor,
    HTMLCurrentStyle_HTMLStyle3_get_scrollbarTrackColor,
    HTMLCurrentStyle_HTMLStyle3_put_writingMode,
    HTMLCurrentStyle_HTMLStyle3_get_writingMode,
    HTMLCurrentStyle_HTMLStyle3_put_textAlignLast,
    HTMLCurrentStyle_HTMLStyle3_get_textAlignLast,
    HTMLCurrentStyle_HTMLStyle3_put_textKashidaSpace,
    HTMLCurrentStyle_HTMLStyle3_get_textKashidaSpace
};

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle5_QueryInterface(IHTMLStyle5 *iface, REFIID riid, void **ppv)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle5(iface);
    return IHTMLCurrentStyle_QueryInterface(&This->IHTMLCurrentStyle_iface, riid, ppv);
}

static ULONG WINAPI HTMLCurrentStyle_HTMLStyle5_AddRef(IHTMLStyle5 *iface)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle5(iface);
    return IHTMLCurrentStyle_AddRef(&This->IHTMLCurrentStyle_iface);
}

static ULONG WINAPI HTMLCurrentStyle_HTMLStyle5_Release(IHTMLStyle5 *iface)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle5(iface);
    return IHTMLCurrentStyle_Release(&This->IHTMLCurrentStyle_iface);
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle5_GetTypeInfoCount(IHTMLStyle5 *iface, UINT *pctinfo)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle5(iface);
    return IDispatchEx_GetTypeInfoCount(&This->css_style.dispex.IDispatchEx_iface, pctinfo);
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle5_GetTypeInfo(IHTMLStyle5 *iface, UINT iTInfo,
        LCID lcid, ITypeInfo **ppTInfo)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle5(iface);
    return IDispatchEx_GetTypeInfo(&This->css_style.dispex.IDispatchEx_iface, iTInfo, lcid, ppTInfo);
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle5_GetIDsOfNames(IHTMLStyle5 *iface, REFIID riid,
        LPOLESTR *rgszNames, UINT cNames, LCID lcid, DISPID *rgDispId)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle5(iface);
    return IDispatchEx_GetIDsOfNames(&This->css_style.dispex.IDispatchEx_iface, riid, rgszNames, cNames,
            lcid, rgDispId);
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle5_Invoke(IHTMLStyle5 *iface, DISPID dispIdMember,
                            REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS *pDispParams,
                            VARIANT *pVarResult, EXCEPINFO *pExcepInfo, UINT *puArgErr)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle5(iface);
    return IDispatchEx_Invoke(&This->css_style.dispex.IDispatchEx_iface, dispIdMember, riid, lcid,
            wFlags, pDispParams, pVarResult, pExcepInfo, puArgErr);
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle5_put_msInterpolationMode(IHTMLStyle5 *iface, BSTR v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle5(iface);
    TRACE("(%p)->(%s)\n", This, debugstr_w(v));
    return E_FAIL;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle5_get_msInterpolationMode(IHTMLStyle5 *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle5(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle5_put_maxHeight(IHTMLStyle5 *iface, VARIANT v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle5(iface);
    WARN("(%p)->(%s)\n", This, debugstr_variant(&v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle5_get_maxHeight(IHTMLStyle5 *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle5(iface);
    WARN("(%p)->(%s)\n", This, debugstr_variant(p));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle5_put_minWidth(IHTMLStyle5 *iface, VARIANT v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle5(iface);
    WARN("(%p)->(%s)\n", This, debugstr_variant(&v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle5_get_minWidth(IHTMLStyle5 *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle5(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle5_put_maxWidth(IHTMLStyle5 *iface, VARIANT v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle5(iface);
    WARN("(%p)->(%s)\n", This, debugstr_variant(&v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle5_get_maxWidth(IHTMLStyle5 *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle5(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static const IHTMLStyle5Vtbl HTMLStyle5Vtbl = {
    HTMLCurrentStyle_HTMLStyle5_QueryInterface,
    HTMLCurrentStyle_HTMLStyle5_AddRef,
    HTMLCurrentStyle_HTMLStyle5_Release,
    HTMLCurrentStyle_HTMLStyle5_GetTypeInfoCount,
    HTMLCurrentStyle_HTMLStyle5_GetTypeInfo,
    HTMLCurrentStyle_HTMLStyle5_GetIDsOfNames,
    HTMLCurrentStyle_HTMLStyle5_Invoke,
    HTMLCurrentStyle_HTMLStyle5_put_msInterpolationMode,
    HTMLCurrentStyle_HTMLStyle5_get_msInterpolationMode,
    HTMLCurrentStyle_HTMLStyle5_put_maxHeight,
    HTMLCurrentStyle_HTMLStyle5_get_maxHeight,
    HTMLCurrentStyle_HTMLStyle5_put_minWidth,
    HTMLCurrentStyle_HTMLStyle5_get_minWidth,
    HTMLCurrentStyle_HTMLStyle5_put_maxWidth,
    HTMLCurrentStyle_HTMLStyle5_get_maxWidth
};

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle6_QueryInterface(IHTMLStyle6 *iface, REFIID riid, void **ppv)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle6(iface);
    return IHTMLCurrentStyle_QueryInterface(&This->IHTMLCurrentStyle_iface, riid, ppv);
}

static ULONG WINAPI HTMLCurrentStyle_HTMLStyle6_AddRef(IHTMLStyle6 *iface)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle6(iface);
    return IHTMLCurrentStyle_AddRef(&This->IHTMLCurrentStyle_iface);
}

static ULONG WINAPI HTMLCurrentStyle_HTMLStyle6_Release(IHTMLStyle6 *iface)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle6(iface);
    return IHTMLCurrentStyle_Release(&This->IHTMLCurrentStyle_iface);
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle6_GetTypeInfoCount(IHTMLStyle6 *iface, UINT *pctinfo)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle6(iface);
    return IDispatchEx_GetTypeInfoCount(&This->css_style.dispex.IDispatchEx_iface, pctinfo);
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle6_GetTypeInfo(IHTMLStyle6 *iface, UINT iTInfo,
        LCID lcid, ITypeInfo **ppTInfo)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle6(iface);
    return IDispatchEx_GetTypeInfo(&This->css_style.dispex.IDispatchEx_iface, iTInfo, lcid, ppTInfo);
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle6_GetIDsOfNames(IHTMLStyle6 *iface, REFIID riid,
        LPOLESTR *rgszNames, UINT cNames, LCID lcid, DISPID *rgDispId)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle6(iface);
    return IDispatchEx_GetIDsOfNames(&This->css_style.dispex.IDispatchEx_iface, riid, rgszNames, cNames,
            lcid, rgDispId);
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle6_Invoke(IHTMLStyle6 *iface, DISPID dispIdMember,
                            REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS *pDispParams,
                            VARIANT *pVarResult, EXCEPINFO *pExcepInfo, UINT *puArgErr)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle6(iface);
    return IDispatchEx_Invoke(&This->css_style.dispex.IDispatchEx_iface, dispIdMember, riid, lcid,
            wFlags, pDispParams, pVarResult, pExcepInfo, puArgErr);
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle6_put_content(IHTMLStyle6 *iface, BSTR v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle6(iface);
    WARN("(%p)->(%s)\n", This, debugstr_w(v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle6_get_content(IHTMLStyle6 *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle6(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle6_put_contentSide(IHTMLStyle6 *iface, BSTR v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle6(iface);
    WARN("(%p)->(%s)\n", This, debugstr_w(v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle6_get_contentSide(IHTMLStyle6 *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle6(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle6_put_counterIncrement(IHTMLStyle6 *iface, BSTR v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle6(iface);
    WARN("(%p)->(%s)\n", This, debugstr_w(v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle6_get_counterIncrement(IHTMLStyle6 *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle6(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle6_put_counterReset(IHTMLStyle6 *iface, BSTR v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle6(iface);
    WARN("(%p)->(%s)\n", This, debugstr_w(v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle6_get_counterReset(IHTMLStyle6 *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle6(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle6_put_outline(IHTMLStyle6 *iface, BSTR v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle6(iface);
    WARN("(%p)->(%s)\n", This, debugstr_w(v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle6_get_outline(IHTMLStyle6 *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle6(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle6_put_outlineWidth(IHTMLStyle6 *iface, VARIANT v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle6(iface);
    WARN("(%p)->(%s)\n", This, debugstr_variant(&v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle6_get_outlineWidth(IHTMLStyle6 *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle6(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle6_put_outlineStyle(IHTMLStyle6 *iface, BSTR v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle6(iface);
    WARN("(%p)->(%s)\n", This, debugstr_w(v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle6_get_outlineStyle(IHTMLStyle6 *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle6(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle6_put_outlineColor(IHTMLStyle6 *iface, VARIANT v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle6(iface);
    WARN("(%p)->(%s)\n", This, debugstr_variant(&v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle6_get_outlineColor(IHTMLStyle6 *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle6(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle6_put_boxSizing(IHTMLStyle6 *iface, BSTR v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle6(iface);
    WARN("(%p)->(%s)\n", This, debugstr_w(v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle6_get_boxSizing(IHTMLStyle6 *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle6(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle6_put_borderSpacing(IHTMLStyle6 *iface, BSTR v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle6(iface);
    WARN("(%p)->(%s)\n", This, debugstr_w(v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle6_get_borderSpacing(IHTMLStyle6 *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle6(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle6_put_orphans(IHTMLStyle6 *iface, VARIANT v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle6(iface);
    WARN("(%p)->(%s)\n", This, debugstr_variant(&v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle6_get_orphans(IHTMLStyle6 *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle6(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle6_put_windows(IHTMLStyle6 *iface, VARIANT v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle6(iface);
    WARN("(%p)->(%s)\n", This, debugstr_variant(&v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle6_get_windows(IHTMLStyle6 *iface, VARIANT *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle6(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle6_put_pageBreakInside(IHTMLStyle6 *iface, BSTR v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle6(iface);
    WARN("(%p)->(%s)\n", This, debugstr_w(v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle6_get_pageBreakInside(IHTMLStyle6 *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle6(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle6_put_emptyCells(IHTMLStyle6 *iface, BSTR v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle6(iface);
    WARN("(%p)->(%s)\n", This, debugstr_w(v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle6_get_emptyCells(IHTMLStyle6 *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle6(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle6_put_msBlockProgression(IHTMLStyle6 *iface, BSTR v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle6(iface);
    TRACE("(%p)->(%s)\n", This, debugstr_w(v));
    return E_FAIL;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle6_get_msBlockProgression(IHTMLStyle6 *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle6(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle6_put_quotes(IHTMLStyle6 *iface, BSTR v)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle6(iface);
    WARN("(%p)->(%s)\n", This, debugstr_w(v));
    return E_UNEXPECTED;
}

static HRESULT WINAPI HTMLCurrentStyle_HTMLStyle6_get_quotes(IHTMLStyle6 *iface, BSTR *p)
{
    HTMLCurrentStyle *This = impl_from_IHTMLStyle6(iface);
    WARN("(%p)->(%p)\n", This, p);
    return E_UNEXPECTED;
}

static const IHTMLStyle6Vtbl HTMLStyle6Vtbl = {
    HTMLCurrentStyle_HTMLStyle6_QueryInterface,
    HTMLCurrentStyle_HTMLStyle6_AddRef,
    HTMLCurrentStyle_HTMLStyle6_Release,
    HTMLCurrentStyle_HTMLStyle6_GetTypeInfoCount,
    HTMLCurrentStyle_HTMLStyle6_GetTypeInfo,
    HTMLCurrentStyle_HTMLStyle6_GetIDsOfNames,
    HTMLCurrentStyle_HTMLStyle6_Invoke,
    HTMLCurrentStyle_HTMLStyle6_put_content,
    HTMLCurrentStyle_HTMLStyle6_get_content,
    HTMLCurrentStyle_HTMLStyle6_put_contentSide,
    HTMLCurrentStyle_HTMLStyle6_get_contentSide,
    HTMLCurrentStyle_HTMLStyle6_put_counterIncrement,
    HTMLCurrentStyle_HTMLStyle6_get_counterIncrement,
    HTMLCurrentStyle_HTMLStyle6_put_counterReset,
    HTMLCurrentStyle_HTMLStyle6_get_counterReset,
    HTMLCurrentStyle_HTMLStyle6_put_outline,
    HTMLCurrentStyle_HTMLStyle6_get_outline,
    HTMLCurrentStyle_HTMLStyle6_put_outlineWidth,
    HTMLCurrentStyle_HTMLStyle6_get_outlineWidth,
    HTMLCurrentStyle_HTMLStyle6_put_outlineStyle,
    HTMLCurrentStyle_HTMLStyle6_get_outlineStyle,
    HTMLCurrentStyle_HTMLStyle6_put_outlineColor,
    HTMLCurrentStyle_HTMLStyle6_get_outlineColor,
    HTMLCurrentStyle_HTMLStyle6_put_boxSizing,
    HTMLCurrentStyle_HTMLStyle6_get_boxSizing,
    HTMLCurrentStyle_HTMLStyle6_put_borderSpacing,
    HTMLCurrentStyle_HTMLStyle6_get_borderSpacing,
    HTMLCurrentStyle_HTMLStyle6_put_orphans,
    HTMLCurrentStyle_HTMLStyle6_get_orphans,
    HTMLCurrentStyle_HTMLStyle6_put_windows,
    HTMLCurrentStyle_HTMLStyle6_get_windows,
    HTMLCurrentStyle_HTMLStyle6_put_pageBreakInside,
    HTMLCurrentStyle_HTMLStyle6_get_pageBreakInside,
    HTMLCurrentStyle_HTMLStyle6_put_emptyCells,
    HTMLCurrentStyle_HTMLStyle6_get_emptyCells,
    HTMLCurrentStyle_HTMLStyle6_put_msBlockProgression,
    HTMLCurrentStyle_HTMLStyle6_get_msBlockProgression,
    HTMLCurrentStyle_HTMLStyle6_put_quotes,
    HTMLCurrentStyle_HTMLStyle6_get_quotes
};

static inline HTMLCurrentStyle *impl_from_DispatchEx(DispatchEx *dispex)
{
    return CONTAINING_RECORD(dispex, HTMLCurrentStyle, css_style.dispex);
}

static void *HTMLCurrentStyle_query_interface(DispatchEx *dispex, REFIID riid)
{
    HTMLCurrentStyle *This = impl_from_DispatchEx(dispex);

    if(IsEqualGUID(&IID_IHTMLCurrentStyle, riid))
        return &This->IHTMLCurrentStyle_iface;
    if(IsEqualGUID(&IID_IHTMLCurrentStyle2, riid))
        return &This->IHTMLCurrentStyle2_iface;
    if(IsEqualGUID(&IID_IHTMLCurrentStyle3, riid))
        return &This->IHTMLCurrentStyle3_iface;
    if(IsEqualGUID(&IID_IHTMLCurrentStyle4, riid))
        return &This->IHTMLCurrentStyle4_iface;
    if(IsEqualGUID(&IID_IHTMLStyle, riid))
        return &This->IHTMLStyle_iface;
    if(IsEqualGUID(&IID_IHTMLStyle2, riid))
        return &This->IHTMLStyle2_iface;
    if(IsEqualGUID(&IID_IHTMLStyle3, riid))
        return &This->IHTMLStyle3_iface;
    if(IsEqualGUID(&IID_IHTMLStyle5, riid))
        return &This->IHTMLStyle5_iface;
    if(IsEqualGUID(&IID_IHTMLStyle6, riid))
        return &This->IHTMLStyle6_iface;
    return CSSStyle_query_interface(&This->css_style.dispex, riid);
}

static void HTMLCurrentStyle_traverse(DispatchEx *dispex, nsCycleCollectionTraversalCallback *cb)
{
    HTMLCurrentStyle *This = impl_from_DispatchEx(dispex);
    CSSStyle_traverse(&This->css_style.dispex, cb);

    if(This->elem)
        note_cc_edge((nsISupports*)&This->elem->node.IHTMLDOMNode_iface, "elem", cb);
}

static void HTMLCurrentStyle_unlink(DispatchEx *dispex)
{
    HTMLCurrentStyle *This = impl_from_DispatchEx(dispex);
    CSSStyle_unlink(&This->css_style.dispex);

    if(This->elem) {
        HTMLElement *elem = This->elem;
        This->elem = NULL;
        IHTMLDOMNode_Release(&elem->node.IHTMLDOMNode_iface);
    }
}

static const dispex_static_data_vtbl_t HTMLCurrentStyle_dispex_vtbl = {
    CSSSTYLE_DISPEX_VTBL_ENTRIES,
    .query_interface   = HTMLCurrentStyle_query_interface,
    .traverse          = HTMLCurrentStyle_traverse,
    .unlink            = HTMLCurrentStyle_unlink
};

static const tid_t HTMLCurrentStyle_iface_tids[] = {
    IHTMLCurrentStyle_tid,
    IHTMLCurrentStyle2_tid,
    IHTMLCurrentStyle3_tid,
    IHTMLCurrentStyle4_tid,
    0
};
dispex_static_data_t HTMLCurrentStyle_dispex = {
    "MSCurrentStyleCSSProperties",
    &HTMLCurrentStyle_dispex_vtbl,
    PROTO_ID_HTMLCurrentStyle,
    DispHTMLCurrentStyle_tid,
    HTMLCurrentStyle_iface_tids,
    CSSStyle_init_dispex_info
};

HRESULT HTMLCurrentStyle_Create(HTMLElement *elem, IHTMLCurrentStyle **p)
{
    nsIDOMCSSStyleDeclaration *nsstyle;
    mozIDOMWindowProxy *nsview;
    nsIDOMWindow *nswindow;
    nsAString nsempty_str;
    HTMLCurrentStyle *ret;
    nsresult nsres;

    if(!elem->node.doc->dom_document)  {
        WARN("NULL dom_document\n");
        return E_UNEXPECTED;
    }

    nsres = nsIDOMDocument_GetDefaultView(elem->node.doc->dom_document, &nsview);
    if(NS_FAILED(nsres)) {
        ERR("GetDefaultView failed: %08lx\n", nsres);
        return E_FAIL;
    }

    nsres = mozIDOMWindowProxy_QueryInterface(nsview, &IID_nsIDOMWindow, (void**)&nswindow);
    mozIDOMWindowProxy_Release(nsview);
    assert(nsres == NS_OK);

    nsAString_Init(&nsempty_str, NULL);
    nsres = nsIDOMWindow_GetComputedStyle(nswindow, elem->dom_element, &nsempty_str, &nsstyle);
    nsAString_Finish(&nsempty_str);
    nsIDOMWindow_Release(nswindow);
    if(NS_FAILED(nsres)) {
        ERR("GetComputedStyle failed: %08lx\n", nsres);
        return E_FAIL;
    }

    if(!nsstyle) {
        ERR("GetComputedStyle returned NULL nsstyle\n");
        return E_FAIL;
    }

    ret = calloc(1, sizeof(HTMLCurrentStyle));
    if(!ret) {
        nsIDOMCSSStyleDeclaration_Release(nsstyle);
        return E_OUTOFMEMORY;
    }

    ret->IHTMLCurrentStyle_iface.lpVtbl  = &HTMLCurrentStyleVtbl;
    ret->IHTMLCurrentStyle2_iface.lpVtbl = &HTMLCurrentStyle2Vtbl;
    ret->IHTMLCurrentStyle3_iface.lpVtbl = &HTMLCurrentStyle3Vtbl;
    ret->IHTMLCurrentStyle4_iface.lpVtbl = &HTMLCurrentStyle4Vtbl;
    ret->IHTMLStyle_iface.lpVtbl         = &HTMLStyleVtbl;
    ret->IHTMLStyle2_iface.lpVtbl        = &HTMLStyle2Vtbl;
    ret->IHTMLStyle3_iface.lpVtbl        = &HTMLStyle3Vtbl;
    ret->IHTMLStyle5_iface.lpVtbl        = &HTMLStyle5Vtbl;
    ret->IHTMLStyle6_iface.lpVtbl        = &HTMLStyle6Vtbl;

    init_css_style(&ret->css_style, nsstyle, &HTMLCurrentStyle_dispex, get_inner_window(elem->node.doc),
                   dispex_compat_mode(&elem->node.event_target.dispex));
    nsIDOMCSSStyleDeclaration_Release(nsstyle);

    IHTMLElement_AddRef(&elem->IHTMLElement_iface);
    ret->elem = elem;

    *p = &ret->IHTMLCurrentStyle_iface;
    return S_OK;
}
