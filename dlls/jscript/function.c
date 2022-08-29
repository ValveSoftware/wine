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

#include <assert.h>
#include <mshtmdid.h>

#include "jscript.h"
#include "engine.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(jscript);

typedef struct _function_vtbl_t function_vtbl_t;

typedef struct {
    jsdisp_t dispex;
    const function_vtbl_t *vtbl;
    DWORD flags;
    DWORD length;
} FunctionInstance;

struct _function_vtbl_t {
    HRESULT (*call)(script_ctx_t*,FunctionInstance*,jsval_t,unsigned,unsigned,jsval_t*,jsval_t*);
    HRESULT (*toString)(FunctionInstance*,jsstr_t**);
    function_code_t* (*get_code)(FunctionInstance*);
    void (*destructor)(FunctionInstance*);
    HRESULT (*gc_traverse)(FunctionInstance*,void*);
};

typedef struct {
    FunctionInstance function;
    scope_chain_t *scope_chain;
    bytecode_t *code;
    function_code_t *func_code;
} InterpretedFunction;

typedef struct {
    FunctionInstance function;
    builtin_invoke_t proc;
    const WCHAR *name;
} NativeFunction;

typedef struct {
    FunctionInstance function;
    struct proxy_func_invoker func;
} ProxyFunction;

typedef struct {
    FunctionInstance function;
    IDispatch *disp;
    const WCHAR *name;
} ProxyConstructor;

typedef struct {
    FunctionInstance function;
    FunctionInstance *target;
    IDispatch *this;
    unsigned argc;
    jsval_t args[1];
} BindFunction;

typedef struct {
    jsdisp_t jsdisp;
    InterpretedFunction *function;
    jsval_t *buf;
    call_frame_t *frame;
    unsigned argc;
} ArgumentsInstance;

static const WCHAR ProxyConstructor_createW[] = L"create";

static HRESULT create_bind_function(script_ctx_t*,FunctionInstance*,IDispatch*,unsigned,jsval_t*,jsdisp_t**r);

static HRESULT no_gc_traverse(FunctionInstance *function, void *arg)
{
    return S_OK;
}

static inline FunctionInstance *function_from_jsdisp(jsdisp_t *jsdisp)
{
    return CONTAINING_RECORD(jsdisp, FunctionInstance, dispex);
}

static inline FunctionInstance *function_this(jsval_t vthis)
{
    jsdisp_t *jsdisp = is_object_instance(vthis) ? to_jsdisp(get_object(vthis)) : NULL;
    return (jsdisp && is_class(jsdisp, JSCLASS_FUNCTION)) ? function_from_jsdisp(jsdisp) : NULL;
}

static inline ArgumentsInstance *arguments_from_jsdisp(jsdisp_t *jsdisp)
{
    return CONTAINING_RECORD(jsdisp, ArgumentsInstance, jsdisp);
}

static HRESULT Arguments_value(script_ctx_t *ctx, jsval_t vthis, WORD flags, unsigned argc, jsval_t *argv,
        jsval_t *r)
{
    FIXME("\n");
    return E_NOTIMPL;
}

static void Arguments_destructor(jsdisp_t *jsdisp)
{
    ArgumentsInstance *arguments = arguments_from_jsdisp(jsdisp);

    TRACE("(%p)\n", arguments);

    if(arguments->buf) {
        unsigned i;
        for(i = 0; i < arguments->argc; i++)
            jsval_release(arguments->buf[i]);
        heap_free(arguments->buf);
    }

    if(arguments->function)
        jsdisp_release(&arguments->function->function.dispex);
    heap_free(arguments);
}

static unsigned Arguments_idx_length(jsdisp_t *jsdisp)
{
    ArgumentsInstance *arguments = arguments_from_jsdisp(jsdisp);
    return arguments->argc;
}

static jsval_t *get_argument_ref(ArgumentsInstance *arguments, unsigned idx)
{
    if(arguments->buf)
        return arguments->buf + idx;
    if(arguments->frame->base_scope->frame || idx >= arguments->frame->function->param_cnt)
        return arguments->jsdisp.ctx->stack + arguments->frame->arguments_off + idx;
    return NULL;
}

static HRESULT Arguments_idx_get(jsdisp_t *jsdisp, unsigned idx, jsval_t *r)
{
    ArgumentsInstance *arguments = arguments_from_jsdisp(jsdisp);
    jsval_t *ref;

    TRACE("%p[%u]\n", arguments, idx);

    if((ref = get_argument_ref(arguments, idx)))
        return jsval_copy(*ref, r);

    /* FIXME: Accessing by name won't work for duplicated argument names */
    return jsdisp_propget_name(arguments->frame->base_scope->jsobj,
                               arguments->function->func_code->params[idx], r);
}

static HRESULT Arguments_idx_put(jsdisp_t *jsdisp, unsigned idx, jsval_t val)
{
    ArgumentsInstance *arguments = arguments_from_jsdisp(jsdisp);
    jsval_t *ref;
    HRESULT hres;

    TRACE("%p[%u] = %s\n", arguments, idx, debugstr_jsval(val));

    if((ref = get_argument_ref(arguments, idx))) {
        jsval_t copy;
        hres = jsval_copy(val, &copy);
        if(FAILED(hres))
            return hres;

        jsval_release(*ref);
        *ref = copy;
        return S_OK;
    }

    /* FIXME: Accessing by name won't work for duplicated argument names */
    return jsdisp_propput_name(arguments->frame->base_scope->jsobj,
                               arguments->function->func_code->params[idx], val);
}

static HRESULT Arguments_gc_traverse(jsdisp_t *jsdisp, void *arg)
{
    ArgumentsInstance *arguments = arguments_from_jsdisp(jsdisp);
    HRESULT hres;
    unsigned i;

    if(arguments->buf) {
        for(i = 0; i < arguments->argc; i++) {
            hres = gc_process_linked_val(jsdisp, &arguments->buf[i], arg);
            if(FAILED(hres))
                return hres;
        }
    }

    return gc_process_linked_obj(jsdisp, &arguments->function->function.dispex, (void**)&arguments->function, arg);
}

static const builtin_info_t Arguments_info = {
    JSCLASS_ARGUMENTS,
    Arguments_value,
    0, NULL,
    Arguments_destructor,
    NULL,
    Arguments_idx_length,
    Arguments_idx_get,
    Arguments_idx_put,
    Arguments_gc_traverse
};

