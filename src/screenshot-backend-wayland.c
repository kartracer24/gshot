/* screenshot-backend-wayland.c - Wayland wlr-screencopy backend
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

#include "config.h"

#ifdef HAVE_WAYLAND

#include "screenshot-backend-wayland.h"
#include "screenshot-config.h"

#include <wayland-client.h>
#include <gdk/gdkwayland.h>
#include <sys/mman.h>
#include <unistd.h>
#include <wayland-protocols/ext-image-copy-capture-v1-enum.h>
#include "ext-image-copy-capture-v1-client-protocol.h"
#include "ext-image-capture-source-v1-client-protocol.h"
#include "ext-foreign-toplevel-list-v1-client-protocol.h"

struct _ScreenshotBackendWayland
{
  GObject parent_instance;
};

#ifndef DRM_FORMAT_XRGB8888
#define DRM_FORMAT_XRGB8888 0x34325258
#endif
#ifndef DRM_FORMAT_ARGB8888
#define DRM_FORMAT_ARGB8888 0x34325241
#endif
#ifndef DRM_FORMAT_XBGR8888
#define DRM_FORMAT_XBGR8888 0x34324258
#endif
#ifndef DRM_FORMAT_ABGR8888
#define DRM_FORMAT_ABGR8888 0x34324241
#endif

typedef struct
{
  struct wl_display *display;
  struct wl_registry *registry;
  struct wl_shm *shm;
  struct ext_image_copy_capture_manager_v1 *image_copy_capture_manager;
  struct ext_output_image_capture_source_manager_v1 *output_source_manager;
  struct ext_foreign_toplevel_list_v1 *toplevel_list;
  struct ext_foreign_toplevel_image_capture_source_manager_v1 *toplevel_capture_manager;
  struct wl_output *output;
  struct wl_output *all_outputs[4];
  int num_outputs;
  
  /* Toplevel tracking */
  struct ext_foreign_toplevel_handle_v1 *selected_toplevel;
  char **toplevel_titles;
  int toplevel_count;

  struct ext_image_capture_source_v1 *source;
  struct ext_image_copy_capture_session_v1 *session;
  struct ext_image_copy_capture_frame_v1 *frame;

  uint32_t width;
  uint32_t height;
  uint32_t format;
  uint32_t stride;
  gboolean constraints_done;

  GdkPixbuf *pixbuf;
  gboolean done;
  GMainLoop *loop;
} WaylandState;

static void screenshot_backend_wayland_backend_init (ScreenshotBackendInterface *iface);

G_DEFINE_TYPE_WITH_CODE (ScreenshotBackendWayland, screenshot_backend_wayland, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (SCREENSHOT_TYPE_BACKEND, screenshot_backend_wayland_backend_init))

/* Registry listener callbacks */
static void
registry_handle_global (void *data,
                        struct wl_registry *registry,
                        uint32_t name,
                        const char *interface,
                        uint32_t version)
{
  WaylandState *state = (WaylandState *)data;

  if (g_strcmp0 (interface, "wl_shm") == 0)
    {
      state->shm = wl_registry_bind (registry, name, &wl_shm_interface, 1);
    }
  else if (g_strcmp0 (interface, "wl_output") == 0)
    {
      /* Store all outputs */
      if (state->num_outputs < 4)
        {
          struct wl_output *output = wl_registry_bind (registry, name, &wl_output_interface, version);
          state->all_outputs[state->num_outputs] = output;
          state->num_outputs++;
          /* Default to first output, will be updated later */
          if (state->num_outputs == 1)
            state->output = output;
        }
    }
  else if (g_strcmp0 (interface, ext_image_copy_capture_manager_v1_interface.name) == 0)
    {
      state->image_copy_capture_manager = wl_registry_bind (registry, name,
                                                            &ext_image_copy_capture_manager_v1_interface,
                                                            1);
    }
  else if (g_strcmp0 (interface, ext_output_image_capture_source_manager_v1_interface.name) == 0)
    {
      state->output_source_manager = wl_registry_bind (registry, name,
                                                       &ext_output_image_capture_source_manager_v1_interface,
                                                       1);
    }
  else if (g_strcmp0 (interface, ext_foreign_toplevel_list_v1_interface.name) == 0)
    {
      state->toplevel_list = wl_registry_bind (registry, name,
                                                &ext_foreign_toplevel_list_v1_interface,
                                                1);
    }
  else if (g_strcmp0 (interface, ext_foreign_toplevel_image_capture_source_manager_v1_interface.name) == 0)
    {
      state->toplevel_capture_manager = wl_registry_bind (registry, name,
                                                          &ext_foreign_toplevel_image_capture_source_manager_v1_interface,
                                                          1);
    }
}

