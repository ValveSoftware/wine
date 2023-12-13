/*
 * Copyright 2008-2009 Jacek Caban for CodeWeavers
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
#include "mscoree.h"

#include "wine/debug.h"

#include "mshtml_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(mshtml);

#define MAX_ARGS 16

static ExternalCycleCollectionParticipant dispex_ccp;

static CRITICAL_SECTION cs_dispex_static_data;
static CRITICAL_SECTION_DEBUG cs_dispex_static_data_dbg =
{
    0, 0, &cs_dispex_static_data,
    { &cs_dispex_static_data_dbg.ProcessLocksList, &cs_dispex_static_data_dbg.ProcessLocksList },
      0, 0, { (DWORD_PTR)(__FILE__ ": dispex_static_data") }
};
static CRITICAL_SECTION cs_dispex_static_data = { &cs_dispex_static_data_dbg, -1, 0, 0, 0, 0 };

typedef struct {
    IID iid;
    VARIANT default_value;
} func_arg_info_t;

typedef struct {
    DISPID id;
    BSTR name;
    tid_t tid;
    dispex_hook_invoke_t hook;
    SHORT call_vtbl_off;
    SHORT put_vtbl_off;
    SHORT get_vtbl_off;
    SHORT func_disp_idx;
    USHORT argc;
    USHORT default_value_cnt;
    VARTYPE prop_vt;
    VARTYPE *arg_types;
    func_arg_info_t *arg_info;
} func_info_t;

struct dispex_data_t {
    dispex_static_data_t *desc;
    compat_mode_t compat_mode;

    DWORD func_cnt;
    DWORD func_size;
    func_info_t *funcs;
    func_info_t **name_table;
    DWORD func_disp_cnt;

    struct list entry;
};

typedef struct {
    VARIANT var;
    LPWSTR name;
    DWORD flags;
} dynamic_prop_t;

#define DYNPROP_DELETED    0x01
#define DYNPROP_HIDDEN     0x02

typedef struct {
    DispatchEx dispex;
    DispatchEx *obj;
    func_info_t *info;
} func_disp_t;

typedef struct {
    DispatchEx dispex;
    DWORD idx;          /* index into function_props */
} func_prop_disp_t;

typedef struct {
    func_disp_t *func_obj;
    VARIANT val;
} func_obj_entry_t;

struct dispex_dynamic_data_t {
    DWORD buf_size;
    DWORD prop_cnt;
    dynamic_prop_t *props;
    func_obj_entry_t *func_disps;
};

#define DISPID_DYNPROP_0    0x50000000
#define DISPID_DYNPROP_MAX  0x5fffffff

#define FDEX_VERSION_MASK 0xf0000000

static ITypeLib *typelib, *typelib_private;
static ITypeInfo *typeinfos[LAST_tid];
static struct list dispex_data_list = LIST_INIT(dispex_data_list);

static REFIID tid_ids[] = {
#define XIID(iface) &IID_ ## iface,
#define XDIID(iface) &DIID_ ## iface,
TID_LIST
    NULL,
PRIVATE_TID_LIST
#undef XIID
#undef XDIID
};

static HRESULT invoke_builtin_function(IDispatch*,func_info_t*,DISPPARAMS*,VARIANT*,EXCEPINFO*,IServiceProvider*);

static HRESULT load_typelib(void)
{
    WCHAR module_path[MAX_PATH + 3];
    HRESULT hres;
    ITypeLib *tl;
    DWORD len;

    hres = LoadRegTypeLib(&LIBID_MSHTML, 4, 0, LOCALE_SYSTEM_DEFAULT, &tl);
    if(FAILED(hres)) {
        ERR("LoadRegTypeLib failed: %08lx\n", hres);
        return hres;
    }

    if(InterlockedCompareExchangePointer((void**)&typelib, tl, NULL))
        ITypeLib_Release(tl);

    len = GetModuleFileNameW(hInst, module_path, MAX_PATH + 1);
    if (!len || len == MAX_PATH + 1)
    {
        ERR("Could not get module file name, len %lu.\n", len);
        return E_FAIL;
    }
    lstrcatW(module_path, L"\\1");

    hres = LoadTypeLibEx(module_path, REGKIND_NONE, &tl);
    if(FAILED(hres)) {
        ERR("LoadTypeLibEx failed for private typelib: %08lx\n", hres);
        return hres;
    }

    if(InterlockedCompareExchangePointer((void**)&typelib_private, tl, NULL))
        ITypeLib_Release(tl);

    return S_OK;
}

static HRESULT get_typeinfo(tid_t tid, ITypeInfo **typeinfo)
{
    HRESULT hres;

    if (!typelib)
        hres = load_typelib();
    if (!typelib)
        return hres;

    if(!typeinfos[tid]) {
        ITypeInfo *ti;

        hres = ITypeLib_GetTypeInfoOfGuid(tid > LAST_public_tid ? typelib_private : typelib, tid_ids[tid], &ti);
        if(FAILED(hres)) {
            ERR("GetTypeInfoOfGuid(%s) failed: %08lx\n", debugstr_mshtml_guid(tid_ids[tid]), hres);
            return hres;
        }

        if(InterlockedCompareExchangePointer((void**)(typeinfos+tid), ti, NULL))
            ITypeInfo_Release(ti);
    }

    *typeinfo = typeinfos[tid];
    return S_OK;
}

void release_typelib(void)
{
    dispex_data_t *iter;
    unsigned i, j;

    while(!list_empty(&dispex_data_list)) {
        iter = LIST_ENTRY(list_head(&dispex_data_list), dispex_data_t, entry);
        list_remove(&iter->entry);

        for(i = 0; i < iter->func_cnt; i++) {
            if(iter->funcs[i].default_value_cnt && iter->funcs[i].arg_info) {
                for(j = 0; j < iter->funcs[i].argc; j++)
                    VariantClear(&iter->funcs[i].arg_info[j].default_value);
            }
            free(iter->funcs[i].arg_types);
            free(iter->funcs[i].arg_info);
            SysFreeString(iter->funcs[i].name);
        }

        free(iter->funcs);
        free(iter->name_table);
        free(iter);
    }

    if(!typelib)
        return;

    for(i=0; i < ARRAY_SIZE(typeinfos); i++)
        if(typeinfos[i])
            ITypeInfo_Release(typeinfos[i]);

    ITypeLib_Release(typelib);
    ITypeLib_Release(typelib_private);
    DeleteCriticalSection(&cs_dispex_static_data);
}

HRESULT get_class_typeinfo(const CLSID *clsid, ITypeInfo **typeinfo)
{
    HRESULT hres;

    if (!typelib)
        hres = load_typelib();
    if (!typelib)
        return hres;

    hres = ITypeLib_GetTypeInfoOfGuid(typelib, clsid, typeinfo);
    if (FAILED(hres))
        hres = ITypeLib_GetTypeInfoOfGuid(typelib_private, clsid, typeinfo);
    if(FAILED(hres))
        ERR("GetTypeInfoOfGuid failed: %08lx\n", hres);
    return hres;
}

/* Not all argument types are supported yet */
#define BUILTIN_ARG_TYPES_SWITCH                        \
    CASE_VT(VT_I2, INT16, V_I2);                        \
    CASE_VT(VT_UI2, UINT16, V_UI2);                     \
    CASE_VT(VT_I4, INT32, V_I4);                        \
    CASE_VT(VT_UI4, UINT32, V_UI4);                     \
    CASE_VT(VT_R4, float, V_R4);                        \
    CASE_VT(VT_BSTR, BSTR, V_BSTR);                     \
    CASE_VT(VT_DISPATCH, IDispatch*, V_DISPATCH);       \
    CASE_VT(VT_BOOL, VARIANT_BOOL, V_BOOL)

/* List all types used by IDispatchEx-based properties */
#define BUILTIN_TYPES_SWITCH                            \
    BUILTIN_ARG_TYPES_SWITCH;                           \
    CASE_VT(VT_VARIANT, VARIANT, *);                    \
    CASE_VT(VT_PTR, void*, V_BYREF);                    \
    CASE_VT(VT_UNKNOWN, IUnknown*, V_UNKNOWN);          \
    CASE_VT(VT_UI8, ULONGLONG, V_UI8)

static BOOL is_arg_type_supported(VARTYPE vt)
{
    switch(vt) {
#define CASE_VT(x,a,b) case x: return TRUE
    BUILTIN_ARG_TYPES_SWITCH;
#undef CASE_VT
    }
    return FALSE;
}

static void add_func_info(dispex_data_t *data, tid_t tid, const FUNCDESC *desc, ITypeInfo *dti,
                          dispex_hook_invoke_t hook, const WCHAR *name_override)
{
    func_info_t *info;
    BSTR name;
    HRESULT hres;

    if(name_override)
        name = SysAllocString(name_override);
    else if(desc->wFuncFlags & FUNCFLAG_FRESTRICTED)
        return;
    else {
        hres = ITypeInfo_GetDocumentation(dti, desc->memid, &name, NULL, NULL, NULL);
        if(FAILED(hres)) {
            WARN("GetDocumentation failed: %08lx\n", hres);
            return;
        }
    }

    for(info = data->funcs; info < data->funcs+data->func_cnt; info++) {
        if(info->id == desc->memid || !wcscmp(info->name, name)) {
            if(info->tid != tid) {
                SysFreeString(name);
                return; /* Duplicated in other interface */
            }
            break;
        }
    }

    TRACE("adding %s...\n", debugstr_w(name));

    if(info == data->funcs+data->func_cnt) {
        if(data->func_cnt == data->func_size) {
            info = realloc(data->funcs, data->func_size * 2 * sizeof(func_info_t));
            if(!info) {
                SysFreeString(name);
                return;
            }
            memset(info + data->func_size, 0, data->func_size * sizeof(func_info_t));
            data->funcs = info;
            data->func_size *= 2;
        }
        info = data->funcs+data->func_cnt;

        data->func_cnt++;

        info->id = desc->memid;
        info->name = name;
        info->tid = tid;
        info->func_disp_idx = -1;
        info->prop_vt = VT_EMPTY;
        info->hook = hook;
    }else {
        SysFreeString(name);
    }

    if(desc->invkind & DISPATCH_METHOD) {
        unsigned i;

        info->func_disp_idx = data->func_disp_cnt++;
        info->argc = desc->cParams;

        assert(info->argc < MAX_ARGS);
        assert(desc->funckind == FUNC_DISPATCH);

        info->arg_info = calloc(info->argc, sizeof(*info->arg_info));
        if(!info->arg_info)
            return;

        info->prop_vt = desc->elemdescFunc.tdesc.vt;
        if(info->prop_vt != VT_VOID && info->prop_vt != VT_PTR && !is_arg_type_supported(info->prop_vt)) {
            TRACE("%s: return type %d\n", debugstr_w(info->name), info->prop_vt);
            return; /* Fallback to ITypeInfo::Invoke */
        }

        info->arg_types = malloc(sizeof(*info->arg_types) * (info->argc + (info->prop_vt == VT_VOID ? 0 : 1)));
        if(!info->arg_types)
            return;

        for(i=0; i < info->argc; i++)
            info->arg_types[i] = desc->lprgelemdescParam[i].tdesc.vt;

        if(info->prop_vt == VT_PTR)
            info->arg_types[info->argc] = VT_BYREF | VT_DISPATCH;
        else if(info->prop_vt != VT_VOID)
            info->arg_types[info->argc] = VT_BYREF | info->prop_vt;

        if(desc->cParamsOpt) {
            TRACE("%s: optional params\n", debugstr_w(info->name));
            return; /* Fallback to ITypeInfo::Invoke */
        }

        for(i=0; i < info->argc; i++) {
            TYPEDESC *tdesc = &desc->lprgelemdescParam[i].tdesc;
            if(tdesc->vt == VT_PTR && tdesc->lptdesc->vt == VT_USERDEFINED) {
                ITypeInfo *ref_type_info;
                TYPEATTR *attr;

                hres = ITypeInfo_GetRefTypeInfo(dti, tdesc->lptdesc->hreftype, &ref_type_info);
                if(FAILED(hres)) {
                    ERR("Could not get referenced type info: %08lx\n", hres);
                    return;
                }

                hres = ITypeInfo_GetTypeAttr(ref_type_info, &attr);
                if(SUCCEEDED(hres)) {
                    assert(attr->typekind == TKIND_DISPATCH);
                    info->arg_info[i].iid = attr->guid;
                    ITypeInfo_ReleaseTypeAttr(ref_type_info, attr);
                }else {
                    ERR("GetTypeAttr failed: %08lx\n", hres);
                }
                ITypeInfo_Release(ref_type_info);
                if(FAILED(hres))
                    return;
                info->arg_types[i] = VT_DISPATCH;
            }else if(!is_arg_type_supported(info->arg_types[i])) {
                TRACE("%s: unsupported arg type %s\n", debugstr_w(info->name), debugstr_vt(info->arg_types[i]));
                return; /* Fallback to ITypeInfo for unsupported arg types */
            }

            if(desc->lprgelemdescParam[i].paramdesc.wParamFlags & PARAMFLAG_FHASDEFAULT) {
                hres = VariantCopy(&info->arg_info[i].default_value,
                                   &desc->lprgelemdescParam[i].paramdesc.pparamdescex->varDefaultValue);
                if(FAILED(hres)) {
                    ERR("Could not copy default value: %08lx\n", hres);
                    return;
                }
                TRACE("%s param %d: default value %s\n", debugstr_w(info->name),
                      i, debugstr_variant(&info->arg_info[i].default_value));
                info->default_value_cnt++;
            }
        }

        assert(info->argc <= MAX_ARGS);
        assert(desc->callconv == CC_STDCALL);

        info->call_vtbl_off = desc->oVft/sizeof(void*);
    }else if(desc->invkind & (DISPATCH_PROPERTYPUT|DISPATCH_PROPERTYGET)) {
        VARTYPE vt = VT_EMPTY;

        if(desc->wFuncFlags & FUNCFLAG_FHIDDEN)
            info->func_disp_idx = -2;

        if(desc->invkind & DISPATCH_PROPERTYGET) {
            vt = desc->elemdescFunc.tdesc.vt;
            info->get_vtbl_off = desc->oVft/sizeof(void*);
        }
        if(desc->invkind & DISPATCH_PROPERTYPUT) {
            assert(desc->cParams == 1);
            vt = desc->lprgelemdescParam->tdesc.vt;
            info->put_vtbl_off = desc->oVft/sizeof(void*);
        }

        assert(info->prop_vt == VT_EMPTY || vt == info->prop_vt);
        info->prop_vt = vt;
    }
}

