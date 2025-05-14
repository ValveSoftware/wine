#include <gdk/gdk.h>
#include <jni.h>
#include <stdio.h>

// FIXME: put the header in a common place
#include "../api-impl-jni/defines.h"

#define ANDROID_BITMAP_RESULT_SUCCESS 0

struct AndroidBitmapInfo {
	uint32_t width;
	uint32_t height;
	uint32_t stride;
	int32_t  format;
	uint32_t flags;
};

int AndroidBitmap_getInfo(JNIEnv* env, jobject bitmap, struct AndroidBitmapInfo *info)
{
	info->width = _GET_INT_FIELD(bitmap, "width");
	info->height = _GET_INT_FIELD(bitmap, "height");
	info->stride = _GET_INT_FIELD(bitmap, "stride");
	info->format = _GET_INT_FIELD(_GET_OBJ_FIELD(bitmap, "config", "Landroid/graphics/Bitmap$Config;"), "android_memory_format");
	return ANDROID_BITMAP_RESULT_SUCCESS;
}

int AndroidBitmap_lockPixels(JNIEnv* env, jobject bitmap, void** pixels)
{
	printf("AndroidBitmap_lockPixels\n");
	GdkTexture *texture = _PTR((*env)->CallLongMethod(env, bitmap, _METHOD(_CLASS(bitmap), "getTexture", "()J")));
	int stride = _GET_INT_FIELD(bitmap, "stride");
	int format = _GET_INT_FIELD(_GET_OBJ_FIELD(bitmap, "config", "Landroid/graphics/Bitmap$Config;"), "gdk_memory_format");
	if (format == -1) {
		printf("AndroidBitmap_lockPixels: format not implemented\n");
		exit(1);
	}
	GdkTextureDownloader *downloader = gdk_texture_downloader_new(texture);
	gdk_texture_downloader_set_format(downloader, format);
	GBytes *bytes = NULL;
	if (GDK_IS_MEMORY_TEXTURE(texture)) { // try to get the bytes non-copying
		gsize texture_stride;
		bytes = gdk_texture_downloader_download_bytes(downloader, &texture_stride);
		if (texture_stride != stride) {  // texture was not created by us, fall back to copy
			g_bytes_unref(bytes);
			bytes = NULL;
		}
	}
	if (bytes == NULL) {
		guchar *data = g_malloc(stride * gdk_texture_get_height(texture));
		gdk_texture_downloader_download_into(downloader, data, stride);
		bytes = g_bytes_new_take(data, stride * gdk_texture_get_height(texture));
	}
	gdk_texture_downloader_free(downloader);
	_SET_LONG_FIELD(bitmap, "bytes", _INTPTR(bytes));
	*pixels = (void *)g_bytes_get_data(bytes, NULL);
	return ANDROID_BITMAP_RESULT_SUCCESS;
}

int AndroidBitmap_unlockPixels(JNIEnv* env, jobject bitmap)
{
	printf("AndroidBitmap_unlockPixels\n");
	GBytes *bytes = _PTR(_GET_LONG_FIELD(bitmap, "bytes"));
	if (!bytes) {
		printf("AndroidBitmap_unlockPixels: no bytes! Was AndroidBitmap_lockPixels called?\n");
		exit(1);
	}
	int width = _GET_INT_FIELD(bitmap, "width");
	int height = _GET_INT_FIELD(bitmap, "height");
	int stride = _GET_INT_FIELD(bitmap, "stride");
	int format = _GET_INT_FIELD(_GET_OBJ_FIELD(bitmap, "config", "Landroid/graphics/Bitmap$Config;"), "gdk_memory_format");
	if (format == -1) {
		printf("AndroidBitmap_lockPixels: format not implemented\n");
		exit(1);
	}
	GdkTexture *texture = gdk_memory_texture_new(width, height, format, bytes, stride);
	g_bytes_unref(bytes);
	(*env)->CallVoidMethod(env, bitmap, _METHOD(_CLASS(bitmap), "recycle", "()V"));
	_SET_LONG_FIELD(bitmap, "texture", _INTPTR(texture));
	_SET_LONG_FIELD(bitmap, "bytes", 0);
	return ANDROID_BITMAP_RESULT_SUCCESS;
}