HRESULT setup_arguments_object(script_ctx_t *ctx, call_frame_t *frame)
{
    ArgumentsInstance *args;
    HRESULT hres;

    args = heap_alloc_zero(sizeof(*args));
    if(!args)
        return E_OUTOFMEMORY;

    hres = init_dispex_from_constr(&args->jsdisp, ctx, &Arguments_info, ctx->object_constr);
    if(FAILED(hres)) {
        heap_free(args);
        return hres;
    }

    args->function = (InterpretedFunction*)function_from_jsdisp(jsdisp_addref(frame->function_instance));
    args->argc = frame->argc;
    args->frame = frame;

    hres = jsdisp_define_data_property(&args->jsdisp, L"length", PROPF_WRITABLE | PROPF_CONFIGURABLE,
                                       jsval_number(args->argc));
    if(SUCCEEDED(hres))
        hres = jsdisp_define_data_property(&args->jsdisp, L"callee", PROPF_WRITABLE | PROPF_CONFIGURABLE,
                                           jsval_obj(&args->function->function.dispex));
    if(SUCCEEDED(hres))
        hres = jsdisp_propput(frame->base_scope->jsobj, L"arguments", PROPF_WRITABLE, TRUE, jsval_obj(&args->jsdisp));
    if(FAILED(hres)) {
        jsdisp_release(&args->jsdisp);
        return hres;
    }

    frame->arguments_obj = &args->jsdisp;
    return S_OK;
}

void detach_arguments_object(jsdisp_t *args_disp)
{
    ArgumentsInstance *arguments = arguments_from_jsdisp(args_disp);
    call_frame_t *frame = arguments->frame;
    const BOOL on_stack = frame->base_scope->frame == frame;
    HRESULT hres;

    /* Reset arguments value to cut the reference cycle. Note that since all activation contexts have
     * their own arguments property, it's impossible to use prototype's one during name lookup */
    jsdisp_propput_name(frame->base_scope->jsobj, L"arguments", jsval_undefined());
    arguments->frame = NULL;

    /* Don't bother coppying arguments if call frame holds the last reference. */
    if(arguments->jsdisp.ref > 1) {
        arguments->buf = heap_alloc(arguments->argc * sizeof(*arguments->buf));
        if(arguments->buf) {
            int i;

            for(i = 0; i < arguments->argc ; i++) {
                if(on_stack || i >= frame->function->param_cnt)
                    hres = jsval_copy(arguments->jsdisp.ctx->stack[frame->arguments_off + i], arguments->buf+i);
                else
                    hres = jsdisp_propget_name(frame->base_scope->jsobj, frame->function->params[i], arguments->buf+i);
                if(FAILED(hres))
                    arguments->buf[i] = jsval_undefined();
            }
        }else {
            ERR("out of memory\n");
            arguments->argc = 0;
        }
    }

    jsdisp_release(frame->arguments_obj);
}

HRESULT Function_invoke(jsdisp_t *func_this, IDispatch *jsthis, WORD flags, unsigned argc, jsval_t *argv, jsval_t *r)
{
    FunctionInstance *function;
    jsval_t vthis;

    TRACE("func %p this %p\n", func_this, jsthis);

    assert(is_class(func_this, JSCLASS_FUNCTION));
    function = function_from_jsdisp(func_this);

    if(jsthis)
        vthis = jsval_disp(jsthis);
    else
        vthis = function->dispex.ctx->version < SCRIPTLANGUAGEVERSION_ES5 ? jsval_null() : jsval_undefined();
    return function->vtbl->call(function->dispex.ctx, function, vthis, flags, argc, argv, r);
}

static HRESULT Function_get_length(script_ctx_t *ctx, jsdisp_t *jsthis, jsval_t *r)
{
    TRACE("%p\n", jsthis);

    *r = jsval_number(function_from_jsdisp(jsthis)->length);
    return S_OK;
}

static HRESULT Function_toString(script_ctx_t *ctx, jsval_t vthis, WORD flags, unsigned argc, jsval_t *argv,
        jsval_t *r)
{
    FunctionInstance *function;
    jsstr_t *str;
    HRESULT hres;

    TRACE("\n");

    if(!(function = function_this(vthis)))
        return JS_E_FUNCTION_EXPECTED;

    hres = function->vtbl->toString(function, &str);
    if(FAILED(hres))
        return hres;

    if(r)
        *r = jsval_string(str);
    else
        jsstr_release(str);
    return S_OK;
}

static HRESULT array_to_args(script_ctx_t *ctx, jsdisp_t *arg_array, unsigned *argc, jsval_t **ret)
{
    jsval_t *argv, val;
    UINT32 length, i;
    HRESULT hres;

    hres = jsdisp_propget_name(arg_array, L"length", &val);
    if(FAILED(hres))
        return hres;

    hres = to_uint32(ctx, val, &length);
    jsval_release(val);
    if(FAILED(hres))
        return hres;

    argv = heap_alloc(length * sizeof(*argv));
    if(!argv)
        return E_OUTOFMEMORY;

    for(i=0; i<length; i++) {
        hres = jsdisp_get_idx(arg_array, i, argv+i);
        if(hres == DISP_E_UNKNOWNNAME) {
            argv[i] = jsval_undefined();
        }else if(FAILED(hres)) {
            while(i--)
                jsval_release(argv[i]);
            heap_free(argv);
            return hres;
        }
    }

    *argc = length;
    *ret = argv;
    return S_OK;
}

