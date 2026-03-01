/* screenshot-config.c - Holds current configuration for gnome-screenshot
 *
 * Copyright (C) 2001 Jonathan Blandford <jrb@alum.mit.edu>
 * Copyright (C) 2006 Emmanuele Bassi <ebassi@gnome.org>
 * Copyright (C) 2008, 2011 Cosimo Cecchi <cosimoc@gnome.org>
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
 * along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301
 * USA
 */

#include "config.h"

#include <glib/gi18n.h>
#include <glib/gkeyfile.h>

#include "screenshot-config.h"

#define CONFIG_GROUP            "Settings"
#define DELAY_KEY               "delay"
#define INCLUDE_POINTER_KEY     "include-pointer"
#define INCLUDE_ICC_PROFILE     "include-icc-profile"
#define DARK_MODE_KEY          "dark-mode"
#define AUTO_SAVE_DIRECTORY_KEY "auto-save-directory"
#define LAST_SAVE_DIRECTORY_KEY "last-save-directory"
#define DEFAULT_FILE_TYPE_KEY   "default-file-type"

#define CONFIG_DIR_NAME        "gshot"
#define CONFIG_FILE_NAME       "config"

ScreenshotConfig *screenshot_config;
GdkMonitor *screenshot_target_monitor = NULL;

static gchar *
get_config_path (void)
{
  const gchar *config_home;
  gchar *config_dir;
  gchar *config_file;

  config_home = g_get_user_config_dir ();
  config_dir = g_build_filename (config_home, CONFIG_DIR_NAME, NULL);
  config_file = g_build_filename (config_dir, CONFIG_FILE_NAME, NULL);

  g_mkdir_with_parents (config_dir, 0755);

  return config_file;
}

void
screenshot_load_config (void)
{
  ScreenshotConfig *config;
  GKeyFile *keyfile;
  gchar *config_path;
  GError *error = NULL;

  config = g_slice_new0 (ScreenshotConfig);

  keyfile = g_key_file_new ();
  config_path = get_config_path ();

  if (g_key_file_load_from_file (keyfile, config_path, G_KEY_FILE_NONE, &error))
    {
      config->save_dir = g_key_file_get_string (keyfile, CONFIG_GROUP, LAST_SAVE_DIRECTORY_KEY, NULL);
      if (config->save_dir == NULL || *config->save_dir == '\0')
        {
          g_free (config->save_dir);
          config->save_dir = g_build_filename (g_get_user_special_dir (G_USER_DIRECTORY_PICTURES), NULL);
        }

      config->delay = g_key_file_get_integer (keyfile, CONFIG_GROUP, DELAY_KEY, NULL);
      config->include_pointer = g_key_file_get_boolean (keyfile, CONFIG_GROUP, INCLUDE_POINTER_KEY, NULL);
      config->include_icc_profile = g_key_file_get_boolean (keyfile, CONFIG_GROUP, INCLUDE_ICC_PROFILE, NULL);
      if (!g_key_file_has_key (keyfile, CONFIG_GROUP, DARK_MODE_KEY, NULL))
        config->dark_mode = TRUE;
      else
        config->dark_mode = g_key_file_get_boolean (keyfile, CONFIG_GROUP, DARK_MODE_KEY, NULL);

      config->file_type = g_key_file_get_string (keyfile, CONFIG_GROUP, DEFAULT_FILE_TYPE_KEY, NULL);
      if (config->file_type == NULL)
        config->file_type = g_strdup ("png");
    }
  else
    {
      config->save_dir = g_build_filename (g_get_user_special_dir (G_USER_DIRECTORY_PICTURES), NULL);
      config->delay = 0;
      config->include_pointer = FALSE;
      config->include_icc_profile = TRUE;
      config->dark_mode = TRUE;
      config->file_type = g_strdup ("png");
    }

  g_key_file_set_string (keyfile, CONFIG_GROUP, LAST_SAVE_DIRECTORY_KEY, config->save_dir);
  g_key_file_set_integer (keyfile, CONFIG_GROUP, DELAY_KEY, config->delay);
  g_key_file_set_boolean (keyfile, CONFIG_GROUP, INCLUDE_POINTER_KEY, config->include_pointer);
  g_key_file_set_boolean (keyfile, CONFIG_GROUP, INCLUDE_ICC_PROFILE, config->include_icc_profile);
  g_key_file_set_boolean (keyfile, CONFIG_GROUP, DARK_MODE_KEY, config->dark_mode);
  g_key_file_set_string (keyfile, CONFIG_GROUP, DEFAULT_FILE_TYPE_KEY, config->file_type);

  config->keyfile = keyfile;
  g_free (config_path);

  config->take_window_shot = FALSE;
  config->take_area_shot = FALSE;

  screenshot_config = config;

  config_path = get_config_path ();
  g_key_file_save_to_file (keyfile, config_path, NULL);
  g_free (config_path);
}

