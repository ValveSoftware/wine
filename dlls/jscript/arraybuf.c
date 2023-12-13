/*
 * Copyright 2023 Gabriel Ivăncescu for CodeWeavers
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


#include <limits.h>
#include <assert.h>

#include "jscript.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(jscript);

typedef struct {
    jsdisp_t dispex;
    DWORD size;
    DECLSPEC_ALIGN(sizeof(double)) BYTE buf[];
} ArrayBufferInstance;

typedef struct {
    jsdisp_t dispex;

    jsdisp_t *buffer;
    DWORD offset;
    DWORD size;
} DataViewInstance;

typedef struct {
    jsdisp_t dispex;

    jsdisp_t *buffer;
    DWORD offset;
    DWORD length;
} TypedArrayInstance;

static inline ArrayBufferInstance *arraybuf_from_jsdisp(jsdisp_t *jsdisp)
{
    return CONTAINING_RECORD(jsdisp, ArrayBufferInstance, dispex);
}

static inline DataViewInstance *dataview_from_jsdisp(jsdisp_t *jsdisp)
{
    return CONTAINING_RECORD(jsdisp, DataViewInstance, dispex);
}

static inline TypedArrayInstance *typedarr_from_jsdisp(jsdisp_t *jsdisp)
{
    return CONTAINING_RECORD(jsdisp, TypedArrayInstance, dispex);
}

static inline ArrayBufferInstance *arraybuf_this(jsval_t vthis)
{
    jsdisp_t *jsdisp = is_object_instance(vthis) ? to_jsdisp(get_object(vthis)) : NULL;
    return (jsdisp && is_class(jsdisp, JSCLASS_ARRAYBUFFER)) ? arraybuf_from_jsdisp(jsdisp) : NULL;
}

static HRESULT create_arraybuf(script_ctx_t*,DWORD,jsdisp_t**);

static HRESULT ArrayBuffer_get_byteLength(script_ctx_t *ctx, jsdisp_t *jsthis, jsval_t *r)
{
    TRACE("%p\n", jsthis);

    *r = jsval_number(arraybuf_from_jsdisp(jsthis)->size);
    return S_OK;
}

static HRESULT ArrayBuffer_slice(script_ctx_t *ctx, jsval_t vthis, WORD flags, unsigned argc, jsval_t *argv,
        jsval_t *r)
{
    ArrayBufferInstance *arraybuf;
    DWORD begin = 0, end, size;
    jsdisp_t *obj;
    HRESULT hres;
    double n;

    TRACE("\n");

    if(!(arraybuf = arraybuf_this(vthis)))
        return JS_E_ARRAYBUFFER_EXPECTED;
    end = arraybuf->size;
    if(!r)
        return S_OK;

    if(argc) {
        hres = to_integer(ctx, argv[0], &n);
        if(FAILED(hres))
            return hres;
        if(n < 0.0)
            n += arraybuf->size;
        if(n >= 0.0 && n < arraybuf->size) {
            begin = n;
            if(argc > 1 && !is_undefined(argv[1])) {
                hres = to_integer(ctx, argv[1], &n);
                if(FAILED(hres))
                    return hres;
                if(n < 0.0)
                    n += arraybuf->size;
                if(n >= 0.0) {
                    end = n < arraybuf->size ? n : arraybuf->size;
                    end = end < begin ? begin : end;
                }else
                    end = begin;
            }
        }else
            end = 0;
    }

    size = end - begin;
    hres = create_arraybuf(ctx, size, &obj);
    if(FAILED(hres))
        return hres;
    memcpy(arraybuf_from_jsdisp(obj)->buf, arraybuf->buf + begin, size);

    *r = jsval_obj(obj);
    return S_OK;
}

static const builtin_prop_t ArrayBuffer_props[] = {
    {L"byteLength",            NULL, 0,                    ArrayBuffer_get_byteLength},
    {L"slice",                 ArrayBuffer_slice,          PROPF_METHOD|2},
};

static const builtin_info_t ArrayBuffer_info = {
    JSCLASS_ARRAYBUFFER,
    NULL,
    ARRAY_SIZE(ArrayBuffer_props),
    ArrayBuffer_props,
    NULL,
    NULL
};

static const builtin_prop_t ArrayBufferInst_props[] = {
    {L"byteLength",            NULL, 0,                    ArrayBuffer_get_byteLength},
};

static const builtin_info_t ArrayBufferInst_info = {
    JSCLASS_ARRAYBUFFER,
    NULL,
    ARRAY_SIZE(ArrayBufferInst_props),
    ArrayBufferInst_props,
    NULL,
    NULL
};

static HRESULT create_arraybuf(script_ctx_t *ctx, DWORD size, jsdisp_t **ret)
{
    ArrayBufferInstance *arraybuf;
    HRESULT hres;

    if(!(arraybuf = calloc(1, FIELD_OFFSET(ArrayBufferInstance, buf[size]))))
        return E_OUTOFMEMORY;

    hres = init_dispex_from_constr(&arraybuf->dispex, ctx, &ArrayBufferInst_info, ctx->arraybuf_constr);
    if(FAILED(hres)) {
        free(arraybuf);
        return hres;
    }

    arraybuf->size = size;

    *ret = &arraybuf->dispex;
    return S_OK;
}

static HRESULT ArrayBufferConstr_isView(script_ctx_t *ctx, jsval_t vthis, WORD flags, unsigned argc, jsval_t *argv,
        jsval_t *r)
{
    BOOL ret = FALSE;
    jsdisp_t *obj;

    TRACE("\n");

    if(!r)
        return S_OK;

    if(argc && is_object_instance(argv[0]) && (obj = to_jsdisp(get_object(argv[0]))) &&
       obj->builtin_info->class >= FIRST_VIEW_JSCLASS && obj->builtin_info->class <= LAST_VIEW_JSCLASS)
        ret = TRUE;

    *r = jsval_bool(ret);
    return S_OK;
}

static HRESULT ArrayBufferConstr_value(script_ctx_t *ctx, jsval_t vthis, WORD flags, unsigned argc, jsval_t *argv,
        jsval_t *r)
{
    DWORD size = 0;
    jsdisp_t *obj;
    HRESULT hres;

    TRACE("\n");

    switch(flags) {
    case DISPATCH_METHOD:
    case DISPATCH_CONSTRUCT: {
        if(argc) {
            double n;
            hres = to_integer(ctx, argv[0], &n);
            if(FAILED(hres))
                return hres;
            if(n < 0.0)
                return JS_E_INVALID_LENGTH;
            if(n > (UINT_MAX - FIELD_OFFSET(ArrayBufferInstance, buf[0])))
                return E_OUTOFMEMORY;
            size = n;
        }

        if(r) {
            hres = create_arraybuf(ctx, size, &obj);
            if(FAILED(hres))
                return hres;
            *r = jsval_obj(obj);
        }
        break;
    }
    default:
        FIXME("unimplemented flags: %x\n", flags);
        return E_NOTIMPL;
    }

    return S_OK;
}

static const builtin_prop_t ArrayBufferConstr_props[] = {
    {L"isView",                ArrayBufferConstr_isView,   PROPF_METHOD|1},
};

static const builtin_info_t ArrayBufferConstr_info = {
    JSCLASS_FUNCTION,
    Function_value,
    ARRAY_SIZE(ArrayBufferConstr_props),
    ArrayBufferConstr_props,
    NULL,
    NULL
};

static inline DataViewInstance *dataview_this(jsval_t vthis)
{
    jsdisp_t *jsdisp = is_object_instance(vthis) ? to_jsdisp(get_object(vthis)) : NULL;
    return (jsdisp && is_class(jsdisp, JSCLASS_DATAVIEW)) ? dataview_from_jsdisp(jsdisp) : NULL;
}

static HRESULT DataView_get_buffer(script_ctx_t *ctx, jsval_t vthis, WORD flags, unsigned argc, jsval_t *argv,
        jsval_t *r)
{
    DataViewInstance *view;

    TRACE("\n");

    if(!(view = dataview_this(vthis)))
        return JS_E_NOT_DATAVIEW;
    if(r) *r = jsval_obj(jsdisp_addref(view->buffer));
    return S_OK;
}

static HRESULT DataView_get_byteLength(script_ctx_t *ctx, jsval_t vthis, WORD flags, unsigned argc, jsval_t *argv,
        jsval_t *r)
{
    DataViewInstance *view;

    TRACE("\n");

    if(!(view = dataview_this(vthis)))
        return JS_E_NOT_DATAVIEW;
    if(r) *r = jsval_number(view->size);
    return S_OK;
}

static HRESULT DataView_get_byteOffset(script_ctx_t *ctx, jsval_t vthis, WORD flags, unsigned argc, jsval_t *argv,
        jsval_t *r)
{
    DataViewInstance *view;

    TRACE("\n");

    if(!(view = dataview_this(vthis)))
        return JS_E_NOT_DATAVIEW;
    if(r) *r = jsval_number(view->offset);
    return S_OK;
}

static inline void copy_type_data(void *dst, const void *src, unsigned type_size, BOOL little_endian)
{
#ifdef WORDS_BIGENDIAN
    BOOL swap = little_endian;
#else
    BOOL swap = !little_endian;
#endif
    const BYTE *in = src;
    BYTE *out = dst;
    unsigned i;

    if(swap)
        for(i = 0; i < type_size; i++)
            out[i] = in[type_size - i - 1];
    else
        memcpy(out, in, type_size);
}

static HRESULT get_data(script_ctx_t *ctx, jsval_t vthis, unsigned argc, jsval_t *argv, unsigned type_size, void *ret)
{
    BOOL little_endian = FALSE;
    DataViewInstance *view;
    HRESULT hres;
    DWORD offset;
    BYTE *data;
    double n;

    if(!(view = dataview_this(vthis)))
        return JS_E_NOT_DATAVIEW;
    if(!argc || is_undefined(argv[0]))
        return JS_E_DATAVIEW_NO_ARGUMENT;

    hres = to_integer(ctx, argv[0], &n);
    if(FAILED(hres))
        return hres;

    if(n < 0.0 || n >= view->size)
        return JS_E_DATAVIEW_INVALID_ACCESS;

    offset = n;
    if(view->size - offset < type_size)
        return JS_E_DATAVIEW_INVALID_ACCESS;
    data = &arraybuf_from_jsdisp(view->buffer)->buf[view->offset + offset];

    if(type_size == 1) {
        *(BYTE*)ret = data[0];
        return S_OK;
    }

    if(argc > 1) {
        hres = to_boolean(argv[1], &little_endian);
        if(FAILED(hres))
            return hres;
    }

    copy_type_data(ret, data, type_size, little_endian);
    return S_OK;
}

static HRESULT set_data(script_ctx_t *ctx, jsval_t vthis, unsigned argc, jsval_t *argv, unsigned type_size, const void *val)
{
    BOOL little_endian = FALSE;
    DataViewInstance *view;
    HRESULT hres;
    DWORD offset;
    BYTE *data;
    double n;

    if(!(view = dataview_this(vthis)))
        return JS_E_NOT_DATAVIEW;
    if(is_undefined(argv[0]) || is_undefined(argv[1]))
        return JS_E_DATAVIEW_NO_ARGUMENT;

    hres = to_integer(ctx, argv[0], &n);
    if(FAILED(hres))
        return hres;

    if(n < 0.0 || n >= view->size)
        return JS_E_DATAVIEW_INVALID_ACCESS;

    offset = n;
    if(view->size - offset < type_size)
        return JS_E_DATAVIEW_INVALID_ACCESS;
    data = &arraybuf_from_jsdisp(view->buffer)->buf[view->offset + offset];

    if(type_size == 1) {
        data[0] = *(const BYTE*)val;
        return S_OK;
    }

    if(argc > 2) {
        hres = to_boolean(argv[2], &little_endian);
        if(FAILED(hres))
            return hres;
    }

    copy_type_data(data, val, type_size, little_endian);
    return S_OK;
}

static HRESULT DataView_getFloat32(script_ctx_t *ctx, jsval_t vthis, WORD flags, unsigned argc, jsval_t *argv, jsval_t *r)
{
    HRESULT hres;
    float v;

    TRACE("\n");

    hres = get_data(ctx, vthis, argc, argv, sizeof(v), &v);
    if(FAILED(hres))
        return hres;
    if(r) *r = jsval_number(v);
    return S_OK;
}

static HRESULT DataView_getFloat64(script_ctx_t *ctx, jsval_t vthis, WORD flags, unsigned argc, jsval_t *argv, jsval_t *r)
{
    HRESULT hres;
    double v;

    TRACE("\n");

    hres = get_data(ctx, vthis, argc, argv, sizeof(v), &v);
    if(FAILED(hres))
        return hres;
    if(r) *r = jsval_number(v);
    return S_OK;
}

static HRESULT DataView_getInt8(script_ctx_t *ctx, jsval_t vthis, WORD flags, unsigned argc, jsval_t *argv, jsval_t *r)
{
    HRESULT hres;
    INT8 v;

    TRACE("\n");

    hres = get_data(ctx, vthis, argc, argv, sizeof(v), &v);
    if(FAILED(hres))
        return hres;
    if(r) *r = jsval_number(v);
    return S_OK;
}

static HRESULT DataView_getInt16(script_ctx_t *ctx, jsval_t vthis, WORD flags, unsigned argc, jsval_t *argv, jsval_t *r)
{
    HRESULT hres;
    INT16 v;

    TRACE("\n");

    hres = get_data(ctx, vthis, argc, argv, sizeof(v), &v);
    if(FAILED(hres))
        return hres;
    if(r) *r = jsval_number(v);
    return S_OK;
}

static HRESULT DataView_getInt32(script_ctx_t *ctx, jsval_t vthis, WORD flags, unsigned argc, jsval_t *argv, jsval_t *r)
{
    HRESULT hres;
    INT32 v;

    TRACE("\n");

    hres = get_data(ctx, vthis, argc, argv, sizeof(v), &v);
    if(FAILED(hres))
        return hres;
    if(r) *r = jsval_number(v);
    return S_OK;
}

static HRESULT DataView_getUint8(script_ctx_t *ctx, jsval_t vthis, WORD flags, unsigned argc, jsval_t *argv, jsval_t *r)
{
    HRESULT hres;
    UINT8 v;

    TRACE("\n");

    hres = get_data(ctx, vthis, argc, argv, sizeof(v), &v);
    if(FAILED(hres))
        return hres;
    if(r) *r = jsval_number(v);
    return S_OK;
}

static HRESULT DataView_getUint16(script_ctx_t *ctx, jsval_t vthis, WORD flags, unsigned argc, jsval_t *argv, jsval_t *r)
{
    HRESULT hres;
    UINT16 v;

    TRACE("\n");

    hres = get_data(ctx, vthis, argc, argv, sizeof(v), &v);
    if(FAILED(hres))
        return hres;
    if(r) *r = jsval_number(v);
    return S_OK;
}

static HRESULT DataView_getUint32(script_ctx_t *ctx, jsval_t vthis, WORD flags, unsigned argc, jsval_t *argv, jsval_t *r)
{
    HRESULT hres;
    UINT32 v;

    TRACE("\n");

    hres = get_data(ctx, vthis, argc, argv, sizeof(v), &v);
    if(FAILED(hres))
        return hres;
    if(r) *r = jsval_number(v);
    return S_OK;
}

static HRESULT DataView_setFloat32(script_ctx_t *ctx, jsval_t vthis, WORD flags, unsigned argc, jsval_t *argv, jsval_t *r)
{
    HRESULT hres;
    double n;
    float v;

    TRACE("\n");

    if(argc < 2)
        return JS_E_DATAVIEW_NO_ARGUMENT;
    hres = to_number(ctx, argv[1], &n);
    if(FAILED(hres))
        return hres;
    v = n;  /* FIXME: don't assume rounding mode is round-to-nearest ties-to-even */

    hres = set_data(ctx, vthis, argc, argv, sizeof(v), &v);
    if(FAILED(hres))
        return hres;
    if(r) *r = jsval_undefined();
    return S_OK;
}