static void
registry_handle_global_remove (void *data,
                               struct wl_registry *registry,
                               uint32_t name)
{
}

static const struct wl_registry_listener registry_listener = {
  registry_handle_global,
  registry_handle_global_remove,
};

/* Session listener callbacks */
static void
session_handle_buffer_size (void *data,
                            struct ext_image_copy_capture_session_v1 *session,
                            uint32_t width,
                            uint32_t height)
{
  WaylandState *state = (WaylandState *)data;
  state->width = width;
  state->height = height;
}

static void
session_handle_shm_format (void *data,
                           struct ext_image_copy_capture_session_v1 *session,
                           uint32_t format)
{
  WaylandState *state = (WaylandState *)data;
  
  /* Prefer ARGB8888 or ABGR8888 */
  if (state->format == 0 ||
      format == WL_SHM_FORMAT_ARGB8888 ||
      format == DRM_FORMAT_ARGB8888 ||
      format == DRM_FORMAT_ABGR8888)
    state->format = format;
}

static void
session_handle_dmabuf_device (void *data,
                              struct ext_image_copy_capture_session_v1 *session,
                              struct wl_array *device)
{
}

static void
session_handle_dmabuf_format (void *data,
                              struct ext_image_copy_capture_session_v1 *session,
                              uint32_t format,
                              struct wl_array *modifiers)
{
}

static void
session_handle_done (void *data,
                     struct ext_image_copy_capture_session_v1 *session)
{
  WaylandState *state = (WaylandState *)data;
  state->constraints_done = TRUE;
}

static void
session_handle_stopped (void *data,
                        struct ext_image_copy_capture_session_v1 *session)
{
  WaylandState *state = (WaylandState *)data;
  state->done = TRUE;
  if (state->loop)
    g_main_loop_quit (state->loop);
}

static const struct ext_image_copy_capture_session_v1_listener session_listener = {
  session_handle_buffer_size,
  session_handle_shm_format,
  session_handle_dmabuf_device,
  session_handle_dmabuf_format,
  session_handle_done,
  session_handle_stopped,
};

/* Frame listener callbacks */
static void
frame_handle_transform (void *data,
                        struct ext_image_copy_capture_frame_v1 *frame,
                        uint32_t transform)
{
}

static void
frame_handle_damage (void *data,
                     struct ext_image_copy_capture_frame_v1 *frame,
                     int32_t x,
                     int32_t y,
                     int32_t width,
                     int32_t height)
{
}

static void
frame_handle_presentation_time (void *data,
                                struct ext_image_copy_capture_frame_v1 *frame,
                                uint32_t tv_sec_hi,
                                uint32_t tv_sec_lo,
                                uint32_t tv_nsec)
{
}

static void
frame_handle_ready (void *data,
                    struct ext_image_copy_capture_frame_v1 *frame)
{
  WaylandState *state = (WaylandState *)data;
  state->done = TRUE;
  if (state->loop)
    g_main_loop_quit (state->loop);
}

