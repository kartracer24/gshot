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
#include <gtk/gtkgestureclick.h>

#include "screenshot-area-selection.h"
#include "screenshot-config.h"

typedef struct {
  GdkRectangle  rect;
  gboolean      button_pressed;
  GtkWidget    *window;
  GtkDrawingArea *drawing_area;

  gboolean      aborted;
  gint          start_x;
  gint          start_y;
} select_area_data;

static void
select_area_draw (GtkDrawingArea *drawing_area,
                  cairo_t        *cr,
                  int             width,
                  int             height,
                  gpointer        user_data)
{
  select_area_data *data = user_data;

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
}

static gboolean
select_area_button_press (GtkEventController *controller,
                         gint                n_press,
                         double              x,
                         double              y,
                         select_area_data   *data)
{
  g_print ("button_press: n_press=%d, x=%.0f, y=%.0f\n", n_press, x, y);

  if (data->button_pressed)
    return TRUE;

  data->button_pressed = TRUE;
  data->start_x = (gint)x;
  data->start_y = (gint)y;
  data->rect.x = data->start_x;
  data->rect.y = data->start_y;
  data->rect.width = 0;
  data->rect.height = 0;

  GtkWidget *widget = gtk_event_controller_get_widget (controller);
  g_print ("button_press: widget=%p\n", widget);

  if (widget == NULL)
    {
      g_print ("button_press: widget is NULL!\n");
      return TRUE;
    }

  GtkNative *native = gtk_widget_get_native (widget);
  g_print ("button_press: native=%p\n", native);

  if (native == NULL)
    {
      g_print ("button_press: native is NULL!\n");
      return TRUE;
    }

  GdkSurface *surface = gtk_native_get_surface (native);
  g_print ("button_press: surface=%p\n", surface);

  if (surface == NULL)
    {
      g_print ("button_press: surface is NULL!\n");
      return TRUE;
    }

  GdkDisplay *display = gtk_widget_get_display (widget);
  g_print ("button_press: display=%p\n", display);

  if (display == NULL)
    {
      g_print ("button_press: display is NULL!\n");
      return TRUE;
    }

  GdkMonitor *monitor = gdk_display_get_monitor_at_surface (display, surface);
  g_print ("button_press: monitor=%p\n", monitor);
  screenshot_target_monitor = monitor;
  g_print ("button_press: target_monitor set\n");

  gtk_widget_queue_draw (widget);
  g_print ("button_press: queue_draw done\n");

  return TRUE;
}

static gboolean
select_area_button_release (GtkEventController *controller,
                          gint                n_press,
                          double              x,
                          double              y,
                          select_area_data   *data)
{
  g_print ("button_release: start\n");
  if (!data->button_pressed)
    return TRUE;

  data->rect.width = ABS ((gint)x - data->start_x);
  data->rect.height = ABS ((gint)y - data->start_y);
  data->rect.x = MIN ((gint)x, data->start_x);
  data->rect.y = MIN ((gint)y, data->start_y);

  g_print ("button_release: destroying window\n");
  gtk_window_destroy (GTK_WINDOW (data->window));
  g_print ("button_release: done\n");

  return TRUE;
}

static gboolean
select_area_motion (GtkEventController *controller,
                    double              x,
                    double              y,
                    select_area_data   *data)
{
  if (!data->button_pressed)
    return TRUE;

  data->rect.width = ABS ((gint)x - data->start_x);
  data->rect.height = ABS ((gint)y - data->start_y);
  data->rect.x = MIN ((gint)x, data->start_x);
  data->rect.y = MIN ((gint)y, data->start_y);

  GtkWidget *widget = gtk_event_controller_get_widget (controller);
  gtk_widget_queue_draw (widget);

  return TRUE;
}

static gboolean
select_area_key_press (GtkEventController *controller,
                       guint               keyval,
                       guint               keycode,
                       GdkModifierType     state,
                       select_area_data   *data)
{
  if (keyval == GDK_KEY_Escape)
    {
      data->rect.x = 0;
      data->rect.y = 0;
      data->rect.width  = 0;
      data->rect.height = 0;
      data->aborted = TRUE;

      gtk_window_destroy (GTK_WINDOW (data->window));
    }

  return TRUE;
}

