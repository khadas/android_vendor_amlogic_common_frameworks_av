/*
 * Copyright (C) 2021 Amlogic Corporation.
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

#define LOG_TAG "MS12V2DAP_Effect"
//#define LOG_NDEBUG 0

#include <cutils/log.h>
#include <utils/Log.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <dlfcn.h>
#include <hardware/audio_effect.h>
#include <cutils/properties.h>
#include <unistd.h>
#include <utils/String8.h>
#include <utils/Errors.h>
#include "aml_android_hidl_utils.h"
#include "ms12v2_dap_wapper.h"
#include "IniParser.h"

using namespace android;

extern "C" {

#include "../Utility/LibAudioEffect.h"

#define MAX_BAND_VALUE 20
#define DAP_DOLBY_PROFILE_NUM_MAX 5
#define DAP_DOLBY_PROFILE_NUM_MIN 0

#define DAP_LEVELER_SETTING_MIN 0
#define DAP_LEVELER_SETTING_MAX 2
#define DAP_LEVELER_AMOUNT_DEFALUT 4
#define DAP_LEVELER_AMOUNT_MIN 0
#define DAP_LEVELER_AMOUNT_MAX 10
#define DAP_CONTENT_PROCESS_MIN 0
#define DAP_CONTENT_PROCESS_MAX 2


#define DAP_VIRTUALIZER_MODE_MIN 0
#define DAP_VIRTUALIZER_MODE_MAX 2
#define BUFFER_MAX_LENGTH 1024
#define DEFAULT_BOOST_VALUE 96
#define DAP_POST_GAIN_MAX_VALUE 480
#define DAP_POST_GAIN_MIN_VALUE -2080

extern const struct effect_interface_s DAPV2Interface;

// DAP effect TYPE: 34033483-c5e9-4ff6-8b6b-0002a5d5c51b
// DAP effect UUID: 86cafba6-3ff3-485d-b8df-0de96b34b272
const effect_descriptor_t DAPV2Descriptor = {
    {0x34033483, 0xc5e9, 0x4ff6, 0x8b6b, {0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b}}, // type
    {0x86cafba6, 0x3ff3, 0x485d, 0xb8df, {0x0d, 0xe9, 0x6b, 0x34, 0xb2, 0x72}}, // uuid
    EFFECT_CONTROL_API_VERSION,
    EFFECT_FLAG_TYPE_POST_PROC | EFFECT_FLAG_DEVICE_IND | EFFECT_FLAG_NO_PROCESS | EFFECT_FLAG_OFFLOAD_SUPPORTED,
    DAP_CUP_LOAD_ARM9E,
    DAP_MEM_USAGE,
    "MS12v2 DAP",
    "Dobly Labs",
};

enum DAPV2_state_e {
    DAPV2_STATE_UNINITIALIZED,
    DAPV2_STATE_INITIALIZED,
    DAPV2_STATE_ACTIVE,
};

typedef struct dolby_bass_enhancer {
  int enable;
  int boost;
  int cutoff;
  int width;
} dolby_bass_enhancer_t;

typedef struct dolby_surround_virtualizer {
  int mode;
  int boost;
} dolby_virtual_surround_t;

typedef struct dolby_dialog_enhance {
  int de_enable;
  int de_amount;
} dolby_dialog_enhance_t;

typedef struct dolby_vol_leveler {
  int vl_enable;
  int vl_amount;
} dolby_vol_leveler_t;

typedef struct dolby_geq_params {
  int geq_enable;
  int geq_nb_bands;
  int a_geq_band_center[MAX_BAND_VALUE];
  int a_geq_band_target[MAX_BAND_VALUE];
} dolby_geq_params_t;

typedef struct dolby_ieq_params {
  int ieq_enable;
  int ieq_amount;
  int ieq_nb_bands;
  int a_ieq_band_center[MAX_BAND_VALUE];
  int a_ieq_band_target[MAX_BAND_VALUE];
} dolby_ieq_params_t;

typedef struct dolby_surround_params {
   int misteering;
   int surround_decoder_enable;
   int postgain;
} dolby_surround_param_t;

typedef struct dolby_base_t {
    dolby_bass_enhancer_t dap_bass_enhancer;
    dolby_virtual_surround_t dap_virtual_surround;
    dolby_dialog_enhance_t dap_dialog_enhance;
    dolby_vol_leveler_t dap_vol_leveler;
    dolby_geq_params_t dap_geq_param;
    dolby_ieq_params_t dap_ieq_param;
    dolby_surround_param_t dap_surround_param;
} dolby_base;

static dolby_base default_dolby_base {
    {0,192,200,16},{0,96},{0,0},{0,4},{0,10,{32,64,125,250,500,1000,2000,4000,8000,16000},{0,0,0,0,0,0,0,0,0,0}},
        {0,10,10,{32,64,125,250,500,1000,2000,4000,8000,16000},{0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},{0,1,0},
};

typedef enum {
    DAP_DOLBY_PROFILE_MOVIE,
    DAP_DOLBY_PROFILE_MUSIC,
    DAP_DOLBY_PROFILE_DAILOGUE,
    DAP_DOLBY_PROFILE_NIGHT,
    DAP_DOLBY_PROFILE_UNKNOWN,
} dolby_dap_profile_params;

typedef enum {
    AUTO_VOLUME_OFF = 0,
    AUTO_VOLUME_LOW = 4,
    AUTO_VOLUME_MID = 9,
    AUTO_VOLUME_HIGH = 10,
} auto_volume_control;

typedef struct Ms12data_s {
    int dap_enabled;
    int de_enable; // dialogenchancer
    int de_amount;
    int VolumeLevelerMode;
    int VirtualizerSurroundBoost;
    int VolumeLevelerStrength;
    int VirtualizerMode;
    int bassenhancerenable;
    int bassenhancerboost;
    int bassenhancercutoff;
    int bassenhancerwidth;
    int geqenable;
    int geqnbbands;
    int geqbandcenter[MAX_BAND_VALUE];
    int geqbandtarget[MAX_BAND_VALUE];
    int ieqenable;
    int ieqamount;
    int ieqnbbands;
    int ieqbandcenter[MAX_BAND_VALUE];
    int ieqbandtarget[MAX_BAND_VALUE];
    int postgain;
    int misteering;
    int profile;
    int drcenable;
    int surrounddecoderenable;
    dolby_base dolby_base_profile[DAP_DOLBY_PROFILE_NUM_MAX];
    float bypass_gain;
    int report_dapleveler_mode;
    int content_process;
} Ms12data;

typedef enum {
    DAP_PARAM_PROFILE,
    DAP_PARAM_UP_MIXER, /*dap surroud decoder enable*/
    DAP_PARAM_CONTENT_PROCESS, /*dap volume level*/
    DAP_PARAM_SPEAKER_VIRTUALIZER,

    // tuning param
    DAP_PARAM_SURROUND_VIRTUALIZER,
    DAP_PARAM_SPK_VIRANGLE,
    DAP_PARAM_SPK_VIRSTRAT_FRE,
    DAP_PARAM_DIALOGUE,
    DAP_PARAM_BASS_ENHANCER,
    DAP_PARAM_GEQ,
    DAP_PARAM_IEQ,
    DAP_PARAM_POST_GAIN,
    DAP_PARAM_MI_STEERING,
    DAP_PARAM_SURROUND_DECODER_ENABLE,
    DAP_PARAM_DAP_DRC,
    DAP_PARAM_DAP_LEVELER,
} DAPV2params;