static void
frame_handle_failed (void *data,
                     struct ext_image_copy_capture_frame_v1 *frame,
                     uint32_t reason)
{
  WaylandState *state = (WaylandState *)data;
  g_message ("Frame capture failed with reason %u", reason);
  state->done = TRUE;
  if (state->loop)
    g_main_loop_quit (state->loop);
}

static const struct ext_image_copy_capture_frame_v1_listener frame_listener = {
  frame_handle_transform,
  frame_handle_damage,
  frame_handle_presentation_time,
  frame_handle_ready,
  frame_handle_failed,
};

/* Toplevel list listener for window capture */
static void
toplevel_handle_toplevel (void *data,
                           struct ext_foreign_toplevel_list_v1 *list,
                           struct ext_foreign_toplevel_handle_v1 *toplevel)
{
  WaylandState *state = (WaylandState *)data;
  /* TODO: Store toplevel handle for later capture */
  g_message ("Found toplevel window");
}

static void
toplevel_handle_finished (void *data,
                          struct ext_foreign_toplevel_list_v1 *list)
{
  WaylandState *state = (WaylandState *)data;
  g_message ("Toplevel list finished");
}

static const struct ext_foreign_toplevel_list_v1_listener toplevel_list_listener = {
  toplevel_handle_toplevel,
  toplevel_handle_finished,
};