static HRESULT process_interface(dispex_data_t *data, tid_t tid, ITypeInfo *disp_typeinfo, const dispex_hook_t *hooks)
{
    unsigned i = 7; /* skip IDispatch functions */
    ITypeInfo *typeinfo;
    FUNCDESC *funcdesc;
    HRESULT hres;

    hres = get_typeinfo(tid, &typeinfo);
    if(FAILED(hres))
        return hres;

    while(1) {
        const dispex_hook_t *hook = NULL;

        hres = ITypeInfo_GetFuncDesc(typeinfo, i++, &funcdesc);
        if(FAILED(hres))
            break;

        if(hooks) {
            for(hook = hooks; hook->dispid != DISPID_UNKNOWN; hook++) {
                if(hook->dispid == funcdesc->memid)
                    break;
            }
            if(hook->dispid == DISPID_UNKNOWN)
                hook = NULL;
        }

        if(!hook || hook->invoke || hook->name) {
            add_func_info(data, tid, funcdesc, disp_typeinfo ? disp_typeinfo : typeinfo,
                          hook ? hook->invoke : NULL, hook ? hook->name : NULL);
        }

        ITypeInfo_ReleaseFuncDesc(typeinfo, funcdesc);
    }

    return S_OK;
}

void dispex_info_add_interface(dispex_data_t *info, tid_t tid, const dispex_hook_t *hooks)
{
    HRESULT hres;

    hres = process_interface(info, tid, NULL, hooks);
    if(FAILED(hres))
        ERR("process_interface failed: %08lx\n", hres);
}

static int __cdecl dispid_cmp(const void *p1, const void *p2)
{
    return ((const func_info_t*)p1)->id - ((const func_info_t*)p2)->id;
}

static int __cdecl func_name_cmp(const void *p1, const void *p2)
{
    return wcsicmp((*(func_info_t* const*)p1)->name, (*(func_info_t* const*)p2)->name);
}

static dispex_data_t *preprocess_dispex_data(dispex_static_data_t *desc, compat_mode_t compat_mode)
{
    const tid_t *tid;
    dispex_data_t *data;
    DWORD i;
    ITypeInfo *dti;
    HRESULT hres;

    if(desc->disp_tid) {
        hres = get_typeinfo(desc->disp_tid, &dti);
        if(FAILED(hres)) {
            ERR("Could not get disp type info: %08lx\n", hres);
            return NULL;
        }
    }

    data = malloc(sizeof(dispex_data_t));
    if (!data) {
        ERR("Out of memory\n");
        return NULL;
    }
    data->desc = desc;
    data->compat_mode = compat_mode;
    data->func_cnt = 0;
    data->func_disp_cnt = 0;
    data->func_size = 16;
    data->funcs = calloc(data->func_size, sizeof(func_info_t));
    if (!data->funcs) {
        free(data);
        ERR("Out of memory\n");
        return NULL;
    }
    list_add_tail(&dispex_data_list, &data->entry);

    if(desc->init_info)
        desc->init_info(data, compat_mode);

    for(tid = desc->iface_tids; *tid; tid++) {
        hres = process_interface(data, *tid, dti, NULL);
        if(FAILED(hres))
            break;
    }

    if(!data->func_cnt) {
        free(data->funcs);
        data->name_table = NULL;
        data->funcs = NULL;
        data->func_size = 0;
        return data;
    }


    data->funcs = realloc(data->funcs, data->func_cnt * sizeof(func_info_t));
    qsort(data->funcs, data->func_cnt, sizeof(func_info_t), dispid_cmp);

    data->name_table = malloc(data->func_cnt * sizeof(func_info_t*));
    for(i=0; i < data->func_cnt; i++)
        data->name_table[i] = data->funcs+i;
    qsort(data->name_table, data->func_cnt, sizeof(func_info_t*), func_name_cmp);
    return data;
}

static int __cdecl id_cmp(const void *p1, const void *p2)
{
    return *(const DISPID*)p1 - *(const DISPID*)p2;
}

HRESULT get_dispids(tid_t tid, DWORD *ret_size, DISPID **ret)
{
    unsigned i, func_cnt;
    FUNCDESC *funcdesc;
    ITypeInfo *ti;
    TYPEATTR *attr;
    DISPID *ids;
    HRESULT hres;

    hres = get_typeinfo(tid, &ti);
    if(FAILED(hres))
        return hres;

    hres = ITypeInfo_GetTypeAttr(ti, &attr);
    if(FAILED(hres)) {
        ITypeInfo_Release(ti);
        return hres;
    }

    func_cnt = attr->cFuncs;
    ITypeInfo_ReleaseTypeAttr(ti, attr);

    ids = malloc(func_cnt * sizeof(DISPID));
    if(!ids) {
        ITypeInfo_Release(ti);
        return E_OUTOFMEMORY;
    }

    for(i=0; i < func_cnt; i++) {
        hres = ITypeInfo_GetFuncDesc(ti, i, &funcdesc);
        if(FAILED(hres))
            break;

        ids[i] = funcdesc->memid;
        ITypeInfo_ReleaseFuncDesc(ti, funcdesc);
    }

    ITypeInfo_Release(ti);
    if(FAILED(hres)) {
        free(ids);
        return hres;
    }

    qsort(ids, func_cnt, sizeof(DISPID), id_cmp);

    *ret_size = func_cnt;
    *ret = ids;
    return S_OK;
}

static inline BOOL is_custom_dispid(DISPID id)
{
    return MSHTML_DISPID_CUSTOM_MIN <= id && id <= MSHTML_DISPID_CUSTOM_MAX;
}

static inline BOOL is_dynamic_dispid(DISPID id)
{
    return DISPID_DYNPROP_0 <= id && id <= DISPID_DYNPROP_MAX;
}

dispex_prop_type_t get_dispid_type(DISPID id)
{
    if(is_dynamic_dispid(id))
        return DISPEXPROP_DYNAMIC;
    if(is_custom_dispid(id))
        return DISPEXPROP_CUSTOM;
    return DISPEXPROP_BUILTIN;
}

static HRESULT variant_copy(VARIANT *dest, VARIANT *src)
{
    if(V_VT(src) == VT_BSTR && !V_BSTR(src)) {
        V_VT(dest) = VT_BSTR;
        V_BSTR(dest) = NULL;
        return S_OK;
    }

    return VariantCopy(dest, src);
}

static inline dispex_dynamic_data_t *get_dynamic_data(DispatchEx *This)
{
    if(This->dynamic_data)
        return This->dynamic_data;

    This->dynamic_data = calloc(1, sizeof(dispex_dynamic_data_t));
    if(!This->dynamic_data)
        return NULL;

    if(This->info->desc->vtbl->populate_props)
        This->info->desc->vtbl->populate_props(This);

    return This->dynamic_data;
}

static HRESULT get_dynamic_prop(DispatchEx *This, const WCHAR *name, DWORD flags, dynamic_prop_t **ret)
{
    const BOOL alloc = flags & fdexNameEnsure;
    dispex_dynamic_data_t *data;
    dynamic_prop_t *prop;

    data = get_dynamic_data(This);
    if(!data)
        return E_OUTOFMEMORY;

    for(prop = data->props; prop < data->props+data->prop_cnt; prop++) {
        if(flags & fdexNameCaseInsensitive ? !wcsicmp(prop->name, name) : !wcscmp(prop->name, name)) {
            if(prop->flags & DYNPROP_DELETED) {
                if(!alloc)
                    return DISP_E_UNKNOWNNAME;
                prop->flags &= ~DYNPROP_DELETED;
            }
            *ret = prop;
            return S_OK;
        }
    }

    if(!alloc)
        return DISP_E_UNKNOWNNAME;

    TRACE("creating dynamic prop %s\n", debugstr_w(name));

    if(!data->buf_size) {
        data->props = malloc(sizeof(dynamic_prop_t) * 4);
        if(!data->props)
            return E_OUTOFMEMORY;
        data->buf_size = 4;
    }else if(data->buf_size == data->prop_cnt) {
        dynamic_prop_t *new_props;

        new_props = realloc(data->props, sizeof(dynamic_prop_t) * (data->buf_size << 1));
        if(!new_props)
            return E_OUTOFMEMORY;

        data->props = new_props;
        data->buf_size <<= 1;
    }

    prop = data->props + data->prop_cnt;

    prop->name = wcsdup(name);
    if(!prop->name)
        return E_OUTOFMEMORY;

    VariantInit(&prop->var);
    prop->flags = 0;
    data->prop_cnt++;
    *ret = prop;
    return S_OK;
}

HRESULT dispex_get_dprop_ref(DispatchEx *This, const WCHAR *name, BOOL alloc, VARIANT **ret)
{
    dynamic_prop_t *prop;
    HRESULT hres;

    hres = get_dynamic_prop(This, name, alloc ? fdexNameEnsure : 0, &prop);
    if(FAILED(hres))
        return hres;

    if(alloc)
        prop->flags |= DYNPROP_HIDDEN;
    *ret = &prop->var;
    return S_OK;
}

HRESULT dispex_get_dynid(DispatchEx *This, const WCHAR *name, BOOL hidden, DISPID *id)
{
    dynamic_prop_t *prop;
    HRESULT hres;

    hres = get_dynamic_prop(This, name, fdexNameEnsure, &prop);
    if(FAILED(hres))
        return hres;

    if(hidden)
        prop->flags |= DYNPROP_HIDDEN;
    *id = DISPID_DYNPROP_0 + (prop - This->dynamic_data->props);
    return S_OK;
}

static HRESULT dispex_value(DispatchEx *This, LCID lcid, WORD flags, DISPPARAMS *params,
        VARIANT *res, EXCEPINFO *ei, IServiceProvider *caller)
{
    HRESULT hres;

    if(This->info->desc->vtbl->value)
        return This->info->desc->vtbl->value(This, lcid, flags, params, res, ei, caller);

    switch(flags) {
    case DISPATCH_PROPERTYGET:
        V_VT(res) = VT_BSTR;
        hres = dispex_to_string(This, &V_BSTR(res));
        if(FAILED(hres))
            return hres;
        break;
    default:
        FIXME("Unimplemented flags %x\n", flags);
        return E_NOTIMPL;
    }

    return S_OK;
}

static HRESULT typeinfo_invoke(IUnknown *iface, func_info_t *func, WORD flags, DISPPARAMS *dp, VARIANT *res,
        EXCEPINFO *ei)
{
    DISPPARAMS params = {dp->rgvarg, NULL, dp->cArgs, 0};
    ITypeInfo *ti;
    UINT argerr=0;
    HRESULT hres;

    if(params.cArgs > func->argc) {
        params.rgvarg += params.cArgs - func->argc;
        params.cArgs = func->argc;
    }

    hres = get_typeinfo(func->tid, &ti);
    if(FAILED(hres)) {
        ERR("Could not get type info: %08lx\n", hres);
        return hres;
    }

    return ITypeInfo_Invoke(ti, iface, func->id, flags, &params, res, ei, &argerr);
}