const char *DapV2EnableStr[] = {"Disable", "Enable"};

typedef struct DAPV2Context_s {
    const struct effect_interface_s  *itfe;
    effect_config_t                  config;
    DAPV2_state_e                    state;
    Ms12data                         gMs12data;
} DAPV2Context;

int DAPV2_get_model_name(char *model_name, int size)
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

int DAPV2_get_ini_file(char *ini_name, int size)
{
    int result = -1;
    char model_name[PROPERTY_VALUE_MAX] = {0};
    IniParser* pIniParser = NULL;
    const char *ini_value = NULL;
    const char *filename = MODEL_SUM_DEFAULT_PATH;

    DAPV2_get_model_name(model_name, sizeof(model_name));
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
int DAPV2_parse_profile(dolby_base *dolby_base_param, const char *buffer)
{
    char *Rch = (char *)buffer;
    /*volume leveler param*/
    Rch = strtok(Rch, ",");
    if (Rch == NULL)
        goto error;
    dolby_base_param->dap_vol_leveler.vl_enable = atoi(Rch);
    //ALOGD("%s: volume leveler enable = %d", __FUNCTION__, dolby_base_param->dap_vol_leveler.vl_enable);
    Rch = strtok(NULL, ",");
    if (Rch == NULL)
        goto error;
    dolby_base_param->dap_vol_leveler.vl_amount = atoi(Rch);
    //ALOGD("%s: volume leveler amount = %d", __FUNCTION__, dolby_base_param->dap_vol_leveler.vl_amount);

    /*virtual_surround param*/
    Rch = strtok(NULL, ",");
    if (Rch == NULL)
        goto error;
    dolby_base_param->dap_virtual_surround.mode = atoi(Rch);
    //ALOGD("%s: dolby_virtual_surround mode = %d", __FUNCTION__, dolby_base_param->dap_virtual_surround.mode);
    Rch = strtok(NULL, ",");
    if (Rch == NULL)
        goto error;
    dolby_base_param->dap_virtual_surround.boost = atoi(Rch);
    //ALOGD("%s: dolby_virtual_surround boost = %d", __FUNCTION__, dolby_base_param->dap_virtual_surround.boost);

    /* Dialogue Enhancer param*/
    Rch = strtok(NULL, ",");
    if (Rch == NULL)
        goto error;
    dolby_base_param->dap_dialog_enhance.de_enable = atoi(Rch);
    //ALOGD("%s: dolby_dialog_enhance enable = %d", __FUNCTION__, dolby_base_param->dap_dialog_enhance.de_enable);
    Rch = strtok(NULL, ",");
    if (Rch == NULL)
        goto error;
    dolby_base_param->dap_dialog_enhance.de_amount = atoi(Rch);
    //ALOGD("%s: dolby_dialog_enhance amount = %d", __FUNCTION__, dolby_base_param->dap_dialog_enhance.de_amount);

    /*Bass Enhancer param*/
    Rch = strtok(NULL, ",");
    if (Rch == NULL)
        goto error;
    dolby_base_param->dap_bass_enhancer.enable = atoi(Rch);
    //ALOGD("%s: dap_bass_enhancer.enable is %d",__FUNCTION__,dolby_base_param->dap_bass_enhancer.enable);
    Rch = strtok(NULL, ",");
    if (Rch == NULL)
        goto error;
    dolby_base_param->dap_bass_enhancer.boost = atoi(Rch);
    //ALOGD("%s: dap_bass_enhancer.boost is %d",__FUNCTION__,dolby_base_param->dap_bass_enhancer.boost);
    Rch = strtok(NULL, ",");
    if (Rch == NULL)
        goto error;
    dolby_base_param->dap_bass_enhancer.cutoff = atoi(Rch);
    //ALOGD("%s: dap_bass_enhancer.cutoff is %d",__FUNCTION__,dolby_base_param->dap_bass_enhancer.cutoff);
    Rch = strtok(NULL, ",");
    if (Rch == NULL)
        goto error;
    dolby_base_param->dap_bass_enhancer.width = atoi(Rch);
    //ALOGD("%s: dap_bass_enhancer.width is %d",__FUNCTION__,dolby_base_param->dap_bass_enhancer.width);

    /*Ieq param */
    Rch = strtok(NULL, ",");
    if (Rch == NULL)
        goto error;
    dolby_base_param->dap_ieq_param.ieq_enable = atoi(Rch);
    //ALOGD("%s:ieq_enable is %d",__FUNCTION__,dolby_base_param->dap_ieq_param.ieq_enable);
    Rch = strtok(NULL, ",");
    if (Rch == NULL)
        goto error;
    dolby_base_param->dap_ieq_param.ieq_amount = atoi(Rch);
    //ALOGD("%s:ieq_amount is %d",__FUNCTION__,dolby_base_param->dap_ieq_param.ieq_amount);
    Rch = strtok(NULL, ",");
    if (Rch == NULL)
        goto error;
    dolby_base_param->dap_ieq_param.ieq_nb_bands = atoi(Rch);
    //ALOGD("%s:ieq_nb_bands is %d",__FUNCTION__,dolby_base_param->dap_ieq_param.ieq_nb_bands);

    for (int i = 0; i < dolby_base_param->dap_ieq_param.ieq_nb_bands; i++) {
        Rch = strtok(NULL, ",");
        if (Rch == NULL)
            goto error;
        dolby_base_param->dap_ieq_param.a_ieq_band_center[i] = atoi(Rch);
    }
    for (int i = 0; i < dolby_base_param->dap_ieq_param.ieq_nb_bands; i++) {
        Rch = strtok(NULL, ",");
        if (Rch == NULL)
            goto error;
        dolby_base_param->dap_ieq_param.a_ieq_band_target[i] = atoi(Rch);
    }
    /*Geq Param */
    Rch = strtok(NULL, ",");
    if (Rch == NULL)
        goto error;
    dolby_base_param->dap_geq_param.geq_enable = atoi(Rch);
    //ALOGD("%s:geq_enable is %d",__FUNCTION__,dolby_base_param->dap_geq_param.geq_enable);
    Rch = strtok(NULL, ",");
    if (Rch == NULL)
        goto error;
    dolby_base_param->dap_geq_param.geq_nb_bands = atoi(Rch);
    //ALOGD("%s:geq_bands is %d",__FUNCTION__,dolby_base_param->dap_geq_param.geq_nb_bands);
    for (int i = 0; i < dolby_base_param->dap_geq_param.geq_nb_bands; i++) {
        Rch = strtok(NULL, ",");
        if (Rch == NULL)
            goto error;
        dolby_base_param->dap_geq_param.a_geq_band_center[i] = atoi(Rch);
    }
    for (int i = 0; i < dolby_base_param->dap_geq_param.geq_nb_bands; i++) {
        Rch = strtok(NULL, ",");
        if (Rch == NULL)
            goto error;
        dolby_base_param->dap_geq_param.a_geq_band_target[i] = atoi(Rch);
    }

    /*mi_steering param*/
    Rch = strtok(NULL, ",");
    if (Rch == NULL)
        goto error;
    dolby_base_param->dap_surround_param.misteering = atoi(Rch);
    //ALOGD("%s: misteering enable = %d", __FUNCTION__, dolby_base_param->dap_surround_param.misteering);
    Rch = strtok(NULL, ",");
    if (Rch == NULL)
        goto error;
    dolby_base_param->dap_surround_param.surround_decoder_enable = atoi(Rch);
    //ALOGD("%s: surround decoder enable = %d", __FUNCTION__, dolby_base_param->dap_surround_param.surround_decoder_enable);
    Rch = strtok(NULL, ",");
    if (Rch == NULL)
        goto error;
    dolby_base_param->dap_surround_param.postgain = atoi(Rch);
    //ALOGD("%s: get postgain = %d", __FUNCTION__, dolby_base_param->dap_surround_param.postgain);

    return 0;
error:
    ALOGE("%s: DAPV2 Parse failed", __FUNCTION__);
    return -1;
}

int DAPV2_parse_dapleveler(dolby_vol_leveler_t *p_param, const char *buffer)
{
    char *Rch = (char *)buffer;
    Rch = strtok(Rch, ",");
    if (Rch == NULL)
        goto error;
    p_param->vl_enable = atoi(Rch);
    Rch = strtok(NULL, ",");
    if (Rch == NULL)
        goto error;
    p_param->vl_amount = atoi(Rch);
    return 0;
error:
    ALOGE("%s: fap leveler mode parse failed", __FUNCTION__);
    return -1;
}


float db_to_ampl(float decibels)
{
    /*exp( dB * ln(10) / 20 )*/
    return exp( decibels * 0.115129f);
}

int DAPV2_load_ini_file(DAPV2Context *pContext)
{
    int result = -1;
    char ini_name[100] = {0};
    const char *ini_value = NULL;
    Ms12data *data = &pContext->gMs12data;
    IniParser* pIniParser = NULL;

    if (DAPV2_get_ini_file(ini_name, sizeof(ini_name)) < 0)
        goto error;

    pIniParser = new IniParser();
    if (pIniParser->parse((const char *)ini_name) < 0) {
        ALOGD("%s: %s load failed", __FUNCTION__, ini_name);
        goto error;
    }
    ini_value = pIniParser->GetString("DAPV2", "dap_enable", "1");
    if (ini_value == NULL) {
        goto error;
    }
    data->dap_enabled = atoi(ini_value);
    //ALOGD("%s: dap_enable -> %s", __FUNCTION__, ini_value);

    ini_value = pIniParser->GetString("DAPV2", "bypass_gain", "0.0");
    if (ini_value == NULL) {
        goto error;
    }
    data->bypass_gain = db_to_ampl(atof(ini_value));

    ini_value = pIniParser->GetString("DAPV2", "movie", "NULL");
    if (ini_value == NULL) {
        goto error;
    }
    if (DAPV2_parse_profile(&data->dolby_base_profile[0], ini_value) < 0)
        goto error;
    ini_value = pIniParser->GetString("DAPV2", "music", "NULL");
    if (ini_value == NULL) {
        goto error;
    }
    if (DAPV2_parse_profile(&data->dolby_base_profile[1], ini_value) < 0)
        goto error;

    ini_value = pIniParser->GetString("DAPV2", "dialogue", "NULL");
    if (ini_value == NULL) {
        goto error;
    }
    if (DAPV2_parse_profile(&data->dolby_base_profile[2], ini_value) < 0)
        goto error;

    ini_value = pIniParser->GetString("DAPV2", "night", "NULL");
    if (ini_value == NULL) {
        goto error;
    }
    if (DAPV2_parse_profile(&data->dolby_base_profile[3], ini_value) < 0)
        goto error;

    ini_value = pIniParser->GetString("DAPV2", "none", "NULL");
    if (ini_value == NULL) {
        goto error;
    }
    if (DAPV2_parse_profile(&data->dolby_base_profile[4], ini_value) < 0)
        goto error;

    result = 0;
error:
    ALOGD("%s: %s", __FUNCTION__, result == 0 ? "sucessful" : "failed");
    delete pIniParser;
    pIniParser = NULL;
    return result;
}

int DAPV2_init(DAPV2Context *pContext)
{
    Ms12data *data = &pContext->gMs12data;
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

    data->bassenhancerboost = data->dolby_base_profile[0].dap_bass_enhancer.boost;
    data->bassenhancercutoff = data->dolby_base_profile[0].dap_bass_enhancer.cutoff;
    data->bassenhancerenable = data->dolby_base_profile[0].dap_bass_enhancer.enable;
    data->bassenhancerwidth = data->dolby_base_profile[0].dap_bass_enhancer.width;
    data->de_amount = data->dolby_base_profile[0].dap_dialog_enhance.de_amount;
    data->de_enable = data->dolby_base_profile[0].dap_dialog_enhance.de_enable;
    data->VolumeLevelerMode = data->dolby_base_profile[0].dap_vol_leveler.vl_enable;
    data->VolumeLevelerStrength = data->dolby_base_profile[0].dap_vol_leveler.vl_amount;
    data->VirtualizerMode = data->dolby_base_profile[0].dap_virtual_surround.mode;
    data->VirtualizerSurroundBoost = data->dolby_base_profile[0].dap_virtual_surround.boost;
    data->misteering = data->dolby_base_profile[0].dap_surround_param.misteering;
    data->postgain = data->dolby_base_profile[0].dap_surround_param.postgain;
    data->surrounddecoderenable = data->dolby_base_profile[0].dap_surround_param.surround_decoder_enable;
    data->report_dapleveler_mode = 0;
    data->content_process = 0;

    if (!data->dap_enabled) {
        char parm[BUFFER_MAX_LENGTH] = "";
        sprintf(parm, "bypass_dap=%d %f", 1 ,data->bypass_gain);
        if (strlen(parm) > 0) {
            setParameters(String8(parm));
        }
    }
    ALOGD("%s: sucessful", __FUNCTION__);
    return 0;
}

int DAPV2_configure(DAPV2Context *pContext, effect_config_t *pConfig)
{
    if (pConfig->inputCfg.samplingRate != pConfig->outputCfg.samplingRate) {
        ALOGE("%s:inputCfg.samplingRate(%d) != outputCfg.samplingRate(%d)", __FUNCTION__, pConfig->inputCfg.samplingRate, pConfig->outputCfg.samplingRate);
        return -EINVAL;
    }
    if (pConfig->inputCfg.channels != pConfig->outputCfg.channels) {
        ALOGE("%s:inputCfg.channels(%d) != outputCfg.channels(%d)", __FUNCTION__, pConfig->inputCfg.channels, pConfig->outputCfg.channels);
        return -EINVAL;
    }
    if (pConfig->inputCfg.format != pConfig->outputCfg.format) {
        ALOGE("%s:inputCfg.format(%d) != outputCfg.format(%d)", __FUNCTION__, pConfig->inputCfg.format, pConfig->outputCfg.format);
        return -EINVAL;
    }

    if (pConfig->inputCfg.channels != AUDIO_CHANNEL_OUT_STEREO) {
        ALOGW("%s: channels in = 0x%x channels out = 0x%x", __FUNCTION__, pConfig->inputCfg.channels, pConfig->outputCfg.channels);
        pConfig->inputCfg.channels = pConfig->outputCfg.channels = AUDIO_CHANNEL_OUT_STEREO;
    }
    if (pConfig->outputCfg.accessMode != EFFECT_BUFFER_ACCESS_WRITE &&
        pConfig->outputCfg.accessMode != EFFECT_BUFFER_ACCESS_ACCUMULATE) {
        ALOGE("%s:outputCfg.accessMode != EFFECT_BUFFER_ACCESS_WRITE", __FUNCTION__);
        return -EINVAL;
    }

    if (pConfig->inputCfg.format != AUDIO_FORMAT_PCM_16_BIT) {
        ALOGW("%s: format in = 0x%x format out = 0x%x", __FUNCTION__, pConfig->inputCfg.format, pConfig->outputCfg.format);
        pConfig->inputCfg.format = pConfig->outputCfg.format = AUDIO_FORMAT_PCM_16_BIT;
    }

    memcpy(&pContext->config, pConfig, sizeof(effect_config_t));
    return 0;
}

int DAPV2_getParameter(DAPV2Context *pContext, void *pParam, size_t *pValueSize, void *pValue) {
    int32_t param = *(int32_t *)pParam;
    Ms12data *data = &pContext->gMs12data;
    int32_t value;
    switch (param) {
    case DAP_PARAM_SPEAKER_VIRTUALIZER:
        if (*pValueSize < sizeof(uint32_t)) {
            *pValueSize = 0;
            return -EINVAL;
        }
        value = (uint32_t)data->VirtualizerMode;
        *(uint32_t *) pValue = value;
        ALOGD("%s: get DAP virtualizer mode  is %d", __FUNCTION__, value);
        break;
    case DAP_PARAM_UP_MIXER:
        if (*pValueSize < sizeof(uint32_t)) {
            *pValueSize = 0;
            return -EINVAL;
        }
        value = (uint32_t)data->surrounddecoderenable;
        *(uint32_t *) pValue = value;
        ALOGD("%s: get DAP up mixer is %d", __FUNCTION__, value);
        break;
    case DAP_PARAM_PROFILE:
        if (*pValueSize < sizeof(uint32_t)) {
            *pValueSize = 0;
            return -EINVAL;
        }
        value = (uint32_t)data->profile;
        *(uint32_t *) pValue = value;
        ALOGD("%s: get DAP profile is %d", __FUNCTION__, value);
        break;
    case DAP_PARAM_CONTENT_PROCESS:
        if (*pValueSize < sizeof(uint32_t)) {
            *pValueSize = 0;
            return -EINVAL;
        }
        value = (uint32_t)data->content_process;
        *(uint32_t *) pValue = value;
        ALOGD("%s: get DAP content process is %d", __FUNCTION__, value);
        break;
    case DAP_PARAM_SURROUND_VIRTUALIZER:
        if (*pValueSize < sizeof(uint32_t)) {
            *pValueSize = 0;
            return -EINVAL;
        }
        value = (int32_t)data->VirtualizerMode;
        *(uint32_t *) pValue = value;
        ALOGD("%s: get DAP VirtualizerMode is %d", __FUNCTION__, value);
        break;
    case DAP_PARAM_POST_GAIN:
        if (*pValueSize < sizeof(uint32_t)) {
            *pValueSize = 0;
            return -EINVAL;
        }
        value = (int32_t)data->postgain;
        *(int32_t *) pValue = value;
        ALOGD("%s: get DAP postgain is %d", __FUNCTION__, value);
        break;
    case DAP_PARAM_MI_STEERING:
        if (*pValueSize < sizeof(uint32_t)) {
            *pValueSize = 0;
            return -EINVAL;
        }
        value = (int32_t)data->misteering;
        *(uint32_t *) pValue = value;
        ALOGD("%s: get DAP misteering is %d", __FUNCTION__, value);
        break;
    default:
        ALOGE("%s: unknown param %d", __FUNCTION__, param);
        return -EINVAL;
    }
    return 0;
}

int DAPV2_setParameter(DAPV2Context *pContext, void *pParam, void *pValue) {
    uint32_t param = *(uint32_t *)pParam;
    Ms12data *data = &pContext->gMs12data;
    int32_t value, i;
    int32_t *ptmp;
    String8 tmpParam("");
    char tempbuf[BUFFER_MAX_LENGTH] = {0};
    char temp[BUFFER_MAX_LENGTH] = {0};
    switch (param) {
    case DAP_PARAM_PROFILE:
        value = *(int32_t *)pValue;
        if (value < DAP_DOLBY_PROFILE_NUM_MIN || value > (DAP_DOLBY_PROFILE_NUM_MAX - 1)) {
            ALOGE("%s: incorrect profile value %d", __FUNCTION__, value);
            return -EINVAL;
        }
        data->profile = value;
        sprintf(tempbuf,"ms12_runtime=-dap_mi_steering %d -dap_bass_enhancer %d,%d,%d,%d -dap_dialogue_enhancer %d,%d"
            " -dap_gains %d -dap_ieq %d,%d,%d",
            data->dolby_base_profile[value].dap_surround_param.misteering, data->dolby_base_profile[value].dap_bass_enhancer.enable,
            data->dolby_base_profile[value].dap_bass_enhancer.boost, data->dolby_base_profile[value].dap_bass_enhancer.cutoff,
            data->dolby_base_profile[value].dap_bass_enhancer.width, data->dolby_base_profile[value].dap_dialog_enhance.de_enable,
            data->dolby_base_profile[value].dap_dialog_enhance.de_amount,
            data->dolby_base_profile[value].dap_surround_param.postgain, data->dolby_base_profile[value].dap_ieq_param.ieq_enable,
            data->dolby_base_profile[value].dap_ieq_param.ieq_amount, data->dolby_base_profile[value].dap_ieq_param.ieq_nb_bands);
        tmpParam += String8::format("%s", tempbuf);
        for (i = 0; i < data->dolby_base_profile[value].dap_ieq_param.ieq_nb_bands; i++) {
            sprintf(tempbuf, ",%d", data->dolby_base_profile[value].dap_ieq_param.a_ieq_band_center[i]);
            tmpParam += String8::format("%s", tempbuf);
        }
        for (i = 0; i < data->dolby_base_profile[value].dap_ieq_param.ieq_nb_bands; i++) {
            sprintf(tempbuf, ",%d",data->dolby_base_profile[value].dap_ieq_param.a_ieq_band_target[i]);
            tmpParam += String8::format("%s", tempbuf);
        }
        memcpy(tempbuf, tmpParam.string(), strlen(tmpParam.string()) > BUFFER_MAX_LENGTH ? BUFFER_MAX_LENGTH : strlen(tmpParam.string()));
        tmpParam.clear();
        sprintf(temp, " -dap_graphic_eq %d,%d",data->dolby_base_profile[value].dap_geq_param.geq_enable,
            data->dolby_base_profile[value].dap_geq_param.geq_nb_bands);
        strcat(tempbuf,temp);
        tmpParam += String8::format("%s",tempbuf);
        for (i = 0; i < data->dolby_base_profile[value].dap_geq_param.geq_nb_bands; i++) {
            sprintf(tempbuf,",%d", data->dolby_base_profile[value].dap_geq_param.a_geq_band_center[i]);
            tmpParam += String8::format("%s",tempbuf);
        }
        for (i = 0; i < data->dolby_base_profile[value].dap_geq_param.geq_nb_bands; i++) {
            sprintf(tempbuf,",%d", data->dolby_base_profile[value].dap_geq_param.a_geq_band_target[i]);
            tmpParam += String8::format("%s",tempbuf);
        }
        memcpy(tempbuf, tmpParam.string(), strlen(tmpParam.string()) > BUFFER_MAX_LENGTH ? BUFFER_MAX_LENGTH : strlen(tmpParam.string()));
        setParameters(String8(tempbuf));
        ALOGD("set profile is %d",value);
        break;
    case DAP_PARAM_UP_MIXER:
        value = *(int32_t *)pValue;
        if (value < 0) {
            value = 0;
        } else if (value > 1)
            value = 1;
        data->surrounddecoderenable = value;
        sprintf(tempbuf, "ms12_runtime=-dap_surround_decoder_enable %d", data->surrounddecoderenable);
        setParameters(String8(tempbuf));
        ALOGD("set up mixer is %d ", data->surrounddecoderenable);
        break;
    case DAP_PARAM_CONTENT_PROCESS:
        value = *(int32_t *)pValue;
        if (value <  DAP_CONTENT_PROCESS_MIN || value > DAP_CONTENT_PROCESS_MAX) {
            ALOGE("%s: incorrect content process value %d", __FUNCTION__, value);
            return -EINVAL;
        }
        data->content_process = value;
        if (value == DAP_CONTENT_PROCESS_MIN)
            data->VolumeLevelerStrength = DAP_LEVELER_AMOUNT_MIN;
        else if (value == (DAP_CONTENT_PROCESS_MIN + 1))
            data->VolumeLevelerStrength = 3;
        else
            data->VolumeLevelerStrength = DAP_LEVELER_AMOUNT_MAX;
        sprintf(tempbuf, "ms12_runtime=-dap_leveler %d,%d", DAP_LEVELER_SETTING_MAX - 1, data->VolumeLevelerStrength); /* add for customer, dap mode is 1*/
        setParameters(String8(tempbuf));
        ALOGD("set content process value %d", value);
        break;
    case DAP_PARAM_SPEAKER_VIRTUALIZER:
        value = *(int32_t *)pValue;
        if (value <  DAP_VIRTUALIZER_MODE_MIN || value > DAP_VIRTUALIZER_MODE_MAX) {
            ALOGE("%s: incorrect speaker virtualizer value %d", __FUNCTION__, value);
            return -EINVAL;
        }
        data->VirtualizerMode = value;
        sprintf(tempbuf,"ms12_runtime=-dap_surround_virtualizer %d,%d",data->VirtualizerMode, DEFAULT_BOOST_VALUE);
        setParameters(String8(tempbuf));
        ALOGD("set spk virtualizer is %d",value);
        break;
    case DAP_PARAM_SURROUND_VIRTUALIZER:
        ptmp = (int32_t *)pValue;
        data->VirtualizerMode = ptmp[0];
        data->VirtualizerSurroundBoost = ptmp[1];
        sprintf(tempbuf,"ms12_runtime=-dap_surround_virtualizer %d,%d",data->VirtualizerMode,data->VirtualizerSurroundBoost);
        setParameters(String8(tempbuf));
        break;
    case DAP_PARAM_DIALOGUE:
        ptmp = (int32_t *)pValue;
        data->de_enable = ptmp[0];
        data->de_amount = ptmp[1];
        sprintf(tempbuf,"ms12_runtime=-dap_dialogue_enhancer %d,%d",data->de_enable, data->de_amount);
        setParameters(String8(tempbuf));
        ALOGD("set dialog enhance enable is %d and dialgue is %d",data->de_enable, data->de_amount);
        break;
    case DAP_PARAM_BASS_ENHANCER:
        ptmp = (int32_t *)pValue;
        data->bassenhancerenable = *ptmp++;
        data->bassenhancerboost = *ptmp++;
        data->bassenhancercutoff = *ptmp++;
        data->bassenhancerwidth = *ptmp;
        sprintf(tempbuf,"ms12_runtime=-dap_bass_enhancer %d,%d,%d,%d",data->bassenhancerenable,
                data->bassenhancerboost, data->bassenhancercutoff, data->bassenhancerwidth);
        setParameters(String8(tempbuf));
        break;
    case DAP_PARAM_GEQ:
        ptmp = (int32_t *)pValue;
        data->geqenable = *ptmp++;
        data->geqnbbands = *ptmp++;
        sprintf(tempbuf, "ms12_runtime=-dap_graphic_eq %d,%d",data->geqenable, data->geqnbbands);
        tmpParam += String8::format("%s",tempbuf);
        for (i = 0; i < data->geqnbbands; i++) {
            sprintf(tempbuf,",%d", *ptmp++);
            tmpParam += String8::format("%s",tempbuf);
        }
        for (i = 0; i < data->geqnbbands; i++) {
            sprintf(tempbuf,",%d", *ptmp++);
            tmpParam += String8::format("%s",tempbuf);
        }
        if (strlen(tmpParam.string()) > sizeof(tempbuf)) {
            ALOGE("set DAP_PARAM_GEQ param size exceeds %d", sizeof(tempbuf));
            return -EINVAL;
        }
        memcpy(tempbuf, tmpParam.string(), strlen(tmpParam.string()));
        setParameters(String8(tempbuf));
        break;
    case DAP_PARAM_IEQ:
        ptmp = (int32_t *)pValue;
        data->ieqenable = *ptmp++;
        data->ieqamount = *ptmp++;
        data->ieqnbbands = *ptmp++;
        sprintf(tempbuf, "ms12_runtime=-dap_ieq %d,%d,%d",data->ieqenable, data->ieqamount,data->ieqnbbands);
        tmpParam += String8::format("%s", tempbuf);
        for (i = 0; i < data->ieqnbbands; i++) {
            sprintf(tempbuf, ",%d", *ptmp++);
            tmpParam += String8::format("%s", tempbuf);
        }
        for (i = 0; i < data->ieqnbbands; i++) {
            sprintf(tempbuf, ",%d", *ptmp++);
            tmpParam += String8::format("%s", tempbuf);
        }
        if (strlen(tmpParam.string()) > sizeof(tempbuf)) {
            ALOGE("set DAP_PARAM_IEQ param size exceeds %d", sizeof(tempbuf));
            return -EINVAL;
        }
        memcpy(tempbuf, tmpParam.string(), strlen(tmpParam.string()));
        setParameters(String8(tempbuf));
        break;
    case DAP_PARAM_POST_GAIN:
        value = *(int32_t *)pValue;
        data->postgain = value;
        if ((data->postgain  >= DAP_POST_GAIN_MIN_VALUE) && (data->postgain  <= DAP_POST_GAIN_MAX_VALUE)) {
            sprintf(tempbuf,"ms12_runtime=-dap_gains %d",data->postgain);
            setParameters(String8(tempbuf));
        }
        ALOGD("set post gain is %d",value);
        break;
    case DAP_PARAM_MI_STEERING:
        value = *(int32_t *)pValue;
        data->misteering = value;
        sprintf(tempbuf, "ms12_runtime=-dap_mi_steering %d",data->misteering);
        setParameters(String8(tempbuf));
        ALOGD("set mi steering is %d",value);
        break;
    case DAP_PARAM_SURROUND_DECODER_ENABLE:
        value = *(int32_t *)pValue;
        data->surrounddecoderenable = value;
        sprintf(tempbuf, "ms12_runtime=-dap_surround_decoder_enable %d",data->surrounddecoderenable);
        setParameters(String8(tempbuf));
        ALOGD("set surround decoder enable is %d",data->surrounddecoderenable);
        break;
    case DAP_PARAM_DAP_DRC:
        value = *(int32_t *)pValue;
        data->drcenable = value;
        sprintf(tempbuf, "ms12_runtime=-dap_drc %d",data->drcenable);
        setParameters(String8(tempbuf));
        ALOGD("set dap drc is %d",data->drcenable);
        break;
    case DAP_PARAM_DAP_LEVELER:
        ptmp = (int32_t *)pValue;
        data->VolumeLevelerMode = ptmp[0];
        data->VolumeLevelerStrength = ptmp[1];
        sprintf(tempbuf, "ms12_runtime=-dap_leveler %d,%d",data->VolumeLevelerMode,data->VolumeLevelerStrength);
        setParameters(String8(tempbuf));
        break;
    default:
        ALOGE("%s: unknown param %08x", __FUNCTION__, param);
        return -EINVAL;
    }

    return 0;
}

int DAPV2_release(DAPV2Context *pContext __unused)
{
    return 0;
}

int DAPV2_process(effect_handle_t self, audio_buffer_t *inBuffer, audio_buffer_t *outBuffer)
{
    DAPV2Context *pContext = (DAPV2Context *)self;
    if (pContext == NULL) {
        ALOGE("%s, pContext == NULL", __FUNCTION__);
        return -EINVAL;
    }
    if (inBuffer == NULL || inBuffer->raw == NULL ||
        outBuffer == NULL || outBuffer->raw == NULL ||
        inBuffer->frameCount != outBuffer->frameCount ||
        inBuffer->frameCount == 0) {

        if (inBuffer == NULL) {
            ALOGE("%s, inBuffer == NULL", __FUNCTION__);
            return -EINVAL;
        }
        if (inBuffer->raw == NULL) {
            ALOGE("%s, inBuffer->raw == NULL", __FUNCTION__);
        }
        if (outBuffer == NULL) {
            ALOGE("%s, outBuffer == NULL", __FUNCTION__);
            return -EINVAL;
        }
        if (outBuffer->raw == NULL) {
            ALOGE("%s, outBuffer->raw == NULL", __FUNCTION__);
        }
        if (inBuffer->frameCount != outBuffer->frameCount) {
            ALOGE("%s, inBuffer->frameCount = %d,outBuffer->frameCount = %d", __FUNCTION__, inBuffer->frameCount, outBuffer->frameCount);
        }
        if (inBuffer->frameCount == 0) {
            ALOGE("%s, inBuffer->frameCount == 0", __FUNCTION__);
        }
        ALOGE("%s, invalid buffer config", __FUNCTION__);
        return -EINVAL;
    }
    return 0;
}

int DAPV2_command(effect_handle_t self, uint32_t cmdCode, uint32_t cmdSize,
                void *pCmdData, uint32_t *replySize, void *pReplyData) {
    DAPV2Context * pContext = (DAPV2Context *)self;
    effect_param_t *p;
    int voffset;

    if (pContext == NULL || pContext->state == DAPV2_STATE_UNINITIALIZED) {
        return -EINVAL;
    }

    ALOGD("%s: cmd = %u", __FUNCTION__, cmdCode);
    switch (cmdCode) {
    case EFFECT_CMD_INIT:
        if (pReplyData == NULL || replySize == NULL || *replySize != sizeof(int)) {
            ALOGE("%s: ERR:EFFECT_CMD_INIT", __FUNCTION__);
            return -EINVAL;
        }
        *(int *) pReplyData = DAPV2_init(pContext);
        break;
    case EFFECT_CMD_SET_CONFIG:
        if (pCmdData == NULL || cmdSize != sizeof(effect_config_t) || pReplyData == NULL || replySize == NULL || *replySize != sizeof(int)) {
            ALOGE("%s: ERR:EFFECT_CMD_SET_CONFIG", __FUNCTION__);
            return -EINVAL;
        }
        *(int *) pReplyData = DAPV2_configure(pContext, (effect_config_t *) pCmdData);
        break;
    case EFFECT_CMD_RESET:
        //DAPV2_reset(pContext);
        break;
    case EFFECT_CMD_ENABLE:
        if (pReplyData == NULL || replySize == NULL || *replySize != sizeof(int)) {
            return -EINVAL;
        }
        if (pContext->state != DAPV2_STATE_INITIALIZED) {
            return -ENOSYS;
        }
        pContext->state = DAPV2_STATE_ACTIVE;
        *(int *)pReplyData = 0;
        break;
    case EFFECT_CMD_DISABLE:
        if (pReplyData == NULL || replySize == NULL || *replySize != sizeof(int)) {
            return -EINVAL;
        }
        if (pContext->state != DAPV2_STATE_ACTIVE) {
            return -ENOSYS;
        }
        pContext->state = DAPV2_STATE_INITIALIZED;;
        *(int *)pReplyData = 0;
        break;
    case EFFECT_CMD_GET_PARAM:
        if (pCmdData == NULL ||
            cmdSize != (int)(sizeof(effect_param_t) + sizeof(uint32_t)) ||
            pReplyData == NULL || replySize == NULL ||
            *replySize < (int)(sizeof(effect_param_t) + sizeof(uint32_t) + sizeof(uint32_t))) {
            return -EINVAL;
        }

        p = (effect_param_t *)pCmdData;
        if (EFFECT_PARAM_SIZE_MAX - sizeof(effect_param_t) < (size_t)p->psize) {
            ALOGE("EFFECT_CMD_GET_PARAM: psize too big");
            return -EINVAL;
        }
        memcpy(pReplyData, pCmdData, sizeof(effect_param_t) + p->psize);
        p = (effect_param_t *)pReplyData;
        voffset = ((p->psize - 1) / sizeof(int32_t) + 1) * sizeof(int32_t);
        p->status = DAPV2_getParameter(pContext, p->data, (size_t *)&p->vsize, p->data + voffset);
        *replySize = sizeof(effect_param_t) + voffset + p->vsize;
        break;
    case EFFECT_CMD_SET_PARAM:
        if (pCmdData == NULL ||
            pReplyData == NULL || replySize == NULL || *replySize != sizeof(int32_t)) {
            ALOGE("%s: EFFECT_CMD_SET_PARAM cmd size error!", __FUNCTION__);
            return -EINVAL;
        }
        p = (effect_param_t *)pCmdData;
        if (p->psize != sizeof(uint32_t)) {
            ALOGE("%s: EFFECT_CMD_SET_PARAM value size error!", __FUNCTION__);
            *(int32_t *)pReplyData = -EINVAL;
            break;
        }
        *(int *)pReplyData = DAPV2_setParameter(pContext, (void *)p->data, p->data + p->psize);
        break;
    case EFFECT_CMD_OFFLOAD:
        ALOGI("%s: EFFECT_CMD_OFFLOAD do nothing", __FUNCTION__);
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

int DAPV2_getDescriptor(effect_handle_t self, effect_descriptor_t *pDescriptor)
{
    DAPV2Context * pContext = (DAPV2Context *) self;

    if (pContext == NULL || pDescriptor == NULL) {
        ALOGE("%s: invalid param", __FUNCTION__);
        return -EINVAL;
    }

    *pDescriptor = DAPV2Descriptor;

    return 0;
}

    //-------------------- Effect Library Interface Implementation------------------------

int DAPV2Lib_Create(const effect_uuid_t *uuid, int32_t sessionId __unused, int32_t ioId __unused, effect_handle_t *pHandle)
{
    if (pHandle == NULL || uuid == NULL) {
        return -EINVAL;
    }

    if (memcmp(uuid, &DAPV2Descriptor.uuid, sizeof(effect_uuid_t)) != 0) {
        return -EINVAL;
    }

    DAPV2Context *pContext = new DAPV2Context;
    if (!pContext) {
        ALOGE("%s: alloc DAPContext failed", __FUNCTION__);
        return -EINVAL;
    }
    memset(pContext, 0, sizeof(DAPV2Context));
    memcpy((void *) & pContext->gMs12data.dolby_base_profile[0], (void *) &default_dolby_base, sizeof(pContext->gMs12data.dolby_base_profile[0]));

    if (DAPV2_load_ini_file(pContext) < 0) {
        ALOGE("%s: Load INI File faied, use default param", __FUNCTION__);
        pContext->gMs12data.dap_enabled = 1;
        pContext->gMs12data.bypass_gain = 1.0;
    }

    pContext->itfe = &DAPV2Interface;
    pContext->state = DAPV2_STATE_UNINITIALIZED;

    *pHandle = (effect_handle_t)pContext;

    pContext->state = DAPV2_STATE_INITIALIZED;

    ALOGD("%s: %p", __FUNCTION__, pContext);

    return 0;
}

int DAPV2Lib_Release(effect_handle_t handle)
{
    DAPV2Context * pContext = (DAPV2Context *)handle;

    if (pContext == NULL) {
        return -EINVAL;
    }
    DAPV2_release(pContext);
    pContext->state = DAPV2_STATE_UNINITIALIZED;

    delete pContext;

    return 0;
}

int DAPV2Lib_GetDescriptor(const effect_uuid_t *uuid, effect_descriptor_t *pDescriptor)
{
    if (pDescriptor == NULL || uuid == NULL) {
        ALOGE("%s: called with NULL pointer", __FUNCTION__);
        return -EINVAL;
    }
    if (memcmp(uuid, &DAPV2Descriptor.uuid, sizeof(effect_uuid_t)) == 0) {
        *pDescriptor = DAPV2Descriptor;
        return 0;
    }
    return  -EINVAL;
}

// effect_handle_t interface implementation for DAP effect
const struct effect_interface_s DAPV2Interface = {
    DAPV2_process,
    DAPV2_command,
    DAPV2_getDescriptor,
    NULL,
};

audio_effect_library_t AUDIO_EFFECT_LIBRARY_INFO_SYM = {
    .tag = AUDIO_EFFECT_LIBRARY_TAG,
    .version = EFFECT_LIBRARY_API_VERSION,
    .name = "MS12V2 DAP",
    .implementor = "Dobly Labs",
    .create_effect = DAPV2Lib_Create,
    .release_effect = DAPV2Lib_Release,
    .get_descriptor = DAPV2Lib_GetDescriptor,
};

}; // extern "C"