void
screenshot_save_config (void)
{
  ScreenshotConfig *c = screenshot_config;
  gchar *config_path;
  GError *error = NULL;

  g_assert (c != NULL);

  g_key_file_set_boolean (c->keyfile, CONFIG_GROUP, INCLUDE_POINTER_KEY, c->include_pointer);
  g_key_file_set_boolean (c->keyfile, CONFIG_GROUP, DARK_MODE_KEY, c->dark_mode);
  g_key_file_set_integer (c->keyfile, CONFIG_GROUP, DELAY_KEY, c->delay);

  config_path = get_config_path ();
  g_key_file_save_to_file (c->keyfile, config_path, &error);
  if (error)
    {
      g_warning ("Failed to save config: %s", error->message);
      g_error_free (error);
    }
  g_free (config_path);
}

gboolean
screenshot_config_parse_command_line (gboolean clipboard_arg,
                                      gboolean window_arg,
                                      gboolean area_arg,
                                      gboolean include_border_arg,
                                      gboolean disable_border_arg,
                                      gboolean include_pointer_arg,
                                      const gchar *border_effect_arg,
                                      guint delay_arg,
                                      gboolean interactive_arg,
                                      const gchar *file_arg)
{
  if (window_arg && area_arg)
    {
      g_printerr (_("Conflicting options: --window and --area should not be "
                    "used at the same time.\n"));
      return FALSE;
    }

  screenshot_config->interactive = interactive_arg;

  if (include_border_arg)
    g_warning ("Option --include-border is deprecated and will be removed in "
               "gnome-screenshot 3.38.0. Window border is always included.");
  if (disable_border_arg)
    g_warning ("Option --remove-border is deprecated and will be removed in "
               "gnome-screenshot 3.38.0. Window border is always included.");
  if (border_effect_arg != NULL)
    g_warning ("Option --border-effect is deprecated and will be removed in "
               "gnome-screenshot 3.38.0. No effect will be used.");

  if (screenshot_config->interactive)
    {
      if (clipboard_arg)
        g_warning ("Option --clipboard is ignored in interactive mode.");
      if (include_pointer_arg)
        g_warning ("Option --include-pointer is ignored in interactive mode.");
      if (file_arg)
        g_warning ("Option --file is ignored in interactive mode.");

      if (delay_arg > 0)
        screenshot_config->delay = delay_arg;
    }
  else
    {
      g_free (screenshot_config->save_dir);
      screenshot_config->save_dir = g_key_file_get_string (screenshot_config->keyfile,
                                                           CONFIG_GROUP,
                                                           AUTO_SAVE_DIRECTORY_KEY,
                                                           NULL);
      if (screenshot_config->save_dir == NULL || *screenshot_config->save_dir == '\0')
        {
          g_free (screenshot_config->save_dir);
          screenshot_config->save_dir = g_build_filename (g_get_user_special_dir (G_USER_DIRECTORY_PICTURES), NULL);
        }

      screenshot_config->delay = delay_arg;
      screenshot_config->include_pointer = include_pointer_arg;
      screenshot_config->copy_to_clipboard = clipboard_arg;
      if (file_arg != NULL)
        screenshot_config->file = g_file_new_for_commandline_arg (file_arg);
    }

  screenshot_config->take_window_shot = window_arg;
  screenshot_config->take_area_shot = area_arg;

  return TRUE;
}