static HRESULT get_disp_prop(IDispatch *disp, IDispatchEx *dispex, const WCHAR *name, LCID lcid, VARIANT *res,
        EXCEPINFO *ei, IServiceProvider *caller)
{
    DISPPARAMS dp = { 0 };
    DISPID dispid;
    HRESULT hres;
    UINT err = 0;
    BSTR bstr;

    if(!(bstr = SysAllocString(name)))
        return E_OUTOFMEMORY;

    if(dispex)
        hres = IDispatchEx_GetDispID(dispex, bstr, fdexNameCaseSensitive, &dispid);
    else
        hres = IDispatch_GetIDsOfNames(disp, &IID_NULL, &bstr, 1, 0, &dispid);
    SysFreeString(bstr);
    if(FAILED(hres))
        return hres;
    if(dispid == DISPID_UNKNOWN)
        return DISP_E_UNKNOWNNAME;

    if(dispex)
        hres = IDispatchEx_InvokeEx(dispex, dispid, lcid, DISPATCH_PROPERTYGET, &dp, res, ei, caller);
    else
        hres = IDispatch_Invoke(disp, dispid, &IID_NULL, lcid, DISPATCH_PROPERTYGET, &dp, res, ei, &err);
    return hres;
}

static HRESULT get_disp_prop_vt(IDispatch *disp, IDispatchEx *dispex, const WCHAR *name, LCID lcid, VARIANT *res,
        VARTYPE vt, EXCEPINFO *ei, IServiceProvider *caller)
{
    HRESULT hres;

    hres = get_disp_prop(disp, dispex, name, lcid, res, ei, caller);
    if(FAILED(hres))
        return hres;
    if(V_VT(res) != vt) {
        VARIANT tmp = *res;
        hres = change_type(res, &tmp, vt, caller);
        VariantClear(&tmp);
    }
    return hres;
}

static HRESULT format_func_disp_string(const WCHAR *name, IServiceProvider *caller, VARIANT *res)
{
    unsigned name_len;
    WCHAR *ptr;
    BSTR str;

    static const WCHAR func_prefixW[] =
        {'\n','f','u','n','c','t','i','o','n',' '};
    static const WCHAR func_suffixW[] =
        {'(',')',' ','{','\n',' ',' ',' ',' ','[','n','a','t','i','v','e',' ','c','o','d','e',']','\n','}','\n'};

    /* FIXME: This probably should be more generic. Also we should try to get IID_IActiveScriptSite and SID_GetCaller. */
    if(!caller)
        return E_ACCESSDENIED;

    name_len = wcslen(name);
    ptr = str = SysAllocStringLen(NULL, name_len + ARRAY_SIZE(func_prefixW) + ARRAY_SIZE(func_suffixW));
    if(!str)
        return E_OUTOFMEMORY;

    memcpy(ptr, func_prefixW, sizeof(func_prefixW));
    ptr += ARRAY_SIZE(func_prefixW);

    memcpy(ptr, name, name_len * sizeof(WCHAR));
    ptr += name_len;

    memcpy(ptr, func_suffixW, sizeof(func_suffixW));

    V_VT(res) = VT_BSTR;
    V_BSTR(res) = str;
    return S_OK;
}

static HRESULT function_apply(func_disp_t *func, DISPPARAMS *dp, LCID lcid, VARIANT *res, EXCEPINFO *ei, IServiceProvider *caller)
{
    IDispatchEx *dispex = NULL;
    DISPPARAMS params = { 0 };
    IDispatch *this_obj;
    UINT argc = 0;
    VARIANT *arg;
    HRESULT hres;

    arg = dp->rgvarg + dp->cArgs - 1;
    if(dp->cArgs < 1 || V_VT(arg) != VT_DISPATCH || !V_DISPATCH(arg))
        return CTL_E_ILLEGALFUNCTIONCALL;
    this_obj = V_DISPATCH(arg);

    if(dp->cArgs >= 2) {
        IDispatch *disp;
        UINT i;

        arg--;
        if((V_VT(arg) & ~VT_BYREF) != VT_DISPATCH)
            return CTL_E_ILLEGALFUNCTIONCALL;
        disp = (V_VT(arg) & VT_BYREF) ? *(IDispatch**)(V_BYREF(arg)) : V_DISPATCH(arg);

        /* FIXME: Native doesn't seem to detect jscript arrays by querying for length or indexed props,
           and it doesn't QI nor call any other IDispatchEx methods (except AddRef) on external disps.
           Array-like JS objects don't work either, they all return 0x800a01ae. So how does it do it?! */
        IDispatch_QueryInterface(disp, &IID_IDispatchEx, (void**)&dispex);
        hres = get_disp_prop_vt(disp, dispex, L"length", lcid, res, VT_I4, ei, caller);
        if(FAILED(hres)) {
            if(hres == DISP_E_UNKNOWNNAME)
                hres = CTL_E_ILLEGALFUNCTIONCALL;
            goto fail;
        }
        if(V_I4(res) < 0) {
            hres = CTL_E_ILLEGALFUNCTIONCALL;
            goto fail;
        }
        params.cArgs = V_I4(res);

        /* alloc new params */
        if(params.cArgs) {
            if(!(params.rgvarg = malloc(params.cArgs * sizeof(VARIANTARG)))) {
                hres = E_OUTOFMEMORY;
                goto fail;
            }
            for(i = 0; i < params.cArgs; i++) {
                WCHAR buf[12];

                arg = params.rgvarg + params.cArgs - i - 1;
                swprintf(buf, ARRAY_SIZE(buf), L"%u", i);
                hres = get_disp_prop(disp, dispex, buf, lcid, arg, ei, caller);
                if(FAILED(hres)) {
                    if(hres == DISP_E_UNKNOWNNAME) {
                        V_VT(arg) = VT_EMPTY;
                        continue;
                    }
                    break;
                }
            }
            argc = i;
            if(argc < params.cArgs)
                goto cleanup;
        }
    }

    hres = invoke_builtin_function(this_obj, func->info, &params, res, ei, caller);

cleanup:
    while(argc--)
        VariantClear(&params.rgvarg[params.cArgs - argc - 1]);
    free(params.rgvarg);
fail:
    if(dispex)
        IDispatchEx_Release(dispex);
    return (hres == E_UNEXPECTED) ? CTL_E_ILLEGALFUNCTIONCALL : hres;
}

static HRESULT function_call(func_disp_t *func, DISPPARAMS *dp, LCID lcid, VARIANT *res, EXCEPINFO *ei, IServiceProvider *caller)
{
    DISPPARAMS params = { dp->rgvarg, NULL, dp->cArgs - 1, 0 };
    VARIANT *arg;
    HRESULT hres;

    arg = dp->rgvarg + dp->cArgs - 1;
    if(dp->cArgs < 1 || V_VT(arg) != VT_DISPATCH || !V_DISPATCH(arg))
        return CTL_E_ILLEGALFUNCTIONCALL;

    hres = invoke_builtin_function(V_DISPATCH(arg), func->info, &params, res, ei, caller);

    return (hres == E_UNEXPECTED) ? CTL_E_ILLEGALFUNCTIONCALL : hres;
}

static const struct {
    const WCHAR *name;
    HRESULT (*invoke)(func_disp_t*,DISPPARAMS*,LCID,VARIANT*,EXCEPINFO*,IServiceProvider*);
} function_props[] = {
    { L"apply", function_apply },
    { L"call",  function_call }
};

static inline func_prop_disp_t *func_prop_disp_from_DispatchEx(DispatchEx *iface)
{
    return CONTAINING_RECORD(iface, func_prop_disp_t, dispex);
}

static void function_prop_destructor(DispatchEx *dispex)
{
    func_prop_disp_t *This = func_prop_disp_from_DispatchEx(dispex);
    free(This);
}

static HRESULT function_prop_value(DispatchEx *dispex, LCID lcid, WORD flags, DISPPARAMS *params,
        VARIANT *res, EXCEPINFO *ei, IServiceProvider *caller)
{
    func_prop_disp_t *This = func_prop_disp_from_DispatchEx(dispex);
    HRESULT hres;

    switch(flags) {
    case DISPATCH_CONSTRUCT:
        return MSHTML_E_INVALID_PROPERTY;
    case DISPATCH_METHOD|DISPATCH_PROPERTYGET:
        if(!res)
            return E_INVALIDARG;
        /* fall through */
    case DISPATCH_METHOD:
        return MSHTML_E_INVALID_PROPERTY;
    case DISPATCH_PROPERTYGET:
        hres = format_func_disp_string(function_props[This->idx].name, caller, res);
        break;
    default:
        FIXME("Unimplemented flags %x\n", flags);
        hres = E_NOTIMPL;
    }

    return hres;
}

static const dispex_static_data_vtbl_t function_prop_dispex_vtbl = {
    .destructor       = function_prop_destructor,
    .value            = function_prop_value
};

static const tid_t function_prop_iface_tids[] = { 0 };

static dispex_static_data_t function_prop_dispex = {
    "Function",
    &function_prop_dispex_vtbl,
    NULL_tid,
    function_prop_iface_tids
};

static inline func_disp_t *impl_from_DispatchEx(DispatchEx *iface)
{
    return CONTAINING_RECORD(iface, func_disp_t, dispex);
}

static void function_destructor(DispatchEx *dispex)
{
    func_disp_t *This = impl_from_DispatchEx(dispex);
    assert(!This->obj);
    free(This);
}

static HRESULT function_value(DispatchEx *dispex, LCID lcid, WORD flags, DISPPARAMS *params,
        VARIANT *res, EXCEPINFO *ei, IServiceProvider *caller)
{
    func_disp_t *This = impl_from_DispatchEx(dispex);
    HRESULT hres;

    switch(flags) {
    case DISPATCH_CONSTRUCT:
        return MSHTML_E_INVALID_PROPERTY;
    case DISPATCH_METHOD|DISPATCH_PROPERTYGET:
        if(!res)
            return E_INVALIDARG;
        /* fall through */
    case DISPATCH_METHOD:
        if(!This->obj)
            return E_UNEXPECTED;
        hres = invoke_builtin_function((IDispatch*)&This->obj->IDispatchEx_iface, This->info, params, res, ei, caller);
        break;
    case DISPATCH_PROPERTYGET:
        hres = format_func_disp_string(This->info->name, caller, res);
        break;
    default:
        FIXME("Unimplemented flags %x\n", flags);
        hres = E_NOTIMPL;
    }

    return hres;
}

static HRESULT function_get_dispid(DispatchEx *dispex, BSTR name, DWORD flags, DISPID *dispid)
{
    DWORD i;

    for(i = 0; i < ARRAY_SIZE(function_props); i++) {
        if((flags & fdexNameCaseInsensitive) ? wcsicmp(name, function_props[i].name) : wcscmp(name, function_props[i].name))
            continue;
        *dispid = MSHTML_DISPID_CUSTOM_MIN + i;
        return S_OK;
    }
    return DISP_E_UNKNOWNNAME;
}

static HRESULT function_get_name(DispatchEx *dispex, DISPID id, BSTR *name)
{
    DWORD idx = id - MSHTML_DISPID_CUSTOM_MIN;

    if(idx >= ARRAY_SIZE(function_props))
        return DISP_E_MEMBERNOTFOUND;

    return (*name = SysAllocString(function_props[idx].name)) ? S_OK : E_OUTOFMEMORY;
}

static HRESULT function_invoke(DispatchEx *dispex, IDispatch *this_obj, DISPID id, LCID lcid, WORD flags,
        DISPPARAMS *params, VARIANT *res, EXCEPINFO *ei, IServiceProvider *caller)
{
    func_disp_t *This = impl_from_DispatchEx(dispex);
    DWORD idx = id - MSHTML_DISPID_CUSTOM_MIN;

    if(idx >= ARRAY_SIZE(function_props))
        return DISP_E_MEMBERNOTFOUND;

    switch(flags) {
    case DISPATCH_METHOD|DISPATCH_PROPERTYGET:
        if(!res)
            return E_INVALIDARG;
        /* fall through */
    case DISPATCH_METHOD:
        return function_props[idx].invoke(This, params, lcid, res, ei, caller);
    case DISPATCH_PROPERTYGET: {
        func_prop_disp_t *disp = calloc(1, sizeof(func_prop_disp_t));

        if(!disp)
            return E_OUTOFMEMORY;
        disp->idx = idx;
        init_dispatch(&disp->dispex, &function_prop_dispex, NULL, dispex_compat_mode(&This->dispex));

        V_VT(res) = VT_DISPATCH;
        V_DISPATCH(res) = (IDispatch*)&disp->dispex.IDispatchEx_iface;
        break;
    }
    default:
        return MSHTML_E_INVALID_PROPERTY;
    }

    return S_OK;
}

