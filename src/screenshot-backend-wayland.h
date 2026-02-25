/* screenshot-backend-wayland.h - Wayland wlr-screencopy backend
 *
 * Copyright (C) 2024 GNOME Screenshot Contributors
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

#pragma once

#include <glib-object.h>
#include "screenshot-backend.h"

G_BEGIN_DECLS

#define SCREENSHOT_TYPE_BACKEND_WAYLAND (screenshot_backend_wayland_get_type())

G_DECLARE_FINAL_TYPE (ScreenshotBackendWayland, screenshot_backend_wayland, SCREENSHOT, BACKEND_WAYLAND, GObject)

ScreenshotBackend *screenshot_backend_wayland_new (void);

G_END_DECLS