static struct wl_buffer *
create_shm_buffer (WaylandState *state, void **out_data, size_t *out_size)
{
  struct wl_shm_pool *pool;
  struct wl_buffer *buffer;
  int fd;
  void *data;
  size_t size;
  uint32_t stride;

  stride = state->width * 4;
  size = stride * state->height;

  fd = memfd_create ("gnome-screenshot", MFD_CLOEXEC);
  if (fd < 0)
    return NULL;

  if (ftruncate (fd, size) < 0)
    {
      close (fd);
      return NULL;
    }

  data = mmap (NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (data == MAP_FAILED)
    {
      close (fd);
      return NULL;
    }

  pool = wl_shm_create_pool (state->shm, fd, size);
  buffer = wl_shm_pool_create_buffer (pool, 0, state->width, state->height, stride, state->format);
  wl_shm_pool_destroy (pool);
  close (fd);

  *out_data = data;
  *out_size = size;
  state->stride = stride;

  return buffer;
}

static gboolean
screenshot_backend_wayland_is_available (void)
{
  GdkDisplay *display = gdk_display_get_default ();
  return GDK_IS_WAYLAND_DISPLAY (display);
}

static void
convert_to_rgba (uint8_t *data, uint32_t width, uint32_t height, uint32_t stride, uint32_t format)
{
  gboolean swap_rb = FALSE;
  gboolean has_alpha = FALSE;

  /* On Little Endian:
   * XRGB8888/ARGB8888 are B-G-R-X/B-G-R-A
   * XBGR8888/ABGR8888 are R-G-B-X/R-G-B-A
   */
  if (format == WL_SHM_FORMAT_XRGB8888 ||
      format == WL_SHM_FORMAT_ARGB8888 ||
      format == DRM_FORMAT_XRGB8888 ||
      format == DRM_FORMAT_ARGB8888)
    {
      swap_rb = TRUE;
    }

  if (format == WL_SHM_FORMAT_ARGB8888 ||
      format == DRM_FORMAT_ARGB8888 ||
      format == DRM_FORMAT_ABGR8888)
    {
      has_alpha = TRUE;
    }

  for (uint32_t y = 0; y < height; y++)
    {
      uint8_t *row = &data[y * stride];
      for (uint32_t x = 0; x < width; x++)
        {
          uint8_t *pixel = &row[x * 4];
          if (swap_rb)
            {
              uint8_t b = pixel[0];
              uint8_t g = pixel[1];
              uint8_t r = pixel[2];
              pixel[0] = r;
              pixel[1] = g;
              pixel[2] = b;
            }
          if (!has_alpha)
            pixel[3] = 0xff;
        }
    }
}

static GdkPixbuf *
screenshot_backend_wayland_capture_screen (WaylandState *state,
                                           GdkRectangle *rectangle)
{
  struct wl_buffer *buffer = NULL;
  void *shm_data = NULL;
  size_t shm_size = 0;
  uint32_t options = 0;
  GdkRectangle capture_rect;
  GdkRectangle *use_rect = NULL;

  if (screenshot_config->take_window_shot && rectangle == NULL)
    {
      g_message ("Wayland protocol backend does not support selecting windows yet");
      return NULL;
    }

  if (screenshot_config->take_area_shot && rectangle != NULL)
    {
      /* Get monitor geometry and adjust rectangle */
      GdkDisplay *display = gdk_display_get_default ();
      GdkMonitor *monitor = screenshot_target_monitor;
      if (!monitor)
        monitor = gdk_display_get_primary_monitor (display);
      
      if (monitor)
        {
          GdkRectangle monitor_geom;
          gdk_monitor_get_geometry (monitor, &monitor_geom);
          g_message ("Area capture: monitor at %d,%d, original rect: %d,%d %dx%d",
                     monitor_geom.x, monitor_geom.y,
                     rectangle->x, rectangle->y, rectangle->width, rectangle->height);
          
          /* Adjust rectangle to be relative to the monitor */
          capture_rect = *rectangle;
          capture_rect.x = rectangle->x - monitor_geom.x;
          capture_rect.y = rectangle->y - monitor_geom.y;
          
          g_message ("Area capture: adjusted rect: %d,%d %dx%d",
                     capture_rect.x, capture_rect.y, capture_rect.width, capture_rect.height);
          
          use_rect = &capture_rect;
        }
    }

  if (!state->image_copy_capture_manager || !state->output_source_manager || !state->output)
    {
      g_message ("Required Wayland interfaces not available");
      return NULL;
    }

  if (screenshot_config->include_pointer)
    options |= EXT_IMAGE_COPY_CAPTURE_MANAGER_V1_OPTIONS_PAINT_CURSORS;

  /* 1. Create source */
  state->source = ext_output_image_capture_source_manager_v1_create_source (state->output_source_manager, state->output);
  
  /* 2. Create session */
  state->session = ext_image_copy_capture_manager_v1_create_session (state->image_copy_capture_manager,
                                                                    state->source,
                                                                    options);
  ext_image_copy_capture_session_v1_add_listener (state->session, &session_listener, state);

  /* 3. Wait for constraints */
  state->constraints_done = FALSE;
  gint64 start_time = g_get_monotonic_time ();
  while (!state->constraints_done && (g_get_monotonic_time () - start_time) < 2000000)
    {
      if (wl_display_dispatch (state->display) < 0)
        break;
    }

  if (!state->constraints_done)
    {
      g_message ("Failed to get buffer constraints (timeout)");
      return NULL;
    }

  /* 4. Create frame */
  state->frame = ext_image_copy_capture_session_v1_create_frame (state->session);
  ext_image_copy_capture_frame_v1_add_listener (state->frame, &frame_listener, state);

  /* 5. Create buffer */
  buffer = create_shm_buffer (state, &shm_data, &shm_size);
  if (!buffer)
    {
      g_message ("Failed to create SHM buffer");
      return NULL;
    }

  /* 6. Capture */
  ext_image_copy_capture_frame_v1_attach_buffer (state->frame, buffer);
  ext_image_copy_capture_frame_v1_damage_buffer (state->frame, 0, 0, state->width, state->height);
  ext_image_copy_capture_frame_v1_capture (state->frame);

  /* 7. Wait for ready */
  state->done = FALSE;
  start_time = g_get_monotonic_time ();
  while (!state->done && (g_get_monotonic_time () - start_time) < 5000000)
    {
      if (wl_display_dispatch (state->display) < 0)
        break;
    }

  if (state->done && shm_data)
    {
      convert_to_rgba (shm_data, state->width, state->height, state->stride, state->format);

      /* Convert raw pixel data to GdkPixbuf */
      state->pixbuf = gdk_pixbuf_new_from_data ((const guchar *)shm_data,
                                                GDK_COLORSPACE_RGB,
                                                TRUE,  /* has_alpha */
                                                8,     /* bits_per_sample */
                                                state->width,
                                                state->height,
                                                state->stride,
                                                NULL,
                                                NULL);

      if (state->pixbuf)
        {
          /* Make a copy so we own the data */
          GdkPixbuf *copy = gdk_pixbuf_copy (state->pixbuf);
          g_object_unref (state->pixbuf);
          state->pixbuf = copy;
        }

      if (state->pixbuf && use_rect)
        {
          GdkPixbuf *cropped = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, use_rect->width, use_rect->height);
          gdk_pixbuf_copy_area (state->pixbuf, use_rect->x, use_rect->y, use_rect->width, use_rect->height, cropped, 0, 0);
          g_object_unref (state->pixbuf);
          state->pixbuf = cropped;
        }
    }

  if (shm_data)
    munmap (shm_data, shm_size);
  if (buffer)
    wl_buffer_destroy (buffer);

  return state->pixbuf;
}

