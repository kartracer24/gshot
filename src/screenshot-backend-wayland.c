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

struct _ScreenshotBackendWayland
{
  GObject parent_instance;
};

typedef struct
{
  struct wl_display *display;
  struct wl_registry *registry;
  struct wl_shm *shm;
  struct ext_image_copy_capture_manager_v1 *image_copy_capture_manager;
  struct ext_output_image_capture_source_manager_v1 *output_source_manager;
  struct wl_output *output;
  
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
      /* Bind to the first output for now */
      if (state->output == NULL)
        state->output = wl_registry_bind (registry, name, &wl_output_interface, 1);
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
  /* We prefer ARGB8888 or XRGB8888 for GdkPixbuf */
  if (state->format == 0 || format == WL_SHM_FORMAT_ARGB8888)
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
swizzle_bgra_to_rgba (uint8_t *data, uint32_t width, uint32_t height, uint32_t stride, gboolean has_alpha)
{
  for (uint32_t y = 0; y < height; y++)
    {
      uint8_t *row = &data[y * stride];
      for (uint32_t x = 0; x < width; x++)
        {
          uint8_t *pixel = &row[x * 4];
          uint8_t b = pixel[0];
          uint8_t g = pixel[1];
          uint8_t r = pixel[2];
          pixel[0] = r;
          pixel[1] = g;
          pixel[2] = b;
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
  while (!state->constraints_done)
    {
      if (wl_display_dispatch (state->display) < 0)
        break;
    }

  if (!state->constraints_done)
    {
      g_message ("Failed to get buffer constraints");
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
  while (!state->done)
    {
      if (wl_display_dispatch (state->display) < 0)
        break;
    }

  if (state->done && shm_data)
    {
      swizzle_bgra_to_rgba (shm_data, state->width, state->height, state->stride,
                            state->format == WL_SHM_FORMAT_ARGB8888);

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

      if (state->pixbuf && rectangle)
        {
          GdkPixbuf *cropped = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, rectangle->width, rectangle->height);
          gdk_pixbuf_copy_area (state->pixbuf, rectangle->x, rectangle->y, rectangle->width, rectangle->height, cropped, 0, 0);
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