static HRESULT disp_to_args(script_ctx_t *ctx, IDispatch *disp, unsigned *argc, jsval_t **ret)
{
    IDispatchEx *dispex;
    DWORD length, i;
    jsval_t *argv;
    DISPID dispid;
    EXCEPINFO ei;
    UINT err = 0;
    HRESULT hres;
    VARIANT var;
    BSTR name;

    if(!(name = SysAllocString(L"length")))
        return E_OUTOFMEMORY;
    hres = IDispatch_QueryInterface(disp, &IID_IDispatchEx, (void**)&dispex);
    if(SUCCEEDED(hres) && dispex)
        hres = IDispatchEx_GetDispID(dispex, name, fdexNameCaseSensitive, &dispid);
    else {
        hres = IDispatch_GetIDsOfNames(disp, &IID_NULL, &name, 1, 0, &dispid);
        dispex = NULL;
    }
    SysFreeString(name);
    if(SUCCEEDED(hres) && dispid == DISPID_UNKNOWN)
        hres = DISP_E_UNKNOWNNAME;
    if(FAILED(hres)) {
        if(hres == DISP_E_UNKNOWNNAME)
            hres = JS_E_ARRAY_OR_ARGS_EXPECTED;
        goto fail;
    }

    if(dispex)
        hres = IDispatchEx_InvokeEx(dispex, dispid, ctx->lcid, DISPATCH_PROPERTYGET, NULL,
                                    &var, &ei, &ctx->jscaller->IServiceProvider_iface);
    else
        hres = IDispatch_Invoke(disp, dispid, &IID_NULL, ctx->lcid, DISPATCH_PROPERTYGET, NULL, &var, &ei, &err);
    if(FAILED(hres)) {
        if(hres == DISP_E_EXCEPTION)
            disp_fill_exception(ctx, &ei);
        if(hres == DISP_E_MEMBERNOTFOUND)
            hres = JS_E_ARRAY_OR_ARGS_EXPECTED;
        goto fail;
    }

    if(FAILED(VariantChangeType(&var, &var, 0, VT_UI4))) {
        VariantClear(&var);
        hres = JS_E_ARRAY_OR_ARGS_EXPECTED;
        goto fail;
    }
    length = V_UI4(&var);

    argv = heap_alloc(length * sizeof(*argv));
    if(!argv) {
        hres = E_OUTOFMEMORY;
        goto fail;
    }

    for(i = 0; i < length; i++) {
        WCHAR buf[12];

        swprintf(buf, ARRAY_SIZE(buf), L"%u", i);
        if(!(name = SysAllocString(buf)))
            hres = E_OUTOFMEMORY;
        else {
            if(dispex)
                hres = IDispatchEx_GetDispID(dispex, name, fdexNameCaseSensitive, &dispid);
            else
                hres = IDispatch_GetIDsOfNames(disp, &IID_NULL, &name, 1, 0, &dispid);
            SysFreeString(name);
        }
        if(SUCCEEDED(hres)) {
            if(dispex)
                hres = IDispatchEx_InvokeEx(dispex, dispid, ctx->lcid, DISPATCH_PROPERTYGET, NULL,
                                            &var, &ei, &ctx->jscaller->IServiceProvider_iface);
            else
                hres = IDispatch_Invoke(disp, dispid, &IID_NULL, ctx->lcid, DISPATCH_PROPERTYGET, NULL, &var, &ei, &err);
            if(SUCCEEDED(hres)) {
                hres = variant_to_jsval(ctx, &var, &argv[i]);
                VariantClear(&var);
            }else if(hres == DISP_E_EXCEPTION) {
                disp_fill_exception(ctx, &ei);
            }
        }
        if(FAILED(hres)) {
            if(hres == DISP_E_UNKNOWNNAME || hres == DISP_E_MEMBERNOTFOUND) {
                argv[i] = jsval_undefined();
                continue;
            }
            while(i--)
                jsval_release(argv[i]);
            heap_free(argv);
            goto fail;
        }
    }

    *argc = length;
    *ret = argv;
    hres = S_OK;
fail:
    if(dispex)
        IDispatchEx_Release(dispex);
    return hres;
}

static HRESULT Function_apply(script_ctx_t *ctx, jsval_t vthis, WORD flags, unsigned argc, jsval_t *argv, jsval_t *r)
{
    jsval_t this_val = jsval_undefined();
    FunctionInstance *function;
    jsval_t *args = NULL;
    unsigned i, cnt = 0;
    HRESULT hres = S_OK;

    TRACE("\n");

    if(is_null_disp(vthis))
        return JS_E_OBJECT_REQUIRED;
    if(!is_object_instance(vthis) || (!(function = function_this(vthis)) && to_jsdisp(get_object(vthis))))
        return JS_E_FUNCTION_EXPECTED;

    if(argc) {
        if(ctx->version < SCRIPTLANGUAGEVERSION_ES5 && !is_undefined(argv[0]) && !is_null(argv[0])) {
            IDispatch *this_obj;
            hres = to_object(ctx, argv[0], &this_obj);
            if(FAILED(hres))
                return hres;
            this_val = jsval_disp(this_obj);
        }else {
            hres = jsval_copy(argv[0], &this_val);
            if(FAILED(hres))
                return hres;
        }
    }

    if(argc >= 2) {
        jsdisp_t *arg_array = NULL;
        IDispatch *obj = NULL;

        if(is_object_instance(argv[1])) {
            obj = get_object(argv[1]);
            arg_array = iface_to_jsdisp(obj);

            if(ctx->version < SCRIPTLANGUAGEVERSION_ES5) {
                if(!arg_array) {
                    if(!ctx->html_mode)
                        obj = NULL;
                }else if(!is_class(arg_array, JSCLASS_ARRAY) && !is_class(arg_array, JSCLASS_ARGUMENTS)) {
                    jsdisp_release(arg_array);
                    arg_array = NULL;
                    obj = NULL;
                }
            }
        }

        if(arg_array) {
            hres = array_to_args(ctx, arg_array, &cnt, &args);
            jsdisp_release(arg_array);
        }else if(obj) {
            hres = disp_to_args(ctx, obj, &cnt, &args);
        }else {
            hres = ctx->html_mode ? JS_E_ARRAY_OR_ARGS_EXPECTED : JS_E_JSCRIPT_EXPECTED;
        }
    }

    if(SUCCEEDED(hres)) {
        if(function) {
            hres = function->vtbl->call(ctx, function, this_val, flags, cnt, args, r);
        }else {
            jsval_t res;
            hres = disp_call_value(ctx, get_object(vthis), is_object_instance(this_val) ? get_object(this_val) : NULL,
                                   DISPATCH_METHOD, cnt, args, &res);
            if(SUCCEEDED(hres)) {
                if(r)
                    *r = res;
                else
                    jsval_release(res);
            }
        }
    }

    jsval_release(this_val);
    for(i=0; i < cnt; i++)
        jsval_release(args[i]);
    heap_free(args);
    return hres;
}

static HRESULT Function_call(script_ctx_t *ctx, jsval_t vthis, WORD flags, unsigned argc, jsval_t *argv,
        jsval_t *r)
{
    jsval_t this_val = jsval_undefined();
    FunctionInstance *function;
    unsigned cnt = 0;
    HRESULT hres;

    TRACE("\n");

    if(is_null_disp(vthis))
        return JS_E_OBJECT_REQUIRED;
    if(!(function = function_this(vthis)))
        return JS_E_FUNCTION_EXPECTED;

    if(argc) {
        if(ctx->version < SCRIPTLANGUAGEVERSION_ES5 && !is_undefined(argv[0]) && !is_null(argv[0])) {
            IDispatch *this_obj;
            hres = to_object(ctx, argv[0], &this_obj);
            if(FAILED(hres))
                return hres;
            this_val = jsval_disp(this_obj);
        }else {
            hres = jsval_copy(argv[0], &this_val);
            if(FAILED(hres))
                return hres;
        }
        cnt = argc-1;
    }

    hres = function->vtbl->call(ctx, function, this_val, flags, cnt, argv + 1, r);

    jsval_release(this_val);
    return hres;
}