static const dispex_static_data_vtbl_t function_dispex_vtbl = {
    .destructor       = function_destructor,
    .value            = function_value,
    .get_dispid       = function_get_dispid,
    .get_name         = function_get_name,
    .invoke           = function_invoke
};

static const tid_t function_iface_tids[] = {0};

static dispex_static_data_t function_dispex = {
    "Function",
    &function_dispex_vtbl,
    NULL_tid,
    function_iface_tids
};

static func_disp_t *create_func_disp(DispatchEx *obj, func_info_t *info)
{
    func_disp_t *ret;

    ret = calloc(1, sizeof(func_disp_t));
    if(!ret)
        return NULL;

    init_dispatch(&ret->dispex, &function_dispex, NULL, dispex_compat_mode(obj));
    ret->obj = obj;
    ret->info = info;

    return ret;
}

static HRESULT invoke_disp_value(IDispatch *this_obj, IDispatch *func_disp, LCID lcid, WORD flags, DISPPARAMS *dp,
        VARIANT *res, EXCEPINFO *ei, IServiceProvider *caller)
{
    DISPID named_arg = DISPID_THIS;
    DISPPARAMS new_dp = {NULL, &named_arg, 0, 1};
    IDispatchEx *dispex;
    HRESULT hres;

    if(dp->cNamedArgs) {
        FIXME("named args not supported\n");
        return E_NOTIMPL;
    }

    new_dp.rgvarg = malloc((dp->cArgs + 1) * sizeof(VARIANTARG));
    if(!new_dp.rgvarg)
        return E_OUTOFMEMORY;

    new_dp.cArgs = dp->cArgs+1;
    memcpy(new_dp.rgvarg+1, dp->rgvarg, dp->cArgs*sizeof(VARIANTARG));

    V_VT(new_dp.rgvarg) = VT_DISPATCH;
    V_DISPATCH(new_dp.rgvarg) = this_obj;

    hres = IDispatch_QueryInterface(func_disp, &IID_IDispatchEx, (void**)&dispex);
    TRACE(">>>\n");
    if(SUCCEEDED(hres)) {
        hres = IDispatchEx_InvokeEx(dispex, DISPID_VALUE, lcid, flags, &new_dp, res, ei, caller);
        IDispatchEx_Release(dispex);
    }else {
        UINT err = 0;
        hres = IDispatch_Invoke(func_disp, DISPID_VALUE, &IID_NULL, lcid, flags, &new_dp, res, ei, &err);
    }
    if(SUCCEEDED(hres))
        TRACE("<<< %s\n", debugstr_variant(res));
    else
        WARN("<<< %08lx\n", hres);

    free(new_dp.rgvarg);
    return hres;
}

static HRESULT get_func_obj_entry(DispatchEx *This, func_info_t *func, func_obj_entry_t **ret)
{
    dispex_dynamic_data_t *dynamic_data;
    func_obj_entry_t *entry;

    dynamic_data = get_dynamic_data(This);
    if(!dynamic_data)
        return E_OUTOFMEMORY;

    if(!dynamic_data->func_disps) {
        dynamic_data->func_disps = calloc(This->info->func_disp_cnt, sizeof(*dynamic_data->func_disps));
        if(!dynamic_data->func_disps)
            return E_OUTOFMEMORY;
    }

    entry = dynamic_data->func_disps + func->func_disp_idx;
    if(!entry->func_obj) {
        entry->func_obj = create_func_disp(This, func);
        if(!entry->func_obj)
            return E_OUTOFMEMORY;

        IDispatchEx_AddRef(&entry->func_obj->dispex.IDispatchEx_iface);
        V_VT(&entry->val) = VT_DISPATCH;
        V_DISPATCH(&entry->val) = (IDispatch*)&entry->func_obj->dispex.IDispatchEx_iface;
    }

    *ret = entry;
    return S_OK;
}

static HRESULT get_builtin_func(dispex_data_t *data, DISPID id, func_info_t **ret)
{
    int min, max, n;

    min = 0;
    max = data->func_cnt-1;

    while(min <= max) {
        n = (min+max)/2;

        if(data->funcs[n].id == id) {
            *ret = data->funcs+n;
            return S_OK;
        }

        if(data->funcs[n].id < id)
            min = n+1;
        else
            max = n-1;
    }

    WARN("invalid id %lx\n", id);
    return DISP_E_MEMBERNOTFOUND;
}

HRESULT dispex_get_builtin_id(DispatchEx *This, BSTR name, DWORD grfdex, DISPID *ret)
{
    int min, max, n, c;

    min = 0;
    max = This->info->func_cnt-1;

    while(min <= max) {
        n = (min+max)/2;

        c = wcsicmp(This->info->name_table[n]->name, name);
        if(!c) {
            if((grfdex & fdexNameCaseSensitive) && wcscmp(This->info->name_table[n]->name, name))
                break;

            *ret = This->info->name_table[n]->id;
            return S_OK;
        }

        if(c > 0)
            max = n-1;
        else
            min = n+1;
    }

    if(This->info->desc->vtbl->get_dispid) {
        HRESULT hres;

        hres = This->info->desc->vtbl->get_dispid(This, name, grfdex, ret);
        if(hres != DISP_E_UNKNOWNNAME)
            return hres;
    }

    return DISP_E_UNKNOWNNAME;
}

static inline DispatchEx *get_dispex_for_hook(IUnknown *iface)
{
    IWineDispatchProxyPrivate *itf;
    DispatchEx *dispex;

    if(FAILED(IUnknown_QueryInterface(iface, &IID_IWineDispatchProxyPrivate, (void**)&itf)) || !itf)
        return NULL;
    dispex = CONTAINING_RECORD(itf->lpVtbl->GetProxyFieldRef(itf), DispatchEx, proxy);

    /* The dispex and the proxy interface requested might be different (e.g. inner vs outer windows) */
    IDispatchEx_AddRef(&dispex->IDispatchEx_iface);
    IDispatchEx_Release((IDispatchEx*)itf);

    return dispex;
}

HRESULT change_type(VARIANT *dst, VARIANT *src, VARTYPE vt, IServiceProvider *caller)
{
    V_VT(dst) = VT_EMPTY;

    if(caller) {
        IVariantChangeType *change_type = NULL;
        HRESULT hres;

        hres = IServiceProvider_QueryService(caller, &SID_VariantConversion, &IID_IVariantChangeType, (void**)&change_type);
        if(SUCCEEDED(hres)) {
            hres = IVariantChangeType_ChangeType(change_type, dst, src, LOCALE_NEUTRAL, vt);
            IVariantChangeType_Release(change_type);
            if(SUCCEEDED(hres))
                return S_OK;
        }
    }

    switch(vt) {
    case VT_BOOL:
        if(V_VT(src) == VT_BSTR) {
            V_VT(dst) = VT_BOOL;
            V_BOOL(dst) = variant_bool(V_BSTR(src) && *V_BSTR(src));
            return S_OK;
        }
        break;
    case VT_UNKNOWN:
    case VT_DISPATCH:
        if(V_VT(src) == VT_EMPTY || V_VT(src) == VT_NULL) {
            V_VT(dst) = vt;
            V_DISPATCH(dst) = NULL;
            return S_OK;
        }
        break;
    }

    return VariantChangeType(dst, src, 0, vt);
}

static HRESULT builtin_propget(IUnknown *iface, func_info_t *func, DISPPARAMS *dp, VARIANT *res)
{
    HRESULT hres;

    if(dp && dp->cArgs) {
        FIXME("cArgs %d\n", dp->cArgs);
        return E_NOTIMPL;
    }

    assert(func->get_vtbl_off);

    switch(func->prop_vt) {
#define CASE_VT(vt,type,access) \
    case vt: { \
        type val; \
        hres = ((HRESULT (WINAPI*)(IUnknown*,type*))((void**)iface->lpVtbl)[func->get_vtbl_off])(iface,&val); \
        if(SUCCEEDED(hres)) \
            access(res) = val; \
        } \
        break
    BUILTIN_TYPES_SWITCH;
#undef CASE_VT
    default:
        FIXME("Unhandled vt %d\n", func->prop_vt);
        hres = E_NOTIMPL;
    }

    if(FAILED(hres))
        return hres;

    if(func->prop_vt != VT_VARIANT)
        V_VT(res) = func->prop_vt == VT_PTR ? VT_DISPATCH : func->prop_vt;
    return S_OK;
}

static HRESULT builtin_propput(DispatchEx *This, IUnknown *iface, func_info_t *func, DISPPARAMS *dp, IServiceProvider *caller)
{
    VARIANT *v, tmpv;
    HRESULT hres;

    if(dp->cArgs != 1 || (dp->cNamedArgs == 1 && *dp->rgdispidNamedArgs != DISPID_PROPERTYPUT)
            || dp->cNamedArgs > 1) {
        FIXME("invalid args\n");
        return E_INVALIDARG;
    }

    if(!func->put_vtbl_off) {
        if(dispex_compat_mode(This) >= COMPAT_MODE_IE9) {
            WARN("No setter\n");
            return S_OK;
        }
        FIXME("No setter\n");
        return E_FAIL;
    }

    v = dp->rgvarg;
    if(func->prop_vt != VT_VARIANT && V_VT(v) != func->prop_vt) {
        hres = change_type(&tmpv, v, func->prop_vt, caller);
        if(FAILED(hres))
            return hres;
        v = &tmpv;
    }

    switch(func->prop_vt) {
#define CASE_VT(vt,type,access) \
    case vt: \
        hres = ((HRESULT (WINAPI*)(IUnknown*,type))((void**)iface->lpVtbl)[func->put_vtbl_off])(iface,access(v)); \
        break
    BUILTIN_TYPES_SWITCH;
#undef CASE_VT
    default:
        FIXME("Unimplemented vt %d\n", func->prop_vt);
        hres = E_NOTIMPL;
    }

    if(v == &tmpv)
        VariantClear(v);
    return hres;
}

static HRESULT invoke_builtin_function(IDispatch *this_obj, func_info_t *func, DISPPARAMS *dp,
                                       VARIANT *res, EXCEPINFO *ei, IServiceProvider *caller)
{
    VARIANT arg_buf[MAX_ARGS], *arg_ptrs[MAX_ARGS], *arg, retv, ret_ref, vhres;
    unsigned i, nconv = 0;
    DispatchEx *dispex;
    IUnknown *iface;
    HRESULT hres;

    hres = IDispatch_QueryInterface(this_obj, tid_ids[func->tid], (void**)&iface);
    if(FAILED(hres) || !iface)
        return E_UNEXPECTED;

    if(func->hook && (dispex = get_dispex_for_hook(iface))) {
        hres = func->hook(dispex, DISPATCH_METHOD, dp, res, ei, caller);
        IDispatchEx_Release(&dispex->IDispatchEx_iface);
        if(hres != S_FALSE) {
            IUnknown_Release(iface);
            return hres;
        }
    }

    if(!func->call_vtbl_off) {
        hres = typeinfo_invoke(iface, func, DISPATCH_METHOD, dp, res, ei);
        IUnknown_Release(iface);
        return hres;
    }

    if(dp->cArgs + func->default_value_cnt < func->argc) {
        FIXME("Invalid argument count (expected %u, got %u)\n", func->argc, dp->cArgs);
        IUnknown_Release(iface);
        return E_INVALIDARG;
    }

    for(i=0; i < func->argc; i++) {
        BOOL own_value = FALSE;
        if(i >= dp->cArgs) {
            /* use default value */
            arg_ptrs[i] = &func->arg_info[i].default_value;
            continue;
        }
        arg = dp->rgvarg+dp->cArgs-i-1;
        if(func->arg_types[i] == V_VT(arg)) {
            arg_ptrs[i] = arg;
        }else {
            hres = change_type(arg_buf+nconv, arg, func->arg_types[i], caller);
            if(FAILED(hres))
                break;
            arg_ptrs[i] = arg_buf + nconv++;
            own_value = TRUE;
        }

        if(func->arg_types[i] == VT_DISPATCH && !IsEqualGUID(&func->arg_info[i].iid, &IID_NULL)
            && V_DISPATCH(arg_ptrs[i])) {
            IDispatch *iface;
            if(!own_value) {
                arg_buf[nconv] = *arg_ptrs[i];
                arg_ptrs[i] = arg_buf + nconv++;
            }
            hres = IDispatch_QueryInterface(V_DISPATCH(arg_ptrs[i]), &func->arg_info[i].iid, (void**)&iface);
            if(FAILED(hres)) {
                WARN("Could not get %s iface: %08lx\n", debugstr_guid(&func->arg_info[i].iid), hres);
                break;
            }
            if(own_value)
                IDispatch_Release(V_DISPATCH(arg_ptrs[i]));
            V_DISPATCH(arg_ptrs[i]) = iface;
        }
    }

    if(SUCCEEDED(hres)) {
        if(func->prop_vt == VT_VOID) {
            V_VT(&retv) = VT_EMPTY;
        }else {
            V_VT(&retv) = func->prop_vt;
            arg_ptrs[func->argc] = &ret_ref;
            V_VT(&ret_ref) = VT_BYREF|func->prop_vt;

            switch(func->prop_vt) {
#define CASE_VT(vt,type,access)                         \
            case vt:                                    \
                V_BYREF(&ret_ref) = &access(&retv);     \
                break
            BUILTIN_ARG_TYPES_SWITCH;
#undef CASE_VT
            case VT_PTR:
                V_VT(&retv) = VT_DISPATCH;
                V_VT(&ret_ref) = VT_BYREF | VT_DISPATCH;
                V_BYREF(&ret_ref) = &V_DISPATCH(&retv);
                break;
            default:
                assert(0);
            }
        }

        V_VT(&vhres) = VT_ERROR;
        hres = DispCallFunc(iface, func->call_vtbl_off*sizeof(void*), CC_STDCALL, VT_ERROR,
                    func->argc + (func->prop_vt == VT_VOID ? 0 : 1), func->arg_types, arg_ptrs, &vhres);
    }

    while(nconv--)
        VariantClear(arg_buf+nconv);
    IUnknown_Release(iface);
    if(FAILED(hres))
        return hres;
    if(FAILED(V_ERROR(&vhres)))
        return V_ERROR(&vhres);

    if(res)
        *res = retv;
    else
        VariantClear(&retv);
    return V_ERROR(&vhres);
}

