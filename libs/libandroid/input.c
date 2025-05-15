#include <assert.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <unistd.h>

#include <gtk/gtk.h>

#include "looper.h"

enum {
	AINPUT_EVENT_TYPE_KEY = 1,
	AINPUT_EVENT_TYPE_MOTION = 2,
	AINPUT_EVENT_TYPE_FOCUS = 3,
	AINPUT_EVENT_TYPE_CAPTURE = 4,
	AINPUT_EVENT_TYPE_DRAG = 5,
	AINPUT_EVENT_TYPE_TOUCH_MODE = 6
};

enum {
	AINPUT_SOURCE_CLASS_MASK = 0x000000ff,
	AINPUT_SOURCE_CLASS_NONE = 0x00000000,
	AINPUT_SOURCE_CLASS_BUTTON = 0x00000001,
	AINPUT_SOURCE_CLASS_POINTER = 0x00000002,
	AINPUT_SOURCE_CLASS_NAVIGATION = 0x00000004,
	AINPUT_SOURCE_CLASS_POSITION = 0x00000008,
	AINPUT_SOURCE_CLASS_JOYSTICK = 0x00000010
};

enum {
	AINPUT_SOURCE_UNKNOWN = 0x00000000,
	AINPUT_SOURCE_KEYBOARD = 0x00000100 | AINPUT_SOURCE_CLASS_BUTTON,
	AINPUT_SOURCE_DPAD = 0x00000200 | AINPUT_SOURCE_CLASS_BUTTON,
	AINPUT_SOURCE_GAMEPAD = 0x00000400 | AINPUT_SOURCE_CLASS_BUTTON,
	AINPUT_SOURCE_TOUCHSCREEN = 0x00001000 | AINPUT_SOURCE_CLASS_POINTER,
	AINPUT_SOURCE_MOUSE = 0x00002000 | AINPUT_SOURCE_CLASS_POINTER,
	AINPUT_SOURCE_STYLUS = 0x00004000 | AINPUT_SOURCE_CLASS_POINTER,
	AINPUT_SOURCE_BLUETOOTH_STYLUS = 0x00008000 | AINPUT_SOURCE_STYLUS,
	AINPUT_SOURCE_TRACKBALL = 0x00010000 | AINPUT_SOURCE_CLASS_NAVIGATION,
	AINPUT_SOURCE_MOUSE_RELATIVE = 0x00020000 | AINPUT_SOURCE_CLASS_NAVIGATION,
	AINPUT_SOURCE_TOUCHPAD = 0x00100000 | AINPUT_SOURCE_CLASS_POSITION,
	AINPUT_SOURCE_TOUCH_NAVIGATION = 0x00200000 | AINPUT_SOURCE_CLASS_NONE,
	AINPUT_SOURCE_JOYSTICK = 0x01000000 | AINPUT_SOURCE_CLASS_JOYSTICK,
	AINPUT_SOURCE_HDMI = 0x02000000 | AINPUT_SOURCE_CLASS_BUTTON,
	AINPUT_SOURCE_SENSOR = 0x04000000 | AINPUT_SOURCE_CLASS_NONE,
	AINPUT_SOURCE_ROTARY_ENCODER = 0x00400000 | AINPUT_SOURCE_CLASS_NONE,
	AINPUT_SOURCE_ANY = 0xffffff00
};

enum {
	AMOTION_EVENT_ACTION_MASK = 0xff,
	AMOTION_EVENT_ACTION_POINTER_INDEX_MASK = 0xff00,
	AMOTION_EVENT_ACTION_DOWN = 0,
	AMOTION_EVENT_ACTION_UP = 1,
	AMOTION_EVENT_ACTION_MOVE = 2,
	AMOTION_EVENT_ACTION_CANCEL = 3,
	AMOTION_EVENT_ACTION_OUTSIDE = 4,
	AMOTION_EVENT_ACTION_POINTER_DOWN = 5,
	AMOTION_EVENT_ACTION_POINTER_UP = 6,
	AMOTION_EVENT_ACTION_HOVER_MOVE = 7,
	AMOTION_EVENT_ACTION_SCROLL = 8,
	AMOTION_EVENT_ACTION_HOVER_ENTER = 9,
	AMOTION_EVENT_ACTION_HOVER_EXIT = 10,
	AMOTION_EVENT_ACTION_BUTTON_PRESS = 11,
	AMOTION_EVENT_ACTION_BUTTON_RELEASE = 12
};