static HRESULT DataView_setFloat64(script_ctx_t *ctx, jsval_t vthis, WORD flags, unsigned argc, jsval_t *argv, jsval_t *r)
{
    HRESULT hres;
    double v;

    TRACE("\n");

    if(argc < 2)
        return JS_E_DATAVIEW_NO_ARGUMENT;
    hres = to_number(ctx, argv[1], &v);
    if(FAILED(hres))
        return hres;

    hres = set_data(ctx, vthis, argc, argv, sizeof(v), &v);
    if(FAILED(hres))
        return hres;
    if(r) *r = jsval_undefined();
    return S_OK;
}

static HRESULT DataView_setInt8(script_ctx_t *ctx, jsval_t vthis, WORD flags, unsigned argc, jsval_t *argv, jsval_t *r)
{
    HRESULT hres;
    INT32 n;
    INT8 v;

    TRACE("\n");

    if(argc < 2)
        return JS_E_DATAVIEW_NO_ARGUMENT;
    hres = to_int32(ctx, argv[1], &n);
    if(FAILED(hres))
        return hres;
    v = n;

    hres = set_data(ctx, vthis, argc, argv, sizeof(v), &v);
    if(FAILED(hres))
        return hres;
    if(r) *r = jsval_undefined();
    return S_OK;
}