static HRESULT Function_bind(script_ctx_t *ctx, jsval_t vthis, WORD flags, unsigned argc, jsval_t *argv,
        jsval_t *r)
{
    IDispatch *bound_this = NULL;
    FunctionInstance *function;
    jsdisp_t *new_function;
    HRESULT hres;

    TRACE("\n");

    if(!(function = function_this(vthis)))
        return JS_E_FUNCTION_EXPECTED;

    if(argc < 1) {
        argc = 1;
    }else if(!is_undefined(argv[0]) && !is_null(argv[0])) {
        hres = to_object(ctx, argv[0], &bound_this);
        if(FAILED(hres))
            return hres;
    }

    hres = create_bind_function(ctx, function, bound_this, argc - 1, argv + 1, &new_function);
    if(bound_this)
        IDispatch_Release(bound_this);
    if(FAILED(hres))
        return hres;

    if(r)
        *r = jsval_obj(new_function);
    else
        jsdisp_release(new_function);
    return S_OK;
}

HRESULT Function_value(script_ctx_t *ctx, jsval_t vthis, WORD flags, unsigned argc, jsval_t *argv,
        jsval_t *r)
{
    FunctionInstance *function;

    TRACE("\n");

    if(!(function = function_this(vthis))) {
        ERR("dispex is not a function\n");
        return E_FAIL;
    }

    return function->vtbl->call(ctx, function, vthis, flags, argc, argv, r);
}

HRESULT Function_get_value(script_ctx_t *ctx, jsdisp_t *jsthis, jsval_t *r)
{
    FunctionInstance *function = function_from_jsdisp(jsthis);
    jsstr_t *str;
    HRESULT hres;

    TRACE("\n");

    hres = function->vtbl->toString(function, &str);
    if(FAILED(hres))
        return hres;

    *r = jsval_string(str);
    return S_OK;
}

static HRESULT Function_get_arguments(script_ctx_t *ctx, jsdisp_t *jsthis, jsval_t *r)
{
    FunctionInstance *function = function_from_jsdisp(jsthis);
    call_frame_t *frame;
    HRESULT hres;

    TRACE("\n");

    for(frame = ctx->call_ctx; frame; frame = frame->prev_frame) {
        if(frame->function_instance == &function->dispex) {
            if(!frame->arguments_obj) {
                hres = setup_arguments_object(ctx, frame);
                if(FAILED(hres))
                    return hres;
            }
            *r = jsval_obj(jsdisp_addref(frame->arguments_obj));
            return S_OK;
        }
    }

    *r = jsval_null();
    return S_OK;
}

function_code_t *Function_get_code(jsdisp_t *jsthis)
{
    FunctionInstance *function;

    assert(is_class(jsthis, JSCLASS_FUNCTION));
    function = function_from_jsdisp(jsthis);

    return function->vtbl->get_code(function);
}

static void Function_destructor(jsdisp_t *dispex)
{
    FunctionInstance *function = function_from_jsdisp(dispex);
    function->vtbl->destructor(function);
    heap_free(function);
}

static HRESULT Function_gc_traverse(jsdisp_t *dispex, void *arg)
{
    FunctionInstance *function = function_from_jsdisp(dispex);
    return function->vtbl->gc_traverse(function, arg);
}

static const builtin_prop_t Function_props[] = {
    {L"apply",               Function_apply,                 PROPF_METHOD|2},
    {L"arguments",           NULL, 0,                        Function_get_arguments},
    {L"bind",                Function_bind,                  PROPF_METHOD|PROPF_ES5|1},
    {L"call",                Function_call,                  PROPF_METHOD|1},
    {L"length",              NULL, 0,                        Function_get_length},
    {L"toString",            Function_toString,              PROPF_METHOD}
};

static const builtin_info_t Function_info = {
    JSCLASS_FUNCTION,
    Function_value,
    ARRAY_SIZE(Function_props),
    Function_props,
    Function_destructor,
    NULL,
    NULL,
    NULL,
    NULL,
    Function_gc_traverse
};

static const builtin_prop_t FunctionInst_props[] = {
    {L"arguments",           NULL, 0,                        Function_get_arguments},
    {L"length",              NULL, 0,                        Function_get_length}
};

static const builtin_info_t FunctionInst_info = {
    JSCLASS_FUNCTION,
    Function_value,
    ARRAY_SIZE(FunctionInst_props),
    FunctionInst_props,
    Function_destructor,
    NULL,
    NULL,
    NULL,
    NULL,
    Function_gc_traverse
};

static HRESULT create_function(script_ctx_t *ctx, const builtin_info_t *builtin_info, const function_vtbl_t *vtbl, size_t size,
        DWORD flags, BOOL funcprot, jsdisp_t *prototype, void **ret)
{
    FunctionInstance *function;
    HRESULT hres;

    function = heap_alloc_zero(size);
    if(!function)
        return E_OUTOFMEMORY;

    if(funcprot)
        hres = init_dispex(&function->dispex, ctx, builtin_info, prototype);
    else if(builtin_info)
        hres = init_dispex_from_constr(&function->dispex, ctx, builtin_info, ctx->function_constr);
    else
        hres = init_dispex_from_constr(&function->dispex, ctx, &FunctionInst_info, ctx->function_constr);
    if(FAILED(hres)) {
        heap_free(function);
        return hres;
    }

    function->vtbl = vtbl;
    function->flags = flags;
    function->length = flags & PROPF_ARGMASK;

    *ret = function;
    return S_OK;
}

static HRESULT NativeFunction_call(script_ctx_t *ctx, FunctionInstance *func, jsval_t vthis, unsigned flags,
        unsigned argc, jsval_t *argv, jsval_t *r)
{
    NativeFunction *function = (NativeFunction*)func;

    return function->proc(ctx, vthis, flags & ~DISPATCH_JSCRIPT_INTERNAL_MASK, argc, argv, r);
}

static HRESULT native_code_toString(const WCHAR *name, jsstr_t **ret)
{
    DWORD name_len;
    jsstr_t *str;
    WCHAR *ptr;

    static const WCHAR native_prefixW[] = L"\nfunction ";
    static const WCHAR native_suffixW[] = L"() {\n    [native code]\n}\n";

    name_len = name ? lstrlenW(name) : 0;
    str = jsstr_alloc_buf(ARRAY_SIZE(native_prefixW) + ARRAY_SIZE(native_suffixW) + name_len - 2, &ptr);
    if(!str)
        return E_OUTOFMEMORY;

    memcpy(ptr, native_prefixW, sizeof(native_prefixW));
    ptr += ARRAY_SIZE(native_prefixW) - 1;
    memcpy(ptr, name, name_len*sizeof(WCHAR));
    ptr += name_len;
    memcpy(ptr, native_suffixW, sizeof(native_suffixW));

    *ret = str;
    return S_OK;
}