static HRESULT func_invoke(DispatchEx *This, IDispatch *this_obj, func_info_t *func, WORD flags, DISPPARAMS *dp, VARIANT *res,
        EXCEPINFO *ei, IServiceProvider *caller)
{
    HRESULT hres;

    switch(flags) {
    case DISPATCH_METHOD|DISPATCH_PROPERTYGET:
        if(!res)
            return E_INVALIDARG;
        /* fall through */
    case DISPATCH_METHOD:
        if(This->dynamic_data && This->dynamic_data->func_disps
           && This->dynamic_data->func_disps[func->func_disp_idx].func_obj) {
            func_obj_entry_t *entry = This->dynamic_data->func_disps + func->func_disp_idx;

            if(V_VT(&entry->val) != VT_DISPATCH) {
                FIXME("calling %s not supported\n", debugstr_variant(&entry->val));
                return E_NOTIMPL;
            }

            if((IDispatch*)&entry->func_obj->dispex.IDispatchEx_iface != V_DISPATCH(&entry->val)) {
                if(!V_DISPATCH(&entry->val)) {
                    FIXME("Calling null\n");
                    return E_FAIL;
                }

                hres = invoke_disp_value(this_obj, V_DISPATCH(&entry->val), 0, flags, dp, res, ei, NULL);
                break;
            }
        }

        hres = invoke_builtin_function(this_obj, func, dp, res, ei, caller);
        break;
    case DISPATCH_PROPERTYGET: {
        func_obj_entry_t *entry;

        if(func->id == DISPID_VALUE) {
            BSTR ret;

            hres = dispex_to_string(This, &ret);
            if(FAILED(hres))
                return hres;

            V_VT(res) = VT_BSTR;
            V_BSTR(res) = ret;
            return S_OK;
        }

        hres = get_func_obj_entry(This, func, &entry);
        if(FAILED(hres))
            return hres;

        V_VT(res) = VT_EMPTY;
        return VariantCopy(res, &entry->val);
    }
    case DISPATCH_PROPERTYPUT: {
        func_obj_entry_t *entry;

        if(dp->cArgs != 1 || (dp->cNamedArgs == 1 && *dp->rgdispidNamedArgs != DISPID_PROPERTYPUT)
           || dp->cNamedArgs > 1) {
            FIXME("invalid args\n");
            return E_INVALIDARG;
        }

        /*
         * NOTE: Although we have IDispatchEx tests showing, that it's not allowed to set
         * function property using InvokeEx, it's possible to do that from jscript.
         * Native probably uses some undocumented interface in this case, but it should
         * be fine for us to allow IDispatchEx handle that.
         */
        hres = get_func_obj_entry(This, func, &entry);
        if(FAILED(hres))
            return hres;

        return VariantCopy(&entry->val, dp->rgvarg);
    }
    default:
        FIXME("Unimplemented flags %x\n", flags);
        hres = E_NOTIMPL;
    }

    return hres;
}

static HRESULT invoke_builtin_prop(DispatchEx *This, IDispatch *this_obj, DISPID id, LCID lcid, WORD flags,
        DISPPARAMS *dp, VARIANT *res, EXCEPINFO *ei, IServiceProvider *caller)
{
    DispatchEx *dispex;
    func_info_t *func;
    IUnknown *iface;
    HRESULT hres;

    hres = get_builtin_func(This->info, id, &func);
    if(id == DISPID_VALUE && hres == DISP_E_MEMBERNOTFOUND)
        return dispex_value(This, lcid, flags, dp, res, ei, caller);
    if(FAILED(hres))
        return hres;

    if(func->func_disp_idx >= 0)
        return func_invoke(This, this_obj, func, flags, dp, res, ei, caller);

    hres = IDispatch_QueryInterface(this_obj, tid_ids[func->tid], (void**)&iface);
    if(FAILED(hres) || !iface)
        return E_UNEXPECTED;

    if(func->hook && (dispex = get_dispex_for_hook(iface))) {
        hres = func->hook(dispex, flags, dp, res, ei, caller);
        IDispatchEx_Release(&dispex->IDispatchEx_iface);
        if(hres != S_FALSE) {
            IUnknown_Release(iface);
            return hres;
        }
    }

    switch(flags) {
    case DISPATCH_PROPERTYPUT:
        if(res)
            V_VT(res) = VT_EMPTY;
        hres = builtin_propput(This, iface, func, dp, caller);
        break;
    case DISPATCH_PROPERTYGET:
        hres = builtin_propget(iface, func, dp, res);
        break;
    default:
        if(!func->get_vtbl_off) {
            hres = typeinfo_invoke(iface, func, flags, dp, res, ei);
        }else {
            VARIANT v;

            hres = builtin_propget(iface, func, NULL, &v);
            if(FAILED(hres))
                break;

            if(flags != (DISPATCH_PROPERTYGET|DISPATCH_METHOD) || dp->cArgs) {
                if(V_VT(&v) != VT_DISPATCH) {
                    FIXME("Not a function %s flags %08x\n", debugstr_variant(&v), flags);
                    VariantClear(&v);
                    hres = E_FAIL;
                    break;
                }

                hres = invoke_disp_value(this_obj, V_DISPATCH(&v), lcid, flags, dp, res, ei, caller);
                IDispatch_Release(V_DISPATCH(&v));
            }else if(res) {
                *res = v;
            }else {
                VariantClear(&v);
            }
        }
    }

    IUnknown_Release(iface);
    return hres;
}

HRESULT dispex_call_builtin(DispatchEx *dispex, DISPID id, DISPPARAMS *dp,
                            VARIANT *res, EXCEPINFO *ei, IServiceProvider *caller)
{
    func_info_t *func;
    HRESULT hres;

    hres = get_builtin_func(dispex->info, id, &func);
    if(FAILED(hres))
        return hres;

    return invoke_builtin_function((IDispatch*)&dispex->IDispatchEx_iface, func, dp, res, ei, caller);
}

HRESULT remove_attribute(DispatchEx *This, DISPID id, VARIANT_BOOL *success)
{
    switch(get_dispid_type(id)) {
    case DISPEXPROP_CUSTOM:
        FIXME("DISPEXPROP_CUSTOM not supported\n");
        return E_NOTIMPL;

    case DISPEXPROP_DYNAMIC: {
        DWORD idx = id - DISPID_DYNPROP_0;
        dynamic_prop_t *prop;

        prop = This->dynamic_data->props+idx;
        VariantClear(&prop->var);
        prop->flags |= DYNPROP_DELETED;
        *success = VARIANT_TRUE;
        return S_OK;
    }
    case DISPEXPROP_BUILTIN: {
        VARIANT var;
        DISPPARAMS dp = {&var,NULL,1,0};
        func_info_t *func;
        IUnknown *iface;
        HRESULT hres;

        hres = get_builtin_func(This->info, id, &func);
        if(FAILED(hres))
            return hres;

        /* For builtin functions, we set their value to the original function. */
        if(func->func_disp_idx >= 0) {
            func_obj_entry_t *entry;

            if(!This->dynamic_data || !This->dynamic_data->func_disps
                || !This->dynamic_data->func_disps[func->func_disp_idx].func_obj) {
                *success = VARIANT_FALSE;
                return S_OK;
            }

            entry = This->dynamic_data->func_disps + func->func_disp_idx;
            if(V_VT(&entry->val) == VT_DISPATCH
                    && V_DISPATCH(&entry->val) == (IDispatch*)&entry->func_obj->dispex.IDispatchEx_iface) {
                *success = VARIANT_FALSE;
                return S_OK;
            }

            VariantClear(&entry->val);
            V_VT(&entry->val) = VT_DISPATCH;
            V_DISPATCH(&entry->val) = (IDispatch*)&entry->func_obj->dispex.IDispatchEx_iface;
            IDispatch_AddRef(V_DISPATCH(&entry->val));
            *success = VARIANT_TRUE;
            return S_OK;
        }
        *success = VARIANT_TRUE;

        IDispatchEx_QueryInterface(&This->IDispatchEx_iface, tid_ids[func->tid], (void**)&iface);

        V_VT(&var) = VT_EMPTY;
        hres = builtin_propput(This, iface, func, &dp, NULL);
        if(FAILED(hres)) {
            VARIANT *ref;
            hres = dispex_get_dprop_ref(This, func->name, FALSE, &ref);
            if(FAILED(hres) || V_VT(ref) != VT_BSTR)
                *success = VARIANT_FALSE;
            else
                VariantClear(ref);
        }
        IUnknown_Release(iface);
        return S_OK;
    }
    default:
        assert(0);
        return E_FAIL;
    }
}

compat_mode_t dispex_compat_mode(DispatchEx *dispex)
{
    return dispex->info != dispex->info->desc->delayed_init_info
        ? dispex->info->compat_mode
        : dispex->info->desc->vtbl->get_compat_mode(dispex);
}

HRESULT dispex_to_string(DispatchEx *dispex, BSTR *ret)
{
    static const WCHAR prefix[8] = L"[object ";
    static const WCHAR suffix[] = L"]";
    WCHAR buf[ARRAY_SIZE(prefix) + 28 + ARRAY_SIZE(suffix)], *p = buf;
    compat_mode_t compat_mode = dispex_compat_mode(dispex);
    const char *name = dispex->info->desc->name;

    if(!ret)
        return E_INVALIDARG;

    memcpy(p, prefix, sizeof(prefix));
    p += ARRAY_SIZE(prefix);
    if(compat_mode < COMPAT_MODE_IE9)
        p--;
    else {
        while(*name)
            *p++ = *name++;
        assert(p + ARRAY_SIZE(suffix) - buf <= ARRAY_SIZE(buf));
    }
    memcpy(p, suffix, sizeof(suffix));

    *ret = SysAllocString(buf);
    return *ret ? S_OK : E_OUTOFMEMORY;
}

static dispex_data_t *ensure_dispex_info(dispex_static_data_t *desc, compat_mode_t compat_mode)
{
    if(!desc->info_cache[compat_mode]) {
        EnterCriticalSection(&cs_dispex_static_data);
        if(!desc->info_cache[compat_mode])
            desc->info_cache[compat_mode] = preprocess_dispex_data(desc, compat_mode);
        LeaveCriticalSection(&cs_dispex_static_data);
    }
    return desc->info_cache[compat_mode];
}

static BOOL ensure_real_info(DispatchEx *dispex)
{
    if(dispex->info != dispex->info->desc->delayed_init_info)
        return TRUE;

    dispex->info = ensure_dispex_info(dispex->info->desc, dispex_compat_mode(dispex));
    return dispex->info != NULL;
}

static HRESULT proxy_get_dispid(DispatchEx *dispex, const WCHAR *name, BOOL case_insens, DISPID *id)
{
    DWORD grfdex = case_insens ? fdexNameCaseInsensitive : fdexNameCaseSensitive;
    dynamic_prop_t *dprop;
    HRESULT hres;
    BSTR bstr;

    if(!ensure_real_info(dispex) || !(bstr = SysAllocString(name)))
        return E_OUTOFMEMORY;

    /* FIXME: move builtins to the prototype */
    hres = dispex_get_builtin_id(dispex, bstr, grfdex, id);
    SysFreeString(bstr);
    if(hres != DISP_E_UNKNOWNNAME)
        return hres;

    hres = get_dynamic_prop(dispex, name, grfdex, &dprop);
    if(FAILED(hres))
        return hres;

    *id = DISPID_DYNPROP_0 + (dprop - dispex->dynamic_data->props);
    return S_OK;
}

