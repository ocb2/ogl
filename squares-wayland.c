/*
 * Copyright © 2011 Benjamin Franzke
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <assert.h>
#include <signal.h>

#include <linux/input.h>

#include <wayland-client.h>
#include <wayland-egl.h>
#include <wayland-cursor.h>

#include <GLES2/gl2.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "xdg-shell-unstable-v5-client-protocol.h"
#include <sys/types.h>
#include <unistd.h>

#include "shared/platform.h"

#ifndef EGL_EXT_swap_buffers_with_damage
#define EGL_EXT_swap_buffers_with_damage 1
typedef EGLBoolean (EGLAPIENTRYP PFNEGLSWAPBUFFERSWITHDAMAGEEXTPROC)(EGLDisplay dpy, EGLSurface surface, EGLint *rects, EGLint n_rects);
#endif

#ifndef EGL_EXT_buffer_age
#define EGL_EXT_buffer_age 1
#define EGL_BUFFER_AGE_EXT            0x313D
#endif

struct window;
struct seat;

struct display {
    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_compositor *compositor;
    struct xdg_shell *shell;
    struct wl_seat *seat;
    struct wl_pointer *pointer;
    struct wl_touch *touch;
    struct wl_keyboard *keyboard;
    struct wl_shm *shm;
    struct wl_cursor_theme *cursor_theme;
    struct wl_cursor *default_cursor;
    struct wl_surface *cursor_surface;
    struct {
        EGLDisplay dpy;
        EGLContext ctx;
        EGLConfig conf;
    } egl;
    struct window *window;

    PFNEGLSWAPBUFFERSWITHDAMAGEEXTPROC swap_buffers_with_damage;
};

struct geometry {
    int width, height;
};

struct window {
    struct display *display;
    struct geometry geometry, window_size;
    struct {
        GLuint rotation_uniform;
        GLuint pos;
        GLuint col;
    } gl;

    uint32_t benchmark_time, frames;
    struct wl_egl_window *native;
    struct wl_surface *surface;
    struct xdg_surface *xdg_surface;
    EGLSurface egl_surface;
    struct wl_callback *callback;
    int fullscreen, opaque, buffer_size, frame_sync;
};


static GLuint position_l,
              projection_l,
              model_l,
              color_l,
              identifier_l;

static const GLfloat square[] = {
     0.0f,  1.0f,
    -1.0f,  1.0f,
    -1.0f,  0.0f,
     0.0f,  0.0f
};

GLfloat projection[] = {
    1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f
};

GLfloat model[] = {
    1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f
};

uint32_t maxid = 0;

void draw_square(GLfloat x,
                 GLfloat y,
                 GLfloat s,
                 GLfloat *color) {
    model[12] = x;
    model[13] = y;
    model[0] = s;
    model[5] = s;

    glUniformMatrix4fv(projection_l, 1, GL_FALSE, projection);
    glUniformMatrix4fv(model_l, 1, GL_FALSE, model);
    glUniform4fv(color_l, 1, color);
    glUniform4fv(identifier_l, 1, ++maxid);

    glVertexAttribPointer(position_l, 2, GL_FLOAT, GL_FALSE, 0, square);
    glEnableVertexAttribArray(position_l);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    model[12] = 0;
    model[13] = 0;
    model[0] = 1;
    model[5] = 1;

    return;
}

void squares(struct window *window) {
    EGLint buffer_age = 0;
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    draw_square(0, 0, 1, (GLfloat[]){0.0f, 1.0f, 1.0f, 1.0f});
    draw_square(1, 0, 1, (GLfloat[]){1.0f, 1.0f, 0.0f, 1.0f});
    draw_square(0, -1, 1, (GLfloat[]){1.0f, 0.0f, 1.0f, 1.0f});
    draw_square(1, -1, 1, (GLfloat[]){1.0f, 1.0f, 1.0f, 1.0f});

    eglQuerySurface(window->display->egl.dpy,
                    window->egl_surface,
                    EGL_BUFFER_AGE_EXT,
                    &buffer_age);
    wl_surface_set_opaque_region(window->surface, NULL);
    eglSwapBuffers(window->display->egl.dpy, window->egl_surface);
}

void init_gl() {
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);

    GLuint s_v, s_f, p;
    char msg[512];

    static const char *src_v = "uniform mat4 projection;\n"
                               "uniform mat4 model;\n"
                               "uniform vec4 color_u;\n"
                               "uniform int identifier_u;\n"

                               "attribute vec2 position;\n"

                               "varying vec4 color;\n"
                               "varying vec4 identifier;\n"

                               "void main() {"
                                   "color = color_u;\n"
                                   "identifier = vec4(identifier_u, 0.0f, 0.0f, 0.0f);\n"
                                   "gl_Position = model * vec4(position, 0, 1) * projection;"
                               "}";
    static const char *src_f = "precision mediump float;\n"
                               "varying vec4 color;\n"

                               "void main() {"
                                   "gl_FragColor = color;"
                               "}";

    s_v = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(s_v, 1, &src_v, NULL);
    glCompileShader(s_v);
    glGetShaderInfoLog(s_v, sizeof msg, NULL, msg);
    printf("vertex shader info: %s\n", msg);

    s_f = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(s_f, 1, &src_f, NULL);
    glCompileShader(s_f);
    glGetShaderInfoLog(s_f, sizeof msg, NULL, msg);
    printf("fragment shader info: %s\n", msg);

    p = glCreateProgram();
    glAttachShader(p, s_v);
    glAttachShader(p, s_f);
    glLinkProgram(p);
    glUseProgram(p);

    projection_l = glGetUniformLocation(p, "projection");
    model_l = glGetUniformLocation(p, "model");
    color_l = glGetUniformLocation(p, "color_u");
    identifier_l = glGetUniformLocation(p, "identifier_u");
    position_l = glGetAttribLocation(p, "position");
}
static int running = 1;

static void init_egl(struct display *display,
                     struct window *window)
{
    static const EGLint context_attribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };
    const char *extensions;

    EGLint config_attribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 1,
        EGL_GREEN_SIZE, 1,
        EGL_BLUE_SIZE, 1,
        EGL_ALPHA_SIZE, 1,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_NONE
    };

    EGLint major, minor, n, count, i, size;
    EGLConfig *configs;
    EGLBoolean ret;

    if (window->opaque || window->buffer_size == 16)
        config_attribs[9] = 0;

    display->egl.dpy =
        weston_platform_get_egl_display(EGL_PLATFORM_WAYLAND_KHR,
                        display->display, NULL);
    assert(display->egl.dpy);

    ret = eglInitialize(display->egl.dpy, &major, &minor);
    assert(ret == EGL_TRUE);
    ret = eglBindAPI(EGL_OPENGL_ES_API);
    assert(ret == EGL_TRUE);

    if (!eglGetConfigs(display->egl.dpy, NULL, 0, &count) || count < 1)
        assert(0);

    configs = calloc(count, sizeof *configs);
    assert(configs);

    ret = eglChooseConfig(display->egl.dpy, config_attribs,
                  configs, count, &n);
    assert(ret && n >= 1);

    for (i = 0; i < n; i++) {
        eglGetConfigAttrib(display->egl.dpy,
                   configs[i], EGL_BUFFER_SIZE, &size);
        if (window->buffer_size == size) {
            display->egl.conf = configs[i];
            break;
        }
    }
    free(configs);
    if (display->egl.conf == NULL) {
        fprintf(stderr, "did not find config with buffer size %d\n",
            window->buffer_size);
        exit(EXIT_FAILURE);
    }

    display->egl.ctx = eglCreateContext(display->egl.dpy,
                        display->egl.conf,
                        EGL_NO_CONTEXT, context_attribs);
    assert(display->egl.ctx);

    display->swap_buffers_with_damage = NULL;
    extensions = eglQueryString(display->egl.dpy, EGL_EXTENSIONS);
    if (extensions &&
        strstr(extensions, "EGL_EXT_swap_buffers_with_damage") &&
        strstr(extensions, "EGL_EXT_buffer_age"))
        display->swap_buffers_with_damage =
            (PFNEGLSWAPBUFFERSWITHDAMAGEEXTPROC)
            eglGetProcAddress("eglSwapBuffersWithDamageEXT");

    if (display->swap_buffers_with_damage)
        printf("has EGL_EXT_buffer_age and EGL_EXT_swap_buffers_with_damage\n");

}

static void fini_egl(struct display *display)
{
    eglTerminate(display->egl.dpy);
    eglReleaseThread();
}

static void handle_surface_configure(void *data,
                                     struct xdg_surface *surface,
                                     int32_t width,
                                     int32_t height,
                                     struct wl_array *states,
                                     uint32_t serial) {
    struct window *window = data;
    uint32_t *p;

    window->fullscreen = 0;
    wl_array_for_each(p, states) {
        uint32_t state = *p;
        switch (state) {
        case XDG_SURFACE_STATE_FULLSCREEN:
            window->fullscreen = 1;
            break;
        }
    }

    if (width > 0 && height > 0) {
        if (!window->fullscreen) {
            window->window_size.width = width;
            window->window_size.height = height;
        }
        window->geometry.width = width;
        window->geometry.height = height;
    } else if (!window->fullscreen) {
        window->geometry = window->window_size;
    }

    if (window->native)
        wl_egl_window_resize(window->native,
                     window->geometry.width,
                     window->geometry.height, 0, 0);

    xdg_surface_ack_configure(surface, serial);
}

static void handle_surface_delete(void *data,
                                  struct xdg_surface *xdg_surface) {
    running = 0;
}

static const struct xdg_surface_listener xdg_surface_listener = {
    handle_surface_configure,
    handle_surface_delete,
};

static void create_xdg_surface(struct window *window,
                               struct display *display) {
    window->xdg_surface = xdg_shell_get_xdg_surface(display->shell,
                            window->surface);

    xdg_surface_add_listener(window->xdg_surface,
                 &xdg_surface_listener, window);

    xdg_surface_set_title(window->xdg_surface, "squares-wayland");
}

static void create_surface(struct window *window) {
    struct display *display = window->display;
    EGLBoolean ret;

    window->surface = wl_compositor_create_surface(display->compositor);

    window->native =
        wl_egl_window_create(window->surface,
                     window->geometry.width,
                     window->geometry.height);
    window->egl_surface =
        weston_platform_create_egl_surface(display->egl.dpy,
                           display->egl.conf,
                           window->native, NULL);

    create_xdg_surface(window, display);

    ret = eglMakeCurrent(window->display->egl.dpy, window->egl_surface,
                 window->egl_surface, window->display->egl.ctx);
    assert(ret == EGL_TRUE);

    if (!window->frame_sync)
        eglSwapInterval(display->egl.dpy, 0);

    if (!display->shell)
        return;

    if (window->fullscreen)
        xdg_surface_set_fullscreen(window->xdg_surface, NULL);
}

static void destroy_surface(struct window *window) {
    /* Required, otherwise segfault in egl_dri2.c: dri2_make_current()
     * on eglReleaseThread(). */
    eglMakeCurrent(window->display->egl.dpy, EGL_NO_SURFACE, EGL_NO_SURFACE,
               EGL_NO_CONTEXT);

    eglDestroySurface(window->display->egl.dpy, window->egl_surface);
    wl_egl_window_destroy(window->native);

    xdg_surface_destroy(window->xdg_surface);
    wl_surface_destroy(window->surface);

    if (window->callback)
        wl_callback_destroy(window->callback);
}

