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
  else if (g_strcmp0 (interface, "zwlr_screencopy_manager_v1") == 0)
    {
      state->screencopy_manager = wl_registry_bind (registry, name,
                                                     &zwlr_screencopy_manager_v1_interface,
                                                     version);
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

/* Screencopy frame listener callbacks */
static void
screencopy_frame_handle_buffer (void *data,
                                struct zwlr_screencopy_frame_v1 *frame,
                                uint32_t format,
                                uint32_t width,
                                uint32_t height,
                                uint32_t stride)
{
  WaylandState *state = (WaylandState *)data;
  struct wl_shm_pool *pool;
  int fd;
  int size;
  void *shm_data;
  struct wl_buffer *buffer;

  size = stride * height;
  fd = memfd_create ("gnome-screenshot", MFD_CLOEXEC);
  if (fd < 0)
    {
      g_message ("Failed to create memfd");
      return;
    }

  if (ftruncate (fd, size) < 0)
    {
      g_message ("Failed to truncate memfd");
      close (fd);
      return;
    }

  shm_data = mmap (NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (shm_data == MAP_FAILED)
    {
      g_message ("Failed to mmap shared memory");
      close (fd);
      return;
    }

  pool = wl_shm_create_pool (state->shm, fd, size);
  buffer = wl_shm_pool_create_buffer (pool, 0, width, height, stride, WL_SHM_FORMAT_ARGB8888);
  wl_shm_pool_destroy (pool);
  close (fd);

  zwlr_screencopy_frame_v1_copy (frame, buffer);
  wl_buffer_add_listener (buffer, NULL, NULL);

  /* Convert raw pixel data to GdkPixbuf */
  state->pixbuf = gdk_pixbuf_new_from_data ((const guchar *)shm_data,
                                            GDK_COLORSPACE_RGB,
                                            TRUE,  /* has_alpha */
                                            8,     /* bits_per_sample */
                                            width,
                                            height,
                                            stride,
                                            NULL,
                                            NULL);

  if (state->pixbuf)
    {
      /* Make a copy so we own the data */
      GdkPixbuf *copy = gdk_pixbuf_copy (state->pixbuf);
      g_object_unref (state->pixbuf);
      state->pixbuf = copy;
    }

  munmap (shm_data, size);
  wl_buffer_destroy (buffer);
}

static void
screencopy_frame_handle_flags (void *data,
                               struct zwlr_screencopy_frame_v1 *frame,
                               uint32_t flags)
{
}

static void
screencopy_frame_handle_ready (void *data,
                               struct zwlr_screencopy_frame_v1 *frame,
                               uint32_t tv_sec_hi,
                               uint32_t tv_sec_lo,
                               uint32_t tv_nsec)
{
  WaylandState *state = (WaylandState *)data;
  state->done = TRUE;
  if (state->loop)
    g_main_loop_quit (state->loop);
  zwlr_screencopy_frame_v1_destroy (frame);
}

static void
screencopy_frame_handle_failed (void *data,
                                struct zwlr_screencopy_frame_v1 *frame)
{
  WaylandState *state = (WaylandState *)data;
  g_message ("Screencopy frame failed");
  state->done = TRUE;
  if (state->loop)
    g_main_loop_quit (state->loop);
  zwlr_screencopy_frame_v1_destroy (frame);
}

static const struct zwlr_screencopy_frame_v1_listener screencopy_frame_listener = {
  screencopy_frame_handle_buffer,
  screencopy_frame_handle_flags,
  screencopy_frame_handle_ready,
  screencopy_frame_handle_failed,
};

static gboolean
screenshot_backend_wayland_is_available (void)
{
  GdkDisplay *display = gdk_display_get_default ();
  return GDK_IS_WAYLAND_DISPLAY (display);
}

static GdkPixbuf *
screenshot_backend_wayland_capture_screen (WaylandState *state,
                                           GdkRectangle *rectangle)
{
  struct zwlr_screencopy_frame_v1 *frame;

  if (!state->screencopy_manager || !state->output)
    {
      g_message ("wlr-screencopy not available on this Wayland compositor");
      return NULL;
    }

  /* For now, capture the full output ignoring rectangle parameter */
  /* TODO: Implement area capture by modifying Wayland protocol usage */
  frame = zwlr_screencopy_manager_v1_capture_output (state->screencopy_manager,
                                                     0, /* overlay_cursor */
                                                     state->output);

  if (!frame)
    {
      g_message ("Failed to create screencopy frame");
      return NULL;
    }

  zwlr_screencopy_frame_v1_add_listener (frame, &screencopy_frame_listener, state);

  /* Run main loop until frame is ready */
  state->done = FALSE;
  state->loop = g_main_loop_new (NULL, FALSE);
  
  /* Process events */
  wl_display_roundtrip (state->display);

  /* Wait for frame with timeout */
  if (!state->done)
    {
      guint timeout_id = g_timeout_add (5000, (GSourceFunc) g_main_loop_quit, state->loop);
      g_main_loop_run (state->loop);
      g_source_remove (timeout_id);
    }

  if (!state->done)
    {
      g_message ("Wayland screencopy timed out after 5 seconds");
      zwlr_screencopy_frame_v1_destroy (frame);
    }

  g_main_loop_unref (state->loop);
  state->loop = NULL;

  return state->pixbuf;
}

static void
screenshot_backend_wayland_cleanup (WaylandState *state)
{
  if (state->screencopy_manager)
    zwlr_screencopy_manager_v1_destroy (state->screencopy_manager);
  if (state->output)
    wl_output_destroy (state->output);
  if (state->shm)
    wl_shm_destroy (state->shm);
  if (state->registry)
    wl_registry_destroy (state->registry);
  if (state->display)
    wl_display_disconnect (state->display);
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

  if (!state.screencopy_manager)
    {
      g_message ("wlr-screencopy not supported by this Wayland compositor");
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