static HRESULT DataView_setInt16(script_ctx_t *ctx, jsval_t vthis, WORD flags, unsigned argc, jsval_t *argv, jsval_t *r)
{
    HRESULT hres;
    INT32 n;
    INT16 v;

    TRACE("\n");

    if(argc < 2)
        return JS_E_DATAVIEW_NO_ARGUMENT;
    hres = to_int32(ctx, argv[1], &n);
    if(FAILED(hres))
        return hres;
    v = n;

    hres = set_data(ctx, vthis, argc, argv, sizeof(v), &v);
    if(FAILED(hres))
        return hres;
    if(r) *r = jsval_undefined();
    return S_OK;
}

static HRESULT DataView_setInt32(script_ctx_t *ctx, jsval_t vthis, WORD flags, unsigned argc, jsval_t *argv, jsval_t *r)
{
    HRESULT hres;
    INT32 v;

    TRACE("\n");

    if(argc < 2)
        return JS_E_DATAVIEW_NO_ARGUMENT;
    hres = to_int32(ctx, argv[1], &v);
    if(FAILED(hres))
        return hres;

    hres = set_data(ctx, vthis, argc, argv, sizeof(v), &v);
    if(FAILED(hres))
        return hres;
    if(r) *r = jsval_undefined();
    return S_OK;
}