static void pointer_handle_enter(void *data,
                                 struct wl_pointer *pointer,
                                 uint32_t serial,
                                 struct wl_surface *surface,
                                 wl_fixed_t sx,
                                 wl_fixed_t sy) {}

static void pointer_handle_leave(void *data,
                                 struct wl_pointer *pointer,
                                 uint32_t serial,
                                 struct wl_surface *surface) {}

uint32_t p_x, p_y;

static void pointer_handle_motion(void *data,
                                  struct wl_pointer *pointer,
                                  uint32_t time,
                                  wl_fixed_t sx,
                                  wl_fixed_t sy) {
    p_x = wl_fixed_to_int(sx);
    p_y = wl_fixed_to_int(sy);
}

static void pointer_handle_button(void *data,
                                  struct wl_pointer *wl_pointer,
                                  uint32_t serial,
                                  uint32_t time,
                                  uint32_t button,
                                  uint32_t state) {
    if (button == BTN_LEFT && state == WL_POINTER_BUTTON_STATE_PRESSED) {
               if (p_x < 128 && p_y < 128) {
            printf("cyan\n");
        } else if (p_x < 128 && p_y > 128) {
            printf("magenta\n");
        } else if (p_x > 128 && p_y < 128) {
            printf("yellow\n");
        } else if (p_x > 128 && p_y > 128) {
            printf("white\n");
        }
    }
}

