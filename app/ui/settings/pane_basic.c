#include "window.h"
#include "priv.h"
#include "ui/root.h"

#include <stddef.h>
#include <stdio.h>

#include "util/bus.h"
#include "util/user_event.h"

#include "stream/platform.h"

struct _resolution_option
{
    int w, h;
    char name[8];
};

struct _fps_option
{
    unsigned short fps;
    char name[8];
};

static const struct _resolution_option _supported_resolutions[] = {
    {1280, 720, "720P\0"},
    {1920, 1080, "1080P\0"},
    {2560, 1440, "1440P\0"},
    {3840, 2160, "4K\0"},
};
#define _supported_resolutions_len sizeof(_supported_resolutions) / sizeof(struct _resolution_option)

static const struct _fps_option _supported_fps[] = {
    {30, "30 FPS"},
    {60, "60 FPS"},
    {120, "120 FPS"},
};
#define _supported_fps_len sizeof(_supported_fps) / sizeof(struct _fps_option)

struct _ch_option
{
    int configuration;
    char name[16];
};

static const struct _ch_option _supported_ch[] = {
    {AUDIO_CONFIGURATION_STEREO, "Stereo"},
    {AUDIO_CONFIGURATION_51_SURROUND, "5.1 Surround"},
    {AUDIO_CONFIGURATION_71_SURROUND, "7.1 Surround"},
};
#define _supported_ch_len sizeof(_supported_ch) / sizeof(struct _ch_option)

static struct combo_hovered_item_t combo_hovered_item;

static struct nk_vec2 combo_focused_center;

#define BITRATE_MIN 5000
#define BITRATE_MAX 120000
#define BITRATE_STEP 500

static int _max_bitrate = BITRATE_MAX;
static int _max_ch_idx = 0;

static void _set_fps(int fps);
static void _set_res(int w, int h);
static void _update_bitrate();
static bool _ch_combo(struct nk_context *ctx, int *value);
static bool combo_item_select(int offset);
static int _calc_max_ch_idx();

static bool _render(struct nk_context *ctx, bool *showing_combo)
{
    static struct nk_rect item_bounds = {0, 0, 0, 0};
    int item_index = 0;

    nk_layout_row_dynamic_s(ctx, 25, 1);
    nk_label(ctx, "Resolution and FPS", NK_TEXT_LEFT);
    static const float ratio_resolution_fps[] = {0.6, 0.4};
    nk_layout_row_s(ctx, NK_DYNAMIC, 25, 2, ratio_resolution_fps);
    settings_item_update_selected_bounds(ctx, item_index++, &item_bounds);
    if (nk_combo_begin_label(ctx, settings_res_label, nk_vec2(nk_widget_width(ctx), 200 * NK_UI_SCALE)))
    {
        *showing_combo = true;
        if (combo_hovered_item.combo != 1)
        {
            combo_hovered_item.combo = 1;
            combo_hovered_item.count = _supported_resolutions_len;
            combo_hovered_item.request = -1;
            combo_hovered_item.item = -1;
        }
        nk_layout_row_dynamic_s(ctx, 25, 1);
        bool ever_hovered = false;
        for (int i = 0; i < _supported_resolutions_len; i++)
        {
            struct nk_rect ci_bounds = nk_widget_bounds(ctx);
            nk_bool hovered = nk_input_is_mouse_hovering_rect(&ctx->input, ci_bounds);
            if (hovered)
            {
                combo_hovered_item.item = i;
                ever_hovered = true;
            }
            if (combo_hovered_item.request == i)
            {
                // Send mouse pointer to the item
                combo_focused_center = nk_rect_center(ci_bounds);
                bus_pushevent(USER_FAKEINPUT_MOUSE_MOTION, &combo_focused_center, NULL);
                combo_hovered_item.request = -1;
            }
            struct _resolution_option o = _supported_resolutions[i];
            if (nk_combo_item_label(ctx, o.name, NK_TEXT_LEFT))
            {
                _set_res(o.w, o.h);
                _update_bitrate();
            }
        }
        nk_combo_end(ctx);
        if (!ever_hovered)
        {
            combo_hovered_item.item = -1;
        }
    }
    settings_item_update_selected_bounds(ctx, item_index++, &item_bounds);
    if (nk_combo_begin_label(ctx, settings_fps_label, nk_vec2(nk_widget_width(ctx), 200 * NK_UI_SCALE)))
    {
        *showing_combo = true;
        if (combo_hovered_item.combo != 2)
        {
            combo_hovered_item.combo = 2;
            combo_hovered_item.count = _supported_fps_len;
            combo_hovered_item.request = -1;
            combo_hovered_item.item = -1;
        }
        nk_layout_row_dynamic_s(ctx, 25, 1);
        bool ever_hovered = false;
        for (int i = 0; i < _supported_fps_len; i++)
        {
            struct nk_rect ci_bounds = nk_widget_bounds(ctx);
            nk_bool hovered = nk_input_is_mouse_hovering_rect(&ctx->input, ci_bounds);
            if (hovered)
            {
                combo_hovered_item.item = i;
                ever_hovered = true;
            }
            if (combo_hovered_item.request == i)
            {
                // Send mouse pointer to the item
                combo_focused_center = nk_rect_center(ci_bounds);
                bus_pushevent(USER_FAKEINPUT_MOUSE_MOTION, &combo_focused_center, NULL);
                combo_hovered_item.request = -1;
            }
            struct _fps_option o = _supported_fps[i];
            if (nk_combo_item_label(ctx, o.name, NK_TEXT_LEFT))
            {
                _set_fps(o.fps);
                _update_bitrate();
            }
        }
        nk_combo_end(ctx);
        if (!ever_hovered)
        {
            combo_hovered_item.item = -1;
        }
    }
    nk_layout_row_dynamic_s(ctx, 25, 1);
    nk_label(ctx, "Video bitrate", NK_TEXT_LEFT);
    settings_item_update_selected_bounds(ctx, item_index++, &item_bounds);
    nk_property_int(ctx, "kbps:", BITRATE_MIN, &app_configuration->stream.bitrate, _max_bitrate, BITRATE_STEP, 50);

#if HAVE_SURROUND_SOUND
    nk_label(ctx, "Audio channels", NK_TEXT_LEFT);
    settings_item_update_selected_bounds(ctx, item_index++, &item_bounds);
    *showing_combo |= _ch_combo(ctx, &app_configuration->stream.audioConfiguration);
#endif

    nk_layout_row_dynamic_s(ctx, 4, 1);
    return true;
}

