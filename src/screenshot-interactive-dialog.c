/* screenshot-interactive-dialog.c - Interactive options dialog
 *
 * Copyright (C) 2001 Jonathan Blandford <jrb@alum.mit.edu>
 * Copyright (C) 2006 Emmanuele Bassi <ebassi@gnome.org>
 * Copyright (C) 2008, 2011 Cosimo Cecchi <cosimoc@gnome.org>
 * Copyright (C) 2013 Nils Dagsson Moskopp <nils@dieweltistgarnichtso.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301
 * USA
 */

#include "config.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "screenshot-config.h"
#include "screenshot-interactive-dialog.h"

typedef enum {
  SCREENSHOT_MODE_SCREEN,
  SCREENSHOT_MODE_WINDOW,
  SCREENSHOT_MODE_SELECTION,
} ScreenshotMode;

struct _ScreenshotInteractiveDialog
{
  GtkApplicationWindow parent_instance;

  GtkWidget *radio_screen;
  GtkWidget *radio_window;
  GtkWidget *radio_selection;
  GtkWidget *check_pointer;
  GtkWidget *check_dark_mode;
  GtkWidget *spin_delay;
  GtkWidget *button_capture;
};

G_DEFINE_TYPE (ScreenshotInteractiveDialog, screenshot_interactive_dialog, GTK_TYPE_APPLICATION_WINDOW)

enum {
  SIGNAL_CAPTURE,
  N_SIGNALS,
};

static guint signals[N_SIGNALS];

static void
set_mode (ScreenshotInteractiveDialog *self,
          ScreenshotMode               mode)
{
  gboolean take_window_shot = (mode == SCREENSHOT_MODE_WINDOW);
  gboolean take_area_shot = (mode == SCREENSHOT_MODE_SELECTION);

  gtk_widget_set_sensitive (self->check_pointer, !take_area_shot);

  screenshot_config->take_window_shot = take_window_shot;
  screenshot_config->take_area_shot = take_area_shot;
}

static void
screen_toggled_cb (GtkToggleButton             *button,
                    ScreenshotInteractiveDialog *self)
{
  if (gtk_toggle_button_get_active (button))
    set_mode (self, SCREENSHOT_MODE_SCREEN);
}

static void
window_toggled_cb (GtkToggleButton             *button,
                    ScreenshotInteractiveDialog *self)
{
  if (gtk_toggle_button_get_active (button))
    set_mode (self, SCREENSHOT_MODE_WINDOW);
}

static void
selection_toggled_cb (GtkToggleButton             *button,
                      ScreenshotInteractiveDialog *self)
{
  if (gtk_toggle_button_get_active (button))
    set_mode (self, SCREENSHOT_MODE_SELECTION);
}

static void
delay_spin_value_changed_cb (GtkSpinButton               *button,
                             ScreenshotInteractiveDialog *self)
{
  screenshot_config->delay = gtk_spin_button_get_value_as_int (button);
}

static void
include_pointer_toggled_cb (GtkSwitch                   *toggle,
                            ScreenshotInteractiveDialog *self)
{
  screenshot_config->include_pointer = gtk_switch_get_active (toggle);
  gtk_switch_set_state (toggle, gtk_switch_get_active (toggle));
}

static void
capture_button_clicked_cb (GtkButton                   *button,
                           ScreenshotInteractiveDialog *self)
{
  g_signal_emit (self, signals[SIGNAL_CAPTURE], 0);
}

