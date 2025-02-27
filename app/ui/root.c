#if HAVE_GLES2
#include <GLES2/gl2.h>
#elif OS_DARWIN
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#include "app.h"
#include "root.h"
#include "config.h"

#include "stream/platform.h"
#include "stream/session.h"

#include "launcher/window.h"
#include "settings/window.h"
#include "streaming/overlay.h"

#include "util/bus.h"
#include "util/user_event.h"

#include "res.h"

short ui_display_width, ui_display_height;
short ui_logic_width, ui_logic_height;
float ui_scale = 1;

bool ui_settings_showing;
bool ui_fake_mouse_click_started;
enum UI_INPUT_MODE ui_input_mode;
struct nk_vec2 ui_statbar_icon_padding;

static PVIDEO_RENDER_CALLBACKS ui_stream_render;
static bool ui_send_faketouch_cancel;
static bool ui_fake_mouse_event_received;
static struct
{
    struct nk_vec2 *center;
    NAVKEY_STATE state;
} ui_pending_faketouch;

void ui_root_init(struct nk_context *ctx)
{
    launcher_window_init(ctx);
    settings_window_init(ctx);
    streaming_overlay_init(ctx);
    ui_send_faketouch_cancel = false;
    ui_fake_mouse_click_started = false;
    ui_input_mode = UI_INPUT_MODE_POINTER;
    ui_statbar_icon_padding = nk_vec2_s(2, 2);
}

void ui_root_destroy()
{
    launcher_window_destroy();
}

bool ui_root(struct nk_context *ctx)
{
    if (ui_fake_mouse_event_received && ui_pending_faketouch.center)
    {
        bus_pushevent(USER_FAKEINPUT_MOUSE_CLICK, ui_pending_faketouch.center, (void *)ui_pending_faketouch.state);
    }
    else if (ui_send_faketouch_cancel)
    {
        bus_pushevent(USER_FAKEINPUT_MOUSE_CANCEL, NULL, NULL);
        ui_send_faketouch_cancel = false;
    }
    ui_fake_mouse_event_received = false;
    ui_pending_faketouch.center = NULL;
    STREAMING_STATUS stat = streaming_status;
    if (stat == STREAMING_NONE)
    {
        if (!launcher_window(ctx))
        {
            app_request_exit();
            return true;
        }

        if (ui_settings_showing)
        {
            if (!settings_window(ctx))
            {
                settings_window_close();
            }
        }

        return true;
    }
    else
    {
        return streaming_overlay(ctx, stat);
    }
}

void ui_render_background()
{
    if (streaming_status != STREAMING_STREAMING)
    {
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
    }
    else if (ui_stream_render && ui_stream_render->renderDraw)
    {
        ui_stream_render->renderDraw();
    }
    else
    {
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }
}

bool ui_dispatch_userevent(struct nk_context *ctx, int which, void *data1, void *data2)
{
    bool handled = false;
    handled |= launcher_window_dispatch_userevent(which, data1, data2);
    handled |= streaming_overlay_dispatch_userevent(which);
    if (!handled)
    {
        switch (which)
        {
        case USER_FAKEINPUT_MOUSE_MOTION:
        {
            struct nk_vec2 *center = data1;
            if (app_has_redraw)
                nk_input_motion(ctx, center->x, center->y);
            handled = true;
            break;
        }
        case USER_FAKEINPUT_MOUSE_CLICK:
        {
            struct nk_vec2 *center = data1;
            NAVKEY_STATE state = (NAVKEY_STATE)data2;
            if (ui_fake_mouse_event_received)
            {
                // This is not the first time event received
                ui_pending_faketouch.center = center;
                ui_pending_faketouch.state = state;
                return true;
            }
            ui_fake_mouse_event_received = true;
            ui_fake_mouse_click_started = true;
            if (app_has_redraw)
            {
                nk_input_motion(ctx, center->x, center->y);
                nk_input_button(ctx, NK_BUTTON_LEFT, center->x, center->y, (state & NAVKEY_STATE_DOWN) ? nk_true : nk_false);
            }
            // Reset to (0,0) if only UP flag is present, and no NO_RESET flag
            if (state == NAVKEY_STATE_UP)
            {
                ui_send_faketouch_cancel = true;
                ui_fake_mouse_click_started = false;
            }
            return true;
        }
        case USER_FAKEINPUT_MOUSE_CANCEL:
        {
            if (app_has_redraw)
                nk_input_motion(ctx, 0, 0);
            return true;
        }
        case USER_STREAM_OPEN:
            ui_stream_render = decoder_get_render(decoder_current);
            if (ui_stream_render)
            {
                app_force_redraw = true;
                ui_stream_render->renderSetup((PSTREAM_CONFIGURATION)data1, app_render_queue_submit);
            }
            return true;
        case USER_STREAM_CLOSE:
            if (ui_stream_render)
            {
                ui_stream_render->renderCleanup();
                app_force_redraw = false;
            }
            ui_stream_render = NULL;
            return true;
        case USER_SDL_FRAME:
            if (ui_stream_render)
                ui_stream_render->renderSubmit(data1);
            return true;
        default:
            break;
        }
    }
    return handled;
}

bool ui_should_block_input()
{
    bool ret = false;
    ret |= streaming_overlay_should_block_input();
    return ret;
}

void ui_display_size(short width, short height)
{
    ui_display_width = width;
    ui_display_height = height;
    ui_scale = width / 640.0;
    ui_logic_width = width / ui_scale;
    ui_logic_height = height / ui_scale;
    applog_i("UI", "Display size changed to %d x %d", width, height);
    nk_ext_sprites_init();
}

bool ui_dispatch_navkey(struct nk_context *ctx, NAVKEY key, NAVKEY_STATE state, uint32_t timestamp)
{
    bool handled = false;
    if (streaming_status == STREAMING_NONE)
    {
        if (ui_settings_showing)
            handled |= settings_window_dispatch_navkey(ctx, key, state, timestamp);
        else
            handled |= launcher_window_dispatch_navkey(ctx, key, state, timestamp);
    }
    else
    {
        handled |= streaming_overlay_dispatch_navkey(ctx, key, state);
    }
    return handled;
}

bool ui_set_input_mode(enum UI_INPUT_MODE mode)
{
    if (ui_input_mode == mode)
    {
        return false;
    }
    ui_input_mode = mode;
    return true;
}