static HRESULT NativeFunction_toString(FunctionInstance *func, jsstr_t **ret)
{
    NativeFunction *function = (NativeFunction*)func;
    return native_code_toString(function->name, ret);
}

static function_code_t *NativeFunction_get_code(FunctionInstance *function)
{
    return NULL;
}

static void NativeFunction_destructor(FunctionInstance *function)
{
}

static const function_vtbl_t NativeFunctionVtbl = {
    NativeFunction_call,
    NativeFunction_toString,
    NativeFunction_get_code,
    NativeFunction_destructor,
    no_gc_traverse
};

HRESULT create_builtin_function(script_ctx_t *ctx, builtin_invoke_t value_proc, const WCHAR *name,
        const builtin_info_t *builtin_info, DWORD flags, jsdisp_t *prototype, jsdisp_t **ret)
{
    NativeFunction *function;
    HRESULT hres;

    hres = create_function(ctx, builtin_info, &NativeFunctionVtbl, sizeof(NativeFunction), flags, FALSE, NULL, (void**)&function);
    if(FAILED(hres))
        return hres;

    if(builtin_info)
        hres = jsdisp_define_data_property(&function->function.dispex, L"length", 0,
                                           jsval_number(function->function.length));
    if(SUCCEEDED(hres))
        hres = jsdisp_define_data_property(&function->function.dispex, L"prototype", 0, prototype ? jsval_obj(prototype) : jsval_null());
    if(FAILED(hres)) {
        jsdisp_release(&function->function.dispex);
        return hres;
    }

    function->proc = value_proc;
    function->name = name;

    *ret = &function->function.dispex;
    return S_OK;
}

static HRESULT set_constructor_prop(script_ctx_t *ctx, jsdisp_t *constr, jsdisp_t *prot)
{
    return jsdisp_define_data_property(prot, L"constructor", PROPF_WRITABLE | PROPF_CONFIGURABLE,
                                       jsval_obj(constr));
}

HRESULT create_builtin_constructor(script_ctx_t *ctx, builtin_invoke_t value_proc, const WCHAR *name,
        const builtin_info_t *builtin_info, DWORD flags, jsdisp_t *prototype, jsdisp_t **ret)
{
    jsdisp_t *constr;
    HRESULT hres;

    hres = create_builtin_function(ctx, value_proc, name, builtin_info, flags, prototype, &constr);
    if(FAILED(hres))
        return hres;

    hres = set_constructor_prop(ctx, constr, prototype);
    if(FAILED(hres)) {
        jsdisp_release(constr);
        return hres;
    }

    *ret = constr;
    return S_OK;
}

static HRESULT ProxyFunction_call(script_ctx_t *ctx, FunctionInstance *func, jsval_t vthis, unsigned flags,
        unsigned argc, jsval_t *argv, jsval_t *r)
{
    ProxyFunction *function = (ProxyFunction*)func;
    IDispatch *this_obj, *converted = NULL;
    DISPPARAMS dp = { 0 };
    EXCEPINFO ei = { 0 };
    VARIANT buf[6], ret;
    jsdisp_t *jsdisp;
    HRESULT hres;
    unsigned i;

    if(flags & DISPATCH_CONSTRUCT)
        return E_UNEXPECTED;

    if(argc > function->function.length)
        argc = function->function.length;
    dp.cArgs = argc;

    if(argc <= ARRAY_SIZE(buf))
        dp.rgvarg = buf;
    else if(!(dp.rgvarg = heap_alloc(argc * sizeof(*dp.rgvarg))))
        return E_OUTOFMEMORY;

    for(i = 0; i < argc; i++) {
        hres = jsval_to_variant(argv[i], &dp.rgvarg[argc - i - 1]);
        if(FAILED(hres))
            goto cleanup;
    }

    if(is_undefined(vthis) || is_null(vthis))
        this_obj = lookup_global_host(ctx);
    else {
        hres = to_object(ctx, vthis, &converted);
        if(FAILED(hres))
            goto cleanup;
        this_obj = converted;
    }

    jsdisp = to_jsdisp(this_obj);
    if(jsdisp && jsdisp->proxy)
        this_obj = (IDispatch*)jsdisp->proxy;

    V_VT(&ret) = VT_EMPTY;
    hres = function->func.invoke(this_obj, function->func.context, &dp, r ? &ret : NULL, &ei, &ctx->jscaller->IServiceProvider_iface);
    if(converted)
        IDispatch_Release(converted);

    if(hres == DISP_E_EXCEPTION)
        disp_fill_exception(ctx, &ei);
    else if(SUCCEEDED(hres) && r) {
        hres = variant_to_jsval(ctx, &ret, r);
        VariantClear(&ret);
    }

cleanup:
    while(i)
        VariantClear(&dp.rgvarg[argc - i--]);
    if(dp.rgvarg != buf)
        heap_free(dp.rgvarg);
    return hres;
}

static HRESULT ProxyFunction_toString(FunctionInstance *func, jsstr_t **ret)
{
    ProxyFunction *function = (ProxyFunction*)func;
    return native_code_toString(function->func.name, ret);
}

static function_code_t *ProxyFunction_get_code(FunctionInstance *func)
{
    return NULL;
}

static void ProxyFunction_destructor(FunctionInstance *func)
{
}

static const function_vtbl_t ProxyFunctionVtbl = {
    ProxyFunction_call,
    ProxyFunction_toString,
    ProxyFunction_get_code,
    ProxyFunction_destructor,
    no_gc_traverse
};

HRESULT create_proxy_function(jsdisp_t *jsdisp, DISPID id, DWORD flags, jsdisp_t **ret)
{
    struct proxy_func_invoker func;
    ProxyFunction *function;
    HRESULT hres;

    hres = jsdisp->proxy->lpVtbl->FuncInfo(jsdisp->proxy, id, &func);
    if(FAILED(hres))
        return hres;

    hres = create_function(jsdisp->ctx, NULL, &ProxyFunctionVtbl, sizeof(ProxyFunction), flags, FALSE, NULL, (void**)&function);
    if(FAILED(hres))
        return hres;

    function->func = func;
    *ret = &function->function.dispex;
    return S_OK;
}