static void
dark_mode_toggled_cb (GtkSwitch                   *toggle,
                      ScreenshotInteractiveDialog *self)
{
  GtkCssProvider *provider;
  GdkDisplay *display;

  screenshot_config->dark_mode = gtk_switch_get_active (toggle);

  display = gdk_display_get_default ();
  if (display == NULL)
    return;

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_string (provider, "");

  if (screenshot_config->dark_mode)
    g_object_set (provider, "prefers-color-scheme", GTK_INTERFACE_COLOR_SCHEME_DARK, NULL);

  gtk_style_context_add_provider_for_display (display,
                                             GTK_STYLE_PROVIDER (provider),
                                             GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  g_object_unref (provider);
}

static void
screenshot_interactive_dialog_class_init (ScreenshotInteractiveDialogClass *klass)
{
  signals[SIGNAL_CAPTURE] =
    g_signal_new ("capture",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  0);
}

static void
screenshot_interactive_dialog_init (ScreenshotInteractiveDialog *self)
{
  GtkWidget *box;
  GtkWidget *frame;
  GtkWidget *label;
  GtkWidget *radio1, *radio2, *radio3;

  gtk_window_set_title (GTK_WINDOW (self), _("Screenshot"));
  gtk_window_set_default_size (GTK_WINDOW (self), 300, 250);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
  gtk_widget_set_margin_top (box, 12);
  gtk_widget_set_margin_bottom (box, 12);
  gtk_widget_set_margin_start (box, 12);
  gtk_widget_set_margin_end (box, 12);
  gtk_window_set_child (GTK_WINDOW (self), box);

  frame = gtk_frame_new (_("Take a screenshot of:"));
  gtk_box_append (GTK_BOX (box), frame);

  GtkWidget *radio_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_frame_set_child (GTK_FRAME (frame), radio_box);

  radio1 = gtk_check_button_new_with_label (_("Entire screen"));
  gtk_box_append (GTK_BOX (radio_box), radio1);
  g_signal_connect (radio1, "toggled", G_CALLBACK (screen_toggled_cb), self);
  self->radio_screen = radio1;

  radio2 = gtk_check_button_new_with_label (_("Selected window"));
  gtk_box_append (GTK_BOX (radio_box), radio2);
  g_signal_connect (radio2, "toggled", G_CALLBACK (window_toggled_cb), self);
  self->radio_window = radio2;

  radio3 = gtk_check_button_new_with_label (_("Selected area"));
  gtk_box_append (GTK_BOX (radio_box), radio3);
  g_signal_connect (radio3, "toggled", G_CALLBACK (selection_toggled_cb), self);
  self->radio_selection = radio3;

  frame = gtk_frame_new (NULL);
  gtk_box_append (GTK_BOX (box), frame);

  GtkWidget *options_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_frame_set_child (GTK_FRAME (frame), options_box);

  self->check_pointer = gtk_switch_new ();
  label = gtk_label_new (_("Include pointer"));
  GtkWidget *pointer_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_box_append (GTK_BOX (pointer_box), self->check_pointer);
  gtk_box_append (GTK_BOX (pointer_box), label);
  gtk_box_append (GTK_BOX (options_box), pointer_box);
  g_signal_connect (self->check_pointer, "state-set", G_CALLBACK (include_pointer_toggled_cb), self);

  self->check_dark_mode = gtk_switch_new ();
  label = gtk_label_new (_("Dark mode"));
  GtkWidget *dark_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_box_append (GTK_BOX (dark_box), self->check_dark_mode);
  gtk_box_append (GTK_BOX (dark_box), label);
  gtk_box_append (GTK_BOX (options_box), dark_box);
  g_signal_connect (self->check_dark_mode, "state-set", G_CALLBACK (dark_mode_toggled_cb), self);

  GtkWidget *delay_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  label = gtk_label_new (_("Delay (seconds):"));
  self->spin_delay = gtk_spin_button_new_with_range (0, 60, 1);
  gtk_box_append (GTK_BOX (delay_box), label);
  gtk_box_append (GTK_BOX (delay_box), self->spin_delay);
  gtk_box_append (GTK_BOX (options_box), delay_box);
  g_signal_connect (self->spin_delay, "value-changed", G_CALLBACK (delay_spin_value_changed_cb), self);

  self->button_capture = gtk_button_new_with_label (_("Take Screenshot"));
  gtk_widget_set_hexpand (self->button_capture, TRUE);
  gtk_box_append (GTK_BOX (box), self->button_capture);
  g_signal_connect (self->button_capture, "clicked", G_CALLBACK (capture_button_clicked_cb), self);

  gtk_widget_set_visible (GTK_WIDGET (self), TRUE);

  if (screenshot_config->take_window_shot)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->radio_window), TRUE);

  if (screenshot_config->take_area_shot)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->radio_selection), TRUE);

  gtk_widget_set_sensitive (self->check_pointer, !screenshot_config->take_area_shot);
  gtk_switch_set_active (GTK_SWITCH (self->check_pointer), screenshot_config->include_pointer);
  gtk_switch_set_active (GTK_SWITCH (self->check_dark_mode), screenshot_config->dark_mode);

  gtk_spin_button_set_value (GTK_SPIN_BUTTON (self->spin_delay), screenshot_config->delay);
}

ScreenshotInteractiveDialog *
screenshot_interactive_dialog_new (GtkApplication *app)
{
  g_return_val_if_fail (GTK_IS_APPLICATION (app), NULL);

  return g_object_new (SCREENSHOT_TYPE_INTERACTIVE_DIALOG,
                       "application", app,
                       NULL);
}
