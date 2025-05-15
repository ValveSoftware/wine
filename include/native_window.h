
#include <EGL/egl.h>
#include <gtk/gtk.h>
#include <jni.h>
#include <X11/Xlib.h>

struct ANativeWindow {
	EGLNativeWindowType egl_window;
	GtkWidget *surface_view_widget;
	struct wl_display *wayland_display;
	struct wl_surface *wayland_surface;
	Display *x11_display;
	gulong resize_handler;
	int refcount;
};

extern GHashTable *egl_surface_hashtable;

struct ANativeWindow *ANativeWindow_fromSurface(JNIEnv* env, jobject surface);
EGLSurface bionic_eglCreateWindowSurface(EGLDisplay display, EGLConfig config, struct ANativeWindow *native_window, EGLint const *attrib_list);
EGLDisplay bionic_eglGetDisplay(NativeDisplayType native_display);
void ANativeWindow_release(struct ANativeWindow *native_window);
