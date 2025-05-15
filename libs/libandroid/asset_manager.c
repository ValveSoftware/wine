#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <androidfw/androidfw_c_api.h>

#include "../api-impl-jni/defines.h"
#include "../api-impl-jni/util.h"

struct AAssetDir {
	struct AssetDir *asset_dir;
	size_t curr_index;
};

#define ASSET_DIR "assets/"

int AAsset_openFileDescriptor(struct Asset *asset, off_t *out_start, off_t *out_length)
{
	return Asset_openFileDescriptor(asset, out_start, out_length);
}

struct Asset* AAssetManager_open(struct AssetManager *asset_manager, const char *file_name, int mode)
{
	char *path = malloc(strlen(ASSET_DIR) + strlen(file_name) + 1);
	sprintf(path, "%s%s", ASSET_DIR, file_name);

	android_log_printf(ANDROID_LOG_VERBOSE, "["__FILE__"]", "AAssetManager_open called for %s\n", file_name);
	struct Asset *asset = AssetManager_openNonAsset(asset_manager, path, mode);

	free(path);

	return asset;
}

const void * AAsset_getBuffer(struct Asset *asset)
{
	return Asset_getBuffer(asset, false);
}

off64_t AAsset_getLength64(struct Asset *asset)
{
	return Asset_getLength(asset);
}

off_t AAsset_getLength(struct Asset *asset)
{
	return Asset_getLength(asset);
}

int AAsset_read(struct Asset *asset, void *buf, size_t count) {
	return Asset_read(asset, buf, count);
}

off_t AAsset_seek(struct Asset *asset, off_t offset, int whence) {
	return Asset_seek(asset, offset, whence);
}

off64_t AAsset_seek64(struct Asset *asset, off64_t offset, int whence) {
	return Asset_seek(asset, offset, whence);
}

off_t AAsset_getRemainingLength(struct Asset *asset)
{
	return Asset_getRemainingLength(asset);
}
off64_t AAsset_getRemainingLength64(struct Asset *asset)
{
	return Asset_getRemainingLength(asset);
}

void AAsset_close(struct Asset *asset)
{
	Asset_delete(asset);
}

struct AAssetDir * AAssetManager_openDir(struct AssetManager *asset_manager, const char *dirname)
{
	char* dirpath = malloc(strlen(ASSET_DIR) + strlen(dirname) + 1);
	sprintf(dirpath, "%s%s", ASSET_DIR, dirname);

	struct AssetDir *asset_dir = AssetManager_openDir(asset_manager, dirname);

	struct AAssetDir* dir = malloc(sizeof(struct AAssetDir));
	dir->asset_dir = asset_dir;
	dir->curr_index = 0;

	android_log_printf(ANDROID_LOG_VERBOSE, "["__FILE__"]", "AAssetManager_openDir called for %s\n", dirpath);

	return dir;
}

const char * AAssetDir_getNextFileName(struct AAssetDir *dir)
{
	size_t index = dir->curr_index;
	const size_t max = AssetDir_getFileCount(dir->asset_dir);

	/* skip non-regular files */
	while ((index < max) && AssetDir_getFileType(dir->asset_dir, index) != FILE_TYPE_REGULAR)
		index++;

	if (index >= max) {
		dir->curr_index = index;
		return NULL;
	}

	dir->curr_index = index + 1;
	return AssetDir_getFileName(dir->asset_dir, index);
}


void AAssetDir_close(struct AAssetDir *dir)
{
	AssetDir_delete(dir->asset_dir);
	free(dir);
}

struct AssetManager * AAssetManager_fromJava(JNIEnv *env, jobject asset_manager)
{
	return _PTR(_GET_LONG_FIELD(asset_manager, "mObject"));
}