static HRESULT WINAPI proxy_func_invoke(IDispatch *this_obj, void *context, DISPPARAMS *dp, VARIANT *res, EXCEPINFO *ei, IServiceProvider *caller)
{
    func_info_t *func = context;
    return invoke_builtin_function(this_obj, func, dp, res, ei, caller);
}

static HRESULT WINAPI proxy_getter_invoke(IDispatch *this_obj, void *context, DISPPARAMS *dp, VARIANT *res, EXCEPINFO *ei, IServiceProvider *caller)
{
    func_info_t *func = context;
    DispatchEx *dispex;
    IUnknown *iface;
    HRESULT hres;

    hres = IDispatch_QueryInterface(this_obj, tid_ids[func->tid], (void**)&iface);
    if(FAILED(hres) || !iface)
        return E_UNEXPECTED;

    if(func->hook && (dispex = get_dispex_for_hook(iface))) {
        hres = func->hook(dispex, DISPATCH_PROPERTYGET, dp, res, ei, caller);
        IDispatchEx_Release(&dispex->IDispatchEx_iface);
        if(hres != S_FALSE)
            goto done;
    }
    hres = builtin_propget(iface, func, dp, res);

done:
    IUnknown_Release(iface);
    return hres;
}

static HRESULT WINAPI proxy_setter_invoke(IDispatch *this_obj, void *context, DISPPARAMS *dp, VARIANT *res, EXCEPINFO *ei, IServiceProvider *caller)
{
    static DISPID propput_dispid = DISPID_PROPERTYPUT;
    func_info_t *func = context;
    DispatchEx *dispex;
    IUnknown *iface;
    HRESULT hres;

    dp->cNamedArgs = 1;
    dp->rgdispidNamedArgs = &propput_dispid;

    hres = IDispatch_QueryInterface(this_obj, tid_ids[func->tid], (void**)&iface);
    if(FAILED(hres) || !iface)
        return E_UNEXPECTED;

    if(func->hook && (dispex = get_dispex_for_hook(iface))) {
        hres = func->hook(dispex, DISPATCH_PROPERTYPUT, dp, res, ei, caller);
        IDispatchEx_Release(&dispex->IDispatchEx_iface);
        if(hres != S_FALSE)
            goto done;
    }
    hres = builtin_propput(NULL, iface, func, dp, caller);

done:
    IUnknown_Release(iface);
    return hres;
}

static inline DispatchEx *impl_from_IDispatchEx(IDispatchEx *iface)
{
    return CONTAINING_RECORD(iface, DispatchEx, IDispatchEx_iface);
}

static HRESULT WINAPI DispatchEx_QueryInterface(IDispatchEx *iface, REFIID riid, void **ppv)
{
    DispatchEx *This = impl_from_IDispatchEx(iface);

    TRACE("%s (%p)->(%s %p)\n", This->info->desc->name, This, debugstr_mshtml_guid(riid), ppv);

    if(This->info->desc->vtbl->query_interface) {
        *ppv = This->info->desc->vtbl->query_interface(This, riid);
        if(*ppv)
            goto ret;
    }

    if(IsEqualGUID(&IID_IUnknown, riid) || IsEqualGUID(&IID_IDispatch, riid) || IsEqualGUID(&IID_IDispatchEx, riid))
        *ppv = &This->IDispatchEx_iface;
    else if(IsEqualGUID(&IID_IWineDispatchProxyPrivate, riid))
        *ppv = &This->IDispatchEx_iface;
    else if(IsEqualGUID(&IID_nsXPCOMCycleCollectionParticipant, riid)) {
        *ppv = &dispex_ccp;
        return S_OK;
    }else if(IsEqualGUID(&IID_nsCycleCollectionISupports, riid)) {
        *ppv = &This->IDispatchEx_iface;
        return S_OK;
    }else if(IsEqualGUID(&IID_IDispatchJS, riid) ||
             IsEqualGUID(&IID_UndocumentedScriptIface, riid) ||
             IsEqualGUID(&IID_IMarshal, riid) ||
             IsEqualGUID(&IID_IManagedObject, riid)) {
        *ppv = NULL;
        return E_NOINTERFACE;
    }else {
        *ppv = NULL;
        WARN("%s (%p)->(%s %p)\n", This->info->desc->name, This, debugstr_mshtml_guid(riid), ppv);
        return E_NOINTERFACE;
    }

ret:
    IDispatchEx_AddRef(&This->IDispatchEx_iface);
    return S_OK;
}

static ULONG WINAPI DispatchEx_AddRef(IDispatchEx *iface)
{
    DispatchEx *This = impl_from_IDispatchEx(iface);
    LONG ref = ccref_incr(&This->ccref, (nsISupports*)&This->IDispatchEx_iface);

    TRACE("%s (%p) ref=%ld\n", This->info->desc->name, This, ref);

    return ref;
}

static ULONG WINAPI DispatchEx_Release(IDispatchEx *iface)
{
    DispatchEx *This = impl_from_IDispatchEx(iface);
    LONG ref = ccref_decr(&This->ccref, (nsISupports*)&This->IDispatchEx_iface, &dispex_ccp);

    TRACE("%s (%p) ref=%ld\n", This->info->desc->name, This, ref);

    /* Gecko ccref may not free the object immediately when ref count reaches 0, so we need
     * an extra care for objects that need an immediate clean up. See Gecko's
     * NS_IMPL_CYCLE_COLLECTING_NATIVE_RELEASE_WITH_LAST_RELEASE for details. */
    if(!ref && This->info->desc->vtbl->last_release) {
        ccref_incr(&This->ccref, (nsISupports*)&This->IDispatchEx_iface);
        This->info->desc->vtbl->last_release(This);
        ccref_decr(&This->ccref, (nsISupports*)&This->IDispatchEx_iface, &dispex_ccp);
    }

    return ref;
}

static HRESULT WINAPI DispatchEx_GetTypeInfoCount(IDispatchEx *iface, UINT *pctinfo)
{
    DispatchEx *This = impl_from_IDispatchEx(iface);

    TRACE("%s (%p)->(%p)\n", This->info->desc->name, This, pctinfo);

    *pctinfo = 1;
    return S_OK;
}

static HRESULT WINAPI DispatchEx_GetTypeInfo(IDispatchEx *iface, UINT iTInfo,
                                              LCID lcid, ITypeInfo **ppTInfo)
{
    DispatchEx *This = impl_from_IDispatchEx(iface);
    HRESULT hres;

    TRACE("%s (%p)->(%u %lu %p)\n", This->info->desc->name, This, iTInfo, lcid, ppTInfo);

    hres = get_typeinfo(This->info->desc->disp_tid, ppTInfo);
    if(FAILED(hres))
        return hres;

    ITypeInfo_AddRef(*ppTInfo);
    return S_OK;
}

static HRESULT WINAPI DispatchEx_GetIDsOfNames(IDispatchEx *iface, REFIID riid,
                                                LPOLESTR *rgszNames, UINT cNames,
                                                LCID lcid, DISPID *rgDispId)
{
    DispatchEx *This = impl_from_IDispatchEx(iface);
    HRESULT hres = S_OK;

    if(This->proxy)
        return IDispatchEx_GetIDsOfNames((IDispatchEx*)This->proxy, riid, rgszNames,
                                         cNames, lcid, rgDispId);

    TRACE("%s (%p)->(%s %p %u %lu %p)\n", This->info->desc->name, This, debugstr_guid(riid), rgszNames,
          cNames, lcid, rgDispId);

    /* Native ignores all cNames > 1, and doesn't even fill them */
    if(cNames)
        hres = IDispatchEx_GetDispID(&This->IDispatchEx_iface, rgszNames[0], 0, rgDispId);

    return hres;
}

static HRESULT WINAPI DispatchEx_Invoke(IDispatchEx *iface, DISPID dispIdMember,
                            REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS *pDispParams,
                            VARIANT *pVarResult, EXCEPINFO *pExcepInfo, UINT *puArgErr)
{
    DispatchEx *This = impl_from_IDispatchEx(iface);

    if(This->proxy && dispIdMember >= 0)
        return IDispatchEx_Invoke((IDispatchEx*)This->proxy, dispIdMember, riid, lcid, wFlags,
                                  pDispParams, pVarResult, pExcepInfo, puArgErr);

    TRACE("%s (%p)->(%ld %s %ld %d %p %p %p %p)\n", This->info->desc->name, This, dispIdMember,
          debugstr_guid(riid), lcid, wFlags, pDispParams, pVarResult, pExcepInfo, puArgErr);

    return dispex_invoke(This, (IDispatch*)iface, dispIdMember, lcid, wFlags, pDispParams, pVarResult, pExcepInfo, NULL);
}

static HRESULT WINAPI DispatchEx_GetDispID(IDispatchEx *iface, BSTR bstrName, DWORD grfdex, DISPID *pid)
{
    DispatchEx *This = impl_from_IDispatchEx(iface);
    dynamic_prop_t *dprop;
    HRESULT hres;

    if(This->proxy)
        return IDispatchEx_GetDispID((IDispatchEx*)This->proxy, bstrName, grfdex, pid);

    TRACE("%s (%p)->(%s %lx %p)\n", This->info->desc->name, This, debugstr_w(bstrName), grfdex, pid);

    if(grfdex & ~(fdexNameCaseSensitive|fdexNameCaseInsensitive|fdexNameEnsure|fdexNameImplicit|FDEX_VERSION_MASK))
        FIXME("Unsupported grfdex %lx\n", grfdex);

    if(!ensure_real_info(This))
        return E_OUTOFMEMORY;

    hres = dispex_get_builtin_id(This, bstrName, grfdex, pid);
    if(hres != DISP_E_UNKNOWNNAME)
        return hres;

    hres = get_dynamic_prop(This, bstrName, grfdex, &dprop);
    if(FAILED(hres))
        return hres;

    *pid = DISPID_DYNPROP_0 + (dprop - This->dynamic_data->props);
    return S_OK;
}

static HRESULT WINAPI DispatchEx_InvokeEx(IDispatchEx *iface, DISPID id, LCID lcid, WORD wFlags, DISPPARAMS *pdp,
        VARIANT *pvarRes, EXCEPINFO *pei, IServiceProvider *pspCaller)
{
    DispatchEx *This = impl_from_IDispatchEx(iface);

    if(This->proxy && id >= 0)
        return IDispatchEx_InvokeEx((IDispatchEx*)This->proxy, id, lcid, wFlags, pdp, pvarRes, pei, pspCaller);

    TRACE("%s (%p)->(%lx %lx %x %p %p %p %p)\n", This->info->desc->name, This, id, lcid, wFlags, pdp, pvarRes, pei, pspCaller);

    return dispex_invoke(This, (IDispatch*)iface, id, lcid, wFlags, pdp, pvarRes, pei, pspCaller);
}

static HRESULT WINAPI DispatchEx_DeleteMemberByName(IDispatchEx *iface, BSTR name, DWORD grfdex)
{
    DispatchEx *This = impl_from_IDispatchEx(iface);
    DISPID id;
    HRESULT hres;

    if(This->proxy)
        return IDispatchEx_DeleteMemberByName((IDispatchEx*)This->proxy, name, grfdex);

    TRACE("%s (%p)->(%s %lx)\n", This->info->desc->name, This, debugstr_w(name), grfdex);

    hres = IDispatchEx_GetDispID(&This->IDispatchEx_iface, name, grfdex & ~fdexNameEnsure, &id);
    if(FAILED(hres)) {
        TRACE("property %s not found\n", debugstr_w(name));
        return dispex_compat_mode(This) < COMPAT_MODE_IE8 ? E_NOTIMPL : hres;
    }

    return dispex_delete_prop(This, id);
}

static HRESULT WINAPI DispatchEx_DeleteMemberByDispID(IDispatchEx *iface, DISPID id)
{
    DispatchEx *This = impl_from_IDispatchEx(iface);

    if(This->proxy && id >= 0)
        return IDispatchEx_DeleteMemberByDispID((IDispatchEx*)This->proxy, id);

    TRACE("%s (%p)->(%lx)\n", This->info->desc->name, This, id);

    return dispex_delete_prop(This, id);
}