static void pointer_handle_axis(void *data,
                                struct wl_pointer *wl_pointer,
                                uint32_t time,
                                uint32_t axis,
                                wl_fixed_t value) {}

static const struct wl_pointer_listener pointer_listener = {
    pointer_handle_enter,
    pointer_handle_leave,
    pointer_handle_motion,
    pointer_handle_button,
    pointer_handle_axis,
};

static void seat_handle_capabilities(void *data,
                                     struct wl_seat *seat,
                                     enum wl_seat_capability caps) {
    struct display *d = data;

    if ((caps & WL_SEAT_CAPABILITY_POINTER) && !d->pointer) {
        d->pointer = wl_seat_get_pointer(seat);
        wl_pointer_add_listener(d->pointer, &pointer_listener, d);
    } else if (!(caps & WL_SEAT_CAPABILITY_POINTER) && d->pointer) {
        wl_pointer_destroy(d->pointer);
        d->pointer = NULL;
    }
}

static const struct wl_seat_listener seat_listener = {
    seat_handle_capabilities,
};

static void xdg_shell_ping(void *data,
                           struct xdg_shell *shell,
                           uint32_t serial) {
    xdg_shell_pong(shell, serial);
}

static const struct xdg_shell_listener xdg_shell_listener = {
    xdg_shell_ping,
};

