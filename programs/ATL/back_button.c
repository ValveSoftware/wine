#include "back_button.h"

#include "../api-impl-jni/app/android_app_Activity.h"

GtkWidget *back_button;

GtkWidget* back_button_new() {
	back_button = gtk_button_new_from_icon_name("go-previous");

	g_signal_connect(back_button, "clicked", G_CALLBACK(current_activity_back_pressed), NULL);

	back_button_set_sensitive(false);
	return back_button;
}

void back_button_set_sensitive(bool sensitive) {
	gtk_widget_set_sensitive(GTK_WIDGET(back_button), sensitive);
}