// TODO: since we have to shove this struct through pipes, we might want to use different structs
// for each event and have an event type field consistent between them so we know what to cast to
struct AInputEvent {
	double x;
	double y;
	int32_t action;
};

typedef void AInputQueue;

struct ALooper;
typedef int (*Looper_callbackFunc)(int fd, int events, void* data);

float AMotionEvent_getAxisValue(const struct AInputEvent* motion_event, int32_t axis, size_t pointer_index)
{
	return -1; // no clue what to do here
}

size_t AMotionEvent_getPointerCount(const struct AInputEvent* motion_event)
{
	return 1; // FIXME
}

int32_t AInputEvent_getType(const struct AInputEvent* event)
{
	if(event) {
		return AINPUT_EVENT_TYPE_MOTION; // FIXME
	} else {
		return -1;
	}
}

int32_t AInputEvent_getSource(const struct AInputEvent* event)
{
	if(event) {
		return AINPUT_SOURCE_TOUCHSCREEN; // FIXME
	} else {
		return -1;
	}
}

int32_t AMotionEvent_getAction(const struct AInputEvent* motion_event)
{
	if(motion_event) {
		return motion_event->action;
	} else {
		return -1;
	}
}

int32_t AMotionEvent_getPointerId(const struct AInputEvent* motion_event, size_t pointer_index)
{
	if(motion_event) {
		return 1; // FIXME
	} else {
		return -1; // 0?
	}
}

float AMotionEvent_getX(const struct AInputEvent* motion_event, size_t pointer_index)
{
	if(motion_event) {
		return motion_event->x;
	} else {
		return -1;
	}
}

float AMotionEvent_getY(const struct AInputEvent* motion_event, size_t pointer_index)
{
	if(motion_event) {
		return motion_event->y;
	} else {
		return -1;
	}
}

void AInputQueue_detachLooper(AInputQueue* queue)
{
	return;
}

struct android_poll_source {
    // The identifier of this source.  May be LOOPER_ID_MAIN or
    // LOOPER_ID_INPUT.
    int32_t id;

    // The android_app this ident is associated with.
    struct android_app* app;

    // Function to call to perform the standard processing of data from
    // this source.
    void (*process)(struct android_app* app, struct android_poll_source* source);
};

// TODO: malloc on getEvent and free on finishEvent? malloc isn't very fast though, and events can in principle be pretty frequent
struct AInputEvent fixme_ugly_current_event;

static inline void make_touch_event(GdkEvent* event, GtkEventControllerLegacy* event_controller, struct AInputEvent *ainput_event)
{
	GtkWidget *window = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(event_controller));
	GtkWidget *child;

	gdk_event_get_position(event, &ainput_event->x, &ainput_event->y);

	// the window's coordinate system starts at the top left of the header bar, which is not ideal
	// apps expect it to start at the top left of the area where child widgets get placed, so that
	// the top left of the window is the same as the top left of a single widget filling the entire window
	// while it's quite hacky, the following should realistically work for most if not all cases
	if((child = gtk_window_get_child(GTK_WINDOW(window)))) {
		int ret;
		graphene_point_t p;
		ret = gtk_widget_compute_point(window, child, &GRAPHENE_POINT_INIT(ainput_event->x, ainput_event->y), &p);
		assert(ret);

		ainput_event->x = p.x;
		ainput_event->y = p.y;
	}

	switch(gdk_event_get_event_type(event)) {
		case GDK_BUTTON_PRESS:
		case GDK_TOUCH_BEGIN:
			ainput_event->action = AMOTION_EVENT_ACTION_DOWN;
			break;
		case GDK_BUTTON_RELEASE:
		case GDK_TOUCH_END:
			ainput_event->action = AMOTION_EVENT_ACTION_UP;
			break;
		case GDK_MOTION_NOTIFY:
		case GDK_TOUCH_UPDATE:
			ainput_event->action = AMOTION_EVENT_ACTION_MOVE;
			break;
		default:
			fprintf(stderr, "%s: %s: passed in GdkEvent is not a touch event or equivalent\n", __FILE__, __func__);
			break;
	}
}

