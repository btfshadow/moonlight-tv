/*
 * This file is part of Moonlight Embedded.
 *
 * Copyright (C) 2015-2017 Iwan Timmer
 *
 * Moonlight is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * Moonlight is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Moonlight; if not, see <http://www.gnu.org/licenses/>.
 */
#pragma once

#include <Limelight.h>

#include <dlfcn.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#include "module/api.h"

enum DECODER_T
{
    DECODER_AUTO = -1,
    DECODER_NONE = 0,
    DECODER_FFMPEG,
    DECODER_NDL,
    DECODER_LGNC,
    DECODER_SMP,
    DECODER_DILE,
    DECODER_PI,
#if DEBUG
    DECODER_EMPTY,
#endif
    DECODER_COUNT,
};
typedef enum DECODER_T DECODER;

enum AUDIO_T
{
    AUDIO_DECODER = -2,
    AUDIO_AUTO = -1,
    AUDIO_SDL = 0,
    AUDIO_PULSE,
    AUDIO_ALSA,
    AUDIO_NDL,
#if DEBUG
    AUDIO_EMPTY,
#endif
    AUDIO_COUNT,
};
typedef enum AUDIO_T AUDIO;

typedef bool (*MODULE_INIT_FN)(int argc, char *argv[], PHOST_CONTEXT host);
typedef bool (*DECODER_CHECK_FN)(PDECODER_INFO);
typedef bool (*AUDIO_CHECK_FN)(PAUDIO_INFO);
typedef void (*MODULE_FINALIZE_FN)();

typedef struct DECODER_SYMBOLS_T
{
    bool valid;
    MODULE_INIT_FN init;
    DECODER_CHECK_FN check;
    MODULE_FINALIZE_FN finalize;
    PAUDIO_RENDERER_CALLBACKS adec;
    PDECODER_RENDERER_CALLBACKS vdec;
    PVIDEO_PRESENTER_CALLBACKS pres;
    PVIDEO_RENDER_CALLBACKS rend;
} DECODER_SYMBOLS;

typedef struct AUDIO_SYMBOLS_T
{
    bool valid;
    MODULE_INIT_FN init;
    AUDIO_CHECK_FN check;
    MODULE_FINALIZE_FN finalize;
    PAUDIO_RENDERER_CALLBACKS callbacks;
} AUDIO_SYMBOLS;

typedef struct MODULE_LIB_DEFINITION
{
    const char *suffix;
    const char *library;
} MODULE_LIB_DEFINITION;

typedef struct MODULE_DEFINITION
{
    const char *name;
    const char *id;
    const MODULE_LIB_DEFINITION *dynlibs;
    const size_t liblen;
    const union
    {
        const void *ptr;
        const DECODER_SYMBOLS *decoder;
        const AUDIO_SYMBOLS *audio;
    } symbols;
} MODULE_DEFINITION;

extern DECODER decoder_pref_requested;
extern DECODER decoder_current;
extern int decoder_current_libidx;
extern DECODER_INFO decoder_info;
extern MODULE_DEFINITION decoder_definitions[DECODER_COUNT];

extern DECODER_RENDERER_CALLBACKS decoder_callbacks_dummy;

extern HOST_CONTEXT module_host_context;

DECODER decoder_by_id(const char *id);

DECODER decoder_init(const char *name, int argc, char *argv[]);
PDECODER_RENDERER_CALLBACKS decoder_get_video();
PVIDEO_PRESENTER_CALLBACKS decoder_get_presenter();
PVIDEO_RENDER_CALLBACKS decoder_get_render();
void decoder_finalize();

extern AUDIO audio_pref_requested;
extern AUDIO audio_current;
extern int audio_current_libidx;
extern AUDIO_INFO audio_info;
extern MODULE_DEFINITION audio_definitions[AUDIO_COUNT];

AUDIO audio_by_id(const char *id);

AUDIO audio_init(const char *name, int argc, char *argv[]);
void audio_finalize();

PAUDIO_RENDERER_CALLBACKS module_get_audio(char *audio_device);

int module_audio_configuration();

static const DECODER decoder_orders[] = {
#if TARGET_WEBOS
    DECODER_SMP, DECODER_NDL, DECODER_LGNC
#elif TARGET_LGNC
    DECODER_LGNC
#elif TARGET_RASPI
    DECODER_PI, DECODER_FFMPEG
#else
    DECODER_FFMPEG
#endif
};
static const size_t decoder_orders_len = sizeof(decoder_orders) / sizeof(DECODER);

static const AUDIO audio_orders[] = {
#if TARGET_WEBOS
    AUDIO_NDL, AUDIO_PULSE, AUDIO_SDL
#else
    AUDIO_PULSE, AUDIO_ALSA, AUDIO_SDL
#endif
};
static const size_t audio_orders_len = sizeof(audio_orders) / sizeof(AUDIO);