/* screenshot-area-selection.c - interactive screenshot area selection
 *
 * Copyright (C) 2001-2006  Jonathan Blandford <jrb@alum.mit.edu>
 * Copyright (C) 2008 Cosimo Cecchi <cosimoc@gnome.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 */

#include "config.h"

#include <gtk/gtk.h>

#include "screenshot-area-selection.h"
#include "screenshot-config.h"

typedef struct {
  GdkRectangle  rect;
  gboolean      button_pressed;
  GtkWidget    *window;

  gboolean      aborted;
  gint          start_x;
  gint          start_y;
} select_area_filter_data;

static gboolean
select_area_button_press (GtkWidget               *window,
                          GdkEventButton          *event,
                          select_area_filter_data *data)
{
  if (data->button_pressed)
    return TRUE;

  data->button_pressed = TRUE;
  data->start_x = (gint)event->x;
  data->start_y = (gint)event->y;
  data->rect.x = data->start_x;
  data->rect.y = data->start_y;
  data->rect.width = 0;
  data->rect.height = 0;

  GdkDisplay *display = gtk_widget_get_display (window);
  GdkMonitor *monitor = gdk_display_get_monitor_at_point (display, (gint)event->x_root, (gint)event->y_root);
  screenshot_target_monitor = monitor;

  gtk_widget_queue_draw (window);

  return TRUE;
}

static gboolean
select_area_button_release (GtkWidget               *window,
                             GdkEventButton          *event,
                             select_area_filter_data *data)
{
  if (!data->button_pressed)
    return TRUE;

  data->rect.width = ABS ((gint)event->x - data->start_x);
  data->rect.height = ABS ((gint)event->y - data->start_y);
  data->rect.x = MIN ((gint)event->x, data->start_x);
  data->rect.y = MIN ((gint)event->y, data->start_y);

  gtk_main_quit ();

  return TRUE;
}

static gboolean
select_area_motion_notify (GtkWidget               *window,
                           GdkEventMotion          *event,
                           select_area_filter_data *data)
{
  if (!data->button_pressed)
    return TRUE;

  data->rect.width = ABS ((gint)event->x - data->start_x);
  data->rect.height = ABS ((gint)event->y - data->start_y);
  data->rect.x = MIN ((gint)event->x, data->start_x);
  data->rect.y = MIN ((gint)event->y, data->start_y);

  gtk_widget_queue_draw (window);

  return TRUE;
}

static gboolean
select_area_key_press (GtkWidget               *window,
                       GdkEventKey             *event,
                       select_area_filter_data *data)
{
  if (event->keyval == GDK_KEY_Escape)
    {
      data->rect.x = 0;
      data->rect.y = 0;
      data->rect.width  = 0;
      data->rect.height = 0;
      data->aborted = TRUE;

      gtk_main_quit ();
    }

  return TRUE;
}

static gboolean
select_window_draw (GtkWidget *window, cairo_t *cr, gpointer user_data)
{
  select_area_filter_data *data = user_data;
  GtkStyleContext *style;

  if (!data)
    return TRUE;

  style = gtk_widget_get_style_context (window);

  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
  cairo_set_source_rgba (cr, 0, 0, 0, 0.3);
  cairo_paint (cr);

  if (data->button_pressed && data->rect.width > 0 && data->rect.height > 0)
    {
      cairo_set_source_rgba (cr, 0, 0, 0, 0);
      cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
      cairo_rectangle (cr, data->rect.x, data->rect.y, data->rect.width, data->rect.height);
      cairo_fill (cr);

      cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
      cairo_set_source_rgba (cr, 1, 1, 1, 1);
      cairo_set_line_width (cr, 2);
      cairo_rectangle (cr, data->rect.x, data->rect.y, data->rect.width, data->rect.height);
      cairo_stroke (cr);
    }

  return TRUE;
}