static const builtin_prop_t DataView_props[] = {
    {L"getFloat32",            DataView_getFloat32,        PROPF_METHOD|1},
    {L"getFloat64",            DataView_getFloat64,        PROPF_METHOD|1},
    {L"getInt16",              DataView_getInt16,          PROPF_METHOD|1},
    {L"getInt32",              DataView_getInt32,          PROPF_METHOD|1},
    {L"getInt8",               DataView_getInt8,           PROPF_METHOD|1},
    {L"getUint16",             DataView_getUint16,         PROPF_METHOD|1},
    {L"getUint32",             DataView_getUint32,         PROPF_METHOD|1},
    {L"getUint8",              DataView_getUint8,          PROPF_METHOD|1},
    {L"setFloat32",            DataView_setFloat32,        PROPF_METHOD|1},
    {L"setFloat64",            DataView_setFloat64,        PROPF_METHOD|1},
    {L"setInt16",              DataView_setInt16,          PROPF_METHOD|1},
    {L"setInt32",              DataView_setInt32,          PROPF_METHOD|1},
    {L"setInt8",               DataView_setInt8,           PROPF_METHOD|1},
    {L"setUint16",             DataView_setInt16,          PROPF_METHOD|1},
    {L"setUint32",             DataView_setInt32,          PROPF_METHOD|1},
    {L"setUint8",              DataView_setInt8,           PROPF_METHOD|1},
};

static void DataView_destructor(jsdisp_t *dispex)
{
    DataViewInstance *view = dataview_from_jsdisp(dispex);
    if(view->buffer)
        jsdisp_release(view->buffer);
    free(view);
}

static HRESULT DataView_gc_traverse(struct gc_ctx *gc_ctx, enum gc_traverse_op op, jsdisp_t *dispex)
{
    DataViewInstance *view = dataview_from_jsdisp(dispex);
    return gc_process_linked_obj(gc_ctx, op, dispex, view->buffer, (void**)&view->buffer);
}

static const builtin_info_t DataView_info = {
    JSCLASS_DATAVIEW,
    NULL,
    ARRAY_SIZE(DataView_props),
    DataView_props,
    DataView_destructor,
    NULL,
    NULL,
    NULL,
    NULL,
    DataView_gc_traverse
};

static const builtin_info_t DataViewInst_info = {
    JSCLASS_DATAVIEW,
    NULL,
    0,
    NULL,
    DataView_destructor,
    NULL,
    NULL,
    NULL,
    NULL,
    DataView_gc_traverse
};

static HRESULT DataViewConstr_value(script_ctx_t *ctx, jsval_t vthis, WORD flags, unsigned argc, jsval_t *argv,
        jsval_t *r)
{
    ArrayBufferInstance *arraybuf;
    DataViewInstance *view;
    DWORD offset = 0, size;
    HRESULT hres;

    TRACE("\n");

    switch(flags) {
    case DISPATCH_METHOD:
    case DISPATCH_CONSTRUCT: {
        if(!argc || !(arraybuf = arraybuf_this(argv[0])))
            return JS_E_DATAVIEW_NO_ARGUMENT;
        size = arraybuf->size;

        if(argc > 1) {
            double offs, len, maxsize = size;
            hres = to_integer(ctx, argv[1], &offs);
            if(FAILED(hres))
                return hres;
            if(offs < 0.0 || offs > maxsize)
                return JS_E_DATAVIEW_INVALID_OFFSET;
            offset = offs;

            if(argc > 2 && !is_undefined(argv[2])) {
                hres = to_integer(ctx, argv[2], &len);
                if(FAILED(hres))
                    return hres;
                if(len < 0.0 || offs+len > maxsize)
                    return JS_E_DATAVIEW_INVALID_OFFSET;
                size = len;
            }else
                size -= offset;
        }

        if(!r)
            return S_OK;

        if(!(view = calloc(1, sizeof(DataViewInstance))))
            return E_OUTOFMEMORY;

        hres = init_dispex_from_constr(&view->dispex, ctx, &DataViewInst_info, ctx->dataview_constr);
        if(FAILED(hres)) {
            free(view);
            return hres;
        }

        view->buffer = jsdisp_addref(&arraybuf->dispex);
        view->offset = offset;
        view->size = size;

        *r = jsval_obj(&view->dispex);
        break;
    }
    default:
        FIXME("unimplemented flags: %x\n", flags);
        return E_NOTIMPL;
    }

    return S_OK;
}