HRESULT create_proxy_accessor(jsdisp_t *jsdisp, DISPID id, property_desc_t *desc)
{
    struct proxy_func_invoker func[2];
    ProxyFunction *function;
    HRESULT hres;

    hres = jsdisp->proxy->lpVtbl->AccessorInfo(jsdisp->proxy, id, &func[0]);
    if(FAILED(hres))
        return hres;
    desc->getter = desc->setter = NULL;

    if(func[0].invoke) {
        hres = create_function(jsdisp->ctx, NULL, &ProxyFunctionVtbl, sizeof(ProxyFunction), PROPF_METHOD, FALSE, NULL, (void**)&function);
        if(FAILED(hres))
            return hres;
        function->func = func[0];
        desc->getter = &function->function.dispex;
    }

    if(func[1].invoke) {
        hres = create_function(jsdisp->ctx, NULL, &ProxyFunctionVtbl, sizeof(ProxyFunction), PROPF_METHOD|1, FALSE, NULL, (void**)&function);
        if(FAILED(hres)) {
            if(desc->getter)
                jsdisp_release(desc->getter);
            return hres;
        }
        function->func = func[1];
        desc->setter = &function->function.dispex;
    }

    return S_OK;
}

static HRESULT ProxyConstructor_call(script_ctx_t *ctx, FunctionInstance *func, jsval_t vthis, unsigned flags,
        unsigned argc, jsval_t *argv, jsval_t *r)
{
    ProxyConstructor *constructor = (ProxyConstructor*)func;

    if(constructor->name == ProxyConstructor_createW) {
        /* only allow calls since it's a method */
        if(!(flags & DISPATCH_METHOD))
            return E_UNEXPECTED;
    }

    return disp_call_value(ctx, constructor->disp, NULL, flags & ~DISPATCH_JSCRIPT_INTERNAL_MASK, argc, argv, r);
}

static HRESULT ProxyConstructor_toString(FunctionInstance *func, jsstr_t **ret)
{
    ProxyConstructor *constructor = (ProxyConstructor*)func;
    return native_code_toString(constructor->name, ret);
}

static function_code_t *ProxyConstructor_get_code(FunctionInstance *func)
{
    return NULL;
}

static void ProxyConstructor_destructor(FunctionInstance *func)
{
    ProxyConstructor *constructor = (ProxyConstructor*)func;
    IDispatch_Release(constructor->disp);
}

static const function_vtbl_t ProxyConstructorVtbl = {
    ProxyConstructor_call,
    ProxyConstructor_toString,
    ProxyConstructor_get_code,
    ProxyConstructor_destructor,
    no_gc_traverse
};

static const builtin_prop_t ProxyConstructor_props[] = {
    {L"arguments",           NULL, 0,                        Function_get_arguments}
};

static const builtin_info_t ProxyConstructor_info = {
    JSCLASS_FUNCTION,
    Function_value,
    ARRAY_SIZE(ProxyConstructor_props),
    ProxyConstructor_props,
    Function_destructor,
    NULL
};

HRESULT create_proxy_constructor(DISPID id, const WCHAR *name, jsdisp_t *prototype, jsdisp_t **ret)
{
    IWineDispatchProxyPrivate *proxy = prototype->ctx->global->prototype->proxy;
    script_ctx_t *ctx = prototype->ctx;
    ProxyConstructor *constructor;
    DISPPARAMS dp = { 0 };
    EXCEPINFO ei = { 0 };
    HRESULT hres;
    VARIANT var;

    hres = proxy->lpVtbl->PropInvoke(proxy, (IDispatch*)ctx->global->proxy, id, ctx->lcid, DISPATCH_PROPERTYGET,
                                     &dp, &var, &ei, &ctx->jscaller->IServiceProvider_iface);
    if(hres == DISP_E_EXCEPTION)
        disp_fill_exception(ctx, &ei);
    if(FAILED(hres))
        return hres;
    if(V_VT(&var) != VT_DISPATCH) {
        VariantClear(&var);
        return E_UNEXPECTED;
    }

    /* create wrapper constructor function over the disp's value */
    hres = create_function(ctx, &ProxyConstructor_info, &ProxyConstructorVtbl, sizeof(ProxyConstructor),
                           PROPF_CONSTR, FALSE, NULL, (void**)&constructor);
    if(FAILED(hres)) {
        IDispatch_Release(V_DISPATCH(&var));
        return hres;
    }

    constructor->disp = V_DISPATCH(&var);
    constructor->name = name;

    hres = jsdisp_define_data_property(&constructor->function.dispex, L"prototype", 0, jsval_obj(prototype));

    if(SUCCEEDED(hres) && id == DISPID_IHTMLWINDOW5_XMLHTTPREQUEST) {
        ProxyConstructor *create;

        hres = create_function(ctx, &ProxyConstructor_info, &ProxyConstructorVtbl, sizeof(ProxyConstructor),
                               PROPF_METHOD, FALSE, NULL, (void**)&create);
        if(SUCCEEDED(hres)) {
            IDispatch_AddRef(V_DISPATCH(&var));
            create->disp = V_DISPATCH(&var);
            create->name = ProxyConstructor_createW;

            hres = jsdisp_define_data_property(&create->function.dispex, L"prototype", 0, jsval_null());
            if(SUCCEEDED(hres))
                hres = jsdisp_define_data_property(&constructor->function.dispex, ProxyConstructor_createW,
                                                   0, jsval_obj(&create->function.dispex));
            jsdisp_release(&create->function.dispex);
        }
    }
    if(FAILED(hres)) {
        jsdisp_release(&constructor->function.dispex);
        return hres;
    }

    *ret = &constructor->function.dispex;
    return S_OK;
}

/*
 * Create the actual prototype on demand, since it is a circular ref, which prevents the vast
 * majority of functions from being released quickly, leading to unnecessary scope detach.
 */
static HRESULT InterpretedFunction_get_prototype(script_ctx_t *ctx, jsdisp_t *jsthis, jsval_t *r)
{
    jsdisp_t *prototype;
    HRESULT hres;

    hres = create_object(ctx, NULL, &prototype);
    if(FAILED(hres))
        return hres;

    hres = jsdisp_define_data_property(jsthis, L"prototype", PROPF_WRITABLE, jsval_obj(prototype));
    if(SUCCEEDED(hres))
        hres = set_constructor_prop(ctx, jsthis, prototype);
    if(FAILED(hres)) {
        jsdisp_release(prototype);
        return hres;
    }

    *r = jsval_obj(prototype);
    return S_OK;
}

static HRESULT InterpretedFunction_set_prototype(script_ctx_t *ctx, jsdisp_t *jsthis, jsval_t value)
{
    return jsdisp_define_data_property(jsthis, L"prototype", PROPF_WRITABLE, value);
}

static const builtin_prop_t InterpretedFunction_props[] = {
    {L"arguments",           NULL, 0,                        Function_get_arguments},
    {L"length",              NULL, 0,                        Function_get_length},
    {L"prototype",           NULL, 0,                        InterpretedFunction_get_prototype, InterpretedFunction_set_prototype}
};

