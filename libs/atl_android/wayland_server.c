#include <sys/poll.h>
#include <wayland-server.h>
#include <GL/gl.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <gtk/gtk.h>
#include <gdk/wayland/gdkwayland.h>
#include <gdk/x11/gdkx.h>

#include "../api-impl-jni/defines.h"
#include "../api-impl-jni/widgets/android_view_SurfaceView.h"

static EGLDisplay egl_display_gtk = NULL;
static GdkGLContext *gl_context_gtk = NULL;
static struct wl_event_loop *event_loop = NULL;
static PFNEGLQUERYWAYLANDBUFFERWL eglQueryWaylandBufferWL = NULL;
static PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES = NULL;
static GMutex mutex;  // synchronize access to wl_display_server
static struct wl_display *wl_display_server;

/* runs on main thread */
static gboolean delete_texture(void *data) {
	GLuint texture_id = (uintptr_t)data;
	gdk_gl_context_make_current(gl_context_gtk);
	glDeleteTextures(1, &texture_id);
	return G_SOURCE_REMOVE;
}

struct _BufferData {
	GObject parent;
	gboolean destroyed;
	SurfaceViewWidget *surface_view_widget;
	struct wl_resource *wl_buffer;
	struct wl_listener buffer_destroy_listener;
	GdkGLTextureBuilder *texture_builder;
};
G_DECLARE_FINAL_TYPE(BufferData, buffer_data, ATL, BUFFER_DATA, GObject);
static void buffer_data_dispose(GObject *g_object)
{
	BufferData *buffer = ATL_BUFFER_DATA(g_object);
	if (buffer->texture_builder) {
		GLuint texture_id = gdk_gl_texture_builder_get_id(buffer->texture_builder);
		g_idle_add(delete_texture, _PTR(texture_id));
		g_object_unref(buffer->texture_builder);
	}
	g_object_unref(buffer->surface_view_widget);
}
static void buffer_data_class_init(BufferDataClass *cls)
{
	cls->parent_class.dispose = buffer_data_dispose;
}
static void buffer_data_init(BufferData *self) {}
G_DEFINE_TYPE(BufferData, buffer_data, G_TYPE_OBJECT)

struct surface {
	struct wl_resource *wl_surface;
	SurfaceViewWidget *surface_view_widget;
	struct wl_resource *frame_callback;
	struct wl_event_source *frame_timer;
	BufferData *attached_buffer;
};

/* runs on main thread */
static void destroy_texture(void *data)
{
	BufferData *buffer = ATL_BUFFER_DATA(data);
	g_mutex_lock(&mutex);
	if (!buffer->destroyed) {
		wl_buffer_send_release(buffer->wl_buffer);
	}
	g_mutex_unlock(&mutex);
	g_object_unref(buffer);
}

/* runs on main thread */
static void draw_callback(SurfaceViewWidget *surface_view_widget)
{
	g_mutex_lock(&mutex);
	struct surface *surface = surface_view_widget->frame_callback_data;
	if (surface && surface->frame_callback) {
		wl_callback_send_done(surface->frame_callback, g_get_monotonic_time()/1000);
		wl_resource_destroy(surface->frame_callback);
		surface->frame_callback = NULL;
		wl_event_source_timer_update(surface->frame_timer, 0);
		wl_display_flush_clients(wl_display_server);
	}
	g_mutex_unlock(&mutex);
}

/* runs on main thread */
static gboolean render_texture(void *data)
{
	BufferData *buffer = ATL_BUFFER_DATA(data);
	if (!buffer->texture_builder) {
		g_mutex_lock(&mutex);
		if (buffer->destroyed) {
			g_mutex_unlock(&mutex);
			g_object_unref(buffer);
			return G_SOURCE_REMOVE;
		}
		EGLImage image = eglCreateImage(egl_display_gtk, EGL_NO_CONTEXT, EGL_WAYLAND_BUFFER_WL, buffer->wl_buffer, (EGLAttrib[]){EGL_NONE});
		GLuint texture_id;
		int width, height;
		eglQueryWaylandBufferWL(egl_display_gtk, buffer->wl_buffer, EGL_WIDTH, &width);
		eglQueryWaylandBufferWL(egl_display_gtk, buffer->wl_buffer, EGL_HEIGHT, &height);
		gdk_gl_context_make_current(gl_context_gtk);
		glGenTextures(1, &texture_id);
		glBindTexture(GL_TEXTURE_2D, texture_id);
		glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, image);
		glBindTexture(GL_TEXTURE_2D, 0);
		eglDestroyImage(egl_display_gtk, image);
		g_mutex_unlock(&mutex);
		buffer->texture_builder = gdk_gl_texture_builder_new();
		gdk_gl_texture_builder_set_context(buffer->texture_builder, gl_context_gtk);
		gdk_gl_texture_builder_set_id(buffer->texture_builder, texture_id);
		gdk_gl_texture_builder_set_format(buffer->texture_builder, GDK_MEMORY_R8G8B8A8_PREMULTIPLIED);
		gdk_gl_texture_builder_set_width(buffer->texture_builder, width);
		gdk_gl_texture_builder_set_height(buffer->texture_builder, height);
	}

	GdkTexture *texture = gdk_gl_texture_builder_build(buffer->texture_builder, destroy_texture, buffer);

	surface_view_widget_set_texture(buffer->surface_view_widget, texture);

	return G_SOURCE_REMOVE;
}