static const builtin_info_t DataViewConstr_info = {
    JSCLASS_FUNCTION,
    Function_value,
    0,
    NULL,
    NULL,
    NULL
};

#define TYPEDARRAY_LIST \
X(Int8Array,            JSCLASS_INT8ARRAY,          INT8,   to_int32,   INT)    \
X(Int16Array,           JSCLASS_INT16ARRAY,         INT16,  to_int32,   INT)    \
X(Int32Array,           JSCLASS_INT32ARRAY,         INT32,  to_int32,   INT)    \
X(Uint8Array,           JSCLASS_UINT8ARRAY,         UINT8,  to_int32,   INT)    \
X(Uint16Array,          JSCLASS_UINT16ARRAY,        UINT16, to_int32,   INT)    \
X(Uint32Array,          JSCLASS_UINT32ARRAY,        UINT32, to_int32,   INT)    \
X(Float32Array,         JSCLASS_FLOAT32ARRAY,       float,  to_number,  double) \
X(Float64Array,         JSCLASS_FLOAT64ARRAY,       double, to_number,  double)

#define TYPEDARRAY_INDEX(JSCLASS) ((JSCLASS) - FIRST_TYPEDARRAY_JSCLASS)

#define X(NAME, JSCLASS, TYPE, CONVERT, NUM_TYPE) [TYPEDARRAY_INDEX(JSCLASS)] = L"" #NAME,
static const WCHAR *const TypedArray_name[] = { TYPEDARRAY_LIST };
#undef X

#define X(NAME, JSCLASS, TYPE, CONVERT, NUM_TYPE) [TYPEDARRAY_INDEX(JSCLASS)] = sizeof(TYPE),
static const unsigned TypedArray_elem_size[] = { TYPEDARRAY_LIST };
#undef X

static inline TypedArrayInstance *typedarr_this(jsval_t vthis, jsclass_t jsclass)
{
    jsdisp_t *jsdisp = is_object_instance(vthis) ? to_jsdisp(get_object(vthis)) : NULL;
    return (jsdisp && is_class(jsdisp, jsclass)) ? typedarr_from_jsdisp(jsdisp) : NULL;
}

static HRESULT create_typedarr(script_ctx_t*,jsclass_t,jsdisp_t*,DWORD,DWORD,jsdisp_t**);

static HRESULT TypedArray_get_buffer(script_ctx_t *ctx, jsdisp_t *jsthis, jsval_t *r)
{
    TRACE("%p\n", jsthis);

    *r = jsval_obj(jsdisp_addref(typedarr_from_jsdisp(jsthis)->buffer));
    return S_OK;
}

static HRESULT TypedArray_get_byteLength(script_ctx_t *ctx, jsdisp_t *jsthis, jsval_t *r)
{
    TRACE("%p\n", jsthis);

    *r = jsval_number(typedarr_from_jsdisp(jsthis)->length * TypedArray_elem_size[TYPEDARRAY_INDEX(jsthis->builtin_info->class)]);
    return S_OK;
}

static HRESULT TypedArray_get_byteOffset(script_ctx_t *ctx, jsdisp_t *jsthis, jsval_t *r)
{
    TRACE("%p\n", jsthis);

    *r = jsval_number(typedarr_from_jsdisp(jsthis)->offset);
    return S_OK;
}

static HRESULT TypedArray_get_length(script_ctx_t *ctx, jsdisp_t *jsthis, jsval_t *r)
{
    TRACE("%p\n", jsthis);

    *r = jsval_number(typedarr_from_jsdisp(jsthis)->length);
    return S_OK;
}

static HRESULT fill_typedarr_data_from_object(script_ctx_t *ctx, BYTE *data, jsdisp_t *obj, DWORD length, jsclass_t jsclass)
{
    HRESULT hres = S_OK;
    jsval_t val;
    UINT32 i;

    switch(jsclass) {
#define X(NAME, JSCLASS, TYPE, CONVERT, NUM_TYPE)   \
    case JSCLASS:                                   \
        for(i = 0; i < length; i++) {               \
            NUM_TYPE n;                             \
                                                    \
            hres = jsdisp_get_idx(obj, i, &val);    \
            if(FAILED(hres)) {                      \
                if(hres != DISP_E_UNKNOWNNAME)      \
                    break;                          \
                val = jsval_undefined();            \
            }                                       \
                                                    \
            hres = CONVERT(ctx, val, &n);           \
            jsval_release(val);                     \
            if(FAILED(hres))                        \
                break;                              \
            *(TYPE*)&data[i * sizeof(TYPE)] = n;    \
        }                                           \
        break;
        TYPEDARRAY_LIST
    DEFAULT_UNREACHABLE;
#undef X
    }

    return hres;
}

static HRESULT TypedArray_set(script_ctx_t *ctx, jsval_t vthis, WORD flags, unsigned argc, jsval_t *argv,
        jsval_t *r, jsclass_t jsclass)
{
    TypedArrayInstance *typedarr;

    FIXME("not implemented\n");

    if(!(typedarr = typedarr_this(vthis, jsclass)))
        return JS_E_NOT_TYPEDARRAY;
    return E_NOTIMPL;
}

