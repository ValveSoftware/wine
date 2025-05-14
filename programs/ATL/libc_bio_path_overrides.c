#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <fontconfig/fontconfig.h>

/* helpers */
static bool starts_with(const char *string, const char *substring)
{
	return !strncmp(string, substring, strlen(substring));
}

static bool ends_with(const char *str, const char *suffix) {
	if (!str || !suffix)
		return 0;

	size_t str_len = strlen(str);
	size_t suffix_len = strlen(suffix);
	if (suffix_len > str_len) return 0;
	return (strcmp(str + (str_len - suffix_len), suffix) == 0);
}

static bool file_exists(char *path) {
	return (!access(path, F_OK));
}

/* use fontconfig to find a font */
static char * get_font_path(char *filename) {
	int ret;

	/* just load the list once on first call to this function */
	static FcFontSet *font_set = NULL;
	if(!font_set) {
		ret = FcInit();
		if (!ret)
			return NULL;

		// match all fonts
		FcPattern *pattern = FcPatternCreate();
		if (!pattern)
			return NULL;

		FcObjectSet *object_set = FcObjectSetBuild(FC_FILE, NULL);
		if (!object_set) {
			FcPatternDestroy(pattern);
			return NULL;
		}

		// Retrieve the list of fonts
		font_set = FcFontList(NULL, pattern, object_set);
		if (!font_set) {
			FcObjectSetDestroy(object_set);
			FcPatternDestroy(pattern);
			return NULL;
		}
	}

	for (int i = 0; i < font_set->nfont; i++) {
		FcPattern *font = font_set->fonts[i];
		FcChar8 *file = NULL;
		if (FcPatternGetString(font, FC_FILE, 0, &file) == FcResultMatch && file) {
			if (ends_with((char *)file, filename)) {
				return (char *)file;
			}
		}
	}

	return NULL;
}

static char *fonts_overrides[] = {
	"/etc/fonts.xml",
	/* INSTALL_DATADIR will be something like /usr/local/share */
	INSTALL_DATADIR"/atl/system/etc/fonts.xml",
	NULL,
};

bool apply_path_overrides(char **path) {
	bool free_path = false;

	/* TODO: read the overrides from a config file */
	/* TODO: compare with canonicalized path */
	if(!strcmp(*path, "/system/etc/fonts.xml")) {
		for(char **override = fonts_overrides; *override; override++) {
			if(file_exists(*override)) {
				*path = *override;
				break;
			}
		}
	}

	/* AOSP seems to put all the fonts in `/system/fonts`. On a standard Linux distro, the font
	 * could be anywhere, so we need to get all the fonts fontconfig knows about and check
	 * if the filename matches. */
	if(starts_with(*path, "/system/fonts/")) {
		char *font_filename = strchr(*path + 8, '/'); // after /fonts
		char *new_path = get_font_path(font_filename);
		if(new_path)
			*path = new_path;
		else
			fprintf(stderr, "%s: !!! app trying to access a font at >%s<, but there is no >%s< in the fontconfig cache\n", __func__, *path, font_filename);
	}

	if (starts_with(*path, "/system/") || starts_with(*path, "/data/")) {
		fprintf(stderr, "%s: !!! app trying to access >%s<, which will certainly fail\n", __func__, *path);
		fflush(stderr);
	}

	return free_path;
}
