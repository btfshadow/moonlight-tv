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

#include "ndl_common.h"
#include "stream/module/api.h"
#include "util/logging.h"

#include <NDL_directmedia.h>

#include <stdio.h>
#include <string.h>
#include <opus_multistream.h>

#define audio_callbacks PLUGIN_SYMBOL_NAME(audio_callbacks)

#define MAX_CHANNEL_COUNT 8
#define FRAME_SIZE 240

static OpusMSDecoder *decoder;
static short pcmBuffer[FRAME_SIZE * MAX_CHANNEL_COUNT];
static int channelCount;

static int ndl_renderer_init(int audioConfiguration, POPUS_MULTISTREAM_CONFIGURATION opusConfig, void *context, int arFlags)
{
  int rc;
  decoder = opus_multistream_decoder_create(opusConfig->sampleRate, opusConfig->channelCount, opusConfig->streams, opusConfig->coupledStreams, opusConfig->mapping, &rc);
  if (!decoder)
    return ERROR_AUDIO_OPUS_INIT_FAILED;

  channelCount = opusConfig->channelCount;
#if NDL_WEBOS5
  media_info.audio.type = NDL_AUDIO_TYPE_PCM;
  media_info.audio.pcm.channelMode = NDL_DIRECTMEDIA_AUDIO_PCM_MODE_STEREO;
  media_info.audio.pcm.format = NDL_DIRECTMEDIA_AUDIO_PCM_FORMAT_S16LE;
  media_info.audio.pcm.sampleRate = NDL_DIRECTAUDIO_SAMPLING_FREQ_OF(opusConfig->sampleRate);
  applog_i("NDL", "Opening PCM audio, channelMode=%s, format=%s, sampleRateEnum=%d", media_info.audio.pcm.channelMode,
           media_info.audio.pcm.format, media_info.audio.pcm.sampleRate);
  if (media_reload() != 0)
  {
    applog_e("NDL", "Failed to open audio: %s", NDL_DirectMediaGetError());
    return ERROR_AUDIO_OPEN_FAILED;
  }
#else
  NDL_DIRECTAUDIO_DATA_INFO info = {
      .numChannel = channelCount,
      .bitPerSample = 16,
      .nodelay = 1,
      .upperThreshold = 48,
      .lowerThreshold = 16,
      .channel = NDL_DIRECTAUDIO_CH_MAIN,
      .srcType = NDL_DIRECTAUDIO_SRC_TYPE_PCM,
      .samplingFreq = NDL_DIRECTAUDIO_SAMPLING_FREQ_OF(opusConfig->sampleRate),
  };
  if (NDL_DirectAudioOpen(&info) != 0)
  {
    applog_e("NDL", "Failed to open audio: %s", NDL_DirectMediaGetError());
    return ERROR_AUDIO_OPEN_FAILED;
  }
#endif

  return 0;
}

static void ndl_renderer_cleanup()
{
  if (decoder != NULL)
    opus_multistream_decoder_destroy(decoder);

#if NDL_WEBOS5
  media_unload();
  memset(&media_info.audio, 0, sizeof(media_info.audio));
#else
  NDL_DirectAudioClose();
#endif
}

static void ndl_renderer_decode_and_play_sample(char *data, int length)
{
  int decodeLen = opus_multistream_decode(decoder, data, length, pcmBuffer, FRAME_SIZE, 0);
  if (decodeLen > 0)
  {
#if NDL_WEBOS5
    NDL_DirectAudioPlay(pcmBuffer, decodeLen * channelCount * sizeof(short), 0);
#else
    NDL_DirectAudioPlay(pcmBuffer, decodeLen * channelCount * sizeof(short));
#endif
  }
  else
  {
    applog_e("NDL", "Opus error from decode: %d", decodeLen);
  }
}

AUDIO_RENDERER_CALLBACKS audio_callbacks = {
    .init = ndl_renderer_init,
    .cleanup = ndl_renderer_cleanup,
    .decodeAndPlaySample = ndl_renderer_decode_and_play_sample,
    .capabilities = CAPABILITY_DIRECT_SUBMIT,
};