static HRESULT TypedArray_subarray(script_ctx_t *ctx, jsval_t vthis, WORD flags, unsigned argc, jsval_t *argv,
        jsval_t *r, jsclass_t jsclass)
{
    TypedArrayInstance *typedarr;
    DWORD begin = 0, end;
    jsdisp_t *obj;
    HRESULT hres;
    double n;

    TRACE("\n");

    if(!(typedarr = typedarr_this(vthis, jsclass)))
        return JS_E_NOT_TYPEDARRAY;
    if(!argc)
        return JS_E_TYPEDARRAY_INVALID_SUBARRAY;
    if(!r)
        return S_OK;

    hres = to_integer(ctx, argv[0], &n);
    if(FAILED(hres))
        return hres;
    end = typedarr->length;
    if(n < 0.0)
        n += typedarr->length;
    if(n >= 0.0)
        begin = n < typedarr->length ? n : typedarr->length;

    if(argc > 1 && !is_undefined(argv[1])) {
        hres = to_integer(ctx, argv[1], &n);
        if(FAILED(hres))
            return hres;
        if(n < 0.0)
            n += typedarr->length;
        if(n >= 0.0) {
            end = n < typedarr->length ? n : typedarr->length;
            end = end < begin ? begin : end;
        }else
            end = begin;
    }

    hres = create_typedarr(ctx, jsclass, typedarr->buffer,
                           typedarr->offset + begin * TypedArray_elem_size[TYPEDARRAY_INDEX(jsclass)],
                           end - begin, &obj);
    if(FAILED(hres))
        return hres;

    *r = jsval_obj(obj);
    return S_OK;
}

static unsigned TypedArray_idx_length(jsdisp_t *jsdisp)
{
    TypedArrayInstance *typedarr = typedarr_from_jsdisp(jsdisp);
    return typedarr->length;
}

static void TypedArray_destructor(jsdisp_t *dispex)
{
    TypedArrayInstance *typedarr = typedarr_from_jsdisp(dispex);
    if(typedarr->buffer)
        jsdisp_release(typedarr->buffer);
    free(typedarr);
}

static HRESULT TypedArray_gc_traverse(struct gc_ctx *gc_ctx, enum gc_traverse_op op, jsdisp_t *dispex)
{
    TypedArrayInstance *typedarr = typedarr_from_jsdisp(dispex);
    return gc_process_linked_obj(gc_ctx, op, dispex, typedarr->buffer, (void**)&typedarr->buffer);
}

static const builtin_prop_t TypedArrayInst_props[] = {
    {L"buffer",                NULL, 0,                    TypedArray_get_buffer},
    {L"byteLength",            NULL, 0,                    TypedArray_get_byteLength},
    {L"byteOffset",            NULL, 0,                    TypedArray_get_byteOffset},
    {L"length",                NULL, 0,                    TypedArray_get_length},
};

#define X(NAME, JSCLASS, TYPE, CONVERT, NUM_TYPE) \
static HRESULT NAME ##_idx_get(jsdisp_t *jsdisp, unsigned idx, jsval_t *r)              \
{                                                                                       \
    TypedArrayInstance *typedarr = typedarr_from_jsdisp(jsdisp);                        \
                                                                                        \
    TRACE("%p[%u]\n", typedarr, idx);                                                   \
                                                                                        \
    if(idx >= typedarr->length)                                                         \
        *r = jsval_undefined();                                                         \
    else                                                                                \
        *r = jsval_number(*(TYPE*)&arraybuf_from_jsdisp(typedarr->buffer)->buf[typedarr->offset + idx * sizeof(TYPE)]); \
    return S_OK;                                                                        \
}                                                                                       \
                                                                                        \
static HRESULT NAME ##_idx_put(jsdisp_t *jsdisp, unsigned idx, jsval_t val)             \
{                                                                                       \
    TypedArrayInstance *typedarr = typedarr_from_jsdisp(jsdisp);                        \
    HRESULT hres;                                                                       \
    NUM_TYPE n;                                                                         \
                                                                                        \
    TRACE("%p[%u] = %s\n", typedarr, idx, debugstr_jsval(val));                         \
                                                                                        \
    if(idx >= typedarr->length)                                                         \
        return S_OK;                                                                    \
                                                                                        \
    hres = CONVERT(jsdisp->ctx, val, &n);                                               \
    if(SUCCEEDED(hres))                                                                 \
        *(TYPE*)&arraybuf_from_jsdisp(typedarr->buffer)->buf[typedarr->offset + idx * sizeof(TYPE)] = n; \
    return hres;                                                                        \
}                                                                                       \
                                                                                        \
static HRESULT NAME ##_set(script_ctx_t *ctx, jsval_t vthis, WORD flags, unsigned argc, jsval_t *argv, \
        jsval_t *r)                                                                     \
{                                                                                       \
    return TypedArray_set(ctx, vthis, flags, argc, argv, r, JSCLASS);                   \
}                                                                                       \
                                                                                        \
static HRESULT NAME ##_subarray(script_ctx_t *ctx, jsval_t vthis, WORD flags, unsigned argc, jsval_t *argv, \
        jsval_t *r)                                                                     \
{                                                                                       \
    return TypedArray_subarray(ctx, vthis, flags, argc, argv, r, JSCLASS);              \
}                                                                                       \
                                                                                        \
