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

  if (self->check_pointer != NULL)
    gtk_widget_set_sensitive (self->check_pointer, !take_area_shot);

  if (self->radio_screen != NULL)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->radio_screen), mode == SCREENSHOT_MODE_SCREEN);
  if (self->radio_window != NULL)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->radio_window), mode == SCREENSHOT_MODE_WINDOW);
  if (self->radio_selection != NULL)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->radio_selection), mode == SCREENSHOT_MODE_SELECTION);

  screenshot_config->take_window_shot = take_window_shot;
  screenshot_config->take_area_shot = take_area_shot;
}

static void
screen_toggled_cb (GtkToggleButton           *button,
                   ScreenshotInteractiveDialog *self)
{
  if (gtk_toggle_button_get_active (button))
    {
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->radio_window), FALSE);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->radio_selection), FALSE);
      set_mode (self, SCREENSHOT_MODE_SCREEN);
    }
}

static void
window_toggled_cb (GtkToggleButton           *button,
                   ScreenshotInteractiveDialog *self)
{
  if (gtk_toggle_button_get_active (button))
    {
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->radio_screen), FALSE);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->radio_selection), FALSE);
      set_mode (self, SCREENSHOT_MODE_WINDOW);
    }
}

static void
selection_toggled_cb (GtkToggleButton           *button,
                     ScreenshotInteractiveDialog *self)
{
  if (gtk_toggle_button_get_active (button))
    {
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->radio_screen), FALSE);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->radio_window), FALSE);
      set_mode (self, SCREENSHOT_MODE_SELECTION);
    }
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
  GtkWidget *label;
  GtkWidget *btn_screen, *btn_window, *btn_area;
  GtkWidget *button_box;

  gtk_window_set_title (GTK_WINDOW (self), _("Screenshot"));
  gtk_window_set_default_size (GTK_WINDOW (self), 320, 280);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
  gtk_widget_set_margin_top (box, 18);
  gtk_widget_set_margin_bottom (box, 18);
  gtk_widget_set_margin_start (box, 18);
  gtk_widget_set_margin_end (box, 18);
  gtk_window_set_child (GTK_WINDOW (self), box);

  GtkWidget *capture_label = gtk_label_new (_("Capture Area"));
  gtk_label_set_xalign (GTK_LABEL (capture_label), 0.0);
  gtk_box_append (GTK_BOX (box), capture_label);

  button_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_add_css_class (button_box, "linked");
  gtk_widget_set_margin_top (button_box, 6);
  gtk_widget_set_margin_bottom (button_box, 6);
  gtk_widget_set_margin_start (button_box, 6);
  gtk_widget_set_margin_end (button_box, 6);
  gtk_box_append (GTK_BOX (box), button_box);

  btn_screen = gtk_toggle_button_new ();
  {
    GtkWidget *btn_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_halign (btn_box, GTK_ALIGN_CENTER);
    gtk_widget_set_valign (btn_box, GTK_ALIGN_CENTER);
    GtkWidget *img = gtk_image_new_from_icon_name ("display-symbolic");
    g_object_set (img, "pixel-size", 24, NULL);
    GtkWidget *lbl = gtk_label_new (_("Screen"));
    gtk_box_append (GTK_BOX (btn_box), img);
    gtk_box_append (GTK_BOX (btn_box), lbl);
    gtk_button_set_child (GTK_BUTTON (btn_screen), btn_box);
  }
  gtk_widget_set_tooltip_text (btn_screen, _("Take a screenshot of the entire screen"));
  gtk_widget_set_size_request (btn_screen, 90, 70);
  gtk_box_append (GTK_BOX (button_box), btn_screen);
  g_signal_connect (btn_screen, "toggled", G_CALLBACK (screen_toggled_cb), self);
  self->radio_screen = btn_screen;

  btn_window = gtk_toggle_button_new ();
  {
    GtkWidget *btn_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_halign (btn_box, GTK_ALIGN_CENTER);
    gtk_widget_set_valign (btn_box, GTK_ALIGN_CENTER);
    GtkWidget *img = gtk_image_new_from_icon_name ("window-symbolic");
    g_object_set (img, "pixel-size", 24, NULL);
    GtkWidget *lbl = gtk_label_new (_("Window"));
    gtk_box_append (GTK_BOX (btn_box), img);
    gtk_box_append (GTK_BOX (btn_box), lbl);
    gtk_button_set_child (GTK_BUTTON (btn_window), btn_box);
  }
  gtk_widget_set_tooltip_text (btn_window, _("Take a screenshot of a window"));
  gtk_widget_set_sensitive (btn_window, FALSE);
  gtk_widget_set_size_request (btn_window, 90, 70);
  gtk_box_append (GTK_BOX (button_box), btn_window);
  g_signal_connect (btn_window, "toggled", G_CALLBACK (window_toggled_cb), self);
  self->radio_window = btn_window;

  btn_area = gtk_toggle_button_new ();
  {
    GtkWidget *btn_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_halign (btn_box, GTK_ALIGN_CENTER);
    gtk_widget_set_valign (btn_box, GTK_ALIGN_CENTER);
    GtkWidget *img = gtk_image_new_from_icon_name ("selection-symbolic");
    g_object_set (img, "pixel-size", 24, NULL);
    GtkWidget *lbl = gtk_label_new (_("Area"));
    gtk_box_append (GTK_BOX (btn_box), img);
    gtk_box_append (GTK_BOX (btn_box), lbl);
    gtk_button_set_child (GTK_BUTTON (btn_area), btn_box);
  }
  gtk_widget_set_tooltip_text (btn_area, _("Select an area to capture"));
  gtk_widget_set_size_request (btn_area, 90, 70);
  gtk_box_append (GTK_BOX (button_box), btn_area);
  g_signal_connect (btn_area, "toggled", G_CALLBACK (selection_toggled_cb), self);
  self->radio_selection = btn_area;

  if (screenshot_config->take_window_shot)
    set_mode (self, SCREENSHOT_MODE_WINDOW);
  else if (screenshot_config->take_area_shot)
    set_mode (self, SCREENSHOT_MODE_SELECTION);
  else
    set_mode (self, SCREENSHOT_MODE_SCREEN);

  GtkWidget *options_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
  gtk_widget_set_margin_top (options_box, 6);
  gtk_widget_set_margin_bottom (options_box, 6);
  gtk_widget_set_margin_start (options_box, 6);
  gtk_widget_set_margin_end (options_box, 6);
  gtk_box_append (GTK_BOX (box), options_box);

  self->check_pointer = gtk_switch_new ();
  label = gtk_label_new (_("Show Pointer"));
  GtkWidget *pointer_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_box_append (GTK_BOX (pointer_box), self->check_pointer);
  gtk_box_append (GTK_BOX (pointer_box), label);
  gtk_box_append (GTK_BOX (options_box), pointer_box);
  g_signal_connect (self->check_pointer, "state-set", G_CALLBACK (include_pointer_toggled_cb), self);

  GtkWidget *delay_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  label = gtk_label_new (_("Delay in Seconds"));
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
    set_mode (self, SCREENSHOT_MODE_WINDOW);
  else if (screenshot_config->take_area_shot)
    set_mode (self, SCREENSHOT_MODE_SELECTION);
  else
    set_mode (self, SCREENSHOT_MODE_SCREEN);

  gtk_widget_set_sensitive (self->check_pointer, !screenshot_config->take_area_shot);
  gtk_switch_set_active (GTK_SWITCH (self->check_pointer), screenshot_config->include_pointer);

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