static void buffer_destroy_listener(struct wl_listener *listener, void *data) {
	BufferData *buffer = wl_container_of(listener, buffer, buffer_destroy_listener);
	buffer->destroyed = TRUE;
	g_object_unref(buffer);
}

static void surface_attach(struct wl_client *client, struct wl_resource *resource, struct wl_resource *wl_buffer, int32_t x, int32_t y)
{
	struct surface *surface = wl_resource_get_user_data(resource);
	if (surface->attached_buffer)
		g_object_unref(surface->attached_buffer);
	// check if we already have this buffer
	struct wl_listener *listener = wl_resource_get_destroy_listener(wl_buffer, buffer_destroy_listener);
	BufferData *buffer;
	if (listener) {
		buffer = wl_container_of(listener, buffer, buffer_destroy_listener);
	} else {
		buffer = g_object_new(buffer_data_get_type(), NULL);
		buffer->wl_buffer = wl_buffer;
		buffer->surface_view_widget = g_object_ref(surface->surface_view_widget);
		buffer->buffer_destroy_listener.notify = buffer_destroy_listener;
		wl_resource_add_destroy_listener(wl_buffer, &buffer->buffer_destroy_listener);
	}
	surface->attached_buffer = g_object_ref(buffer);
}

static void surface_frame(struct wl_client *client, struct wl_resource *resource, uint32_t callback)
{
	struct surface *surface = wl_resource_get_user_data(resource);
	if (surface->frame_callback)
		wl_resource_destroy(surface->frame_callback);
	surface->frame_callback = wl_resource_create(client, &wl_callback_interface, 1, callback);
	surface->surface_view_widget->frame_callback = draw_callback;
	surface->surface_view_widget->frame_callback_data = surface;
	// this timer is required in case the main thread and the wayland server thread deadlock each other waiting for the next frame_callback
	wl_event_source_timer_update(surface->frame_timer, 1000/20);
}

static void surface_damage(struct wl_client *client, struct wl_resource *resource, int32_t x, int32_t y, int32_t width, int32_t height)
{
}

static int frame_timer(void *data)
{
	struct surface *surface = data;
	if (surface->frame_callback) {
		wl_callback_send_done(surface->frame_callback, g_get_monotonic_time()/1000);
		wl_resource_destroy(surface->frame_callback);
		surface->frame_callback = NULL;
	}
	return 0;
}

static void surface_commit(struct wl_client *client, struct wl_resource *resource)
{
	struct surface *surface = wl_resource_get_user_data(resource);
	g_idle_add(render_texture, g_object_ref(surface->attached_buffer));
}

static void surface_destroy(struct wl_client *client, struct wl_resource *resource)
{
	struct surface *surface = wl_resource_get_user_data(resource);
	if (surface->attached_buffer) {
		g_object_unref(surface->attached_buffer);
	}
	surface->surface_view_widget->frame_callback = NULL;
	surface->surface_view_widget->frame_callback_data = NULL;
	g_object_unref(surface->surface_view_widget);
	wl_resource_destroy(surface->wl_surface);
	wl_event_source_timer_update(surface->frame_timer, 0);
	if (surface->frame_callback)
		wl_resource_destroy(surface->frame_callback);
	surface->frame_callback = NULL;
	g_free(surface);
}

/* we abuse this method to set lower 32 bits of the GtkPicture pointer */
static void surface_set_buffer_scale(struct wl_client *client, struct wl_resource *resource, int32_t scale)
{
	struct surface *surface = wl_resource_get_user_data(resource);
	surface->surface_view_widget = _PTR((((uint64_t)(uintptr_t)surface->surface_view_widget) & 0xffffffff00000000L) | ((uintptr_t)(uint32_t)scale));
}

