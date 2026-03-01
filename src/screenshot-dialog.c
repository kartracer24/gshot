/* screenshot-dialog.c - main GNOME Screenshot dialog
 *
 * Copyright (C) 2001-2006  Jonathan Blandford <jrb@alum.mit.edu>
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

#include "screenshot-config.h"
#include "screenshot-dialog.h"
#include "screenshot-utils.h"
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <string.h>
#include <stdlib.h>
#include <gtk/gtkfiledialog.h>

struct _ScreenshotDialog
{
  GtkApplicationWindow parent_instance;

  GdkPixbuf *screenshot;
  GdkPixbuf *preview_image;

  GtkWidget *save_widget;
  GtkWidget *filename_entry;
  GtkWidget *preview_darea;
  GtkWidget *save_button;
  GtkWidget *copy_button;
  GtkWidget *back_button;

  GtkFileDialog *file_dialog;

  gint drag_x;
  gint drag_y;
};

G_DEFINE_TYPE (ScreenshotDialog, screenshot_dialog, GTK_TYPE_APPLICATION_WINDOW)

enum {
  SIGNAL_SAVE,
  SIGNAL_COPY,
  SIGNAL_BACK,
  N_SIGNALS,
};

static guint signals[N_SIGNALS];

static void
preview_draw_cb (GtkDrawingArea   *drawing_area,
                 cairo_t          *cr,
                 int               width,
                 int               height,
                 ScreenshotDialog *self)
{
  gint scale, scaled_width, scaled_height, image_width, image_height, x, y;

  scale = gtk_widget_get_scale_factor (GTK_WIDGET (drawing_area));
  scaled_width = width * scale;
  scaled_height = height * scale;

  image_width = gdk_pixbuf_get_width (self->screenshot);
  image_height = gdk_pixbuf_get_height (self->screenshot);

  if (image_width > scaled_width)
    {
      image_height = (gdouble) image_height / image_width * scaled_width;
      image_width = scaled_width;
    }

  if (image_height > scaled_height)
    {
      image_width = (gdouble) image_width / image_height * scaled_height;
      image_height = scaled_height;
    }

  x = (scaled_width - image_width) / 2;
  y = (scaled_height - image_height) / 2;

  if (!self->preview_image ||
      gdk_pixbuf_get_width (self->preview_image) != image_width ||
      gdk_pixbuf_get_height (self->preview_image) != image_height)
    {
      g_clear_object (&self->preview_image);
      self->preview_image = gdk_pixbuf_scale_simple (self->screenshot,
                                                     image_width,
                                                     image_height,
                                                     GDK_INTERP_BILINEAR);
    }

  cairo_save (cr);

  cairo_scale (cr, 1.0 / scale, 1.0 / scale);
  gdk_cairo_set_source_pixbuf (cr, self->preview_image, x, y);
  cairo_paint (cr);

  cairo_restore (cr);
}

static void
back_clicked_cb (GtkButton        *button,
                 ScreenshotDialog *self)
{
  g_signal_emit (self, signals[SIGNAL_BACK], 0);
}

static void
save_clicked_cb (GtkButton        *button,
                 ScreenshotDialog *self)
{
  g_signal_emit (self, signals[SIGNAL_SAVE], 0);
}

static void
copy_clicked_cb (GtkButton        *button,
                 ScreenshotDialog *self)
{
  g_signal_emit (self, signals[SIGNAL_COPY], 0);
}

static void
screenshot_dialog_class_init (ScreenshotDialogClass *klass)
{
  signals[SIGNAL_SAVE] =
    g_signal_new ("save",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  0);

  signals[SIGNAL_COPY] =
    g_signal_new ("copy",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  0);

  signals[SIGNAL_BACK] =
    g_signal_new ("back",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  0);
}

static void
screenshot_dialog_init (ScreenshotDialog *self)
{
}

ScreenshotDialog *
screenshot_dialog_new (GtkApplication *app,
                       GdkPixbuf      *screenshot,
                       char           *initial_uri)
{
  g_autoptr(GFile) tmp_file = NULL, parent_file = NULL;
  g_autofree gchar *current_folder = NULL, *current_name = NULL;
  ScreenshotDialog *self;
  char *ext;
  gint pos;

  tmp_file = g_file_new_for_uri (initial_uri);
  parent_file = g_file_get_parent (tmp_file);

  current_name = g_file_get_basename (tmp_file);
  current_folder = g_file_get_uri (parent_file);

  self = g_object_new (SCREENSHOT_TYPE_DIALOG,
                       "application", app,
                       NULL);

  self->screenshot = screenshot;

  gtk_widget_set_visible (GTK_WIDGET (self), TRUE);

  ext = g_utf8_strrchr (current_name, -1, '.');
  if (ext)
    pos = g_utf8_strlen (current_name, -1) - g_utf8_strlen (ext, -1);
  else
    pos = -1;

  gtk_widget_grab_focus (self->filename_entry);
  gtk_editable_select_region (GTK_EDITABLE (self->filename_entry),
                             0,
                             pos);

  return self;
}

char *
screenshot_dialog_get_uri (ScreenshotDialog *self)
{
  g_autofree gchar *folder = NULL;
  const gchar *file_name;
  gchar *file;
  gchar *tmp;

  folder = screenshot_dialog_get_folder (self);
  file_name = gtk_editable_get_text (GTK_EDITABLE (self->filename_entry));
  file = g_uri_escape_string (file_name, NULL, FALSE);

  tmp = g_build_filename (folder, file, NULL);
  g_free (folder);
  g_free (file);

  return tmp;
}

char *
screenshot_dialog_get_folder (ScreenshotDialog *self)
{
  return g_strdup (g_get_user_special_dir (G_USER_DIRECTORY_PICTURES));
}

char *
screenshot_dialog_get_filename (ScreenshotDialog *self)
{
  g_autoptr(GError) error = NULL;
  const gchar *file_name;
  gchar *tmp;

  file_name = gtk_editable_get_text (GTK_EDITABLE (self->filename_entry));
  tmp = g_filename_from_utf8 (file_name, -1, NULL, NULL, &error);

  if (error != NULL)
    {
      g_warning ("Unable to convert `%s' to valid UTF-8: %s\n"
                 "Falling back to default file.",
                 file_name,
                 error->message);
      tmp = g_strdup (_("Screenshot.png"));
    }

  return tmp;
}

void
screenshot_dialog_set_busy (ScreenshotDialog *self,
                             gboolean          busy)
{
  if (busy)
    {
      GdkCursor *cursor;
      GdkDisplay *display;
      display = gtk_widget_get_display (GTK_WIDGET (self));
      cursor = gdk_cursor_new_from_name ("watch", NULL);
      gtk_widget_set_cursor (GTK_WIDGET (self), cursor);
      g_object_unref (cursor);
    }
  else
    {
      gtk_widget_set_cursor (GTK_WIDGET (self), NULL);
    }

  gtk_widget_set_sensitive (GTK_WIDGET (self), !busy);
}

GtkWidget *
screenshot_dialog_get_filename_entry (ScreenshotDialog *self)
{
  return self->filename_entry;
}