static const builtin_prop_t NAME ##_props[] = {                                         \
    {L"buffer",                NULL, 0,                    TypedArray_get_buffer},      \
    {L"byteLength",            NULL, 0,                    TypedArray_get_byteLength},  \
    {L"byteOffset",            NULL, 0,                    TypedArray_get_byteOffset},  \
    {L"length",                NULL, 0,                    TypedArray_get_length},      \
    {L"set",                   NAME ##_set,                PROPF_METHOD|2},             \
    {L"subarray",              NAME ##_subarray,           PROPF_METHOD|2},             \
};
TYPEDARRAY_LIST
#undef X

#define X(NAME, JSCLASS, TYPE, CONVERT, NUM_TYPE) \
[TYPEDARRAY_INDEX(JSCLASS)] =           \
{                                       \
    JSCLASS,                            \
    NULL,                               \
    ARRAY_SIZE(NAME ##_props),          \
    NAME ##_props,                      \
    TypedArray_destructor,              \
    NULL,                               \
    TypedArray_idx_length,              \
    NAME ##_idx_get,                    \
    NAME ##_idx_put,                    \
    TypedArray_gc_traverse              \
},
static const builtin_info_t TypedArray_info[] = { TYPEDARRAY_LIST };
#undef X

#define X(NAME, JSCLASS, TYPE, CONVERT, NUM_TYPE) \
[TYPEDARRAY_INDEX(JSCLASS)] =           \
{                                       \
    JSCLASS,                            \
    NULL,                               \
    ARRAY_SIZE(TypedArrayInst_props),   \
    TypedArrayInst_props,               \
    TypedArray_destructor,              \
    NULL,                               \
    TypedArray_idx_length,              \
    NAME ##_idx_get,                    \
    NAME ##_idx_put,                    \
    TypedArray_gc_traverse              \
},
static const builtin_info_t TypedArrayInst_info[] = { TYPEDARRAY_LIST };
#undef X

static HRESULT create_typedarr(script_ctx_t *ctx, jsclass_t jsclass, jsdisp_t *buffer, DWORD offset, DWORD length,
        jsdisp_t **ret)
{
    TypedArrayInstance *typedarr;
    HRESULT hres;

    if(!(typedarr = calloc(1, sizeof(TypedArrayInstance))))
        return E_OUTOFMEMORY;

    hres = init_dispex_from_constr(&typedarr->dispex, ctx, &TypedArrayInst_info[TYPEDARRAY_INDEX(jsclass)],
                                   ctx->typedarr_constr[TYPEDARRAY_INDEX(jsclass)]);
    if(FAILED(hres)) {
        free(typedarr);
        return hres;
    }

    typedarr->buffer = jsdisp_addref(buffer);
    typedarr->offset = offset;
    typedarr->length = length;

    *ret = &typedarr->dispex;
    return S_OK;
}

static HRESULT TypedArrayConstr_value(script_ctx_t *ctx, jsval_t vthis, WORD flags, unsigned argc, jsval_t *argv,
        jsval_t *r, jsclass_t jsclass)
{
    const unsigned typedarr_idx = TYPEDARRAY_INDEX(jsclass);
    unsigned elem_size = TypedArray_elem_size[typedarr_idx];
    jsdisp_t *typedarr, *buffer = NULL;
    DWORD offset = 0, length = 0;
    HRESULT hres;
    double n;

    TRACE("\n");

    switch(flags) {
    case DISPATCH_METHOD:
    case DISPATCH_CONSTRUCT: {
        if(argc) {
            if(is_object_instance(argv[0])) {
                jsdisp_t *obj = to_jsdisp(get_object(argv[0]));

                if(!obj)
                    return JS_E_TYPEDARRAY_BAD_CTOR_ARG;

                if(obj->builtin_info->class == JSCLASS_ARRAYBUFFER) {
                    ArrayBufferInstance *arraybuf = arraybuf_from_jsdisp(obj);

                    if(argc > 1) {
                        hres = to_integer(ctx, argv[1], &n);
                        if(FAILED(hres))
                            return hres;
                        if(n < 0.0 || n > arraybuf->size)
                            return JS_E_TYPEDARRAY_INVALID_OFFSLEN;
                        offset = n;
                        if(offset % elem_size)
                            return JS_E_TYPEDARRAY_INVALID_OFFSLEN;
                    }
                    if(argc > 2 && !is_undefined(argv[2])) {
                        hres = to_integer(ctx, argv[2], &n);
                        if(FAILED(hres))
                            return hres;
                        if(n < 0.0 || n > UINT_MAX)
                            return JS_E_TYPEDARRAY_INVALID_OFFSLEN;
                        length = n;
                        if(offset + length * elem_size > arraybuf->size)
                            return JS_E_TYPEDARRAY_INVALID_OFFSLEN;
                    }else {
                        length = arraybuf->size - offset;
                        if(length % elem_size)
                            return JS_E_TYPEDARRAY_INVALID_OFFSLEN;
                        length /= elem_size;
                    }
                    buffer = jsdisp_addref(&arraybuf->dispex);
                }else {
                    jsval_t val;
                    UINT32 len;
                    DWORD size;

                    hres = jsdisp_propget_name(obj, L"length", &val);
                    if(FAILED(hres))
                        return hres;
                    if(is_undefined(val))
                        return JS_E_TYPEDARRAY_BAD_CTOR_ARG;

                    hres = to_uint32(ctx, val, &len);
                    jsval_release(val);
                    if(FAILED(hres))
                        return hres;

                    length = len;
                    size = length * elem_size;
                    if(size < length || size > (UINT_MAX - FIELD_OFFSET(ArrayBufferInstance, buf[0])))
                        return E_OUTOFMEMORY;

                    hres = create_arraybuf(ctx, size, &buffer);
                    if(FAILED(hres))
                        return hres;

                    hres = fill_typedarr_data_from_object(ctx, arraybuf_from_jsdisp(buffer)->buf,
                                                          obj, length, jsclass);
                    if(FAILED(hres)) {
                        jsdisp_release(buffer);
                        return hres;
                    }
                }
            }else if(is_number(argv[0])) {
                hres = to_integer(ctx, argv[0], &n);
                if(FAILED(hres))
                    return hres;
                if(n < 0.0)
                    return JS_E_TYPEDARRAY_INVALID_OFFSLEN;
                if(n * elem_size > (UINT_MAX - FIELD_OFFSET(ArrayBufferInstance, buf[0])))
                    return E_OUTOFMEMORY;
                length = n;
            }else
                return JS_E_TYPEDARRAY_BAD_CTOR_ARG;
        }

        if(!r)
            return S_OK;

        if(!buffer) {
            hres = create_arraybuf(ctx, length * elem_size, &buffer);
            if(FAILED(hres))
                return hres;
        }

        hres = create_typedarr(ctx, jsclass, buffer, offset, length, &typedarr);
        jsdisp_release(buffer);
        if(FAILED(hres))
            return hres;

        *r = jsval_obj(typedarr);
        break;
    }
    default:
        FIXME("unimplemented flags: %x\n", flags);
        return E_NOTIMPL;
    }

    return S_OK;
}

#define X(NAME, JSCLASS, TYPE, CONVERT, NUM_TYPE) \
static HRESULT NAME ## Constr_value(script_ctx_t *ctx, jsval_t vthis, WORD flags, unsigned argc, jsval_t *argv, jsval_t *r) \
{ \
    return TypedArrayConstr_value(ctx, vthis, flags, argc, argv, r, JSCLASS); \
}
TYPEDARRAY_LIST
#undef X

#define X(NAME, JSCLASS, TYPE, CONVERT, NUM_TYPE) [TYPEDARRAY_INDEX(JSCLASS)] = NAME ## Constr_value,
static const builtin_invoke_t TypedArray_constr[] = { TYPEDARRAY_LIST };
#undef X

static const builtin_info_t TypedArrayConstr_info = {
    JSCLASS_FUNCTION,
    Function_value,
    0,
    NULL,
    NULL,
    NULL
};

HRESULT init_arraybuf_constructors(script_ctx_t *ctx)
{
    static const struct {
        const WCHAR *name;
        builtin_invoke_t get;
    } DataView_getters[] = {
        { L"buffer",        DataView_get_buffer },
        { L"byteLength",    DataView_get_byteLength },
        { L"byteOffset",    DataView_get_byteOffset },
    };
    ArrayBufferInstance *arraybuf;
    TypedArrayInstance *typedarr;
    DataViewInstance *view;
    property_desc_t desc;
    HRESULT hres;
    unsigned i;

    if(ctx->version < SCRIPTLANGUAGEVERSION_ES5)
        return S_OK;

    if(!(arraybuf = calloc(1, FIELD_OFFSET(ArrayBufferInstance, buf[0]))))
        return E_OUTOFMEMORY;

    hres = init_dispex(&arraybuf->dispex, ctx, &ArrayBuffer_info, ctx->object_prototype);
    if(FAILED(hres)) {
        free(arraybuf);
        return hres;
    }

    hres = create_builtin_constructor(ctx, ArrayBufferConstr_value, L"ArrayBuffer", &ArrayBufferConstr_info,
                                      PROPF_CONSTR|1, &arraybuf->dispex, &ctx->arraybuf_constr);
    jsdisp_release(&arraybuf->dispex);
    if(FAILED(hres))
        return hres;

    hres = jsdisp_define_data_property(ctx->global, L"ArrayBuffer", PROPF_CONFIGURABLE | PROPF_WRITABLE,
                                       jsval_obj(ctx->arraybuf_constr));
    if(FAILED(hres))
        return hres;

    if(!(view = calloc(1, sizeof(DataViewInstance))))
        return E_OUTOFMEMORY;

    hres = create_arraybuf(ctx, 0, &view->buffer);
    if(FAILED(hres)) {
        free(view);
        return hres;
    }

    hres = init_dispex(&view->dispex, ctx, &DataView_info, ctx->object_prototype);
    if(FAILED(hres)) {
        jsdisp_release(view->buffer);
        free(view);
        return hres;
    }

    desc.flags = PROPF_CONFIGURABLE;
    desc.mask  = PROPF_CONFIGURABLE | PROPF_ENUMERABLE;
    desc.explicit_getter = desc.explicit_setter = TRUE;
    desc.explicit_value = FALSE;
    desc.setter = NULL;

    for(i = 0; i < ARRAY_SIZE(DataView_getters); i++) {
        hres = create_builtin_function(ctx, DataView_getters[i].get, NULL, NULL, PROPF_METHOD, NULL, &desc.getter);
        if(SUCCEEDED(hres)) {
            hres = jsdisp_define_property(&view->dispex, DataView_getters[i].name, &desc);
            jsdisp_release(desc.getter);
        }
        if(FAILED(hres)) {
            jsdisp_release(&view->dispex);
            return hres;
        }
    }

    hres = create_builtin_constructor(ctx, DataViewConstr_value, L"DataView", &DataViewConstr_info,
                                      PROPF_CONSTR|1, &view->dispex, &ctx->dataview_constr);
    jsdisp_release(&view->dispex);
    if(FAILED(hres))
        return hres;

    hres = jsdisp_define_data_property(ctx->global, L"DataView", PROPF_CONFIGURABLE | PROPF_WRITABLE,
                                       jsval_obj(ctx->dataview_constr));
    if(FAILED(hres))
        return hres;

    for(i = 0; i < ARRAY_SIZE(TypedArray_info); i++) {
        if(!(typedarr = calloc(1, sizeof(TypedArrayInstance))))
            return E_OUTOFMEMORY;

        hres = create_arraybuf(ctx, 0, &typedarr->buffer);
        if(FAILED(hres)) {
            free(typedarr);
            return hres;
        }

        hres = init_dispex(&typedarr->dispex, ctx, &TypedArray_info[i], ctx->object_prototype);
        if(FAILED(hres)) {
            jsdisp_release(typedarr->buffer);
            free(typedarr);
            return hres;
        }

        hres = create_builtin_constructor(ctx, TypedArray_constr[i], TypedArray_name[i], &TypedArrayConstr_info,
                                          PROPF_CONSTR|1, &typedarr->dispex, &ctx->typedarr_constr[i]);
        jsdisp_release(&typedarr->dispex);
        if(FAILED(hres))
            return hres;

        hres = jsdisp_define_data_property(ctx->typedarr_constr[i], L"BYTES_PER_ELEMENT", 0,
                                           jsval_number(TypedArray_elem_size[i]));
        if(FAILED(hres))
            return hres;

        hres = jsdisp_define_data_property(ctx->global, TypedArray_name[i], PROPF_CONFIGURABLE | PROPF_WRITABLE,
                                           jsval_obj(ctx->typedarr_constr[i]));
        if(FAILED(hres))
            return hres;
    }

    return hres;
}