static int _itemcount()
{
    return 4;
}

static void _windowopen()
{
    _set_fps(app_configuration->stream.fps);
    _set_res(app_configuration->stream.width, app_configuration->stream.height);
    _max_bitrate = decoder_info.maxBitrate ? decoder_info.maxBitrate : BITRATE_MAX;
    _max_ch_idx = _calc_max_ch_idx();
}

int _calc_max_ch_idx()
{
    int max_ch = CHANNEL_COUNT_FROM_AUDIO_CONFIGURATION(module_audio_configuration());
    for (int i = _supported_ch_len - 1; i >= 0; i--)
    {
        if (max_ch >= CHANNEL_COUNT_FROM_AUDIO_CONFIGURATION(_supported_ch[i].configuration))
            return i;
    }
    return 0;
}

bool _ch_combo(struct nk_context *ctx, int *value)
{
    struct nk_rect combo_bounds = nk_widget_bounds(ctx);
    int combo_height = NK_MIN(200 * NK_UI_SCALE, ui_display_height - (combo_bounds.y + combo_bounds.h));
    int ch_idx = 0;
    for (int i = 0; i <= _max_ch_idx; i++)
    {
        if (_supported_ch[i].configuration == *value)
        {
            ch_idx = i;
            break;
        }
    }
    if (nk_combo_begin_label(ctx, _supported_ch[ch_idx].name, nk_vec2(nk_widget_width(ctx), combo_height)))
    {
        if (combo_hovered_item.combo != 1)
        {
            combo_hovered_item.combo = 1;
            combo_hovered_item.count = _max_ch_idx;
            combo_hovered_item.request = -1;
            combo_hovered_item.item = -1;
        }
        nk_layout_row_dynamic_s(ctx, 25, 1);
        bool ever_hovered = false;
        for (int i = 0; i <= _max_ch_idx; i++)
        {
            struct nk_rect ci_bounds = nk_widget_bounds(ctx);
            nk_bool hovered = nk_input_is_mouse_hovering_rect(&ctx->input, ci_bounds);
            if (hovered)
            {
                combo_hovered_item.item = i;
                ever_hovered = true;
            }
            if (combo_hovered_item.request == i)
            {
                // Send mouse pointer to the item
                combo_focused_center = nk_rect_center(ci_bounds);
                bus_pushevent(USER_FAKEINPUT_MOUSE_MOTION, &combo_focused_center, NULL);
                combo_hovered_item.request = -1;
            }
            if (nk_combo_item_label(ctx, _supported_ch[i].name, NK_TEXT_LEFT))
                app_configuration->stream.audioConfiguration = _supported_ch[i].configuration;
        }
        nk_combo_end(ctx);
        return true;
    }
    return false;
}