static const builtin_info_t InterpretedFunction_info = {
    JSCLASS_FUNCTION,
    Function_value,
    ARRAY_SIZE(InterpretedFunction_props),
    InterpretedFunction_props,
    Function_destructor,
    NULL,
    NULL,
    NULL,
    NULL,
    Function_gc_traverse
};

static HRESULT InterpretedFunction_call(script_ctx_t *ctx, FunctionInstance *func, jsval_t vthis, unsigned flags,
         unsigned argc, jsval_t *argv, jsval_t *r)
{
    InterpretedFunction *function = (InterpretedFunction*)func;
    IDispatch *this_obj = NULL;
    DWORD exec_flags = 0;
    jsdisp_t *new_obj;
    HRESULT hres;

    TRACE("%p\n", function);

    if(ctx->state == SCRIPTSTATE_UNINITIALIZED || ctx->state == SCRIPTSTATE_CLOSED) {
        WARN("Script engine state does not allow running code.\n");
        return E_UNEXPECTED;
    }

    if(flags & DISPATCH_CONSTRUCT) {
        hres = create_object(ctx, &function->function.dispex, &new_obj);
        if(FAILED(hres))
            return hres;
        this_obj = to_disp(new_obj);
    }else if(is_object_instance(vthis)) {
        IDispatch_AddRef(get_object(vthis));
        hres = convert_to_proxy(ctx, &vthis);
        if(FAILED(hres))
            return hres;
        this_obj = get_object(vthis);
    }else if(ctx->version >= SCRIPTLANGUAGEVERSION_ES5 && !is_undefined(vthis) && !is_null(vthis)) {
        hres = to_object(ctx, vthis, &this_obj);
        if(FAILED(hres))
            return hres;
    }

    if(flags & DISPATCH_JSCRIPT_CALLEREXECSSOURCE)
        exec_flags |= EXEC_RETURN_TO_INTERP;
    if(flags & DISPATCH_CONSTRUCT)
        exec_flags |= EXEC_CONSTRUCTOR;
    hres = exec_source(ctx, exec_flags, function->code, function->func_code, function->scope_chain, this_obj,
                       &function->function.dispex, argc, argv, r);
    if(this_obj)
        IDispatch_Release(this_obj);
    return hres;
}

static HRESULT InterpretedFunction_toString(FunctionInstance *func, jsstr_t **ret)
{
    InterpretedFunction *function = (InterpretedFunction*)func;

    *ret = jsstr_alloc_len(function->func_code->source, function->func_code->source_len);
    return *ret ? S_OK : E_OUTOFMEMORY;
}

static function_code_t *InterpretedFunction_get_code(FunctionInstance *func)
{
    InterpretedFunction *function = (InterpretedFunction*)func;

    return function->func_code;
}

static void InterpretedFunction_destructor(FunctionInstance *func)
{
    InterpretedFunction *function = (InterpretedFunction*)func;

    if(function->code)
        release_bytecode(function->code);
    if(function->scope_chain)
        scope_release(function->scope_chain);
}

static HRESULT InterpretedFunction_gc_traverse(FunctionInstance *func, void *arg)
{
    InterpretedFunction *function = (InterpretedFunction*)func;

    /* Make sure to unlink everything during unlink, including deeper scopes, so
       that releasing the function itself won't release any additional objects. */
    if(arg == GC_TRAVERSE_UNLINK) {
        release_bytecode(function->code);
        function->code = NULL;
        if(function->scope_chain) {
            scope_release(function->scope_chain);
            function->scope_chain = NULL;
        }
        return S_OK;
    }

    /* FIXME: Process the code->named_item->script_obj and scope_chain->obj somehow. The issue
       is that the function keeps a ref to the bytecode or scope itself, not the actual obj...
       For now, this will possibly leak. */
    return S_OK;
}

static const function_vtbl_t InterpretedFunctionVtbl = {
    InterpretedFunction_call,
    InterpretedFunction_toString,
    InterpretedFunction_get_code,
    InterpretedFunction_destructor,
    InterpretedFunction_gc_traverse
};

HRESULT create_source_function(script_ctx_t *ctx, bytecode_t *code, function_code_t *func_code,
        scope_chain_t *scope_chain, jsdisp_t **ret)
{
    InterpretedFunction *function;
    HRESULT hres;

    hres = create_function(ctx, &InterpretedFunction_info, &InterpretedFunctionVtbl, sizeof(InterpretedFunction),
                           PROPF_CONSTR, FALSE, NULL, (void**)&function);
    if(FAILED(hres))
        return hres;

    if(scope_chain) {
        scope_addref(scope_chain);
        function->scope_chain = scope_chain;
    }

    bytecode_addref(code);
    function->code = code;
    function->func_code = func_code;
    function->function.length = function->func_code->param_cnt;

    *ret = &function->function.dispex;
    return S_OK;
}

static HRESULT BindFunction_call(script_ctx_t *ctx, FunctionInstance *func, jsval_t vthis, unsigned flags,
         unsigned argc, jsval_t *argv, jsval_t *r)
{
    BindFunction *function = (BindFunction*)func;
    jsval_t *call_args = NULL;
    unsigned call_argc;
    HRESULT hres;

    TRACE("%p\n", function);

    call_argc = function->argc + argc;
    if(call_argc) {
        call_args = heap_alloc(call_argc * sizeof(*call_args));
        if(!call_args)
            return E_OUTOFMEMORY;

        if(function->argc)
            memcpy(call_args, function->args, function->argc * sizeof(*call_args));
        if(argc)
            memcpy(call_args + function->argc, argv, argc * sizeof(*call_args));
    }

    hres = function->target->vtbl->call(ctx, function->target, function->this ? jsval_disp(function->this) : jsval_null(),
                                        flags, call_argc, call_args, r);

    heap_free(call_args);
    return hres;
}

static HRESULT BindFunction_toString(FunctionInstance *function, jsstr_t **ret)
{
    *ret = jsstr_alloc(L"\nfunction() {\n    [native code]\n}\n");
    return *ret ? S_OK : E_OUTOFMEMORY;
}

static function_code_t *BindFunction_get_code(FunctionInstance *function)
{
    return NULL;
}

static void BindFunction_destructor(FunctionInstance *func)
{
    BindFunction *function = (BindFunction*)func;
    unsigned i;

    TRACE("%p\n", function);

    for(i = 0; i < function->argc; i++)
        jsval_release(function->args[i]);
    if(function->target)
        jsdisp_release(&function->target->dispex);
    if(function->this)
        IDispatch_Release(function->this);
}