static HRESULT WINAPI DispatchEx_GetMemberProperties(IDispatchEx *iface, DISPID id, DWORD grfdexFetch, DWORD *pgrfdex)
{
    DispatchEx *This = impl_from_IDispatchEx(iface);

    if(This->proxy && id >= 0)
        return IDispatchEx_GetMemberProperties((IDispatchEx*)This->proxy, id, grfdexFetch, pgrfdex);

    FIXME("%s (%p)->(%lx %lx %p)\n", This->info->desc->name, This, id, grfdexFetch, pgrfdex);
    return E_NOTIMPL;
}

static HRESULT WINAPI DispatchEx_GetMemberName(IDispatchEx *iface, DISPID id, BSTR *pbstrName)
{
    DispatchEx *This = impl_from_IDispatchEx(iface);
    func_info_t *func;
    HRESULT hres;

    if(This->proxy && id >= 0)
        return IDispatchEx_GetMemberName((IDispatchEx*)This->proxy, id, pbstrName);

    TRACE("%s (%p)->(%lx %p)\n", This->info->desc->name, This, id, pbstrName);

    if(!ensure_real_info(This))
        return E_OUTOFMEMORY;

    if(is_custom_dispid(id)) {
        if(This->info->desc->vtbl->get_name)
            return This->info->desc->vtbl->get_name(This, id, pbstrName);
        return DISP_E_MEMBERNOTFOUND;
    }

    if(is_dynamic_dispid(id)) {
        DWORD idx = id - DISPID_DYNPROP_0;

        if(!get_dynamic_data(This) || This->dynamic_data->prop_cnt <= idx)
            return DISP_E_MEMBERNOTFOUND;

        *pbstrName = SysAllocString(This->dynamic_data->props[idx].name);
        if(!*pbstrName)
            return E_OUTOFMEMORY;

        return S_OK;
    }

    hres = get_builtin_func(This->info, id, &func);
    if(FAILED(hres))
        return hres;

    *pbstrName = SysAllocString(func->name);
    if(!*pbstrName)
        return E_OUTOFMEMORY;
    return S_OK;
}

static HRESULT next_dynamic_id(DispatchEx *dispex, DWORD idx, DISPID *ret_id)
{
    while(idx < dispex->dynamic_data->prop_cnt &&
          (dispex->dynamic_data->props[idx].flags & (DYNPROP_DELETED | DYNPROP_HIDDEN)))
        idx++;

    if(idx == dispex->dynamic_data->prop_cnt) {
        *ret_id = DISPID_STARTENUM;
        return S_FALSE;
    }

    *ret_id = DISPID_DYNPROP_0+idx;
    return S_OK;
}

static HRESULT WINAPI DispatchEx_GetNextDispID(IDispatchEx *iface, DWORD grfdex, DISPID id, DISPID *pid)
{
    DispatchEx *This = impl_from_IDispatchEx(iface);
    func_info_t *func;
    HRESULT hres;

    if(This->proxy)
        return IDispatchEx_GetNextDispID((IDispatchEx*)This->proxy, grfdex, id, pid);

    TRACE("%s (%p)->(%lx %lx %p)\n", This->info->desc->name, This, grfdex, id, pid);

    if(!ensure_real_info(This))
        return E_OUTOFMEMORY;

    if(is_dynamic_dispid(id)) {
        DWORD idx = id - DISPID_DYNPROP_0;

        if(!get_dynamic_data(This) || This->dynamic_data->prop_cnt <= idx)
            return DISP_E_MEMBERNOTFOUND;

        return next_dynamic_id(This, idx+1, pid);
    }

    if(!is_custom_dispid(id)) {
        if(id == DISPID_STARTENUM) {
            func = This->info->funcs;
        }else {
            hres = get_builtin_func(This->info, id, &func);
            if(FAILED(hres))
                return hres;
            func++;
        }

        while(func < This->info->funcs + This->info->func_cnt) {
            if(func->func_disp_idx == -1) {
                *pid = func->id;
                return S_OK;
            }
            func++;
        }

        id = DISPID_STARTENUM;
    }

    if(This->info->desc->vtbl->next_dispid) {
        hres = This->info->desc->vtbl->next_dispid(This, id, pid);
        if(hres != S_FALSE)
            return hres;
    }

    if(get_dynamic_data(This) && This->dynamic_data->prop_cnt)
        return next_dynamic_id(This, 0, pid);

    *pid = DISPID_STARTENUM;
    return S_FALSE;
}

static HRESULT WINAPI DispatchEx_GetNameSpaceParent(IDispatchEx *iface, IUnknown **ppunk)
{
    DispatchEx *This = impl_from_IDispatchEx(iface);
    FIXME("%s (%p)->(%p)\n", This->info->desc->name, This, ppunk);
    return E_NOTIMPL;
}

static inline DispatchEx *impl_from_IWineDispatchProxyPrivate(IWineDispatchProxyPrivate *iface)
{
    return impl_from_IDispatchEx((IDispatchEx*)iface);
}

static IWineDispatchProxyCbPrivate** WINAPI WineDispatchProxyPrivate_GetProxyFieldRef(IWineDispatchProxyPrivate *iface)
{
    DispatchEx *This = impl_from_IWineDispatchProxyPrivate(iface);
    return &This->proxy;
}

static BOOL WINAPI WineDispatchProxyPrivate_HasProxy(IWineDispatchProxyPrivate *iface)
{
    DispatchEx *This = impl_from_IWineDispatchProxyPrivate(iface);
    return This->info->compat_mode >= COMPAT_MODE_IE9;
}

static HRESULT WINAPI WineDispatchProxyPrivate_PropFixOverride(IWineDispatchProxyPrivate *iface, struct proxy_prop_info *info)
{
    DispatchEx *This = impl_from_IWineDispatchProxyPrivate(iface);
    HRESULT hres;

    if(!This->info->desc->vtbl->override)
        return S_FALSE;

    /* We only care about custom props, as those are the only ones which can mismatch.
       Some objects with custom props (such as the Storage objects) can be out of sync,
       because the underlying storage is changed asynchronously (e.g. the backing file
       in localStorage), so the prop may not exist at this point, even if it did before. */
    if(info->dispid != DISPID_UNKNOWN && !is_custom_dispid(info->dispid))
        return S_FALSE;

    hres = This->info->desc->vtbl->get_dispid(This, (WCHAR*)info->name, fdexNameCaseSensitive, &info->dispid);
    if(hres == DISP_E_UNKNOWNNAME) {
        if(info->dispid == DISPID_UNKNOWN)
            return S_FALSE;
        info->dispid = DISPID_UNKNOWN;
        return S_OK;
    }
    if(FAILED(hres))
        return hres;
    info->flags = PROPF_WRITABLE | PROPF_CONFIGURABLE | PROPF_ENUMERABLE;
    return S_OK;
}

static HRESULT WINAPI WineDispatchProxyPrivate_PropOverride(IWineDispatchProxyPrivate *iface, const WCHAR *name, VARIANT *value)
{
    DispatchEx *This = impl_from_IWineDispatchProxyPrivate(iface);

    if(!This->info->desc->vtbl->override)
        return S_FALSE;
    return This->info->desc->vtbl->override(This, name, value);
}

static HRESULT WINAPI WineDispatchProxyPrivate_PropDefineOverride(IWineDispatchProxyPrivate *iface, struct proxy_prop_info *info)
{
    DispatchEx *This = impl_from_IWineDispatchProxyPrivate(iface);
    HRESULT hres;

    if(!This->info->desc->vtbl->override)
        return S_FALSE;

    hres = This->info->desc->vtbl->get_dispid(This, (WCHAR*)info->name, fdexNameEnsure | fdexNameCaseSensitive, &info->dispid);
    if(FAILED(hres))
        return (hres == DISP_E_UNKNOWNNAME) ? S_FALSE : hres;

    info->func[0].invoke = NULL;
    info->flags = PROPF_WRITABLE | PROPF_CONFIGURABLE | PROPF_ENUMERABLE;
    return S_OK;
}

static HRESULT WINAPI WineDispatchProxyPrivate_PropGetInfo(IWineDispatchProxyPrivate *iface, const WCHAR *name,
        BOOL case_insens, struct proxy_prop_info *info)
{
    DispatchEx *This = impl_from_IWineDispatchProxyPrivate(iface);
    func_info_t *func;
    HRESULT hres;

    info->func[0].invoke = NULL;

    hres = proxy_get_dispid(This, name, case_insens, &info->dispid);
    if(FAILED(hres))
        return hres;

    if(is_dynamic_dispid(info->dispid)) {
        info->name = This->dynamic_data->props[info->dispid - DISPID_DYNPROP_0].name;
        info->flags = PROPF_WRITABLE | PROPF_CONFIGURABLE | PROPF_ENUMERABLE;
        return S_OK;
    }

    if(is_custom_dispid(info->dispid)) {
        info->name = name;  /* FIXME */
        info->flags = PROPF_WRITABLE;
        if(This->info->desc->vtbl->delete)
            info->flags |= PROPF_CONFIGURABLE;
        if(This->info->desc->vtbl->next_dispid)
            info->flags |= PROPF_ENUMERABLE;
        return S_OK;
    }

    hres = get_builtin_func(This->info, info->dispid, &func);
    if(FAILED(hres))
        return (hres == DISP_E_MEMBERNOTFOUND) ? E_UNEXPECTED : hres;
    info->func[0].context = info->func[1].context = func;
    info->name = func->name;

    if(func->func_disp_idx >= 0) {
        if(This->dynamic_data && This->dynamic_data->func_disps
           && This->dynamic_data->func_disps[func->func_disp_idx].func_obj) {
            func_obj_entry_t *entry = This->dynamic_data->func_disps + func->func_disp_idx;

            if((IDispatch*)&entry->func_obj->dispex.IDispatchEx_iface != V_DISPATCH(&entry->val)) {
                info->flags = PROPF_WRITABLE | PROPF_CONFIGURABLE;
                return S_OK;
            }
        }
        info->flags = PROPF_METHOD | func->argc | PROPF_WRITABLE | PROPF_CONFIGURABLE;
        info->func[0].invoke = proxy_func_invoke;
        info->func[1].invoke = NULL;
        return S_OK;
    }

    info->flags = PROPF_CONFIGURABLE | (func->put_vtbl_off ? PROPF_WRITABLE : 0);
    if(func->func_disp_idx == -1)
        info->flags |= PROPF_ENUMERABLE;
    info->func[0].invoke = proxy_getter_invoke;
    info->func[1].invoke = func->put_vtbl_off ? proxy_setter_invoke : NULL;
    return S_OK;
}

static HRESULT WINAPI WineDispatchProxyPrivate_PropInvoke(IWineDispatchProxyPrivate *iface, IDispatch *this_obj, DISPID id,
        LCID lcid, DWORD flags, DISPPARAMS *dp, VARIANT *ret, EXCEPINFO *ei, IServiceProvider *caller)
{
    DispatchEx *This = impl_from_IWineDispatchProxyPrivate(iface);

    return dispex_invoke(This, this_obj, id, lcid, flags, dp, ret, ei, caller);
}

static HRESULT WINAPI WineDispatchProxyPrivate_PropDelete(IWineDispatchProxyPrivate *iface, DISPID id)
{
    DispatchEx *This = impl_from_IWineDispatchProxyPrivate(iface);

    return dispex_delete_prop(This, id);
}

static HRESULT WINAPI WineDispatchProxyPrivate_PropEnum(IWineDispatchProxyPrivate *iface)
{
    DispatchEx *This = impl_from_IWineDispatchProxyPrivate(iface);
    IWineDispatchProxyCbPrivate *obj = This->proxy;
    dynamic_prop_t *dyn_prop, *dyn_prop_end;
    dispex_dynamic_data_t *dyn_data;
    func_info_t *func, *func_end;
    HRESULT hres;
    HRESULT (STDMETHODCALLTYPE *callback)(IWineDispatchProxyCbPrivate*,const WCHAR*) = obj->lpVtbl->PropEnum;

    if(!ensure_real_info(This))
        return E_OUTOFMEMORY;

    for(func = This->info->funcs, func_end = func + This->info->func_cnt; func != func_end; func++) {
        if(func->func_disp_idx == -1) {
            hres = callback(obj, func->name);
            if(FAILED(hres))
                return hres;
        }
    }

    if(This->info->desc->vtbl->next_dispid) {
        const dispex_static_data_vtbl_t *vtbl = This->info->desc->vtbl;
        DISPID id = DISPID_STARTENUM;
        BSTR name;

        do {
            hres = vtbl->next_dispid(This, id, &id);
            if(hres != S_OK)
                break;
            hres = vtbl->get_name(This, id, &name);
            if(SUCCEEDED(hres)) {
                hres = callback(obj, name);
                SysFreeString(name);
            }
        } while(SUCCEEDED(hres));

        if(FAILED(hres))
            return hres;
    }

    if(!(dyn_data = get_dynamic_data(This)))
        return E_OUTOFMEMORY;

    for(dyn_prop = dyn_data->props, dyn_prop_end = dyn_prop + dyn_data->prop_cnt; dyn_prop != dyn_prop_end; dyn_prop++) {
        if(!(dyn_prop->flags & (DYNPROP_DELETED | DYNPROP_HIDDEN))) {
            hres = callback(obj, dyn_prop->name);
            if(FAILED(hres))
                return hres;
        }
    }

    return S_OK;
}