static bool _navkey(struct nk_context *ctx, NAVKEY navkey, NAVKEY_STATE state, uint32_t timestamp)
{
    switch (navkey)
    {
    case NAVKEY_LEFT:
        if (settings_showing_combo)
        {
            return true;
        }
        else if (settings_hovered_item == 1)
        {
            if (state == NAVKEY_STATE_UP)
                settings_pane_item_offset(-1);
            return true;
        }
        else if (settings_hovered_item == 2)
        {
            if (!navkey_intercept_repeat(state, timestamp))
                app_configuration->stream.bitrate = NK_CLAMP(BITRATE_MIN, app_configuration->stream.bitrate - BITRATE_STEP,
                                                             _max_bitrate);
            return true;
        }
        return false;
    case NAVKEY_RIGHT:
        if (settings_showing_combo)
        {
            return true;
        }
        else if (settings_hovered_item == 0)
        {
            if (state == NAVKEY_STATE_UP)
                settings_pane_item_offset(1);
            return true;
        }
        else if (settings_hovered_item == 2)
        {
            if (!navkey_intercept_repeat(state, timestamp))
                app_configuration->stream.bitrate = NK_CLAMP(BITRATE_MIN, app_configuration->stream.bitrate + BITRATE_STEP,
                                                             _max_bitrate);
            return true;
        }
        return true;
    case NAVKEY_UP:
        if (settings_showing_combo)
        {
            return navkey_intercept_repeat(state, timestamp) || combo_item_select(-1);
        }
        else if (settings_hovered_item >= 2)
        {
            if (state == NAVKEY_STATE_UP)
                settings_pane_item_offset(0 - settings_hovered_item);
        }
        return true;
    case NAVKEY_DOWN:
        if (settings_showing_combo)
        {
            return navkey_intercept_repeat(state, timestamp) || combo_item_select(1);
        }
        else if (settings_hovered_item <= 1)
        {
            if (state == NAVKEY_STATE_UP)
                settings_pane_item_offset(2 - settings_hovered_item);
        }
        else
        {
            if (state == NAVKEY_STATE_UP)
                settings_pane_item_offset(1);
        }
        return true;
    case NAVKEY_CONFIRM:
        if (settings_showing_combo)
        {
            // Fake click on the item
            bus_pushevent(USER_FAKEINPUT_MOUSE_CLICK, &combo_focused_center, (void *)state);
            return true;
        }
    default:
        break;
    }
    return false;
}

void _set_fps(int fps)
{
    // It is not possible to have overflow since fps is capped to 999
    sprintf(settings_fps_label, "%d FPS", fps % 1000);
    app_configuration->stream.fps = fps;
}

void _set_res(int w, int h)
{
    switch (RES_MERGE(w, h))
    {
    case RES_720P:
        sprintf(settings_res_label, "720P");
        break;
    case RES_1080P:
        sprintf(settings_res_label, "1080P");
        break;
    case RES_1440P:
        sprintf(settings_res_label, "1440P");
        break;
    case RES_4K:
        sprintf(settings_res_label, "4K");
        break;
    default:
        sprintf(settings_res_label, "%3d*%3d", w, h);
        break;
    }
    app_configuration->stream.width = w;
    app_configuration->stream.height = h;
}

void _update_bitrate()
{
    int optimal = settings_optimal_bitrate(app_configuration->stream.width, app_configuration->stream.height, app_configuration->stream.fps);
    app_configuration->stream.bitrate = NK_CLAMP(BITRATE_MIN, optimal, _max_bitrate);
}

bool combo_item_select(int offset)
{
    if (combo_hovered_item.item < 0)
    {
        // No item focused before, select first one
        combo_hovered_item.request = 0;
    }
    else
    {
        int item = combo_hovered_item.item + offset;
        if (item >= 0 && item < combo_hovered_item.count)
        {
            combo_hovered_item.request = item;
        }
    }
    return true;
}

struct settings_pane settings_pane_basic = {
    .title = "Basic Settings",
    .render = _render,
    .navkey = _navkey,
    .itemcount = _itemcount,
    .onselect = NULL,
    .onwindowopen = _windowopen,
    .onwindowclose = NULL,
};