static HRESULT BindFunction_gc_traverse(FunctionInstance *func, void *arg)
{
    BindFunction *function = (BindFunction*)func;
    HRESULT hres;
    unsigned i;

    for(i = 0; i < function->argc; i++) {
        hres = gc_process_linked_val(&function->function.dispex, &function->args[i], arg);
        if(FAILED(hres))
            return hres;
    }

    hres = gc_process_linked_obj(&function->function.dispex, &function->target->dispex, (void**)&function->target, arg);
    if(FAILED(hres))
        return hres;

    return function->this ? gc_process_linked_disp(&function->function.dispex, &function->this, arg) : S_OK;
}

static const function_vtbl_t BindFunctionVtbl = {
    BindFunction_call,
    BindFunction_toString,
    BindFunction_get_code,
    BindFunction_destructor,
    BindFunction_gc_traverse
};

static HRESULT create_bind_function(script_ctx_t *ctx, FunctionInstance *target, IDispatch *bound_this, unsigned argc,
                                    jsval_t *argv, jsdisp_t **ret)
{
    BindFunction *function;
    HRESULT hres;

    hres = create_function(ctx, NULL, &BindFunctionVtbl, FIELD_OFFSET(BindFunction, args[argc]), PROPF_METHOD,
                           FALSE, NULL, (void**)&function);
    if(FAILED(hres))
        return hres;

    jsdisp_addref(&target->dispex);
    function->target = target;

    if(bound_this)
        IDispatch_AddRef(function->this = bound_this);

    for(function->argc = 0; function->argc < argc; function->argc++) {
        hres = jsval_copy(argv[function->argc], function->args + function->argc);
        if(FAILED(hres)) {
            jsdisp_release(&function->function.dispex);
            return hres;
        }
    }

    function->function.length = target->length > argc ? target->length - argc : 0;

    *ret = &function->function.dispex;
    return S_OK;
}

static HRESULT construct_function(script_ctx_t *ctx, unsigned argc, jsval_t *argv, IDispatch **ret)
{
    WCHAR *str = NULL, *ptr;
    unsigned len = 0, i = 0;
    bytecode_t *code;
    jsdisp_t *function;
    jsstr_t **params = NULL;
    int j = 0;
    HRESULT hres = S_OK;

    static const WCHAR function_anonymousW[] = L"function anonymous(";
    static const WCHAR function_beginW[] = L") {\n";
    static const WCHAR function_endW[] = L"\n}";

    if(argc) {
        params = heap_alloc(argc*sizeof(*params));
        if(!params)
            return E_OUTOFMEMORY;

        if(argc > 2)
            len = (argc-2)*2; /* separating commas */
        for(i=0; i < argc; i++) {
            hres = to_string(ctx, argv[i], params+i);
            if(FAILED(hres))
                break;
            len += jsstr_length(params[i]);
        }
    }

    if(SUCCEEDED(hres)) {
        len += ARRAY_SIZE(function_anonymousW) + ARRAY_SIZE(function_beginW) + ARRAY_SIZE(function_endW) - 2;
        str = heap_alloc(len*sizeof(WCHAR));
        if(str) {
            memcpy(str, function_anonymousW, sizeof(function_anonymousW));
            ptr = str + ARRAY_SIZE(function_anonymousW) - 1;
            if(argc > 1) {
                while(1) {
                    ptr += jsstr_flush(params[j], ptr);
                    if(++j == argc-1)
                        break;
                    *ptr++ = ',';
                    *ptr++ = ' ';
                }
            }
            memcpy(ptr, function_beginW, sizeof(function_beginW));
            ptr += ARRAY_SIZE(function_beginW) - 1;
            if(argc)
                ptr += jsstr_flush(params[argc-1], ptr);
            memcpy(ptr, function_endW, sizeof(function_endW));

            TRACE("%s\n", debugstr_w(str));
        }else {
            hres = E_OUTOFMEMORY;
        }
    }

    while(i)
        jsstr_release(params[--i]);
    heap_free(params);
    if(FAILED(hres))
        return hres;

    hres = compile_script(ctx, str, 0, 0, NULL, NULL, FALSE, FALSE,
                          ctx->call_ctx ? ctx->call_ctx->bytecode->named_item : NULL, &code);
    heap_free(str);
    if(FAILED(hres))
        return hres;

    if(code->global_code.func_cnt != 1 || code->global_code.var_cnt != 1) {
        ERR("Invalid parser result!\n");
        release_bytecode(code);
        return E_UNEXPECTED;
    }

    hres = create_source_function(ctx, code, code->global_code.funcs, NULL, &function);
    release_bytecode(code);
    if(FAILED(hres))
        return hres;

    *ret = to_disp(function);
    return S_OK;
}

static HRESULT FunctionConstr_value(script_ctx_t *ctx, jsval_t vthis, WORD flags, unsigned argc, jsval_t *argv,
        jsval_t *r)
{
    HRESULT hres;

    TRACE("\n");

    switch(flags) {
    case DISPATCH_METHOD:
    case DISPATCH_CONSTRUCT: {
        IDispatch *ret;

        hres = construct_function(ctx, argc, argv, &ret);
        if(FAILED(hres))
            return hres;

        if(r) *r = jsval_disp(ret);
        else  IDispatch_Release(ret);
        break;
    }
    default:
        FIXME("unimplemented flags %x\n", flags);
        return E_NOTIMPL;
    }

    return S_OK;
}

static HRESULT FunctionProt_value(script_ctx_t *ctx, jsval_t vthis, WORD flags, unsigned argc, jsval_t *argv,
        jsval_t *r)
{
    FIXME("\n");
    return E_NOTIMPL;
}

HRESULT init_function_constr(script_ctx_t *ctx, jsdisp_t *object_prototype)
{
    NativeFunction *prot, *constr;
    HRESULT hres;

    hres = create_function(ctx, &Function_info, &NativeFunctionVtbl, sizeof(NativeFunction), PROPF_CONSTR,
                           TRUE, object_prototype, (void**)&prot);
    if(FAILED(hres))
        return hres;

    prot->proc = FunctionProt_value;
    prot->name = L"prototype";

    hres = create_function(ctx, &FunctionInst_info, &NativeFunctionVtbl, sizeof(NativeFunction), PROPF_CONSTR|1,
                           TRUE, &prot->function.dispex, (void**)&constr);
    if(SUCCEEDED(hres)) {
        constr->proc = FunctionConstr_value;
        constr->name = L"Function";
        hres = jsdisp_define_data_property(&constr->function.dispex, L"prototype", 0, jsval_obj(&prot->function.dispex));
        if(SUCCEEDED(hres))
            hres = set_constructor_prop(ctx, &constr->function.dispex, &prot->function.dispex);
        if(FAILED(hres))
            jsdisp_release(&constr->function.dispex);
    }
    jsdisp_release(&prot->function.dispex);
    if(FAILED(hres))
        return hres;

    ctx->function_constr = &constr->function.dispex;
    return S_OK;
}
