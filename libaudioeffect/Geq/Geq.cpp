/*
 * Copyright (C) 2018 Amlogic Corporation.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *  DESCRIPTION:
 *      This file implements a special EQ  from Amlogic.
 *
 */

#define LOG_TAG "GEQ_Effect"
//#define LOG_NDEBUG 0

#include <cutils/log.h>
#include <cutils/properties.h>
#include <utils/Log.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <hardware/audio_effect.h>
#include <cutils/properties.h>
#include <stdio.h>
#include <unistd.h>

#include "IniParser.h"
#include "Geq.h"

extern "C" {

#include "libAmlGeq.h"
#include "../Utility/AudioFade.h"

#define MODEL_SUM_DEFAULT_PATH "/vendor/etc/tvconfig/model/model_sum.ini"
#define AUDIO_EFFECT_DEFAULT_PATH "/vendor/etc/tvconfig/audio/AMLOGIC_AUDIO_EFFECT_DEFAULT.ini"

// effect_handle_t interface implementation for 9bands EQ effect
extern const struct effect_interface_s GEQInterface;

//GEQ effect TYPE: 5f760a9b-d058-4b2b-acc7-145a5ebff7fe
//GEQ effect UUID: 2e2a5fa6-cae8-45f5-bb70-a29c1f3074b2

const effect_descriptor_t GEQDescriptor = {
        {0x5f760a9b, 0xd058, 0x4b2b, 0xacc7, {0x14, 0x5a, 0x5e, 0xbf, 0xf7, 0xfe}}, // type
        {0x2e2a5fa6, 0xcae8, 0x45f5, 0xbb70, {0xa2, 0x9c, 0x1f, 0x30, 0x74, 0xb2}}, // uuid
        EFFECT_CONTROL_API_VERSION,
        EFFECT_FLAG_TYPE_POST_PROC | EFFECT_FLAG_DEVICE_IND | EFFECT_FLAG_NO_PROCESS | EFFECT_FLAG_OFFLOAD_SUPPORTED,
        GEQ_CUP_LOAD_ARM9E,
        GEQ_MEM_USAGE,
        "Geq",//nine bands eq
        "Amlogic",
};

enum geq_state_e {
    GEQ_STATE_UNINITIALIZED,
    GEQ_STATE_INITIALIZED,
    GEQ_STATE_ACTIVE,
};

typedef enum {
    GEQ_PARAM_INVALID = -1,
    GEQ_PARAM_ENABLE,
    GEQ_PARAM_EFFECT_MODE,
    GEQ_PARAM_EFFECT_CUSTOM,
} GEQparams;

typedef struct GEQcfg_s {
    int band1;
    int band2;
    int band3;
    int band4;
    int band5;
    int band6;
    int band7;
    int band8;
    int band9;
} GEQcfg;

typedef struct GEQcfg_8bit_s {
    signed char band1;
    signed char band2;
    signed char band3;
    signed char band4;
    signed char band5;
    signed char band6;
    signed char band7;
    signed char band8;
    signed char band9;
} GEQcfg_8bit;

typedef struct GEQdata_s {
    /* This struct is used to initialize GEQ default config*/
    GEQcfg       cfg;
    int32_t       *usr_cfg;
    int32_t       count;
    int32_t       enable;
    int32_t       mode;
    int32_t       mode_num;
    int32_t       band_num;
} GEQdata;

typedef struct GEQContext_s {
    const struct effect_interface_s *itfe;
    effect_config_t                 config;
    geq_state_e                    state;
    GEQdata                        gGEQdata;

    // when recieve setting change from app,
    //"fade audio out->do setting->fade audio In"
    int                             bUseFade;
    AudioFade_t                     gAudFade;
    int32_t                         modeValue;
} GEQContext;

const char *GEQStatusstr[] = {"Disable", "Enable"};

static int getprop_bool(const char *path)
{
    char buf[PROPERTY_VALUE_MAX];
    int ret = -1;

    ret = property_get(path, buf, NULL);
    if (ret > 0) {
        if (strcasecmp(buf, "true") == 0 || strcmp(buf, "1") == 0) {
            return 1;
        }
    }
    return 0;
}

int GEQ_get_model_name(char *model_name, int size)
{
     int ret = -1;
    char node[PROPERTY_VALUE_MAX];

    ret = property_get("tv.model_name", node, NULL);

    if (ret < 0)
        snprintf(model_name, size, "DEFAULT");
    else
        snprintf(model_name, size, "%s", node);
    ALOGD("%s: Model Name -> %s", __FUNCTION__, model_name);
    return ret;
}

int GEQ_get_ini_file(char *ini_name, int size)
{
    int result = -1;
    char model_name[50] = {0};
    IniParser* pIniParser = NULL;
    const char *ini_value = NULL;
    const char *filename = MODEL_SUM_DEFAULT_PATH;

    GEQ_get_model_name(model_name, sizeof(model_name));
    pIniParser = new IniParser();
    if (pIniParser->parse(filename) < 0) {
        ALOGW("%s: Load INI file -> %s Failed", __FUNCTION__, filename);
        goto exit;
    }

    ini_value = pIniParser->GetString(model_name, "AMLOGIC_AUDIO_EFFECT_INI_PATH", AUDIO_EFFECT_DEFAULT_PATH);
    if (ini_value == NULL || access(ini_value, F_OK) == -1) {
        ALOGD("%s: INI File is not exist", __FUNCTION__);
        goto exit;
    }
    ALOGD("%s: INI File -> %s", __FUNCTION__, ini_value);
    strncpy(ini_name, ini_value, size);

    result = 0;
exit:
    delete pIniParser;
    pIniParser = NULL;
    return result;
}

int GEQ_parse_mode_config(GEQContext *pContext, int mode_num, int band_num, const char *buffer)
{
    int i;
    char *Rch = (char *)buffer;
    GEQdata *data = &pContext->gGEQdata;

    if (data->usr_cfg == NULL) {
        data->usr_cfg = (int *)calloc(mode_num * band_num, sizeof(int));
        if (!data->usr_cfg) {
            ALOGE("%s: alloc failed", __FUNCTION__);
            return -EINVAL;
        }
    }
    for (i = 0; i < mode_num * band_num; i++) {
        if (i == 0)
            Rch = strtok(Rch, ",");
        else
            Rch = strtok(NULL, ",");
        if (Rch == NULL) {
            ALOGE("%s: Config Parse failed, using default config", __FUNCTION__);
            return -1;
        }
        data->usr_cfg[i] = atoi(Rch);
    }

    return 0;
}

int GEQ_load_ini_file(GEQContext *pContext)
{
    int result = -1;
    char ini_name[100] = {0};
    const char *ini_value = NULL;
    GEQdata *data = &pContext->gGEQdata;
    IniParser* pIniParser = NULL;

    if (GEQ_get_ini_file(ini_name, sizeof(ini_name)) < 0)
        goto error;

    pIniParser = new IniParser();
    if (pIniParser->parse((const char *)ini_name) < 0) {
        ALOGD("%s: %s load failed", __FUNCTION__, ini_name);
        goto error;
    }
    ini_value = pIniParser->GetString("Geq", "geq_enable", "1");
    if (ini_value == NULL)
        goto error;
    ALOGD("%s: enable -> %s", __FUNCTION__, ini_value);
    data->enable = atoi(ini_value);


    ini_value = pIniParser->GetString("Geq", "geq_modenum", "6");
    if (ini_value == NULL)
        goto error;
    ALOGD("%s: sound mode num -> %s", __FUNCTION__, ini_value);
    data->mode_num = atoi(ini_value);
    ini_value = pIniParser->GetString("Geq", "geq_bandnum", "9");
    if (ini_value == NULL)
        goto error;
    ALOGD("%s: sound band num -> %s", __FUNCTION__, ini_value);
    data->band_num = atoi(ini_value);
    // level parse
    ini_value = pIniParser->GetString("Geq", "geq_config", "NULL");
    if (ini_value == NULL)
        goto error;
    ALOGD("%s: condig -> %s", __FUNCTION__, ini_value);
    result = GEQ_parse_mode_config(pContext, data->mode_num, data->band_num, ini_value);

    result = 0;
error:
    ALOGD("%s: %s", __FUNCTION__, result == 0 ? "sucessful" : "failed");
    delete pIniParser;
    pIniParser = NULL;
    return result;
}

int GEQ_init(GEQContext *pContext)
{
    GEQdata *data = &pContext->gGEQdata;
    //int32_t count = data->count;

    pContext->config.inputCfg.accessMode = EFFECT_BUFFER_ACCESS_READ;
    pContext->config.inputCfg.channels = AUDIO_CHANNEL_OUT_STEREO;
    pContext->config.inputCfg.format = AUDIO_FORMAT_PCM_16_BIT;
    pContext->config.inputCfg.samplingRate = 48000;
    pContext->config.inputCfg.bufferProvider.getBuffer = NULL;
    pContext->config.inputCfg.bufferProvider.releaseBuffer = NULL;
    pContext->config.inputCfg.bufferProvider.cookie = NULL;
    pContext->config.inputCfg.mask = EFFECT_CONFIG_ALL;
    pContext->config.outputCfg.accessMode = EFFECT_BUFFER_ACCESS_ACCUMULATE;
    pContext->config.outputCfg.channels = AUDIO_CHANNEL_OUT_STEREO;
    pContext->config.outputCfg.format = AUDIO_FORMAT_PCM_16_BIT;
    pContext->config.outputCfg.samplingRate = 48000;
    pContext->config.outputCfg.bufferProvider.getBuffer = NULL;
    pContext->config.outputCfg.bufferProvider.releaseBuffer = NULL;
    pContext->config.outputCfg.bufferProvider.cookie = NULL;
    pContext->config.outputCfg.mask = EFFECT_CONFIG_ALL;

    /* default band is usr_cfg[(count>>LSR)+1] */
    data->cfg.band1 = 0/*data->usr_cfg[(count >> LSR) + 1].band1*/;
    data->cfg.band2 = 0/*data->usr_cfg[(count >> LSR) + 1].band2*/;
    data->cfg.band3 = 0/*data->usr_cfg[(count >> LSR) + 1].band3*/;
    data->cfg.band4 = 0/*data->usr_cfg[(count >> LSR) + 1].band4*/;
    data->cfg.band5 = 0/*data->usr_cfg[(count >> LSR) + 1].band5*/;
    data->cfg.band6 = 0/*data->usr_cfg[(count >> LSR) + 1].band6*/;
    data->cfg.band7 = 0/*data->usr_cfg[(count >> LSR) + 1].band7*/;
    data->cfg.band8 = 0/*data->usr_cfg[(count >> LSR) + 1].band8*/;
    data->cfg.band9 = 0/*data->usr_cfg[(count >> LSR) + 1].band9*/;

    GEQ_init_api((void *)data);

    ALOGD("%s: sucessful", __FUNCTION__);

    return 0;
}
int GEQ_reset(GEQContext *pContext __unused)
{
    GEQ_reset_api();
    return 0;
}

int GEQ_configure(GEQContext *pContext, effect_config_t *pConfig)
{
    if (pConfig->inputCfg.samplingRate != pConfig->outputCfg.samplingRate)
        return -EINVAL;
    if (pConfig->inputCfg.channels != pConfig->outputCfg.channels)
        return -EINVAL;
    if (pConfig->inputCfg.format != pConfig->outputCfg.format)
        return -EINVAL;
    if (pConfig->inputCfg.channels != AUDIO_CHANNEL_OUT_STEREO) {
        ALOGW("%s: channels in = 0x%x channels out = 0x%x", __FUNCTION__, pConfig->inputCfg.channels, pConfig->outputCfg.channels);
        pConfig->inputCfg.channels = pConfig->outputCfg.channels = AUDIO_CHANNEL_OUT_STEREO;
    }
    if (pConfig->outputCfg.accessMode != EFFECT_BUFFER_ACCESS_WRITE &&
            pConfig->outputCfg.accessMode != EFFECT_BUFFER_ACCESS_ACCUMULATE)
        return -EINVAL;
    if (pConfig->inputCfg.format != AUDIO_FORMAT_PCM_16_BIT) {
        ALOGW("%s: format in = 0x%x format out = 0x%x", __FUNCTION__, pConfig->inputCfg.format, pConfig->outputCfg.format);
        pConfig->inputCfg.format = pConfig->outputCfg.format = AUDIO_FORMAT_PCM_16_BIT;
    }

    memcpy(&pContext->config, pConfig, sizeof(effect_config_t));

    if (pContext->bUseFade) {
        int channels = 2;
        if (pConfig->inputCfg.channels == AUDIO_CHANNEL_OUT_STEREO) {
            channels = 2;
        }
        AudioFadeSetFormat(&pContext->gAudFade, pConfig->inputCfg.samplingRate, channels, pConfig->inputCfg.format);
    }

    return 0;
}

int GEQ_getParameter(GEQContext *pContext, void *pParam, size_t *pValueSize, void *pValue)
{
    int32_t param = *(int32_t *)pParam;
    int32_t i, value;
    GEQcfg_8bit_s custom_value;
    GEQdata *data = &pContext->gGEQdata;

    switch (param) {
    case GEQ_PARAM_ENABLE:
        if (*pValueSize < sizeof(int32_t)) {
            *pValueSize = 0;
            return -EINVAL;
        }
        value = data->enable;
        *(int32_t *) pValue = value;
        ALOGD("%s: Get status -> %s", __FUNCTION__, GEQStatusstr[value]);
        break;
    case GEQ_PARAM_EFFECT_MODE:
        if (*pValueSize < sizeof(int32_t)) {
            *pValueSize = 0;
            return -EINVAL;
        }
        value = data->mode;
        *(int32_t *) pValue = value;
        ALOGD("%s: Get Mode -> %d", __FUNCTION__, value);
        break;
    case GEQ_PARAM_EFFECT_CUSTOM:
        if (*pValueSize < sizeof(GEQcfg_8bit_s)) {
            *pValueSize = 0;
            return -EINVAL;
        }
        custom_value.band1 = (signed char)data->usr_cfg[(data->mode_num - 1) * data->band_num];
        custom_value.band2 = (signed char)data->usr_cfg[(data->mode_num - 1) * data->band_num + 1];
        custom_value.band3 = (signed char)data->usr_cfg[(data->mode_num - 1) * data->band_num + 2];
        custom_value.band4 = (signed char)data->usr_cfg[(data->mode_num - 1) * data->band_num + 3];
        custom_value.band5 = (signed char)data->usr_cfg[(data->mode_num - 1) * data->band_num + 4];
        custom_value.band6 = (signed char)data->usr_cfg[(data->mode_num - 1) * data->band_num + 5];
        custom_value.band7 = (signed char)data->usr_cfg[(data->mode_num - 1) * data->band_num + 6];
        custom_value.band8 = (signed char)data->usr_cfg[(data->mode_num - 1) * data->band_num + 7];
        custom_value.band9 = (signed char)data->usr_cfg[(data->mode_num - 1) * data->band_num + 8];
        *(GEQcfg_8bit_s *) pValue = custom_value;
        for (i = 0; i < data->band_num; i++) {
            ALOGD("%s: Get band[%d] -> %d", __FUNCTION__, i + 1, data->usr_cfg[(data->mode_num - 1) * data->band_num + i]);
        }
    break;
    default:
        ALOGE("%s: unknown param %d", __FUNCTION__, param);
        return -EINVAL;
    }

    return 0;
}

int GEQ_setParameter(GEQContext *pContext, void *pParam, void *pValue)
{
    int32_t param = *(int32_t *)pParam;
    int32_t i, value;
    int needFade = 0;
    GEQcfg_8bit_s custom_value;
    GEQdata *data = &pContext->gGEQdata;

    switch (param) {
    case GEQ_PARAM_ENABLE:
        value = *(int32_t *)pValue;
        data->enable = value;
        ALOGD("%s: Set status -> %s", __FUNCTION__, GEQStatusstr[value]);
        break;
    case GEQ_PARAM_EFFECT_MODE:
        value = *(int32_t *)pValue;
        if (value < 0 || value > data->mode_num) {
            ALOGE("%s: incorrect mode value %d", __FUNCTION__, value);
            return -EINVAL;
        }
        data->mode = value;
        pContext->modeValue = data->mode;
        ALOGD("%s: Set Mode -> %d, bUseFade = %d", __FUNCTION__, value , pContext->bUseFade);
        if (pContext->bUseFade) {
            AudioFade_t *pAudioFade = (AudioFade_t *) & (pContext->gAudFade);
            AudioFadeInit(pAudioFade, fadeLinear, 10, 0);
            AudioFadeSetState(pAudioFade, AUD_FADE_OUT_START);
        } else {
            for (i = 0; i < data->band_num; i++) {
                ALOGD("%s: Set band[%d] -> %d", __FUNCTION__, i + 1, data->usr_cfg[value * data->band_num + i]);
                if (data->usr_cfg[data->mode * data->band_num + i] < -10)
                    data->usr_cfg[data->mode * data->band_num + i] = -10;
                else if (data->usr_cfg[data->mode * data->band_num + i] > 10)
                    data->usr_cfg[data->mode * data->band_num + i] = 10;
                GEQ_setBand_api(data->usr_cfg[data->mode * data->band_num + i], i + 1);
            }
        }
        break;
    case GEQ_PARAM_EFFECT_CUSTOM:
        custom_value = *(GEQcfg_8bit_s *)pValue;
        data->usr_cfg[(data->mode_num - 1) * data->band_num] = (signed int)custom_value.band1;
        data->usr_cfg[(data->mode_num - 1) * data->band_num + 1] = (signed int)custom_value.band2;
        data->usr_cfg[(data->mode_num - 1) * data->band_num + 2] = (signed int)custom_value.band3;
        data->usr_cfg[(data->mode_num - 1) * data->band_num + 3] = (signed int)custom_value.band4;
        data->usr_cfg[(data->mode_num - 1) * data->band_num + 4] = (signed int)custom_value.band5;
        data->usr_cfg[(data->mode_num - 1) * data->band_num + 5] = (signed int)custom_value.band6;
        data->usr_cfg[(data->mode_num - 1) * data->band_num + 6] = (signed int)custom_value.band7;
        data->usr_cfg[(data->mode_num - 1) * data->band_num + 7] = (signed int)custom_value.band8;
        data->usr_cfg[(data->mode_num - 1) * data->band_num + 8] = (signed int)custom_value.band9;

        if (pContext->modeValue != data->mode_num - 1) {
            // if we change from other mode to "CUSTOM" mode , need to do fade
            needFade = 1;
        } else {
            // if we are already in "CUSTOM" mode , no need to do fade
            needFade = 0;
        }

        pContext->modeValue = data->mode_num - 1;
        ALOGD("%s: Set Mode -> %d, bUseFade = %d", __FUNCTION__, pContext->modeValue , pContext->bUseFade);
        if (pContext->bUseFade && needFade) {
            AudioFade_t *pAudioFade = (AudioFade_t *) & (pContext->gAudFade);
            AudioFadeInit(pAudioFade, fadeLinear, 10, 0);
            AudioFadeSetState(pAudioFade, AUD_FADE_OUT_START);
        } else {
            for (i = 0; i < data->band_num; i++) {
                ALOGD("%s: Set band[%d] -> %d", __FUNCTION__, i + 1, data->usr_cfg[(data->mode_num - 1) * data->band_num + i]);
                if (data->usr_cfg[(data->mode_num - 1) * data->band_num + i] > 10)
                    data->usr_cfg[(data->mode_num - 1) * data->band_num + i] = 10;
                else if ( data->usr_cfg[(data->mode_num - 1) * data->band_num + i] < -10)
                    data->usr_cfg[(data->mode_num - 1) * data->band_num + i] = -10;
                GEQ_setBand_api(data->usr_cfg[(data->mode_num - 1) * data->band_num + i], i + 1);
            }
        }
    break;
    default:
        ALOGE("%s: unknown param %08x", __FUNCTION__, param);
        return -EINVAL;
    }

    return 0;
}

int GEQ_release(GEQContext *pContext)
{
    GEQdata *data = &pContext->gGEQdata;
    GEQ_release_api();
    if (data->usr_cfg != NULL) {
        free(data->usr_cfg);
        data->usr_cfg = NULL;
    }
    return 0;
}

//-------------------Effect Control Interface Implementation--------------------------

int GEQ_process(effect_handle_t self, audio_buffer_t *inBuffer, audio_buffer_t *outBuffer)
{
    GEQContext * pContext = (GEQContext *)self;
    if (pContext == NULL) {
        return -EINVAL;
    }

   if (inBuffer == NULL || inBuffer->raw == NULL ||
        outBuffer == NULL || outBuffer->raw == NULL ||
        inBuffer->frameCount != outBuffer->frameCount ||
        inBuffer->frameCount == 0) {
        return -EINVAL;
    }
    if (pContext->state != GEQ_STATE_ACTIVE) {
        return -ENODATA;
    }
    int16_t *in  = (int16_t *)inBuffer->raw;
    int16_t *out = (int16_t *)outBuffer->raw;
    GEQdata *data = &pContext->gGEQdata;
    if (!data->enable) {
        for (size_t i = 0; i < inBuffer->frameCount; i++) {
            *out++ = *in++;
            *out++ = *in++;
        }
    } else {
        if (pContext->bUseFade) {
            int i;
            AudioFade_t *pAudFade = (AudioFade_t *) & (pContext->gAudFade);
            unsigned int modeValue;
            unsigned int nSamples = (unsigned int)inBuffer->frameCount;

            if (pAudFade->mFadeState != AUD_FADE_IDLE) {
                ALOGI("%s: mFadeState -> %d, mCurrentVolume = %d,nSamples = %d", __FUNCTION__, pAudFade->mFadeState, pAudFade->mCurrentVolume, nSamples);
            }

            // do audio traisition
            switch (pAudFade->mFadeState) {
            case AUD_FADE_OUT_START: {
                pAudFade->mTargetVolume = 0;
                pAudFade->mStartVolume = 1 << 16;
                pAudFade->mCurrentVolume = 1 << 16;
                pAudFade->mfadeTimeUsed = 0;
                pAudFade->mfadeFramesUsed = 0;
                pAudFade->mfadeTimeTotal = DEFAULT_FADE_OUT_MS;
                pAudFade->muteCounts = 1;
                AudioFadeBuf(pAudFade, in, nSamples);
                pAudFade->mFadeState = AUD_FADE_OUT;
            }
            break;
            case AUD_FADE_OUT: {
                // do fade out process
                if (pAudFade->mCurrentVolume != 0) {
                    AudioFadeBuf(pAudFade, in, nSamples);
                } else {
                    pAudFade->mFadeState = AUD_FADE_MUTE;
                    // do actrually setting
                    modeValue = pContext->modeValue;
                    for (i = 0; i < data->band_num; i++) {
                        ALOGD("%s: Set band[%d] -> %d", __FUNCTION__, i + 1, data->usr_cfg[modeValue * data->band_num + i]);
                        GEQ_setBand_api(data->usr_cfg[modeValue * data->band_num + i], i + 1);
                    }
                    mutePCMBuf(pAudFade, in, nSamples);
                }
            }
            break;
            case AUD_FADE_MUTE: {
                if (pAudFade->muteCounts <= 0) {
                    pAudFade->mFadeState = AUD_FADE_IN;
                    // slowly increase audio volume
                    pAudFade->mTargetVolume = 1 << 16;
                    pAudFade->mStartVolume = 0;
                    pAudFade->mCurrentVolume = 0;
                    pAudFade->mfadeTimeUsed = 0;
                    pAudFade->mfadeFramesUsed = 0;
                    pAudFade->mfadeTimeTotal = DEFAULT_FADE_IN_MS;
                    mutePCMBuf(pAudFade, in, nSamples);
                } else {
                    mutePCMBuf(pAudFade, in, nSamples);
                    pAudFade->muteCounts--;
                }
            }
            break;
            case AUD_FADE_IN: {
                AudioFadeBuf(pAudFade, in, nSamples);
                if (pAudFade->mCurrentVolume == 1 << 16) {
                    pAudFade->mFadeState = AUD_FADE_IDLE;
                }
            }
            break;
            case AUD_FADE_IDLE:
                // do nothing
                break;
            default:
                break;
            }

#if 0
            if (getprop_bool("media.audiofade.dump1")) {
                FILE *dump_fp = NULL;
                dump_fp = fopen("/data/audio_hal/audio_in.pcm", "a+");
                if (dump_fp != NULL) {
                    fwrite(in, nSamples * 2 * 2, 1, dump_fp);
                    fclose(dump_fp);
                } else {
                    ALOGW("[Error] Can't write to /data/dump_in.pcm");
                }
            }
#endif

            GEQ_process_api(in, out, inBuffer->frameCount);

#if 0
            if (getprop_bool("media.audiofade.dump1")) {
                FILE *dump_fp = NULL;
                dump_fp = fopen("/data/audio_hal/audio_out.pcm", "a+");
                if (dump_fp != NULL) {
                    fwrite(out, nSamples * 2 * 2, 1, dump_fp);
                    fclose(dump_fp);
                } else {
                    ALOGW("[Error] Can't write to /data/dump_in.pcm");
                }
            }
#endif

        } else {
            // original processing
            GEQ_process_api(in, out, inBuffer->frameCount);
        }
    }
    return 0;
}

int GEQ_command(effect_handle_t self, uint32_t cmdCode, uint32_t cmdSize,
        void *pCmdData, uint32_t *replySize, void *pReplyData)
{
    GEQContext * pContext = (GEQContext *)self;
    effect_param_t *p;
    int voffset;

    if (pContext == NULL || pContext->state == GEQ_STATE_UNINITIALIZED) {
        return -EINVAL;
    }

    ALOGD("%s: cmd = %u", __FUNCTION__, cmdCode);
    switch (cmdCode) {
    case EFFECT_CMD_INIT:
        if (pReplyData == NULL || replySize == NULL || *replySize != sizeof(int)) {
            return -EINVAL;
        }
        *(int *) pReplyData = GEQ_init(pContext);
        break;
    case EFFECT_CMD_SET_CONFIG:
        if (pCmdData == NULL || cmdSize != sizeof(effect_config_t) || pReplyData == NULL || replySize == NULL || *replySize != sizeof(int))
            return -EINVAL;
        *(int *) pReplyData = GEQ_configure(pContext, (effect_config_t *) pCmdData);
        break;
    case EFFECT_CMD_RESET:
        GEQ_reset(pContext);
        break;
    case EFFECT_CMD_ENABLE:
        if (pReplyData == NULL || replySize == NULL || *replySize != sizeof(int))
            return -EINVAL;
        if (pContext->state != GEQ_STATE_INITIALIZED)
            return -ENOSYS;
        pContext->state = GEQ_STATE_ACTIVE;
        *(int *)pReplyData = 0;
        break;
    case EFFECT_CMD_DISABLE:
        if (pReplyData == NULL || replySize == NULL || *replySize != sizeof(int))
            return -EINVAL;
        if (pContext->state != GEQ_STATE_ACTIVE)
            return -ENOSYS;
        pContext->state = GEQ_STATE_INITIALIZED;
        *(int *)pReplyData = 0;
        break;
    case EFFECT_CMD_GET_PARAM:
        if (pCmdData == NULL ||
            cmdSize != (int)(sizeof(effect_param_t) + sizeof(uint32_t)) ||
            pReplyData == NULL || replySize == NULL ||
            (*replySize < (int)(sizeof(effect_param_t) + sizeof(uint32_t) + sizeof(uint32_t)) &&
            *replySize < (int)(sizeof(effect_param_t) + sizeof(uint32_t) + sizeof(GEQcfg_8bit_s))))
            return -EINVAL;
        p = (effect_param_t *)pCmdData;
        memcpy(pReplyData, pCmdData, sizeof(effect_param_t) + p->psize);
        p = (effect_param_t *)pReplyData;

        voffset = ((p->psize - 1) / sizeof(int32_t) + 1) * sizeof(int32_t);

        p->status = GEQ_getParameter(pContext, p->data, (size_t  *)&p->vsize, p->data + voffset);
        *replySize = sizeof(effect_param_t) + voffset + p->vsize;
        break;
    case EFFECT_CMD_SET_PARAM:
        if (pCmdData == NULL ||
            (cmdSize != (int)(sizeof(effect_param_t) + sizeof(uint32_t) + sizeof(uint32_t)) &&
            cmdSize != (int)(sizeof(effect_param_t) + sizeof(uint32_t) + sizeof(GEQcfg_8bit_s)))||
            pReplyData == NULL || replySize == NULL || *replySize != sizeof(int32_t))
            return -EINVAL;
        p = (effect_param_t *)pCmdData;
        if (p->psize != sizeof(uint32_t) && p->vsize != sizeof(GEQcfg_8bit_s)) {
            *(int32_t *)pReplyData = -EINVAL;
            break;
        }
        *(int *)pReplyData = GEQ_setParameter(pContext, (void *)p->data, p->data + p->psize);
        break;
    case EFFECT_CMD_OFFLOAD:
        *(int *)pReplyData = 0;
        break;
    case EFFECT_CMD_SET_DEVICE:
    case EFFECT_CMD_SET_VOLUME:
    case EFFECT_CMD_SET_AUDIO_MODE:
        break;
    default:
        ALOGE("%s: invalid command %d", __FUNCTION__, cmdCode);
        return -EINVAL;
    }

    return 0;
}

int GEQ_getDescriptor(effect_handle_t self, effect_descriptor_t *pDescriptor)
{
    GEQContext * pContext = (GEQContext *) self;

    if (pContext == NULL || pDescriptor == NULL) {
        ALOGE("%s: invalid param", __FUNCTION__);
        return -EINVAL;
    }

    *pDescriptor = GEQDescriptor;

    return 0;
}

//-------------------- Effect Library Interface Implementation------------------------

int GEQLib_Create(const effect_uuid_t *uuid, int32_t sessionId __unused, int32_t ioId __unused, effect_handle_t *pHandle)
{
    //int ret;

    if (pHandle == NULL || uuid == NULL) {
        return -EINVAL;
    }

    if (memcmp(uuid, &GEQDescriptor.uuid, sizeof(effect_uuid_t)) != 0) {
        return -EINVAL;
    }

    GEQContext *pContext = new GEQContext;
    if (!pContext) {
        ALOGE("%s: alloc GEQContext failed", __FUNCTION__);
        return -EINVAL;
    }
    memset(pContext, 0, sizeof(GEQContext));
    if (GEQ_load_ini_file(pContext) < 0) {
        ALOGE("%s: Load INI File faied, use default param", __FUNCTION__);
        pContext->gGEQdata.enable = 1;
    }

    pContext->itfe = &GEQInterface;
    pContext->state = GEQ_STATE_UNINITIALIZED;

    *pHandle = (effect_handle_t)pContext;

    pContext->state = GEQ_STATE_INITIALIZED;

    pContext->bUseFade = 1;
    AudioFadeInit((AudioFade_t *) & (pContext->gAudFade), fadeLinear, 10, 0);
    AudioFadeSetState((AudioFade_t *) & (pContext->gAudFade), AUD_FADE_IDLE);

    ALOGD("%s: %p", __FUNCTION__, pContext);

    return 0;
}

int GEQLib_Release(effect_handle_t handle)
{
    GEQContext * pContext = (GEQContext *)handle;

    if (pContext == NULL) {
        return -EINVAL;
    }

    GEQ_release(pContext);
    pContext->state = GEQ_STATE_UNINITIALIZED;
    delete pContext;

    return 0;
}

int GEQLib_GetDescriptor(const effect_uuid_t *uuid, effect_descriptor_t *pDescriptor)
{
    if (pDescriptor == NULL || uuid == NULL) {
        ALOGE("%s: called with NULL pointer", __FUNCTION__);
        return -EINVAL;
    }

    if (memcmp(uuid, &GEQDescriptor.uuid, sizeof(effect_uuid_t)) == 0) {
        *pDescriptor = GEQDescriptor;
        return 0;
    }

    return  -EINVAL;
}

// effect_handle_t interface implementation for GEQ effect
const struct effect_interface_s GEQInterface = {
        GEQ_process,
        GEQ_command,
        GEQ_getDescriptor,
        NULL,
};

audio_effect_library_t AUDIO_EFFECT_LIBRARY_INFO_SYM = {
    .tag = AUDIO_EFFECT_LIBRARY_TAG,
    .version = EFFECT_LIBRARY_API_VERSION,
    .name = "Geq",
    .implementor = "Amlogic",
    .create_effect = GEQLib_Create,
    .release_effect = GEQLib_Release,
    .get_descriptor = GEQLib_GetDescriptor,
};

}; // extern "C"