#define XDG_VERSION 5 /* The version of xdg-shell that we implement */
#ifdef static_assert
static_assert(XDG_VERSION == XDG_SHELL_VERSION_CURRENT,
          "Interface version doesn't match implementation version");
#endif

static void registry_handle_global(void *data,
                                   struct wl_registry *registry,
                                   uint32_t name,
                                   const char *interface,
                                   uint32_t version) {
    struct display *d = data;

    if (strcmp(interface, "wl_compositor") == 0) {
        d->compositor =
            wl_registry_bind(registry, name,
                     &wl_compositor_interface, 1);
    } else if (strcmp(interface, "xdg_shell") == 0) {
        d->shell = wl_registry_bind(registry, name,
                        &xdg_shell_interface, 1);
        xdg_shell_add_listener(d->shell, &xdg_shell_listener, d);
        xdg_shell_use_unstable_version(d->shell, XDG_VERSION);
    } else if (strcmp(interface, "wl_seat") == 0) {
        d->seat = wl_registry_bind(registry, name,
                       &wl_seat_interface, 1);
        wl_seat_add_listener(d->seat, &seat_listener, d);
    }
}

static void registry_handle_global_remove(void *data,
                                          struct wl_registry *registry,
                                          uint32_t name) {}

static const struct wl_registry_listener registry_listener = {
    registry_handle_global,
    registry_handle_global_remove
};

static void signal_int(int signum) {
    running = 0;
}

static void usage(int error_code) {
    fprintf(stderr, "Usage: squares-wayland\n\n");
}

int main(int argc, char **argv) {
    struct sigaction sigint;
    struct display display = { 0 };
    struct window  window  = { 0 };
    int i, ret = 0;

    window.display = &display;
    display.window = &window;
    window.geometry.width  = 250;
    window.geometry.height = 250;
    window.window_size = window.geometry;
    window.buffer_size = 32;
    window.frame_sync = 1;

    display.display = wl_display_connect(NULL);
    assert(display.display);

    display.registry = wl_display_get_registry(display.display);
    wl_registry_add_listener(display.registry,
                             &registry_listener,
                             &display);

    wl_display_dispatch(display.display);

    init_egl(&display, &window);
    create_surface(&window);
    init_gl(&window);

    display.cursor_surface =
        wl_compositor_create_surface(display.compositor);

    sigint.sa_handler = signal_int;
    sigemptyset(&sigint.sa_mask);
    sigint.sa_flags = SA_RESETHAND;
    sigaction(SIGINT, &sigint, NULL);

    /* The mainloop here is a little subtle.  Redrawing will cause
     * EGL to read events so we can just call
     * wl_display_dispatch_pending() to handle any events that got
     * queued up as a side effect. */
    while (running) {
        wl_display_dispatch_pending(display.display);
        squares(&window);
    }

    fprintf(stderr, "squares-wayland exiting\n");

    destroy_surface(&window);
    fini_egl(&display);

    wl_surface_destroy(display.cursor_surface);
    if (display.cursor_theme)
        wl_cursor_theme_destroy(display.cursor_theme);

    if (display.shell)
        xdg_shell_destroy(display.shell);

    if (display.compositor)
        wl_compositor_destroy(display.compositor);

    wl_registry_destroy(display.registry);
    wl_display_flush(display.display);
    wl_display_disconnect(display.display);

    return 0;
}