static GtkWidget *
create_select_window (int *width, int *height)
{
  GtkWidget *window;
  GdkDisplay *display;
  GdkMonitor *monitor;
  GdkRectangle geom;
  GListModel *monitors;
  guint monitor_count = 0;

  display = gdk_display_get_default ();

  if (screenshot_target_monitor != NULL)
    {
      monitor = screenshot_target_monitor;
      gdk_monitor_get_geometry (monitor, &geom);
      monitors = gdk_display_get_monitors (display);
      if (monitors != NULL)
        monitor_count = g_list_model_get_n_items (monitors);
    }
  else
    {
      monitors = gdk_display_get_monitors (display);

      if (monitors != NULL && g_list_model_get_n_items (monitors) > 0)
        {
          monitor = GDK_MONITOR (g_list_model_get_item (monitors, 0));
          gdk_monitor_get_geometry (monitor, &geom);
          monitor_count = g_list_model_get_n_items (monitors);
        }
      else
        {
          geom.x = 0;
          geom.y = 0;
          geom.width = 1920;
          geom.height = 1080;
        }
    }

  *width = geom.width;
  *height = geom.height;

  window = gtk_window_new ();
  gtk_window_set_decorated (GTK_WINDOW (window), FALSE);
  gtk_window_set_resizable (GTK_WINDOW (window), FALSE);
  gtk_window_set_default_size (GTK_WINDOW (window), geom.width, geom.height);
  gtk_widget_set_size_request (window, geom.width, geom.height);

  GtkCssProvider *css_provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_string (css_provider,
      ".window { background-color: rgba(0, 0, 0, 0.0); }");

  gtk_style_context_add_provider_for_display (
      gtk_widget_get_display (window),
      (GtkStyleProvider *) css_provider, GTK_STYLE_PROVIDER_PRIORITY_USER);

  gtk_widget_add_css_class (window, "window");

  if (monitor_count > 1)
    {
      gtk_window_fullscreen (GTK_WINDOW (window));
    }
  else
    {
      gtk_window_present (GTK_WINDOW (window));
    }

  gtk_widget_set_visible (window, TRUE);

  if (monitor_count > 1)
    {
      gtk_window_fullscreen (GTK_WINDOW (window));
    }
  else
    {
      gtk_window_present (GTK_WINDOW (window));
    }

  gtk_widget_set_visible (window, TRUE);

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

static gboolean
quit_main_loop (gpointer user_data)
{
  GMainLoop *loop = user_data;
  g_main_loop_quit (loop);
  return FALSE;
}

static void
screenshot_select_area_wayland_async (CallbackData *cb_data)
{
  select_area_data data;
  GtkEventController *motion_controller;
  GtkEventController *button_controller;
  GtkEventController *key_controller;
  GMainLoop *main_loop;
  int width, height;

  g_print ("select_area_wayland_async: starting\n");

  data.rect.x = 0;
  data.rect.y = 0;
  data.rect.width  = 0;
  data.rect.height = 0;
  data.button_pressed = FALSE;
  data.aborted = FALSE;
  data.start_x = 0;
  data.start_y = 0;

  main_loop = g_main_loop_new (NULL, FALSE);
  g_print ("creating select window\n");

  data.window = create_select_window (&width, &height);

  g_print ("creating drawing area\n");
  data.drawing_area = GTK_DRAWING_AREA (gtk_drawing_area_new ());
  gtk_drawing_area_set_draw_func (data.drawing_area, select_area_draw, &data, NULL);
  gtk_widget_set_hexpand (GTK_WIDGET (data.drawing_area), TRUE);
  gtk_widget_set_vexpand (GTK_WIDGET (data.drawing_area), TRUE);
  gtk_widget_set_can_focus (GTK_WIDGET (data.drawing_area), TRUE);
  gtk_window_set_child (GTK_WINDOW (data.window), GTK_WIDGET (data.drawing_area));
  gtk_widget_set_visible (GTK_WIDGET (data.drawing_area), TRUE);

  g_print ("setting cursor\n");
  GdkCursor *cursor = gdk_cursor_new_from_name ("crosshair", NULL);
  gtk_widget_set_cursor (GTK_WIDGET (data.window), cursor);
  g_object_unref (cursor);

  g_print ("creating key controller\n");
  key_controller = gtk_event_controller_key_new ();
  g_signal_connect (key_controller, "key-pressed", G_CALLBACK (select_area_key_press), &data);
  gtk_widget_add_controller (GTK_WIDGET (data.window), key_controller);

  g_print ("creating motion controller\n");
  motion_controller = gtk_event_controller_motion_new ();
  g_signal_connect (motion_controller, "motion", G_CALLBACK (select_area_motion), &data);
  gtk_widget_add_controller (GTK_WIDGET (data.drawing_area), motion_controller);

  g_print ("creating button controller\n");
  button_controller = GTK_EVENT_CONTROLLER (gtk_gesture_click_new ());
  g_signal_connect (button_controller, "pressed", G_CALLBACK (select_area_button_press), &data);
  g_signal_connect (button_controller, "released", G_CALLBACK (select_area_button_release), &data);
  gtk_widget_add_controller (GTK_WIDGET (data.drawing_area), button_controller);

  g_print ("connecting destroy signal\n");
  g_signal_connect_swapped (data.window, "destroy", G_CALLBACK (quit_main_loop), main_loop);

  g_print ("running main loop\n");
  g_main_loop_run (main_loop);

  g_print ("main loop done\n");
  g_main_loop_unref (main_loop);

  cb_data->aborted = data.aborted;
  cb_data->rectangle = data.rect;

  g_print ("adding idle callback\n");
  g_timeout_add (200, emit_select_callback_in_idle, cb_data);
}

void
screenshot_select_area_async (SelectAreaCallback callback,
                              gpointer callback_data)
{
  g_print ("screenshot_select_area_async called\n");
  CallbackData *cb_data;

  cb_data = g_slice_new0 (CallbackData);
  cb_data->callback = callback;
  cb_data->callback_data = callback_data;

  screenshot_select_area_wayland_async (cb_data);
}