static void
screenshot_backend_wayland_cleanup (WaylandState *state)
{
  if (state->frame)
    ext_image_copy_capture_frame_v1_destroy (state->frame);
  if (state->session)
    ext_image_copy_capture_session_v1_destroy (state->session);
  if (state->source)
    ext_image_capture_source_v1_destroy (state->source);
  if (state->image_copy_capture_manager)
    ext_image_copy_capture_manager_v1_destroy (state->image_copy_capture_manager);
  if (state->output_source_manager)
    ext_output_image_capture_source_manager_v1_destroy (state->output_source_manager);
  if (state->toplevel_list)
    ext_foreign_toplevel_list_v1_destroy (state->toplevel_list);
  if (state->toplevel_capture_manager)
    ext_foreign_toplevel_image_capture_source_manager_v1_destroy (state->toplevel_capture_manager);
  if (state->output)
    wl_output_destroy (state->output);
  if (state->shm)
    wl_shm_destroy (state->shm);
  if (state->registry)
    wl_registry_destroy (state->registry);
}

static GdkPixbuf *
screenshot_backend_wayland_get_pixbuf (ScreenshotBackend *backend,
                                       GdkRectangle      *rectangle)
{
  WaylandState state = { 0 };
  GdkPixbuf *pixbuf = NULL;

  if (!screenshot_backend_wayland_is_available ())
    {
      g_message ("Wayland display not available");
      return NULL;
    }

  /* Get the Wayland display from GDK */
  GdkDisplay *display = gdk_display_get_default ();
  state.display = gdk_wayland_display_get_wl_display (display);

  if (!state.display)
    {
      g_message ("Failed to get Wayland display from GDK");
      return NULL;
    }

  /* Use the target monitor if set (from interactive mode), otherwise find rightmost */
  GdkMonitor *target_monitor = screenshot_target_monitor;
  
  if (!target_monitor)
    {
      /* Find the rightmost monitor as fallback */
      int n_monitors = gdk_display_get_n_monitors (display);
      int max_x = -1;
      for (int i = 0; i < n_monitors; i++)
        {
          GdkMonitor *mon = gdk_display_get_monitor (display, i);
          if (mon)
            {
              GdkRectangle geom;
              gdk_monitor_get_geometry (mon, &geom);
              if (geom.x > max_x)
                {
                  max_x = geom.x;
                  target_monitor = mon;
                }
            }
        }
    }

  state.registry = wl_display_get_registry (state.display);
  if (!state.registry)
    {
      g_message ("Failed to get Wayland registry");
      return NULL;
    }

  wl_registry_add_listener (state.registry, &registry_listener, &state);
  
  int ret = wl_display_roundtrip (state.display);
  if (ret < 0)
    {
      g_message ("Wayland roundtrip failed: %d", ret);
      screenshot_backend_wayland_cleanup (&state);
      return NULL;
    }

  /* If we have a target monitor, use it for capture */
  if (target_monitor)
    {
      GdkRectangle geom;
      gdk_monitor_get_geometry (target_monitor, &geom);
      g_message ("Using target monitor at %d,%d for capture", geom.x, geom.y);
      
      /* Get the wl_output from the target monitor */
      struct wl_output *monitor_output = gdk_wayland_monitor_get_wl_output (target_monitor);
      g_message ("Target monitor wl_output: %p", (void*)monitor_output);
      g_message ("Available outputs:");
      for (int i = 0; i < state.num_outputs; i++)
        {
          g_message ("  output[%d]: %p", i, (void*)state.all_outputs[i]);
        }
      
      if (monitor_output)
        {
          state.output = monitor_output;
        }
      else
        {
          /* Fallback: try to match by comparing all GdkMonitors to find the one matching target_monitor */
          g_message ("wl_output from GDK is NULL, trying to match GdkMonitor to wl_output");
          int n_monitors = gdk_display_get_n_monitors (display);
          for (int i = 0; i < n_monitors; i++)
            {
              GdkMonitor *mon = gdk_display_get_monitor (display, i);
              if (mon)
                {
                  GdkRectangle mon_geom;
                  gdk_monitor_get_geometry (mon, &mon_geom);
                  if (mon_geom.x == geom.x && mon_geom.y == geom.y)
                    {
                      struct wl_output *mon_output = gdk_wayland_monitor_get_wl_output (mon);
                      g_message ("Found matching monitor at %d,%d, wl_output: %p", mon_geom.x, mon_geom.y, (void*)mon_output);
                      if (mon_output && state.num_outputs > i)
                        {
                          state.output = state.all_outputs[i];
                          break;
                        }
                    }
                }
            }
        }
    }

  /* Fall back to first output */
  if (!state.output && state.num_outputs > 0)
    {
      state.output = state.all_outputs[0];
    }

  if (!state.output || !state.shm)
    {
      g_message ("Wayland compositor missing required interfaces (output=%p, shm=%p)",
                 state.output, state.shm);
      screenshot_backend_wayland_cleanup (&state);
      return NULL;
    }

  if (!state.image_copy_capture_manager || !state.output_source_manager)
    {
      g_message ("ext-image-copy-capture not supported by this Wayland compositor");
      screenshot_backend_wayland_cleanup (&state);
      return NULL;
    }

  /* Handle window capture */
  if (screenshot_config && screenshot_config->take_window_shot)
    {
      if (state.toplevel_list && state.toplevel_capture_manager)
        {
          /* Start listening for toplevels */
          ext_foreign_toplevel_list_v1_add_listener (state.toplevel_list, &toplevel_list_listener, &state);
          
          /* Dispatch to get toplevel list */
          wl_display_roundtrip (state.display);
        }
      else
        {
          g_message ("Window capture requires ext_foreign_toplevel_image_capture_source_manager_v1");
          return NULL;
        }
    }

  pixbuf = screenshot_backend_wayland_capture_screen (&state, rectangle);

  screenshot_backend_wayland_cleanup (&state);

  return pixbuf;
}

static void
screenshot_backend_wayland_class_init (ScreenshotBackendWaylandClass *klass)
{
}

static void
screenshot_backend_wayland_init (ScreenshotBackendWayland *self)
{
}

static void
screenshot_backend_wayland_backend_init (ScreenshotBackendInterface *iface)
{
  iface->get_pixbuf = screenshot_backend_wayland_get_pixbuf;
}

ScreenshotBackend *
screenshot_backend_wayland_new (void)
{
  return g_object_new (SCREENSHOT_TYPE_BACKEND_WAYLAND, NULL);
}

#endif /* HAVE_WAYLAND */