static GtkWidget *
create_select_window (void)
{
  GtkWidget *window;
  GdkScreen *screen;
  GdkVisual *visual;
  GdkDisplay *display;
  GdkMonitor *monitor;
  GdkRectangle geom;

  screen = gdk_screen_get_default ();
  visual = gdk_screen_get_rgba_visual (screen);
  display = gdk_display_get_default ();
  monitor = gdk_display_get_primary_monitor (display);

  if (monitor)
    gdk_monitor_get_geometry (monitor, &geom);
  else
    {
      geom.x = 0;
      geom.y = 0;
      geom.width = gdk_screen_get_width (screen);
      geom.height = gdk_screen_get_height (screen);
    }

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_decorated (GTK_WINDOW (window), FALSE);
  gtk_window_set_skip_taskbar_hint (GTK_WINDOW (window), TRUE);
  gtk_window_set_skip_pager_hint (GTK_WINDOW (window), TRUE);
  gtk_window_set_resizable (GTK_WINDOW (window), FALSE);
  
  if (gdk_screen_is_composited (screen) && visual)
    {
      gtk_widget_set_visual (window, visual);
      gtk_widget_set_app_paintable (window, TRUE);
    }

  gtk_window_move (GTK_WINDOW (window), geom.x, geom.y);
  gtk_window_fullscreen (GTK_WINDOW (window));
  gtk_widget_set_size_request (window, geom.width, geom.height);
  gtk_widget_show (window);

  return window;
}

typedef struct {
  GdkRectangle rectangle;
  SelectAreaCallback callback;
  gpointer callback_data;
  gboolean aborted;
} CallbackData;

static gboolean
emit_select_callback_in_idle (gpointer user_data)
{
  CallbackData *data = user_data;

  if (!data->aborted)
    data->callback (&data->rectangle, data->callback_data);
  else
    data->callback (NULL, data->callback_data);

  g_slice_free (CallbackData, data);

  return FALSE;
}

static void
screenshot_select_area_x11_async (CallbackData *cb_data)
{
  select_area_filter_data data;
  GdkCursor *cursor;

  data.rect.x = 0;
  data.rect.y = 0;
  data.rect.width  = 0;
  data.rect.height = 0;
  data.button_pressed = FALSE;
  data.aborted = FALSE;
  data.start_x = 0;
  data.start_y = 0;
  data.window = create_select_window();

  cursor = gdk_cursor_new_for_display (gtk_widget_get_display (data.window), GDK_CROSSHAIR);
  gdk_window_set_cursor (gtk_widget_get_window (data.window), cursor);
  g_object_unref (cursor);

  g_signal_connect (data.window, "draw", G_CALLBACK (select_window_draw), &data);
  g_signal_connect (data.window, "key-press-event", G_CALLBACK (select_area_key_press), &data);
  g_signal_connect (data.window, "button-press-event", G_CALLBACK (select_area_button_press), &data);
  g_signal_connect (data.window, "button-release-event", G_CALLBACK (select_area_button_release), &data);
  g_signal_connect (data.window, "motion-notify-event", G_CALLBACK (select_area_motion_notify), &data);

  gtk_main ();

  gtk_widget_destroy (data.window);

  cb_data->aborted = data.aborted;
  cb_data->rectangle = data.rect;

  g_timeout_add (200, emit_select_callback_in_idle, cb_data);
}

static void
select_area_done (GObject *source_object,
                  GAsyncResult *res,
                  gpointer user_data)
{
  CallbackData *cb_data = user_data;
  g_autoptr(GError) error = NULL;
  g_autoptr(GVariant) ret = NULL;

  ret = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source_object), res, &error);
  if (error != NULL)
    {
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          cb_data->aborted = TRUE;
          g_idle_add (emit_select_callback_in_idle, cb_data);
          return;
        }

      g_message ("Unable to select area using GNOME Shell's builtin screenshot "
                 "interface, using GTK selection.");

      screenshot_select_area_x11_async (cb_data);
      return;
    }

  g_variant_get (ret, "(iiii)",
                 &cb_data->rectangle.x,
                 &cb_data->rectangle.y,
                 &cb_data->rectangle.width,
                 &cb_data->rectangle.height);

  g_idle_add (emit_select_callback_in_idle, cb_data);
}

void
screenshot_select_area_async (SelectAreaCallback callback,
                              gpointer callback_data)
{
  CallbackData *cb_data;
  GDBusConnection *connection;

  cb_data = g_slice_new0 (CallbackData);
  cb_data->callback = callback;
  cb_data->callback_data = callback_data;

  connection = g_application_get_dbus_connection (g_application_get_default ());
  g_dbus_connection_call (connection,
                          "org.gnome.Shell.Screenshot",
                          "/org/gnome/Shell/Screenshot",
                          "org.gnome.Shell.Screenshot",
                          "SelectArea",
                          NULL,
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          G_MAXINT,
                          NULL,
                          select_area_done,
                          cb_data);
}