static HRESULT WINAPI WineDispatchProxyPrivate_ToString(IWineDispatchProxyPrivate *iface, BSTR *string)
{
    DispatchEx *This = impl_from_IWineDispatchProxyPrivate(iface);

    return dispex_to_string(This, string);
}

static IWineDispatchProxyPrivateVtbl WineDispatchProxyPrivateVtbl = {
    {
    DispatchEx_QueryInterface,
    DispatchEx_AddRef,
    DispatchEx_Release,
    DispatchEx_GetTypeInfoCount,
    DispatchEx_GetTypeInfo,
    DispatchEx_GetIDsOfNames,
    DispatchEx_Invoke,
    DispatchEx_GetDispID,
    DispatchEx_InvokeEx,
    DispatchEx_DeleteMemberByName,
    DispatchEx_DeleteMemberByDispID,
    DispatchEx_GetMemberProperties,
    DispatchEx_GetMemberName,
    DispatchEx_GetNextDispID,
    DispatchEx_GetNameSpaceParent
    },

    /* IWineDispatchProxyPrivate extension */
    WineDispatchProxyPrivate_GetProxyFieldRef,
    WineDispatchProxyPrivate_HasProxy,
    WineDispatchProxyPrivate_PropFixOverride,
    WineDispatchProxyPrivate_PropOverride,
    WineDispatchProxyPrivate_PropDefineOverride,
    WineDispatchProxyPrivate_PropGetInfo,
    WineDispatchProxyPrivate_PropInvoke,
    WineDispatchProxyPrivate_PropDelete,
    WineDispatchProxyPrivate_PropEnum,
    WineDispatchProxyPrivate_ToString
};

HRESULT dispex_invoke(DispatchEx *dispex, IDispatch *this_obj, DISPID id, LCID lcid, WORD wFlags, DISPPARAMS *pdp,
        VARIANT *res, EXCEPINFO *pei, IServiceProvider *caller)
{
    HRESULT hres;

    if(!ensure_real_info(dispex))
        return E_OUTOFMEMORY;

    if(wFlags == (DISPATCH_PROPERTYPUT|DISPATCH_PROPERTYPUTREF))
        wFlags = DISPATCH_PROPERTYPUT;

    switch(get_dispid_type(id)) {
    case DISPEXPROP_CUSTOM:
        if(!dispex->info->desc->vtbl->invoke)
            return DISP_E_MEMBERNOTFOUND;
        return dispex->info->desc->vtbl->invoke(dispex, this_obj, id, lcid, wFlags, pdp, res, pei, caller);

    case DISPEXPROP_DYNAMIC: {
        DWORD idx = id - DISPID_DYNPROP_0;
        dynamic_prop_t *prop;

        if(!get_dynamic_data(dispex) || dispex->dynamic_data->prop_cnt <= idx)
            return DISP_E_MEMBERNOTFOUND;

        prop = dispex->dynamic_data->props+idx;

        switch(wFlags) {
        case DISPATCH_METHOD|DISPATCH_PROPERTYGET:
            if(!res)
                return E_INVALIDARG;
            /* fall through */
        case DISPATCH_METHOD:
            if(V_VT(&prop->var) != VT_DISPATCH) {
                FIXME("invoke %s\n", debugstr_variant(&prop->var));
                return E_NOTIMPL;
            }

            return invoke_disp_value(this_obj, V_DISPATCH(&prop->var), lcid, wFlags, pdp, res, pei, caller);
        case DISPATCH_PROPERTYGET:
            if(prop->flags & DYNPROP_DELETED)
                return DISP_E_MEMBERNOTFOUND;
            V_VT(res) = VT_EMPTY;
            return variant_copy(res, &prop->var);
        case DISPATCH_PROPERTYPUT:
            if(pdp->cArgs != 1 || (pdp->cNamedArgs == 1 && *pdp->rgdispidNamedArgs != DISPID_PROPERTYPUT)
               || pdp->cNamedArgs > 1) {
                FIXME("invalid args\n");
                return E_INVALIDARG;
            }

            TRACE("put %s\n", debugstr_variant(pdp->rgvarg));
            VariantClear(&prop->var);
            hres = variant_copy(&prop->var, pdp->rgvarg);
            if(FAILED(hres))
                return hres;

            prop->flags &= ~DYNPROP_DELETED;
            return S_OK;
        default:
            FIXME("unhandled wFlags %x\n", wFlags);
            return E_NOTIMPL;
        }
    }
    case DISPEXPROP_BUILTIN:
        if(wFlags == DISPATCH_CONSTRUCT) {
            if(id == DISPID_VALUE) {
                if(dispex->info->desc->vtbl->value) {
                    return dispex->info->desc->vtbl->value(dispex, lcid, wFlags, pdp, res, pei, caller);
                }
                FIXME("DISPATCH_CONSTRUCT flag but missing value function\n");
                return E_FAIL;
            }
            FIXME("DISPATCH_CONSTRUCT flag without DISPID_VALUE\n");
            return E_FAIL;
        }

        return invoke_builtin_prop(dispex, this_obj, id, lcid, wFlags, pdp, res, pei, caller);
    default:
        assert(0);
        return E_FAIL;
    }
}

HRESULT dispex_delete_prop(DispatchEx *dispex, DISPID id)
{
    if(is_custom_dispid(id) && dispex->info->desc->vtbl->delete)
        return dispex->info->desc->vtbl->delete(dispex, id);

    if(dispex_compat_mode(dispex) < COMPAT_MODE_IE8) {
        /* Not implemented by IE */
        return E_NOTIMPL;
    }

    if(is_dynamic_dispid(id)) {
        DWORD idx = id - DISPID_DYNPROP_0;
        dynamic_prop_t *prop;

        if(!get_dynamic_data(dispex) || idx >= dispex->dynamic_data->prop_cnt)
            return S_OK;

        prop = dispex->dynamic_data->props + idx;
        VariantClear(&prop->var);
        prop->flags |= DYNPROP_DELETED;
        return S_OK;
    }

    return S_OK;
}

static nsresult NSAPI dispex_traverse(void *ccp, void *p, nsCycleCollectionTraversalCallback *cb)
{
    DispatchEx *This = impl_from_IDispatchEx(p);
    dynamic_prop_t *prop;

    describe_cc_node(&This->ccref, This->info->desc->name, cb);

    if(This->proxy)
        note_cc_edge((nsISupports*)This->proxy, "proxy", cb);

    if(This->info->desc->vtbl->traverse)
        This->info->desc->vtbl->traverse(This, cb);

    if(!This->dynamic_data)
        return NS_OK;

    for(prop = This->dynamic_data->props; prop < This->dynamic_data->props + This->dynamic_data->prop_cnt; prop++)
        traverse_variant(&prop->var, "dispex_data", cb);

    if(This->dynamic_data->func_disps) {
        func_obj_entry_t *iter = This->dynamic_data->func_disps, *end = iter + This->info->func_disp_cnt;

        for(iter = This->dynamic_data->func_disps; iter < end; iter++) {
            if(!iter->func_obj)
                continue;
            note_cc_edge((nsISupports*)&iter->func_obj->dispex.IDispatchEx_iface, "func_obj", cb);
            traverse_variant(&iter->val, "func_val", cb);
        }
    }

    return NS_OK;
}

void dispex_props_unlink(DispatchEx *This)
{
    dynamic_prop_t *prop;

    if(This->proxy) {
        IWineDispatchProxyCbPrivate *proxy = This->proxy;
        This->proxy = NULL;
        proxy->lpVtbl->Unlinked(proxy, FALSE);
    }

    if(!This->dynamic_data)
        return;

    for(prop = This->dynamic_data->props; prop < This->dynamic_data->props + This->dynamic_data->prop_cnt; prop++) {
        prop->flags |= DYNPROP_DELETED;
        unlink_variant(&prop->var);
    }

    if(This->dynamic_data->func_disps) {
        func_obj_entry_t *iter = This->dynamic_data->func_disps, *end = iter + This->info->func_disp_cnt;

        for(iter = This->dynamic_data->func_disps; iter < end; iter++) {
            if(!iter->func_obj)
                continue;
            iter->func_obj->obj = NULL;
            IDispatchEx_Release(&iter->func_obj->dispex.IDispatchEx_iface);
            VariantClear(&iter->val);
        }

        free(This->dynamic_data->func_disps);
        This->dynamic_data->func_disps = NULL;
    }
}

static nsresult NSAPI dispex_unlink(void *p)
{
    DispatchEx *This = impl_from_IDispatchEx(p);

    if(This->info->desc->vtbl->unlink)
        This->info->desc->vtbl->unlink(This);

    dispex_props_unlink(This);
    return NS_OK;
}

static void NSAPI dispex_delete_cycle_collectable(void *p)
{
    DispatchEx *This = impl_from_IDispatchEx(p);
    dynamic_prop_t *prop;

    if(This->info->desc->vtbl->unlink)
        This->info->desc->vtbl->unlink(This);

    if(This->proxy)
        This->proxy->lpVtbl->Unlinked(This->proxy, FALSE);

    if(!This->dynamic_data)
        goto destructor;

    for(prop = This->dynamic_data->props; prop < This->dynamic_data->props + This->dynamic_data->prop_cnt; prop++) {
        VariantClear(&prop->var);
        free(prop->name);
    }

    free(This->dynamic_data->props);

    if(This->dynamic_data->func_disps) {
        func_obj_entry_t *iter;

        for(iter = This->dynamic_data->func_disps; iter < This->dynamic_data->func_disps + This->info->func_disp_cnt; iter++) {
            if(iter->func_obj) {
                iter->func_obj->obj = NULL;
                IDispatchEx_Release(&iter->func_obj->dispex.IDispatchEx_iface);
            }
            VariantClear(&iter->val);
        }

        free(This->dynamic_data->func_disps);
    }

    free(This->dynamic_data);

destructor:
    This->info->desc->vtbl->destructor(This);
}

void init_dispex_cc(void)
{
    static const CCObjCallback dispex_ccp_callback = {
        dispex_traverse,
        dispex_unlink,
        dispex_delete_cycle_collectable
    };
    ccp_init(&dispex_ccp, &dispex_ccp_callback);
}

const void *dispex_get_vtbl(DispatchEx *dispex)
{
    return dispex->info->desc->vtbl;
}

void init_dispatch(DispatchEx *dispex, dispex_static_data_t *data, HTMLInnerWindow *window, compat_mode_t compat_mode)
{
    assert(compat_mode < COMPAT_MODE_CNT);

    dispex->IDispatchEx_iface.lpVtbl = (const IDispatchExVtbl*)&WineDispatchProxyPrivateVtbl;
    dispex->proxy = NULL;
    dispex->dynamic_data = NULL;
    ccref_init(&dispex->ccref, 1);

    if(data->vtbl->get_compat_mode) {
        /* delayed init */
        if(!data->delayed_init_info) {
            EnterCriticalSection(&cs_dispex_static_data);
            if(!data->delayed_init_info) {
                dispex_data_t *info = calloc(1, sizeof(*data->delayed_init_info));
                if(info) {
                    info->desc = data;
                    data->delayed_init_info = info;
                }
            }
            LeaveCriticalSection(&cs_dispex_static_data);
        }
        dispex->info = data->delayed_init_info;
    }else {
        dispex->info = ensure_dispex_info(data, compat_mode);
        if(window) {
            if(compat_mode >= COMPAT_MODE_IE9) {
                IWineDispatchProxyCbPrivate *proxy = window->event_target.dispex.proxy;
                if(!proxy) {
                    init_proxies(window);
                    proxy = window->event_target.dispex.proxy;
                }
                if(proxy) {
                    HRESULT hres = proxy->lpVtbl->InitProxy(proxy, (IDispatch*)&dispex->IDispatchEx_iface);
                    if(hres == E_UNEXPECTED) {
                        /* Possible element (e.g. <script>) created on old proxy before
                           script host was initialized, so re-initialize it and retry. */
                        init_proxies(window);
                        if((proxy = window->event_target.dispex.proxy))
                            hres = proxy->lpVtbl->InitProxy(proxy, (IDispatch*)&dispex->IDispatchEx_iface);
                    }
                    if(FAILED(hres))
                        ERR("InitProxy failed: %08lx\n", hres);
                }
            }
        }
    }
}