/* we abuse this method to set higher 32 bits of the GtkPicture pointer */
static void surface_set_buffer_transform(struct wl_client *client, struct wl_resource *resource, int32_t transform)
{
	struct surface *surface = wl_resource_get_user_data(resource);
	surface->surface_view_widget = _PTR((((uint64_t)(uintptr_t)surface->surface_view_widget) & 0x00000000ffffffffL) | ((uintptr_t)(uint32_t)transform)<<32);
}

static struct wl_surface_interface surface_implementation = {
	.attach = surface_attach,
	.frame = surface_frame,
	.damage = surface_damage,
	.commit = surface_commit,
	.destroy = surface_destroy,
	.set_buffer_scale = surface_set_buffer_scale,
	.set_buffer_transform = surface_set_buffer_transform,
};

static void compositor_create_surface(struct wl_client *client, struct wl_resource *resource, uint32_t id)
{
	struct wl_resource *wl_surface = wl_resource_create(client, &wl_surface_interface, 3, id);
	struct surface *surface = g_new0(struct surface, 1);
	wl_resource_set_implementation(wl_surface, &surface_implementation, surface, NULL);
	surface->wl_surface = wl_surface;
	surface->frame_timer = wl_event_loop_add_timer(event_loop, frame_timer, surface);
}

static struct wl_compositor_interface compositor_implementation = {
	.create_surface = compositor_create_surface,
};

static void compositor_bind(struct wl_client *client, void *data, uint32_t version, uint32_t id) {
	struct wl_resource *resource = wl_resource_create(client, &wl_compositor_interface, 1, id);
	wl_resource_set_implementation(resource, &compositor_implementation, NULL, NULL);
}

static gpointer wayland_server_thread(gpointer user_data)
{
	struct wl_display *wl_display_server = user_data;
	struct pollfd pollfd = {
		.fd = wl_event_loop_get_fd(event_loop),
		.events = POLLIN | POLLOUT,
	};
	for (;;) {
		poll(&pollfd, 1, -1);
		g_mutex_lock(&mutex);
		wl_event_loop_dispatch(event_loop, 0);
		wl_display_flush_clients(wl_display_server);
		g_mutex_unlock(&mutex);
	}
	return 0;
}

extern GtkWindow *window;

struct wl_display *wayland_server_start()
{
	GdkDisplay *gdk_display = gtk_root_get_display(GTK_ROOT(window));
	if (GDK_IS_WAYLAND_DISPLAY(gdk_display)) {
		struct wl_display *wl_display = gdk_wayland_display_get_wl_display(gdk_display);
		egl_display_gtk = eglGetPlatformDisplay(EGL_PLATFORM_WAYLAND_KHR, wl_display, NULL);
	} else if (GDK_IS_X11_DISPLAY(gdk_display)) {
		Display *x11_display = gdk_x11_display_get_xdisplay(gdk_display);
		egl_display_gtk = eglGetPlatformDisplay(EGL_PLATFORM_X11_KHR, x11_display, NULL);
	}
	gl_context_gtk = gdk_surface_create_gl_context(gtk_native_get_surface(GTK_NATIVE(window)), NULL);

	wl_display_server = wl_display_create();
	char tmpname[] = "/tmp/tmpdir.XXXXXX\0socket";
	int tmplen = strlen(tmpname);
	mkdtemp(tmpname);
	tmpname[tmplen] = '/';
	wl_display_add_socket(wl_display_server, tmpname);
	struct wl_display *wl_display_client = wl_display_connect(tmpname);
	tmpname[tmplen] = '\0';
	rmdir(tmpname);

	wl_global_create(wl_display_server, &wl_compositor_interface, 3, NULL, &compositor_bind);
	event_loop = wl_display_get_event_loop(wl_display_server);
	g_mutex_init(&mutex);
	PFNEGLBINDWAYLANDDISPLAYWL eglBindWaylandDisplayWL = (PFNEGLBINDWAYLANDDISPLAYWL)eglGetProcAddress("eglBindWaylandDisplayWL");
	eglQueryWaylandBufferWL = (PFNEGLQUERYWAYLANDBUFFERWL)eglGetProcAddress("eglQueryWaylandBufferWL");
	glEGLImageTargetTexture2DOES = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)eglGetProcAddress("glEGLImageTargetTexture2DOES");
	eglBindWaylandDisplayWL(egl_display_gtk, wl_display_server);
	g_thread_new("wayland-server", wayland_server_thread, wl_display_server);

	return wl_display_client;
}