static gboolean on_event(GtkEventControllerLegacy* self, GdkEvent* event, int input_queue_pipe_fd)
{
	struct AInputEvent ainput_event;

	// TODO: this doesn't work for multitouch
	switch(gdk_event_get_event_type(event)) {
		// mouse click/move (currently we convert these to touch events)
		case GDK_BUTTON_PRESS:
		case GDK_BUTTON_RELEASE:
		case GDK_MOTION_NOTIFY:
		// touchscreen
		case GDK_TOUCH_BEGIN:
		case GDK_TOUCH_END:
		case GDK_TOUCH_UPDATE:
			make_touch_event(event, self, &ainput_event);
			write(input_queue_pipe_fd, &ainput_event, sizeof(struct AInputEvent));
			break;
		default:
			return false;
			break;
	}

	return true;
}

// FIXME put this in a header file
struct input_queue {
	int fd;
	GtkEventController *controller;
};

void AInputQueue_attachLooper(struct input_queue* queue, struct ALooper* looper, int ident, Looper_callbackFunc callback, void* data)
{
	struct android_poll_source *poll_source = (struct android_poll_source *)data;
	//printf("AInputQueue_attachLooper called: queue: %p, looper: %p, ident: %d, callback %p, data: %p, process_func: %p\n", queue, looper, ident, callback, poll_source, poll_source ? poll_source->process : 0);
	if (poll_source == NULL)
		return;
	int input_queue_pipe[2];
	if (pipe(input_queue_pipe)) {
		fprintf(stderr, "could not create pipe: %s", strerror(errno));
		return;
	}
	fcntl(input_queue_pipe[0], F_SETFL, O_NONBLOCK);
	ALooper_addFd(looper, input_queue_pipe[0], ident, (1 << 0)/*? ALOOPER_EVENT_INPUT*/, callback, data);
	g_signal_connect(queue->controller, "event", G_CALLBACK(on_event), GINT_TO_POINTER(input_queue_pipe[1]));
	queue->fd = input_queue_pipe[0];
}

int32_t AInputQueue_getEvent(struct input_queue *queue, struct AInputEvent** outEvent)
{
	if(read(queue->fd, &fixme_ugly_current_event, sizeof(struct AInputEvent)) == sizeof(struct AInputEvent)) {
		*outEvent = &fixme_ugly_current_event;
		return 0;
	} else {
		return -1; // no events or error
	}
}

int32_t AInputQueue_preDispatchEvent(AInputQueue* queue, struct AInputEvent* event)
{
	return 0; // we don't want to claim the event for ourselves, let the app process it
}

void AInputQueue_finishEvent(AInputQueue* queue, struct AInputEvent* event, int handled)
{
	// should we do something here?
}

int32_t AKeyEvent_getKeyCode(struct AInputEvent *event)
{
	/*
	 * TODO: Minecraft PE misuses this function on an event
	 * that is not an AINPUT_EVENT_TYPE_KEY, which would lead to
	 * undefined behaviour, so we would need to check if the event
	 * is of type AINPUT_EVENT_TYPE_KEY before casting it to anything
	 * to not break Minecraft PE when we actually implement this function.
	*/

	printf("HACK: getKeyCode stubbed, returning 0!");
	return 0;
}
