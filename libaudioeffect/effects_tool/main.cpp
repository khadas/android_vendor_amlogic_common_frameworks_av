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

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/poll.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
//#include <syslog.h>
#include <utils/Log.h>
#include <errno.h>
#include <media/AudioEffect.h>
#include <media/AudioSystem.h>
#include <media/AudioParameter.h>

#ifdef LOG
#undef LOG
#endif
#define LOG(x...) printf("[AudioEffect] " x)
#define LSR (1)

using namespace android;

//Warning:balance is used for 51 bands

#define BALANCE_MAX_BANDS 51
#define BALANCE_MAX_LEVEL 25

//-----------Balance parameters-------------------------------
typedef struct Balance_param_s {
    effect_param_t param;
    uint32_t command;
    union {
        int32_t v;
        float f;
        float index[BALANCE_MAX_BANDS];
    };
} Balance_param_t;

typedef enum {
    BALANCE_PARAM_LEVEL = 0,
    BALANCE_PARAM_ENABLE,
    BALANCE_PARAM_LEVEL_NUM,
    BALANCE_PARAM_INDEX,
    BALANCE_PARAM_USB,
    BALANCE_PARAM_BT,
} Balance_params;

Balance_param_t gBalanceParam[] = {
    {{0, 4, 4}, BALANCE_PARAM_LEVEL, {BALANCE_MAX_LEVEL}},
    {{0, 4, 4}, BALANCE_PARAM_ENABLE, {1}},
    {{0, 4, 4}, BALANCE_PARAM_LEVEL_NUM, {BALANCE_MAX_BANDS}},
    {{0, 4, BALANCE_MAX_BANDS * 4}, BALANCE_PARAM_INDEX, {0}},
    {{0, 4, 4}, BALANCE_PARAM_USB, {0}},
    {{0, 4, 4}, BALANCE_PARAM_BT, {0}},
};

struct balance_gain {
   float right_gain;
   float left_gain;
};


int balance_level_num = 0;
float index1[BALANCE_MAX_BANDS] = {0};

const char *BalanceStatusstr[] = {"Disable", "Enable"};
//-----------TruSurround parameters---------------------------
typedef struct SRS_param_s {
    effect_param_t param;
    uint32_t command;
    union {
        uint32_t v;
        float f;
    };
} SRS_param_t;

typedef enum {
    SRS_PARAM_MODE = 0,
    SRS_PARAM_DIALOGCLARTY_MODE,
    SRS_PARAM_SURROUND_MODE,
    SRS_PARAM_VOLUME_MODE,
    SRS_PARAM_ENABLE,
    SRS_PARAM_TRUEBASS_ENABLE,
    SRS_PARAM_TRUEBASS_SPKER_SIZE,
    SRS_PARAM_TRUEBASS_GAIN,
    SRS_PARAM_DIALOG_CLARITY_ENABLE,
    SRS_PARAM_DIALOGCLARTY_GAIN,
    SRS_PARAM_DEFINITION_ENABLE,
    SRS_PARAM_DEFINITION_GAIN,
    SRS_PARAM_SURROUND_ENABLE,
    SRS_PARAM_SURROUND_GAIN,
    SRS_PARAM_INPUT_GAIN,
    SRS_PARAM_OUTPUT_GAIN,
    SRS_PARAM_OUTPUT_GAIN_COMP,
} SRS_params;

SRS_param_t gSRSParam[] = {
    {{0, 4, 4}, SRS_PARAM_MODE, {0}},
    {{0, 4, 4}, SRS_PARAM_DIALOGCLARTY_MODE, {0}},
    {{0, 4, 4}, SRS_PARAM_SURROUND_MODE, {0}},
    {{0, 4, 4}, SRS_PARAM_VOLUME_MODE, {0}},
    {{0, 4, 4}, SRS_PARAM_ENABLE, {1}},
    {{0, 4, 4}, SRS_PARAM_TRUEBASS_ENABLE, {0}},
    {{0, 4, 4}, SRS_PARAM_TRUEBASS_SPKER_SIZE, {0}},
    {{0, 4, 4}, SRS_PARAM_TRUEBASS_GAIN, {0}},
    {{0, 4, 4}, SRS_PARAM_DIALOG_CLARITY_ENABLE, {0}},
    {{0, 4, 4}, SRS_PARAM_DIALOGCLARTY_GAIN, {0}},
    {{0, 4, 4}, SRS_PARAM_DEFINITION_ENABLE, {0}},
    {{0, 4, 4}, SRS_PARAM_DEFINITION_GAIN, {0}},
    {{0, 4, 4}, SRS_PARAM_SURROUND_ENABLE, {0}},
    {{0, 4, 4}, SRS_PARAM_SURROUND_GAIN, {0}},
    {{0, 4, 4}, SRS_PARAM_INPUT_GAIN, {0}},
    {{0, 4, 4}, SRS_PARAM_OUTPUT_GAIN, {0}},
    {{0, 4, 4}, SRS_PARAM_OUTPUT_GAIN_COMP, {0}},
};

const char *SRSModestr[] = {"Standard", "Music", "Movie", "Clear Voice", "Enhanced Bass", "Custom"};

const char *SRSStatusstr[] = {"Disable", "Enable"};
const char *SRSTrueBassstr[] = {"Disable", "Enable"};
const char *SRSDialogClaritystr[] = {"Disable", "Enable"};
const char *SRSDefinitionstr[] = {"Disable", "Enable"};
const char *SRSSurroundstr[] = {"Disable", "Enable"};

const char *SRSDialogClarityModestr[] = {"OFF", "LOW", "HIGH"};
const char *SRSSurroundModestr[] = {"ON", "OFF"};

//-------------Virtualx parameter--------------------------
typedef struct Virtualx_param_s {
    effect_param_t param;
    uint32_t command;
    union {
        int32_t v;
        float f;
    };
} Virtualx_param_t;
typedef enum {
    VIRTUALX_PARAM_ENABLE,
    VIRTUALX_PARAM_DIALOGCLARTY_MODE,
    VIRTUALX_PARAM_SURROUND_MODE,
    DTS_PARAM_MBHL_ENABLE_I32,
    DTS_PARAM_MBHL_BYPASS_GAIN_I32,
    DTS_PARAM_MBHL_REFERENCE_LEVEL_I32,
    DTS_PARAM_MBHL_VOLUME_I32,
    DTS_PARAM_MBHL_VOLUME_STEP_I32,
    DTS_PARAM_MBHL_BALANCE_STEP_I32,
    DTS_PARAM_MBHL_OUTPUT_GAIN_I32,
    DTS_PARAM_MBHL_MODE_I32,
    DTS_PARAM_MBHL_PROCESS_DISCARD_I32,
    DTS_PARAM_MBHL_CROSS_LOW_I32,
    DTS_PARAM_MBHL_CROSS_MID_I32,
    DTS_PARAM_MBHL_COMP_ATTACK_I32,
    DTS_PARAM_MBHL_COMP_LOW_RELEASE_I32,
    DTS_PARAM_MBHL_COMP_LOW_RATIO_I32,
    DTS_PARAM_MBHL_COMP_LOW_THRESH_I32,
    DTS_PARAM_MBHL_COMP_LOW_MAKEUP_I32,
    DTS_PARAM_MBHL_COMP_MID_RELEASE_I32,
    DTS_PARAM_MBHL_COMP_MID_RATIO_I32,
    DTS_PARAM_MBHL_COMP_MID_THRESH_I32,
    DTS_PARAM_MBHL_COMP_MID_MAKEUP_I32,
    DTS_PARAM_MBHL_COMP_HIGH_RELEASE_I32,
    DTS_PARAM_MBHL_COMP_HIGH_RATIO_I32,
    DTS_PARAM_MBHL_COMP_HIGH_THRESH_I32,
    DTS_PARAM_MBHL_COMP_HIGH_MAKEUP_I32,
    DTS_PARAM_MBHL_BOOST_I32,
    DTS_PARAM_MBHL_THRESHOLD_I32,
    DTS_PARAM_MBHL_SLOW_OFFSET_I32,
    DTS_PARAM_MBHL_FAST_ATTACK_I32,
    DTS_PARAM_MBHL_FAST_RELEASE_I32,
    DTS_PARAM_MBHL_SLOW_ATTACK_I32,
    DTS_PARAM_MBHL_SLOW_RELEASE_I32,
    DTS_PARAM_MBHL_DELAY_I32,
    DTS_PARAM_MBHL_ENVELOPE_FREQUENCY_I32,
    DTS_PARAM_TBHDX_ENABLE_I32,
    DTS_PARAM_TBHDX_MONO_MODE_I32,
    DTS_PARAM_TBHDX_MAXGAIN_I32,
    DTS_PARAM_TBHDX_SPKSIZE_I32,
    DTS_PARAM_TBHDX_HP_ENABLE_I32,
    DTS_PARAM_TBHDX_TEMP_GAIN_I32,
    DTS_PARAM_TBHDX_PROCESS_DISCARD_I32,
    DTS_PARAM_TBHDX_HPORDER_I32,
    DTS_PARAM_VX_ENABLE_I32,
    DTS_PARAM_VX_INPUT_MODE_I32,
    DTS_PARAM_VX_OUTPUT_MODE_I32,
    DTS_PARAM_VX_HEADROOM_GAIN_I32,
    DTS_PARAM_VX_PROC_OUTPUT_GAIN_I32,
    DTS_PARAM_VX_REFERENCE_LEVEL_I32,
    DTS_PARAM_TSX_ENABLE_I32,
    DTS_PARAM_TSX_PASSIVEMATRIXUPMIX_ENABLE_I32,
    DTS_PARAM_TSX_HEIGHT_UPMIX_ENABLE_I32,
    DTS_PARAM_TSX_LPR_GAIN_I32,
    DTS_PARAM_TSX_CENTER_GAIN_I32,
    DTS_PARAM_TSX_HORIZ_VIR_EFF_CTRL_I32,
    DTS_PARAM_TSX_HEIGHTMIX_COEFF_I32,
    DTS_PARAM_TSX_PROCESS_DISCARD_I32,
    DTS_PARAM_TSX_HEIGHT_DISCARD_I32,
    DTS_PARAM_TSX_FRNT_CTRL_I32,
    DTS_PARAM_TSX_SRND_CTRL_I32,
    DTS_PARAM_VX_DC_ENABLE_I32,
    DTS_PARAM_VX_DC_CONTROL_I32,
    DTS_PARAM_VX_DEF_ENABLE_I32,
    DTS_PARAM_VX_DEF_CONTROL_I32,
    DTS_PARAM_LOUDNESS_CONTROL_ENABLE_I32,
    DTS_PARAM_LOUDNESS_CONTROL_TARGET_LOUDNESS_I32,
    DTS_PARAM_LOUDNESS_CONTROL_PRESET_I32,
    DTS_PARAM_LOUDNESS_CONTROL_IO_MODE_I32,
    DTS_PARAM_TBHDX_APP_SPKSIZE_I32,
    DTS_PARAM_TBHDX_APP_HPRATIO_F32,
    DTS_PARAM_TBHDX_APP_EXTBASS_F32,
    DTS_PARAM_MBHL_APP_FRT_LOWCROSS_F32,
    DTS_PARAM_MBHL_APP_FRT_MIDCROSS_F32,

}Virtualx_params;

Virtualx_param_t gVirtualxParam[] = {
    {{0, 4, 4}, VIRTUALX_PARAM_ENABLE, {1}},
    {{0, 4, 4}, VIRTUALX_PARAM_DIALOGCLARTY_MODE, {1}},
    {{0, 4, 4}, VIRTUALX_PARAM_SURROUND_MODE, {1}},
    {{0, 4, 4}, DTS_PARAM_MBHL_ENABLE_I32, {1}},
    {{0, 4, 4}, DTS_PARAM_MBHL_BYPASS_GAIN_I32, {1}},
    {{0, 4, 4}, DTS_PARAM_MBHL_REFERENCE_LEVEL_I32, {1}},
    {{0, 4, 4}, DTS_PARAM_MBHL_VOLUME_I32, {1}},
    {{0, 4, 4}, DTS_PARAM_MBHL_VOLUME_STEP_I32, {100}},
    {{0, 4, 4}, DTS_PARAM_MBHL_BALANCE_STEP_I32, {0}},
    {{0, 4, 4}, DTS_PARAM_MBHL_OUTPUT_GAIN_I32, {1}},
    {{0, 4, 4}, DTS_PARAM_MBHL_MODE_I32, {0}},
    {{0, 4, 4}, DTS_PARAM_MBHL_PROCESS_DISCARD_I32, {0}},
    {{0, 4, 4}, DTS_PARAM_MBHL_CROSS_LOW_I32, {7}},
    {{0, 4, 4}, DTS_PARAM_MBHL_CROSS_MID_I32, {15}},
    {{0, 4, 4}, DTS_PARAM_MBHL_COMP_ATTACK_I32, {5}},
    {{0, 4, 4}, DTS_PARAM_MBHL_COMP_LOW_RELEASE_I32, {250}},
    {{0, 4, 4}, DTS_PARAM_MBHL_COMP_LOW_RATIO_I32, {4}},
    {{0, 4, 4}, DTS_PARAM_MBHL_COMP_LOW_THRESH_I32, {0}},
    {{0, 4, 4}, DTS_PARAM_MBHL_COMP_LOW_MAKEUP_I32, {1}},
    {{0, 4, 4}, DTS_PARAM_MBHL_COMP_MID_RELEASE_I32, {250}},
    {{0, 4, 4}, DTS_PARAM_MBHL_COMP_MID_RATIO_I32, {4}},
    {{0, 4, 4}, DTS_PARAM_MBHL_COMP_MID_THRESH_I32, {0}},
    {{0, 4, 4}, DTS_PARAM_MBHL_COMP_MID_MAKEUP_I32, {1}},
    {{0, 4, 4}, DTS_PARAM_MBHL_COMP_HIGH_RELEASE_I32, {250}},
    {{0, 4, 4}, DTS_PARAM_MBHL_COMP_HIGH_RATIO_I32, {4}},
    {{0, 4, 4}, DTS_PARAM_MBHL_COMP_HIGH_THRESH_I32, {0}},
    {{0, 4, 4}, DTS_PARAM_MBHL_COMP_HIGH_MAKEUP_I32, {1}},
    {{0, 4, 4}, DTS_PARAM_MBHL_BOOST_I32, {1}},
    {{0, 4, 4}, DTS_PARAM_MBHL_THRESHOLD_I32, {1}},
    {{0, 4, 4}, DTS_PARAM_MBHL_SLOW_OFFSET_I32, {1}},
    {{0, 4, 4}, DTS_PARAM_MBHL_FAST_ATTACK_I32, {5}},
    {{0, 4, 4}, DTS_PARAM_MBHL_FAST_RELEASE_I32, {50}},
    {{0, 4, 4}, DTS_PARAM_MBHL_SLOW_ATTACK_I32, {500}},
    {{0, 4, 4}, DTS_PARAM_MBHL_SLOW_RELEASE_I32, {500}},
    {{0, 4, 4}, DTS_PARAM_MBHL_DELAY_I32, {8}},
    {{0, 4, 4}, DTS_PARAM_MBHL_ENVELOPE_FREQUENCY_I32, {20}},
    {{0, 4, 4}, DTS_PARAM_TBHDX_ENABLE_I32, {0}},
    {{0, 4, 4}, DTS_PARAM_TBHDX_MONO_MODE_I32, {0}},
    {{0, 4, 4}, DTS_PARAM_TBHDX_MAXGAIN_I32, {0}},
    {{0, 4, 4}, DTS_PARAM_TBHDX_SPKSIZE_I32, {2}},
    {{0, 4, 4}, DTS_PARAM_TBHDX_HP_ENABLE_I32, {1}},
    {{0, 4, 4}, DTS_PARAM_TBHDX_TEMP_GAIN_I32, {0}},
    {{0, 4, 4}, DTS_PARAM_TBHDX_PROCESS_DISCARD_I32, {1}},
    {{0, 4, 4}, DTS_PARAM_TBHDX_HPORDER_I32, {4}},
    {{0, 4, 4}, DTS_PARAM_VX_ENABLE_I32, {1}},
    {{0, 4, 4}, DTS_PARAM_VX_INPUT_MODE_I32, {4}},
    {{0, 4, 4}, DTS_PARAM_VX_OUTPUT_MODE_I32, {0}},
    {{0, 4, 4}, DTS_PARAM_VX_HEADROOM_GAIN_I32, {1}},
    {{0, 4, 4}, DTS_PARAM_VX_PROC_OUTPUT_GAIN_I32, {1}},
    {{0, 4, 4}, DTS_PARAM_VX_REFERENCE_LEVEL_I32, {0}},
    {{0, 4, 4}, DTS_PARAM_TSX_ENABLE_I32, {1}},
    {{0, 4, 4}, DTS_PARAM_TSX_PASSIVEMATRIXUPMIX_ENABLE_I32, {1}},
    {{0, 4, 4}, DTS_PARAM_TSX_HEIGHT_UPMIX_ENABLE_I32, {1}},
    {{0, 4, 4}, DTS_PARAM_TSX_LPR_GAIN_I32, {1}},
    {{0, 4, 4}, DTS_PARAM_TSX_CENTER_GAIN_I32, {1}},
    {{0, 4, 4}, DTS_PARAM_TSX_HORIZ_VIR_EFF_CTRL_I32, {0}},
    {{0, 4, 4}, DTS_PARAM_TSX_HEIGHTMIX_COEFF_I32, {1}},
    {{0, 4, 4}, DTS_PARAM_TSX_PROCESS_DISCARD_I32, {0}},
    {{0, 4, 4}, DTS_PARAM_TSX_HEIGHT_DISCARD_I32, {0}},
    {{0, 4, 4}, DTS_PARAM_TSX_FRNT_CTRL_I32, {1}},
    {{0, 4, 4}, DTS_PARAM_TSX_SRND_CTRL_I32, {1}},
    {{0, 4, 4}, DTS_PARAM_VX_DC_ENABLE_I32, {0}},
    {{0, 4, 4}, DTS_PARAM_VX_DC_CONTROL_I32, {0}},
    {{0, 4, 4}, DTS_PARAM_VX_DEF_ENABLE_I32, {0}},
    {{0, 4, 4}, DTS_PARAM_VX_DEF_CONTROL_I32, {0}},
    {{0, 4, 4}, DTS_PARAM_LOUDNESS_CONTROL_ENABLE_I32, {1}},
    {{0, 4, 4}, DTS_PARAM_LOUDNESS_CONTROL_TARGET_LOUDNESS_I32, {-24}},
    {{0, 4, 4}, DTS_PARAM_LOUDNESS_CONTROL_PRESET_I32, {0}},
    {{0, 4, 4}, DTS_PARAM_LOUDNESS_CONTROL_IO_MODE_I32,{0}},
    {{0, 4, 4}, DTS_PARAM_TBHDX_APP_SPKSIZE_I32,{0}},
    {{0, 4, 4}, DTS_PARAM_TBHDX_APP_HPRATIO_F32,{0}},
    {{0, 4, 4}, DTS_PARAM_TBHDX_APP_EXTBASS_F32,{0}},
    {{0, 4, 4}, DTS_PARAM_MBHL_APP_FRT_LOWCROSS_F32,{0}},
    {{0, 4, 4}, DTS_PARAM_MBHL_APP_FRT_MIDCROSS_F32,{0}},

};
const char *VXStatusstr[] = {"Disable", "Enable"};

//-------------TrebleBass parameters--------------------------
typedef struct TrebleBass_param_s {
    effect_param_t param;
    uint32_t command;
    union {
        uint32_t v;
        float f;
    };
} TrebleBass_param_t;

typedef enum {
    TREBASS_PARAM_BASS_LEVEL = 0,
    TREBASS_PARAM_TREBLE_LEVEL,
    TREBASS_PARAM_ENABLE,
} TrebleBass_params;

TrebleBass_param_t gTrebleBassParam[] = {
    {{0, 4, 4}, TREBASS_PARAM_BASS_LEVEL, {0}},
    {{0, 4, 4}, TREBASS_PARAM_TREBLE_LEVEL, {0}},
    {{0, 4, 4}, TREBASS_PARAM_ENABLE, {1}},
};

const char *TREBASSStatusstr[] = {"Disable", "Enable"};

//-------------HPEQ parameters--------------------------
typedef struct HPEQ_param_s {
    effect_param_t param;
    uint32_t command;
    union {
        uint32_t v;
        float f;
        signed char band[5];
    };
} HPEQ_param_t;

typedef enum {
    HPEQ_PARAM_ENABLE = 0,
    HPEQ_PARAM_EFFECT_MODE,
    HPEQ_PARAM_EFFECT_CUSTOM,
} HPEQ_params;

HPEQ_param_t gHPEQParam[] = {
    {{0, 4, 4}, HPEQ_PARAM_ENABLE, {1}},
    {{0, 4, 4}, HPEQ_PARAM_EFFECT_MODE, {0}},
    {{0, 4, 5}, HPEQ_PARAM_EFFECT_CUSTOM, {0}},
};

const char *HPEQStatusstr[] = {"Disable", "Enable"};

//-------------GEQ parameters--------------------------
typedef struct GEQ_param_s {
    effect_param_t param;
    uint32_t command;
    union {
        uint32_t v;
        float f;
        signed char band[9];
    };
} GEQ_param_t;

typedef enum {
    GEQ_PARAM_ENABLE = 0,
    GEQ_PARAM_EFFECT_MODE,
    GEQ_PARAM_EFFECT_CUSTOM,
} GEQ_params;

GEQ_param_t gGEQParam[] = {
    {{0, 4, 4}, GEQ_PARAM_ENABLE, {1}},
    {{0, 4, 4}, GEQ_PARAM_EFFECT_MODE, {0}},
    {{0, 4, 9}, GEQ_PARAM_EFFECT_CUSTOM, {0}},
};

const char *GEQStatusstr[] = {"Disable", "Enable"};

//-------------AVL parameters--------------------------
typedef struct Avl_param_s {
    effect_param_t param;
    uint32_t command;
    union {
        int32_t v;
        float f;
    };
} Avl_param_t;

typedef enum {
    AVL_PARAM_ENABLE,
    AVL_PARAM_PEAK_LEVEL,
    AVL_PARAM_DYNAMIC_THRESHOLD,
    AVL_PARAM_NOISE_THRESHOLD,
    AVL_PARAM_RESPONSE_TIME,
    AVL_PARAM_RELEASE_TIME,
    AVL_PARAM_SOURCE_IN,
} Avl_params;

Avl_param_t gAvlParam[] = {
    {{0, 4, 4}, AVL_PARAM_ENABLE, {1}},
    {{0, 4, 4}, AVL_PARAM_PEAK_LEVEL, {-18}},
    {{0, 4, 4}, AVL_PARAM_DYNAMIC_THRESHOLD, {-24}},
    {{0, 4, 4}, AVL_PARAM_NOISE_THRESHOLD, {-40}},
    {{0, 4, 4}, AVL_PARAM_RESPONSE_TIME, {512}},
    {{0, 4, 4}, AVL_PARAM_RELEASE_TIME, {2}},
    {{0, 4, 4}, AVL_PARAM_SOURCE_IN, {3}},
};

const char *AvlStatusstr[] = {"Disable", "Enable"};

//-------------DBX parameters--------------------------
typedef struct dbxtv_param_s {
    effect_param_t param;
    uint32_t command;
    union {
        uint32_t v;
        signed char mode[3];
    };
} dbxtv_param_t;

typedef enum {
    DBX_PARAM_ENABLE,
    DBX_SET_MODE,
} DBXparams;

dbxtv_param_t gdbxtvparam[] = {
    {{0, 4, 4}, DBX_PARAM_ENABLE, {1}},
    {{0, 4, 4}, DBX_SET_MODE, {0}},
};

const char *DBXStatusstr[] = {"Disable", "Enable"};

//-------UUID------------------------------------------
typedef enum {
    EFFECT_BALANCE = 0,
    EFFECT_SRS,
    EFFECT_TREBLEBASS,
    EFFECT_HPEQ,
    EFFECT_AVL,
    EFFECT_GEQ,
    EFFECT_VIRTUALX,
    EFFECT_DBX,
    EFFECT_MAX,
} EFFECT_params;

effect_uuid_t gEffectStr[] = {
    {0x6f33b3a0, 0x578e, 0x11e5, 0x892f, {0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b}}, // 0:Balance
    {0x8a857720, 0x0209, 0x11e2, 0xa9d8, {0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b}}, // 1:TruSurround
    {0x76733af0, 0x2889, 0x11e2, 0x81c1, {0x08, 0x00, 0x20, 0x0c, 0x9a, 0x66}}, // 2:TrebleBass
    {0x049754aa, 0xc4cf, 0x439f, 0x897e, {0x37, 0xdd, 0x0c, 0x38, 0x11, 0x20}}, // 3:Hpeq
    {0x08246a2a, 0xb2d3, 0x4621, 0xb804, {0x42, 0xc9, 0xb4, 0x78, 0xeb, 0x9d}}, // 4:Avl
    {0x2e2a5fa6, 0xcae8, 0x45f5, 0xbb70, {0xa2, 0x9c, 0x1f, 0x30, 0x74, 0xb2}}, // 5:Geq
    {0x61821587, 0xce3c, 0x4aac, 0x9122, {0x86, 0xd8, 0x74, 0xea, 0x1f, 0xb1}}, // 6:Virtualx
    {0x07210842, 0x7432, 0x4624, 0x8b97, {0x35, 0xac, 0x87, 0x82, 0xef, 0xa3}}, // 7:DBX
};

static inline float DbToAmpl(float decibels)
{
    if (decibels <= -758) {
        return 0.0f;
    }
    return exp( decibels * 0.115129f); // exp( dB * ln(10) / 20 )
}

static int Balance_effect_func(AudioEffect* gAudioEffect, int gParamIndex, int gParamValue)
{
    balance_gain blrg;
    String8 keyValuePairs = String8("");
    String8 mString = String8("");
    char valuel[64] = {0};
    AudioParameter param = AudioParameter();
    audio_io_handle_t handle = AUDIO_IO_HANDLE_NONE;
    int32_t value = 0;
    int32_t total_gain = 0;
    if (balance_level_num == 0) {
        gAudioEffect->getParameter(&gBalanceParam[BALANCE_PARAM_LEVEL_NUM].param);
        balance_level_num = gBalanceParam[BALANCE_PARAM_LEVEL_NUM].v;
        LOG("Balance: Level size = %d\n", balance_level_num);
    }
    gAudioEffect->getParameter(&gBalanceParam[BALANCE_PARAM_INDEX].param);
    for (int i = 0; i < balance_level_num; i++) {
        index1[i] = gBalanceParam[BALANCE_PARAM_INDEX].index[i];
        //LOG("Balance: index = %f\n", index1[i]);
    }
    switch (gParamIndex) {
    case BALANCE_PARAM_LEVEL:
        if (gParamValue  < 0 || gParamValue > ((balance_level_num - 1) << 1)) {
            LOG("Balance: Level gParamValue = %d invalid\n", gParamValue);
            return -1;
        }
        gBalanceParam[gParamIndex].v = gParamValue;
        gAudioEffect->setParameter(&gBalanceParam[gParamIndex].param);
        gAudioEffect->getParameter(&gBalanceParam[gParamIndex].param);
        LOG("Balance: Level is %d -> %d\n", gParamValue, gBalanceParam[gParamIndex].v);
        return 0;
    case BALANCE_PARAM_ENABLE:
        if (gParamValue < 0 || gParamValue > 1) {
            LOG("Balance: Status gParamValue = %d invalid\n", gParamValue);
            return -1;
        }
        gBalanceParam[gParamIndex].v = gParamValue;
        gAudioEffect->setParameter(&gBalanceParam[gParamIndex].param);
        gAudioEffect->getParameter(&gBalanceParam[gParamIndex].param);
        LOG("Balance: Status is %d -> %s\n", gParamValue, BalanceStatusstr[gBalanceParam[gParamIndex].v]);
        return 0;
    case BALANCE_PARAM_USB:
        gBalanceParam[gParamIndex].v = gParamValue;
        value = (((int)gBalanceParam[gParamIndex].v) >> LSR);
        if (value >= balance_level_num)
           value = balance_level_num;
        else if (value < 0)
            value = 0;
        if (value < (balance_level_num >> LSR)) {
           //right process
           blrg.left_gain = 1;
           blrg.right_gain = index1[value];
           snprintf(valuel, 40, "%f %f", blrg.right_gain,blrg.left_gain);
           param.add(String8("USB_GAIN_RIGHT"), String8(valuel));
           keyValuePairs = param.toString();
           if (AudioSystem::setParameters(handle, keyValuePairs) != NO_ERROR) {
             LOG("setusbgain: Set gain failed\n");
             return -1;
           }
         param.remove(String8("USB_GAIN_RIGHT"));
        } else {
          //left process
           blrg.right_gain = 1;
           blrg.left_gain = index1[value];
           snprintf(valuel, 40, "%f %f", blrg.left_gain,blrg.right_gain);
           param.add(String8("USB_GAIN_LEFT"), String8(valuel));
           keyValuePairs = param.toString();
           if (AudioSystem::setParameters(handle, keyValuePairs) != NO_ERROR) {
             LOG("setusbgain: Set gain failed\n");
             return -1;
          }
           param.remove(String8("USB_GAIN_LEFT"));
       }
        return 0;
    case BALANCE_PARAM_BT:
        gBalanceParam[gParamIndex].v = gParamValue;
        value = (((int)gBalanceParam[gParamIndex].v) >> LSR);
        if (value >= balance_level_num)
           value = balance_level_num;
        else if (value < 0)
            value = 0;
        if (value < (balance_level_num >> LSR)) {
           //right process
           blrg.left_gain = 1;
           blrg.right_gain = index1[value];
           LOG("blrg.left_gain is %f riht gian is %f",blrg.left_gain,blrg.right_gain);
           snprintf(valuel, 40, "%f %f", blrg.right_gain,blrg.left_gain);
           param.add(String8("BT_GAIN_RIGHT"), String8(valuel));
           keyValuePairs = param.toString();
           if (AudioSystem::setParameters(handle, keyValuePairs) != NO_ERROR) {
             LOG("setbtgain: Set gain failed\n");
             return -1;
           }
         param.remove(String8("BT_GAIN_RIGHT"));
        } else {
          //left process
           blrg.right_gain = 1;
           blrg.left_gain = index1[value];
           LOG("blrg.left_gain is %f riht gian is %f",blrg.left_gain,blrg.right_gain);
           snprintf(valuel, 40, "%f %f", blrg.left_gain,blrg.right_gain);
           param.add(String8("BT_GAIN_LEFT"), String8(valuel));
           keyValuePairs = param.toString();
           if (AudioSystem::setParameters(handle, keyValuePairs) != NO_ERROR) {
             LOG("setbtgain: Set gain failed\n");
             return -1;
           }
           param.remove(String8("BT_GAIN_LEFT"));
        }
        return 0;
    default:
        LOG("Balance: ParamIndex = %d invalid\n", gParamIndex);
        return -1;
    }
}

static int SRS_effect_func(AudioEffect* gAudioEffect, int gParamIndex, int gParamValue, float gParamScale)
{
    switch (gParamIndex) {
    case SRS_PARAM_MODE:
        if (gParamValue < 0 || gParamValue > 5) {
            LOG("TruSurround: Mode gParamValue = %d invalid\n", gParamValue);
            return -1;
        }
        gSRSParam[gParamIndex].v = gParamValue;
        gAudioEffect->setParameter(&gSRSParam[gParamIndex].param);
        gAudioEffect->getParameter(&gSRSParam[gParamIndex].param);
        LOG("TruSurround: Mode is %d -> %s\n", gParamValue, SRSModestr[gSRSParam[gParamIndex].v]);
        return 0;
    case SRS_PARAM_DIALOGCLARTY_MODE:
        gAudioEffect->getParameter(&gSRSParam[SRS_PARAM_DIALOG_CLARITY_ENABLE].param);
        if (gSRSParam[SRS_PARAM_DIALOG_CLARITY_ENABLE].v == 0) {
            LOG("TruSurround: Set Dialog Clarity Mode failed as Dialog Clarity is disabled\n");
            return -1;
        }
        if (gParamValue < 0 || gParamValue > 2) {
            LOG("TruSurround: Dialog Clarity Mode gParamValue = %d invalid\n", gParamValue);
            return -1;
        }
        gSRSParam[gParamIndex].v = gParamValue;
        gAudioEffect->setParameter(&gSRSParam[gParamIndex].param);
        gAudioEffect->getParameter(&gSRSParam[gParamIndex].param);
        LOG("TruSurround: Dialog Clarity Mode is %s -> %d\n", SRSDialogClarityModestr[gParamValue], gSRSParam[gParamIndex].v);
        return 0;
    case SRS_PARAM_SURROUND_MODE:
        gAudioEffect->getParameter(&gSRSParam[SRS_PARAM_SURROUND_ENABLE].param);
        if (gSRSParam[SRS_PARAM_SURROUND_ENABLE].v == 0) {
            LOG("TruSurround: Set Surround Mode failed as Surround is disabled\n");
            return -1;
        }
        if (gParamValue < 0 || gParamValue > 1) {
            LOG("TruSurround: Surround Mode gParamValue = %d invalid\n", gParamValue);
            return -1;
        }
        gSRSParam[gParamIndex].v = gParamValue;
        gAudioEffect->setParameter(&gSRSParam[gParamIndex].param);
        gAudioEffect->getParameter(&gSRSParam[gParamIndex].param);
        LOG("TruSurround: Surround Mode is %s -> %d\n", SRSSurroundModestr[gParamValue], gSRSParam[gParamIndex].v);
        return 0;
    case SRS_PARAM_VOLUME_MODE:
        return 0;
    case SRS_PARAM_ENABLE:
        if (gParamValue < 0 || gParamValue > 1) {
            LOG("TruSurround: Status gParamValue = %d invalid\n", gParamValue);
            return -1;
        }
        gSRSParam[gParamIndex].v = gParamValue;
        gAudioEffect->setParameter(&gSRSParam[gParamIndex].param);
        gAudioEffect->getParameter(&gSRSParam[gParamIndex].param);
        LOG("TruSurround: Status is %d -> %s\n", gParamValue, SRSStatusstr[gSRSParam[gParamIndex].v]);
        return 0;
    case SRS_PARAM_TRUEBASS_ENABLE:
        if (gParamValue < 0 || gParamValue > 1) {
            LOG("TruSurround: True Bass gParamValue = %d invalid\n", gParamValue);
            return -1;
        }
        gSRSParam[gParamIndex].v = gParamValue;
        gAudioEffect->setParameter(&gSRSParam[gParamIndex].param);
        gAudioEffect->getParameter(&gSRSParam[gParamIndex].param);
        LOG("TruSurround: True Bass is %d -> %s\n", gParamValue, SRSTrueBassstr[gSRSParam[gParamIndex].v]);
        return 0;
    case SRS_PARAM_TRUEBASS_SPKER_SIZE:
        gAudioEffect->getParameter(&gSRSParam[SRS_PARAM_TRUEBASS_ENABLE].param);
        if (gSRSParam[SRS_PARAM_TRUEBASS_ENABLE].v == 0) {
            LOG("TruSurround: Set True Bass Speaker Size failed as True Bass is disabled\n");
            return -1;
        }
        if (gParamValue < 0 || gParamValue > 7) {
            LOG("TruSurround: True Bass Speaker Size gParamValue = %d invalid\n", gParamValue);
            return -1;
        }
        gSRSParam[gParamIndex].v = gParamValue;
        gAudioEffect->setParameter(&gSRSParam[gParamIndex].param);
        gAudioEffect->getParameter(&gSRSParam[gParamIndex].param);
        LOG("TruSurround: True Bass Speaker Size is %d -> %d\n", gParamValue, gSRSParam[gParamIndex].v);
        return 0;
    case SRS_PARAM_TRUEBASS_GAIN:
        gAudioEffect->getParameter(&gSRSParam[SRS_PARAM_TRUEBASS_ENABLE].param);
        if (gSRSParam[SRS_PARAM_TRUEBASS_ENABLE].v == 0) {
            LOG("TruSurround: Set True Bass Gain failed as True Bass is disabled\n");
            return -1;
        }
        if (gParamScale < 0.0 || gParamScale > 1.0) {
            LOG("TruSurround: True Bass Gain gParamScale = %f invalid\n", gParamScale);
            return -1;
        }
        gSRSParam[gParamIndex].f = gParamScale;
        gAudioEffect->setParameter(&gSRSParam[gParamIndex].param);
        gAudioEffect->getParameter(&gSRSParam[gParamIndex].param);
        LOG("TruSurround: True Bass Gain %f -> %f\n", gParamScale, gSRSParam[gParamIndex].f);
        return 0;
    case SRS_PARAM_DIALOG_CLARITY_ENABLE:
        if (gParamValue < 0 || gParamValue > 1) {
            LOG("TruSurround: Dialog Clarity gParamValue = %d invalid\n", gParamValue);
            return -1;
        }
        gSRSParam[gParamIndex].v = gParamValue;
        gAudioEffect->setParameter(&gSRSParam[gParamIndex].param);
        gAudioEffect->getParameter(&gSRSParam[gParamIndex].param);
        LOG("TruSurround: Dialog Clarity is %d -> %s\n", gParamValue, SRSDialogClaritystr[gSRSParam[gParamIndex].v]);
        return 0;
    case SRS_PARAM_DIALOGCLARTY_GAIN:
        gAudioEffect->getParameter(&gSRSParam[SRS_PARAM_DIALOG_CLARITY_ENABLE].param);
        if (gSRSParam[SRS_PARAM_DIALOG_CLARITY_ENABLE].v == 0) {
            LOG("TruSurround: Set Dialog Clarity Gain failed as Dialog Clarity is disabled\n");
            return -1;
        }
        if (gParamScale < 0.0 || gParamScale > 1.0) {
            LOG("TruSurround: Dialog Clarity Gain gParamScale = %f invalid\n", gParamScale);
            return -1;
        }
        gSRSParam[gParamIndex].f = gParamScale;
        gAudioEffect->setParameter(&gSRSParam[gParamIndex].param);
        gAudioEffect->getParameter(&gSRSParam[gParamIndex].param);
        LOG("TruSurround: Dialog Clarity Gain %f -> %f\n", gParamScale, gSRSParam[gParamIndex].f);
        return 0;
    case SRS_PARAM_DEFINITION_ENABLE:
        if (gParamValue < 0 || gParamValue > 1) {
            LOG("TruSurround: Definition gParamValue = %d invalid\n", gParamValue);
            return -1;
        }
        gSRSParam[gParamIndex].v = gParamValue;
        gAudioEffect->setParameter(&gSRSParam[gParamIndex].param);
        gAudioEffect->getParameter(&gSRSParam[gParamIndex].param);
        LOG("TruSurround: Definition is %d -> %s\n", gParamValue, SRSDefinitionstr[gSRSParam[gParamIndex].v]);
        return 0;
    case SRS_PARAM_DEFINITION_GAIN:
        gAudioEffect->getParameter(&gSRSParam[SRS_PARAM_DEFINITION_ENABLE].param);
        if (gSRSParam[SRS_PARAM_DEFINITION_ENABLE].v == 0) {
            LOG("TruSurround: Set Definition Gain failed as Definition is disabled\n");
            return -1;
        }
        if (gParamScale < 0.0 || gParamScale > 1.0) {
            LOG("TruSurround: Definition Gain gParamScale = %f invalid\n", gParamScale);
            return -1;
        }
        gSRSParam[gParamIndex].f = gParamScale;
        gAudioEffect->setParameter(&gSRSParam[gParamIndex].param);
        gAudioEffect->getParameter(&gSRSParam[gParamIndex].param);
        LOG("TruSurround: Definition Gain %f -> %f\n", gParamScale, gSRSParam[gParamIndex].f);
        return 0;
    case SRS_PARAM_SURROUND_ENABLE:
        if (gParamValue < 0 || gParamValue > 1) {
            LOG("TruSurround: Surround gParamValue = %d invalid\n", gParamValue);
            return -1;
        }
        gSRSParam[gParamIndex].v = gParamValue;
        gAudioEffect->setParameter(&gSRSParam[gParamIndex].param);
        gAudioEffect->getParameter(&gSRSParam[gParamIndex].param);
        LOG("TruSurround: Surround is %d -> %s\n", gParamValue, SRSSurroundstr[gSRSParam[gParamIndex].v]);
        return 0;
    case SRS_PARAM_SURROUND_GAIN:
        gAudioEffect->getParameter(&gSRSParam[SRS_PARAM_SURROUND_ENABLE].param);
        if (gSRSParam[SRS_PARAM_SURROUND_ENABLE].v == 0) {
            LOG("TruSurround: Set Surround Gain failed as Surround is disabled\n");
            return -1;
        }
        if (gParamScale < 0.0 || gParamScale > 1.0) {
            LOG("TruSurround: Surround Gain gParamScale = %f invalid\n", gParamScale);
            return -1;
        }
        gSRSParam[gParamIndex].f = gParamScale;
        gAudioEffect->setParameter(&gSRSParam[gParamIndex].param);
        gAudioEffect->getParameter(&gSRSParam[gParamIndex].param);
        LOG("TruSurround: Surround Gain %f -> %f\n", gParamScale, gSRSParam[gParamIndex].f);
        return 0;
    case SRS_PARAM_INPUT_GAIN:
        if (gParamScale < 0.0 || gParamScale > 1.0) {
            LOG("TruSurround: Input Gain gParamScale = %f invalid\n", gParamScale);
            return -1;
        }
        gSRSParam[gParamIndex].f = gParamScale;
        gAudioEffect->setParameter(&gSRSParam[gParamIndex].param);
        gAudioEffect->getParameter(&gSRSParam[gParamIndex].param);
        LOG("TruSurround: Input Gain is %f -> %f\n", gParamScale, gSRSParam[gParamIndex].f);
        return 0;
    case SRS_PARAM_OUTPUT_GAIN:
        if (gParamScale < 0.0 || gParamScale > 1.0) {
            LOG("TruSurround: Output Gain gParamScale = %f invalid\n", gParamScale);
            return -1;
        }
        gSRSParam[gParamIndex].f = gParamScale;
        gAudioEffect->setParameter(&gSRSParam[gParamIndex].param);
        gAudioEffect->getParameter(&gSRSParam[gParamIndex].param);
        LOG("TruSurround: Output Gain is %f -> %f\n", gParamScale, gSRSParam[gParamIndex].f);
        return 0;
    case SRS_PARAM_OUTPUT_GAIN_COMP:
        if (gParamScale < -20.0 || gParamScale > 20.0) {
            LOG("TruSurround: Output Gain gParamScale = %f invalid\n", gParamScale);
            return -1;
        }
        gSRSParam[gParamIndex].f = gParamScale;
        gAudioEffect->setParameter(&gSRSParam[gParamIndex].param);
        gAudioEffect->getParameter(&gSRSParam[gParamIndex].param);
        LOG("TruSurround: Output Gain Comp is %f -> %f\n", gParamScale, gSRSParam[gParamIndex].f);
        return 0;
    default:
        LOG("TruSurround: ParamIndex = %d invalid\n", gParamIndex);
        return -1;
    }
}

static int TrebleBass_effect_func(AudioEffect* gAudioEffect, int gParamIndex, int gParamValue)
{
    switch (gParamIndex) {
    case TREBASS_PARAM_BASS_LEVEL:
        if (gParamValue < 0 || gParamValue > 100) {
            LOG("TrebleBass: Bass gParamValue = %d invalid\n", gParamValue);
            return -1;
        }
        gTrebleBassParam[gParamIndex].v = gParamValue;
        gAudioEffect->setParameter(&gTrebleBassParam[gParamIndex].param);
        gAudioEffect->getParameter(&gTrebleBassParam[gParamIndex].param);
        LOG("TrebleBass: Bass is %d -> %d level\n", gParamValue, gTrebleBassParam[gParamIndex].v);
        return 0;
    case TREBASS_PARAM_TREBLE_LEVEL:
        if (gParamValue < 0 || gParamValue > 100) {
            LOG("TrebleBass: Treble gParamValue = %d invalid\n", gParamValue);
            return -1;
        }
        gTrebleBassParam[gParamIndex].v = gParamValue;
        gAudioEffect->setParameter(&gTrebleBassParam[gParamIndex].param);
        gAudioEffect->getParameter(&gTrebleBassParam[gParamIndex].param);
        LOG("TrebleBass: Treble is %d -> %d level\n", gParamValue, gTrebleBassParam[gParamIndex].v);
        return 0;
    case TREBASS_PARAM_ENABLE:
        if (gParamValue < 0 || gParamValue > 1) {
            LOG("TrebleBass: Status gParamValue = %d invalid\n", gParamValue);
            return -1;
        }
        gTrebleBassParam[gParamIndex].v = gParamValue;
        gAudioEffect->setParameter(&gTrebleBassParam[gParamIndex].param);
        gAudioEffect->getParameter(&gTrebleBassParam[gParamIndex].param);
        LOG("TrebleBass: Status is %d -> %s\n", gParamValue, TREBASSStatusstr[gTrebleBassParam[gParamIndex].v]);
        return 0;
    default:
        LOG("TrebleBass: ParamIndex = %d invalid\n", gParamIndex);
        return -1;
    }
}

static int Virtualx_effect_func(AudioEffect* gAudioEffect, int gParamIndex, int gParamValue,float gParamScale)
{
    int rc = 0;
    switch (gParamIndex) {
        case VIRTUALX_PARAM_ENABLE:
            if (gParamValue < 0 || gParamValue > 1) {
                LOG("Vritualx: Status gParamValue = %d invalid\n", gParamValue);
                return -1;
            }
            gVirtualxParam[gParamIndex].v = gParamValue;
            if (!gAudioEffect->setParameter(&gVirtualxParam[gParamIndex].param))
                LOG("=================successful\n");
            else
                LOG("=====================failed\n");
            //gAudioEffect->getParameter(&gVirtualxParam[gParamIndex].param);
            LOG("Virtualx: Status is %d -> %s\n", gParamValue, VXStatusstr[gVirtualxParam[gParamIndex].v]);
            return 0;
       case DTS_PARAM_MBHL_ENABLE_I32:
            if (gParamValue < 0 || gParamValue > 1) {
                LOG("Vritualx: MBHL ENABLE gParamValue = %d invalid\n", gParamValue);
                return -1;
            }
            gVirtualxParam[gParamIndex].v = gParamValue;
            gAudioEffect->setParameter(&gVirtualxParam[gParamIndex].param);
            //gAudioEffect->getParameter(&gVirtualxParam[gParamIndex].param);
            LOG("mbhl enable is %d \n",gVirtualxParam[gParamIndex].v);
            return 0;
        case DTS_PARAM_MBHL_BYPASS_GAIN_I32:
            if (gParamScale < 0 || gParamScale > 1.0) {
                LOG("Vritualx: mbhl bypass gain gParamValue = %f invalid\n", gParamScale);
                return -1;
            }
            gVirtualxParam[gParamIndex].f = gParamScale;
            gAudioEffect->setParameter(&gVirtualxParam[gParamIndex].param);
            //gAudioEffect->getParameter(&gVirtualxParam[gParamIndex].param);
            LOG("mbhl bypassgain is %f \n",gVirtualxParam[gParamIndex].f);
            return 0;
        case DTS_PARAM_MBHL_REFERENCE_LEVEL_I32:
            if (gParamScale < 0.0009 || gParamScale > 1.0) {
                LOG("Vritualx: mbhl reference level gParamValue = %f invalid\n", gParamScale);
                return -1;
            }
            gVirtualxParam[gParamIndex].f = gParamScale;
            gAudioEffect->setParameter(&gVirtualxParam[gParamIndex].param);
            //gAudioEffect->getParameter(&gVirtualxParam[gParamIndex].param);
            LOG("mbhl reference level is %f \n",gVirtualxParam[gParamIndex].f);
            return 0;
        case DTS_PARAM_MBHL_VOLUME_I32:
            if (gParamScale < 0 || gParamScale > 1.0) {
                LOG("Vritualx: mbhl volume gParamValue = %f invalid\n", gParamScale);
                return -1;
            }
            gVirtualxParam[gParamIndex].f = gParamScale;
            gAudioEffect->setParameter(&gVirtualxParam[gParamIndex].param);
            //gAudioEffect->getParameter(&gVirtualxParam[gParamIndex].param);
            LOG("mbhl volume is %f \n",gVirtualxParam[gParamIndex].f);
            return 0;
        case DTS_PARAM_MBHL_VOLUME_STEP_I32:
            if (gParamValue < 0 || gParamValue > 100) {
                LOG("Vritualx: mbhl volumestep gParamValue = %d invalid\n", gParamValue);
                return -1;
            }
            gVirtualxParam[gParamIndex].v = gParamValue;
            gAudioEffect->setParameter(&gVirtualxParam[gParamIndex].param);
            //gAudioEffect->getParameter(&gVirtualxParam[gParamIndex].param);
            LOG("mbhl volume step is %d \n",gVirtualxParam[gParamIndex].v);
            return 0;
        case DTS_PARAM_MBHL_BALANCE_STEP_I32:
            if (gParamValue < -10 || gParamValue > 10) {
                LOG("Vritualx: mbhl banlance step gParamValue = %d invalid\n", gParamValue);
                return -1;
            }
            gVirtualxParam[gParamIndex].v = gParamValue;
            gAudioEffect->setParameter(&gVirtualxParam[gParamIndex].param);
            //gAudioEffect->getParameter(&gVirtualxParam[gParamIndex].param);
            LOG("mbhl balance step is %d \n",gVirtualxParam[gParamIndex].v);
            return 0;
        case DTS_PARAM_MBHL_OUTPUT_GAIN_I32:
            if (gParamScale < 0 || gParamScale > 1.0) {
                LOG("Vritualx: mbhl output gain gParamValue = %f invalid\n", gParamScale);
                return -1;
            }
            gVirtualxParam[gParamIndex].f = gParamScale;
            gAudioEffect->setParameter(&gVirtualxParam[gParamIndex].param);
           // gAudioEffect->getParameter(&gVirtualxParam[gParamIndex].param);
            LOG("mbhl output gain is %f \n",gVirtualxParam[gParamIndex].f);
            return 0;
        case DTS_PARAM_MBHL_BOOST_I32:
            if (gParamScale < 0.001 || gParamScale > 1000) {
                LOG("Vritualx: mbhl boost  gParamValue = %f invalid\n", gParamScale);
                return -1;
            }
            gVirtualxParam[gParamIndex].f = gParamScale;
            gAudioEffect->setParameter(&gVirtualxParam[gParamIndex].param);
            //gAudioEffect->getParameter(&gVirtualxParam[gParamIndex].param);
            LOG("mbhl boost is %f \n",gVirtualxParam[gParamIndex].f);
            return 0;
        case DTS_PARAM_MBHL_THRESHOLD_I32:
            if (gParamScale < 0.064 || gParamScale > 1.0) {
                LOG("Vritualx: mbhl threshold  gParamValue = %f invalid\n", gParamScale);
                return -1;
            }
            gVirtualxParam[gParamIndex].f = gParamScale;
            gAudioEffect->setParameter(&gVirtualxParam[gParamIndex].param);
            gAudioEffect->getParameter(&gVirtualxParam[gParamIndex].param);
            LOG("mbhl threshold is %f \n",gVirtualxParam[gParamIndex].f);
            return 0;
        case DTS_PARAM_MBHL_SLOW_OFFSET_I32:
            if (gParamScale < 0.3170 || gParamScale > 3.1619) {
                LOG("Vritualx: mbhl slow offset  gParamValue = %f invalid\n", gParamScale);
                return -1;
            }
            gVirtualxParam[gParamIndex].f = gParamScale;
            gAudioEffect->setParameter(&gVirtualxParam[gParamIndex].param);
            //gAudioEffect->getParameter(&gVirtualxParam[gParamIndex].param);
            LOG("mbhl slow offset is %f \n",gVirtualxParam[gParamIndex].f);
            return 0;
        case DTS_PARAM_MBHL_FAST_ATTACK_I32:
            if (gParamScale < 0 || gParamScale > 10) {
                LOG("Vritualx: mbhl fast attack  gParamValue = %f invalid\n", gParamScale);
                return -1;
            }
            gVirtualxParam[gParamIndex].f = gParamScale;
            gAudioEffect->setParameter(&gVirtualxParam[gParamIndex].param);
            //gAudioEffect->getParameter(&gVirtualxParam[gParamIndex].param);
            LOG("mbhl fast attack is %f \n",gVirtualxParam[gParamIndex].f);
            return 0;
        case DTS_PARAM_MBHL_FAST_RELEASE_I32:
            if (gParamValue < 10 || gParamValue > 500) {
                LOG("Vritualx: mbhl fast release gParamValue = %d invalid\n", gParamValue);
                return -1;
            }
            gVirtualxParam[gParamIndex].v = gParamValue;
            gAudioEffect->setParameter(&gVirtualxParam[gParamIndex].param);
            //gAudioEffect->getParameter(&gVirtualxParam[gParamIndex].param);
            LOG("mbhl fast release is %d \n",gVirtualxParam[gParamIndex].v);
            return 0;
        case DTS_PARAM_MBHL_SLOW_ATTACK_I32:
            if (gParamValue < 100 || gParamValue > 1000) {
                LOG("Vritualx: mbhl slow attack gParamValue = %d invalid\n", gParamValue);
                return -1;
            }
            gVirtualxParam[gParamIndex].v = gParamValue;
            gAudioEffect->setParameter(&gVirtualxParam[gParamIndex].param);
            //gAudioEffect->getParameter(&gVirtualxParam[gParamIndex].param);
            LOG("mbhl slow attack is %d \n",gVirtualxParam[gParamIndex].v);
            return 0;
        case DTS_PARAM_MBHL_SLOW_RELEASE_I32:
            if (gParamValue < 100 || gParamValue > 2000) {
                LOG("Vritualx: mbhl slow release gParamValue = %d invalid\n", gParamValue);
                return -1;
            }
            gVirtualxParam[gParamIndex].v = gParamValue;
            gAudioEffect->setParameter(&gVirtualxParam[gParamIndex].param);
            //gAudioEffect->getParameter(&gVirtualxParam[gParamIndex].param);
            LOG("mbhl slow release is %d \n",gVirtualxParam[gParamIndex].v);
            return 0;
        case DTS_PARAM_MBHL_DELAY_I32:
            if (gParamValue < 1 || gParamValue > 16) {
                LOG("Vritualx: mbhl delay gParamValue = %d invalid\n", gParamValue);
                return -1;
            }
            gVirtualxParam[gParamIndex].v = gParamValue;
            gAudioEffect->setParameter(&gVirtualxParam[gParamIndex].param);
            //gAudioEffect->getParameter(&gVirtualxParam[gParamIndex].param);
            LOG("mbhl delay is %d \n",gVirtualxParam[gParamIndex].v);
            return 0;
        case DTS_PARAM_MBHL_ENVELOPE_FREQUENCY_I32:
            if (gParamValue < 5 || gParamValue > 500) {
                LOG("Vritualx: mbhl envelope freq gParamValue = %d invalid\n", gParamValue);
                return -1;
            }
            gVirtualxParam[gParamIndex].v = gParamValue;
            gAudioEffect->setParameter(&gVirtualxParam[gParamIndex].param);
            //gAudioEffect->getParameter(&gVirtualxParam[gParamIndex].param);
            LOG("mbhl envelope freq is %d \n",gVirtualxParam[gParamIndex].v);
            return 0;
        case DTS_PARAM_MBHL_MODE_I32:
            if (gParamValue < 0 || gParamValue > 4) {
                LOG("Vritualx: mbhl mode gParamValue = %d invalid\n", gParamValue);
                return -1;
            }
            gVirtualxParam[gParamIndex].v = gParamValue;
            gAudioEffect->setParameter(&gVirtualxParam[gParamIndex].param);
            //gAudioEffect->getParameter(&gVirtualxParam[gParamIndex].param);
            LOG("mbhl mode is %d \n",gVirtualxParam[gParamIndex].v);
            return 0;
        case DTS_PARAM_MBHL_PROCESS_DISCARD_I32:
            if (gParamValue < 0 || gParamValue > 1) {
                LOG("Vritualx: mbhl process discard gParamValue = %d invalid\n", gParamValue);
                return -1;
            }
            gVirtualxParam[gParamIndex].v = gParamValue;
            gAudioEffect->setParameter(&gVirtualxParam[gParamIndex].param);
            //gAudioEffect->getParameter(&gVirtualxParam[gParamIndex].param);
            LOG("mbhl process discard is %d \n",gVirtualxParam[gParamIndex].v);
            return 0;
        case DTS_PARAM_MBHL_CROSS_LOW_I32:
            if (gParamValue < 0 || gParamValue > 20) {
                LOG("Vritualx: mbhl cross low gParamValue = %d invalid\n", gParamValue);
                return -1;
            }
            gVirtualxParam[gParamIndex].v = gParamValue;
            gAudioEffect->setParameter(&gVirtualxParam[gParamIndex].param);
            //gAudioEffect->getParameter(&gVirtualxParam[gParamIndex].param);
            LOG("mbhl cross low  is %d \n",gVirtualxParam[gParamIndex].v);
            return 0;
        case DTS_PARAM_MBHL_CROSS_MID_I32:
            if (gParamValue < 0 || gParamValue > 20) {
                LOG("Vritualx: mbhl cross mid gParamValue = %d invalid\n", gParamValue);
                return -1;
            }
            gVirtualxParam[gParamIndex].v = gParamValue;
            gAudioEffect->setParameter(&gVirtualxParam[gParamIndex].param);
            //gAudioEffect->getParameter(&gVirtualxParam[gParamIndex].param);
            LOG("mbhl cross mid is %d \n",gVirtualxParam[gParamIndex].v);
            return 0;
        case DTS_PARAM_MBHL_COMP_ATTACK_I32:
            if (gParamValue < 0 || gParamValue > 100) {
                LOG("Vritualx: mbhl compressor attack time gParamValue = %d invalid\n", gParamValue);
                return -1;
            }
            gVirtualxParam[gParamIndex].v = gParamValue;
            gAudioEffect->setParameter(&gVirtualxParam[gParamIndex].param);
            gAudioEffect->getParameter(&gVirtualxParam[gParamIndex].param);
            LOG("mbhl compressor attack time is %d \n",gVirtualxParam[gParamIndex].v);
            return 0;
        case DTS_PARAM_MBHL_COMP_LOW_RELEASE_I32:
            if (gParamValue < 50 || gParamValue > 2000) {
                LOG("Vritualx: mbhl compressor low release time gParamValue = %d invalid\n", gParamValue);
                return -1;
            }
            gVirtualxParam[gParamIndex].v = gParamValue;
            gAudioEffect->setParameter(&gVirtualxParam[gParamIndex].param);
            //gAudioEffect->getParameter(&gVirtualxParam[gParamIndex].param);
            LOG("mbhl compressor low release time is %d \n",gVirtualxParam[gParamIndex].v);
            return 0;
        case DTS_PARAM_MBHL_COMP_LOW_RATIO_I32:
            if (gParamScale < 1.0 || gParamScale > 20.0) {
                LOG("Vritualx: mbhl compressor low ratio gParamValue = %f invalid\n", gParamScale);
                return -1;
            }
            gVirtualxParam[gParamIndex].f = gParamScale;
            gAudioEffect->setParameter(&gVirtualxParam[gParamIndex].param);
            //gAudioEffect->getParameter(&gVirtualxParam[gParamIndex].param);
            LOG("mbhl compressor low ratio is %f \n",gVirtualxParam[gParamIndex].f);
            return 0;
        case DTS_PARAM_MBHL_COMP_LOW_THRESH_I32:
            if (gParamScale < 0.0640 || gParamScale > 15.8479) {
                LOG("Vritualx: mbhl compressor low threshold gParamValue = %f invalid\n", gParamScale);
                return -1;
            }
            gVirtualxParam[gParamIndex].f = gParamScale;
            gAudioEffect->setParameter(&gVirtualxParam[gParamIndex].param);
            //gAudioEffect->getParameter(&gVirtualxParam[gParamIndex].param);
            LOG("mbhl compressor low threshold is %f \n",gVirtualxParam[gParamIndex].f);
            return 0;
        case DTS_PARAM_MBHL_COMP_LOW_MAKEUP_I32:
            if (gParamScale < 0.0640 || gParamScale > 15.8479) {
                LOG("Vritualx: mbhl compressor low makeup gParamValue = %f invalid\n", gParamScale);
                return -1;
            }
            gVirtualxParam[gParamIndex].f = gParamScale;
            gAudioEffect->setParameter(&gVirtualxParam[gParamIndex].param);
            //gAudioEffect->getParameter(&gVirtualxParam[gParamIndex].param);
            LOG("mbhl compressor low makeup is %f \n",gVirtualxParam[gParamIndex].f);
            return 0;
        case DTS_PARAM_MBHL_COMP_MID_RELEASE_I32:
            if (gParamValue < 50 || gParamValue > 2000) {
                LOG("Vritualx: mbhl compressor mid release time gParamValue = %d invalid\n", gParamValue);
                return -1;
            }
            gVirtualxParam[gParamIndex].v = gParamValue;
            gAudioEffect->setParameter(&gVirtualxParam[gParamIndex].param);
            //gAudioEffect->getParameter(&gVirtualxParam[gParamIndex].param);
            LOG("mbhl compressor mid release time is %d \n",gVirtualxParam[gParamIndex].v);
            return 0;
        case DTS_PARAM_MBHL_COMP_MID_RATIO_I32:
             if (gParamScale < 1.0 || gParamScale > 20.0) {
                LOG("Vritualx: mbhl compressor mid ratio gParamValue = %f invalid\n", gParamScale);
                return -1;
            }
            gVirtualxParam[gParamIndex].f = gParamScale;
            gAudioEffect->setParameter(&gVirtualxParam[gParamIndex].param);
            //gAudioEffect->getParameter(&gVirtualxParam[gParamIndex].param);
            LOG("mbhl compressor mid ratio is %f \n",gVirtualxParam[gParamIndex].f);
            return 0;
        case DTS_PARAM_MBHL_COMP_MID_THRESH_I32:
            if (gParamScale < 0.0640 || gParamScale > 15.8479) {
                LOG("Vritualx: mbhl compressor mid threshold gParamValue = %f invalid\n", gParamScale);
                return -1;
            }
            gVirtualxParam[gParamIndex].f = gParamScale;
            gAudioEffect->setParameter(&gVirtualxParam[gParamIndex].param);
            //gAudioEffect->getParameter(&gVirtualxParam[gParamIndex].param);
            LOG("mbhl compressor mid threshold is %f \n",gVirtualxParam[gParamIndex].f);
            return 0;
        case DTS_PARAM_MBHL_COMP_MID_MAKEUP_I32:
             if (gParamScale < 0.0640 || gParamScale > 15.8479) {
                LOG("Vritualx: mbhl compressor mid makeup gParamValue = %f invalid\n", gParamScale);
                return -1;
            }
            gVirtualxParam[gParamIndex].f = gParamScale;
            gAudioEffect->setParameter(&gVirtualxParam[gParamIndex].param);
            //gAudioEffect->getParameter(&gVirtualxParam[gParamIndex].param);
            LOG("mbhl compressor mid makeup is %f \n",gVirtualxParam[gParamIndex].f);
            return 0;
        case DTS_PARAM_MBHL_COMP_HIGH_RELEASE_I32:
            if (gParamValue < 50 || gParamValue > 2000) {
                LOG("Vritualx: mbhl compressor high release time gParamValue = %d invalid\n", gParamValue);
                return -1;
            }
            gVirtualxParam[gParamIndex].v = gParamValue;
            gAudioEffect->setParameter(&gVirtualxParam[gParamIndex].param);
            //gAudioEffect->getParameter(&gVirtualxParam[gParamIndex].param);
            LOG("mbhl compressor high release time is %d \n",gVirtualxParam[gParamIndex].v);
            return 0;
        case DTS_PARAM_MBHL_COMP_HIGH_RATIO_I32:
            if (gParamScale < 1.0 || gParamScale > 20.0) {
                LOG("Vritualx: mbhl compressor high ratio gParamValue = %f invalid\n", gParamScale);
                return -1;
            }
            gVirtualxParam[gParamIndex].f = gParamScale;
            gAudioEffect->setParameter(&gVirtualxParam[gParamIndex].param);
            //gAudioEffect->getParameter(&gVirtualxParam[gParamIndex].param);
            LOG("mbhl compressor high ratio is %f \n",gVirtualxParam[gParamIndex].f);
            return 0;
        case DTS_PARAM_MBHL_COMP_HIGH_THRESH_I32:
            if (gParamScale < 0.0640 || gParamScale > 15.8479) {
                LOG("Vritualx: mbhl compressor high threshold gParamValue = %f invalid\n", gParamScale);
                return -1;
            }
            gVirtualxParam[gParamIndex].f = gParamScale;
            gAudioEffect->setParameter(&gVirtualxParam[gParamIndex].param);
            //gAudioEffect->getParameter(&gVirtualxParam[gParamIndex].param);
            LOG("mbhl compressor high threshold is %f \n",gVirtualxParam[gParamIndex].f);
            return 0;
        case DTS_PARAM_MBHL_COMP_HIGH_MAKEUP_I32:
             if (gParamScale < 0.0640 || gParamScale > 15.8479) {
                LOG("Vritualx: mbhl compressor high makeup gParamValue = %f invalid\n", gParamScale);
                return -1;
            }
            gVirtualxParam[gParamIndex].f = gParamScale;
            gAudioEffect->setParameter(&gVirtualxParam[gParamIndex].param);
            //gAudioEffect->getParameter(&gVirtualxParam[gParamIndex].param);
            LOG("mbhl compressor high makeup is %f \n",gVirtualxParam[gParamIndex].f);
            return 0;
        case DTS_PARAM_TBHDX_ENABLE_I32:
            if (gParamValue < 0 || gParamValue > 1) {
                LOG("Vritualx: tbhdx enable gParamValue = %d invalid\n", gParamValue);
                return -1;
            }
            gVirtualxParam[gParamIndex].v = gParamValue;
            gAudioEffect->setParameter(&gVirtualxParam[gParamIndex].param);
            //gAudioEffect->getParameter(&gVirtualxParam[gParamIndex].param);
            LOG("tbhdx enable is %d \n",gVirtualxParam[gParamIndex].v);
            return 0;
        case DTS_PARAM_TBHDX_MONO_MODE_I32:
            if (gParamValue < 0 || gParamValue > 1) {
                LOG("Vritualx: tbhdx mono mode gParamValue = %d invalid\n", gParamValue);
                return -1;
            }
            gVirtualxParam[gParamIndex].v = gParamValue;
            gAudioEffect->setParameter(&gVirtualxParam[gParamIndex].param);
            //gAudioEffect->getParameter(&gVirtualxParam[gParamIndex].param);
            LOG("tbhdx mono mode is %d \n",gVirtualxParam[gParamIndex].v);
            return 0;
        case DTS_PARAM_TBHDX_SPKSIZE_I32:
            if (gParamValue < 0 || gParamValue > 12) {
                LOG("Vritualx: tbhdx spksize gParamValue = %d invalid\n", gParamValue);
                return -1;
            }
            gVirtualxParam[gParamIndex].v = gParamValue;
            gAudioEffect->setParameter(&gVirtualxParam[gParamIndex].param);
            //gAudioEffect->getParameter(&gVirtualxParam[gParamIndex].param);
            LOG("tbhdx spksize is %d \n",gVirtualxParam[gParamIndex].v);
            return 0;
        case DTS_PARAM_TBHDX_TEMP_GAIN_I32:
            if (gParamScale < 0.0 || gParamScale > 1.0) {
                LOG("Vritualx: tbhdx temp gain gParamValue = %f invalid\n", gParamScale);
                return -1;
            }
            gVirtualxParam[gParamIndex].f = gParamScale;
            gAudioEffect->setParameter(&gVirtualxParam[gParamIndex].param);
            //gAudioEffect->getParameter(&gVirtualxParam[gParamIndex].param);
            LOG("tbhdx temp gain is %f \n",gVirtualxParam[gParamIndex].f);
            return 0;
        case DTS_PARAM_TBHDX_MAXGAIN_I32:
            if (gParamScale < 0.0 || gParamScale > 1.0) {
                LOG("Vritualx: tbhdx max gain gParamValue = %f invalid\n", gParamScale);
                return -1;
            }
            gVirtualxParam[gParamIndex].f = gParamScale;
            gAudioEffect->setParameter(&gVirtualxParam[gParamIndex].param);
            //gAudioEffect->getParameter(&gVirtualxParam[gParamIndex].param);
            LOG("tbhdx max gain is %f \n",gVirtualxParam[gParamIndex].f);
            return 0;
        case DTS_PARAM_TBHDX_PROCESS_DISCARD_I32:
            if (gParamValue < 0 || gParamValue > 1) {
                LOG("Vritualx: tbhdx process discard gParamValue = %d invalid\n", gParamValue);
                return -1;
            }
            gVirtualxParam[gParamIndex].v = gParamValue;
            gAudioEffect->setParameter(&gVirtualxParam[gParamIndex].param);
           //gAudioEffect->getParameter(&gVirtualxParam[gParamIndex].param);
            LOG("tbhdx process discard is %d \n",gVirtualxParam[gParamIndex].v);
            return 0;
        case DTS_PARAM_TBHDX_HPORDER_I32:
            if (gParamValue < 0 || gParamValue > 8) {
                LOG("Vritualx: tbhdx high pass filter order gParamValue = %d invalid\n", gParamValue);
                return -1;
            }
            gVirtualxParam[gParamIndex].v = gParamValue;
            gAudioEffect->setParameter(&gVirtualxParam[gParamIndex].param);
            //gAudioEffect->getParameter(&gVirtualxParam[gParamIndex].param);
            LOG("tbhdx high pass filter order is %d \n",gVirtualxParam[gParamIndex].v);
            return 0;
        case DTS_PARAM_TBHDX_HP_ENABLE_I32:
            if (gParamValue < 0 || gParamValue > 1) {
                LOG("Vritualx: tbhdx high pass enable gParamValue = %d invalid\n", gParamValue);
                return -1;
            }
            gVirtualxParam[gParamIndex].v = gParamValue;
            gAudioEffect->setParameter(&gVirtualxParam[gParamIndex].param);
            //gAudioEffect->getParameter(&gVirtualxParam[gParamIndex].param);
            LOG("tbhdx high pass enable is %d \n",gVirtualxParam[gParamIndex].v);
            return 0;
        case DTS_PARAM_VX_ENABLE_I32:
            if (gParamValue < 0 || gParamValue > 1) {
                LOG("Vritualx: vxlib1 enable gParamValue = %d invalid\n", gParamValue);
                return -1;
            }
            gVirtualxParam[gParamIndex].v = gParamValue;
            gAudioEffect->setParameter(&gVirtualxParam[gParamIndex].param);
            //gAudioEffect->getParameter(&gVirtualxParam[gParamIndex].param);
            LOG("vxlib1 enable is %d \n",gVirtualxParam[gParamIndex].v);
            return 0;
        case DTS_PARAM_VX_INPUT_MODE_I32:
            if (gParamValue < 0 || gParamValue > 4) {
                LOG("Vritualx: vxlib1 input mode gParamValue = %d invalid\n", gParamValue);
                return -1;
            }
            gVirtualxParam[gParamIndex].v = gParamValue;
            gAudioEffect->setParameter(&gVirtualxParam[gParamIndex].param);
            //gAudioEffect->getParameter(&gVirtualxParam[gParamIndex].param);
            LOG("vxlib1 input mode is %d \n",gVirtualxParam[gParamIndex].v);
            return 0;
        case DTS_PARAM_VX_HEADROOM_GAIN_I32:
            if (gParamScale < 0.1250 || gParamScale > 1.0) {
                LOG("Vritualx: vxlib1 headroom gain gParamValue = %f invalid\n", gParamScale);
                return -1;
            }
            gVirtualxParam[gParamIndex].f = gParamScale;
            gAudioEffect->setParameter(&gVirtualxParam[gParamIndex].param);
            //gAudioEffect->getParameter(&gVirtualxParam[gParamIndex].param);
            LOG("vxlib1 headroom gain is %f \n",gVirtualxParam[gParamIndex].f);
            return 0;
        case DTS_PARAM_VX_PROC_OUTPUT_GAIN_I32:
            if (gParamScale < 0.5 || gParamScale > 4.0) {
                LOG("Vritualx: vxlib1 output gain gParamValue = %f invalid\n", gParamScale);
                return -1;
            }
            gVirtualxParam[gParamIndex].f = gParamScale;
            gAudioEffect->setParameter(&gVirtualxParam[gParamIndex].param);
            //gAudioEffect->getParameter(&gVirtualxParam[gParamIndex].param);
            LOG("vxlib1 output gain is %f \n",gVirtualxParam[gParamIndex].f);
            return 0;
        case DTS_PARAM_TSX_ENABLE_I32:
            if (gParamValue < 0 || gParamValue > 1) {
                LOG("Vritualx: tsx enable gParamValue = %d invalid\n", gParamValue);
                return -1;
            }
            gVirtualxParam[gParamIndex].v = gParamValue;
            gAudioEffect->setParameter(&gVirtualxParam[gParamIndex].param);
            //gAudioEffect->getParameter(&gVirtualxParam[gParamIndex].param);
            LOG("vxlib1 tsx enable is %d \n",gVirtualxParam[gParamIndex].v);
            return 0;
        case DTS_PARAM_TSX_PASSIVEMATRIXUPMIX_ENABLE_I32:
            if (gParamValue < 0 || gParamValue > 1) {
                LOG("Vritualx: tsx passive matrix upmixer enable gParamValue = %d invalid\n", gParamValue);
                return -1;
            }
            gVirtualxParam[gParamIndex].v = gParamValue;
            gAudioEffect->setParameter(&gVirtualxParam[gParamIndex].param);
            //gAudioEffect->getParameter(&gVirtualxParam[gParamIndex].param);
            LOG("vxlib1 tsx passive matrix upmixer enable is %d \n",gVirtualxParam[gParamIndex].v);
            return 0;
        case DTS_PARAM_TSX_HORIZ_VIR_EFF_CTRL_I32:
            if (gParamValue < 0 || gParamValue > 1) {
                LOG("Vritualx: tsx horizontal Effect gParamValue = %d invalid\n", gParamValue);
                return -1;
            }
            gVirtualxParam[gParamIndex].v = gParamValue;
            gAudioEffect->setParameter(&gVirtualxParam[gParamIndex].param);
            //gAudioEffect->getParameter(&gVirtualxParam[gParamIndex].param);
            LOG("vxlib1 tsx horizontal Effect is %d \n",gVirtualxParam[gParamIndex].v);
            return 0;
        case DTS_PARAM_TSX_FRNT_CTRL_I32:
            if (gParamScale < 0.5 || gParamScale > 2.0) {
                LOG("Vritualx: tsx frnt ctrl gParamValue = %f invalid\n", gParamScale);
                return -1;
            }
            gVirtualxParam[gParamIndex].f = gParamScale;
            gAudioEffect->setParameter(&gVirtualxParam[gParamIndex].param);
            //gAudioEffect->getParameter(&gVirtualxParam[gParamIndex].param);
            LOG("tsx frnt ctrl is %f \n",gVirtualxParam[gParamIndex].f);
            return 0;
        case DTS_PARAM_TSX_SRND_CTRL_I32:
            if (gParamScale < 0.5 || gParamScale > 2.0) {
                LOG("Vritualx: tsx srnd ctrl gParamValue = %f invalid\n", gParamScale);
                return -1;
            }
            gVirtualxParam[gParamIndex].f = gParamScale;
            gAudioEffect->setParameter(&gVirtualxParam[gParamIndex].param);
            //gAudioEffect->getParameter(&gVirtualxParam[gParamIndex].param);
            LOG("tsx srnd ctrl is %f \n",gVirtualxParam[gParamIndex].f);
            return 0;
        case DTS_PARAM_TSX_LPR_GAIN_I32:
             if (gParamScale < 0.0 || gParamScale > 2.0) {
                LOG("Vritualx: tsx lprtoctr mix gain gParamValue = %f invalid\n", gParamScale);
                return -1;
            }
            gVirtualxParam[gParamIndex].f = gParamScale;
            gAudioEffect->setParameter(&gVirtualxParam[gParamIndex].param);
            //gAudioEffect->getParameter(&gVirtualxParam[gParamIndex].param);
            LOG("tsx lprtoctr mix gain is %f \n",gVirtualxParam[gParamIndex].f);
            return 0;
        case DTS_PARAM_TSX_HEIGHTMIX_COEFF_I32:
             if (gParamScale < 0.5 || gParamScale > 2.0) {
                LOG("Vritualx: tsx heightmix coeff gParamValue = %f invalid\n", gParamScale);
                return -1;
            }
            gVirtualxParam[gParamIndex].f = gParamScale;
            gAudioEffect->setParameter(&gVirtualxParam[gParamIndex].param);
            //gAudioEffect->getParameter(&gVirtualxParam[gParamIndex].param);
            LOG("tsx heightmix coeff is %f \n",gVirtualxParam[gParamIndex].f);
            return 0;
        case DTS_PARAM_TSX_CENTER_GAIN_I32:
             if (gParamScale < 1.0 || gParamScale > 2.0) {
                LOG("Vritualx: tsx center gain gParamValue = %f invalid\n", gParamScale);
                return -1;
            }
            gVirtualxParam[gParamIndex].f = gParamScale;
            gAudioEffect->setParameter(&gVirtualxParam[gParamIndex].param);
            //gAudioEffect->getParameter(&gVirtualxParam[gParamIndex].param);
            LOG("tsx center gain is %f \n",gVirtualxParam[gParamIndex].f);
            return 0;
        case DTS_PARAM_TSX_HEIGHT_DISCARD_I32:
            if (gParamValue < 0 || gParamValue > 1) {
                LOG("Vritualx: tsx height discard gParamValue = %d invalid\n", gParamValue);
                return -1;
            }
            gVirtualxParam[gParamIndex].v = gParamValue;
            gAudioEffect->setParameter(&gVirtualxParam[gParamIndex].param);
            //gAudioEffect->getParameter(&gVirtualxParam[gParamIndex].param);
            LOG("tsx height discard is %d \n",gVirtualxParam[gParamIndex].v);
            return 0;
        case DTS_PARAM_TSX_PROCESS_DISCARD_I32:
            if (gParamValue < 0 || gParamValue > 1) {
                LOG("Vritualx: tsx process discard gParamValue = %d invalid\n", gParamValue);
                return -1;
            }
            gVirtualxParam[gParamIndex].v = gParamValue;
            gAudioEffect->setParameter(&gVirtualxParam[gParamIndex].param);
            //gAudioEffect->getParameter(&gVirtualxParam[gParamIndex].param);
            LOG("tsx process discard is %d \n",gVirtualxParam[gParamIndex].v);
            return 0;
        case DTS_PARAM_TSX_HEIGHT_UPMIX_ENABLE_I32:
            if (gParamValue < 0 || gParamValue > 1) {
                LOG("Vritualx: tsx height upmix enable gParamValue = %d invalid\n", gParamValue);
                return -1;
            }
            gVirtualxParam[gParamIndex].v = gParamValue;
            gAudioEffect->setParameter(&gVirtualxParam[gParamIndex].param);
            //gAudioEffect->getParameter(&gVirtualxParam[gParamIndex].param);
            LOG("tsx height upmix enable is %d \n",gVirtualxParam[gParamIndex].v);
            return 0;
        case DTS_PARAM_VX_DC_ENABLE_I32:
            if (gParamValue < 0 || gParamValue > 1) {
                LOG("Vritualx: tsx dc enable gParamValue = %d invalid\n", gParamValue);
                return -1;
            }
            gVirtualxParam[gParamIndex].v = gParamValue;
            gAudioEffect->setParameter(&gVirtualxParam[gParamIndex].param);
            //gAudioEffect->getParameter(&gVirtualxParam[gParamIndex].param);
            LOG("tsx dc enable is %d \n",gVirtualxParam[gParamIndex].v);
            return 0;
        case DTS_PARAM_VX_DC_CONTROL_I32:
             if (gParamScale < 0.0 || gParamScale > 1.0) {
                LOG("Vritualx: tsx dc level gParamValue = %f invalid\n", gParamScale);
                return -1;
            }
            gVirtualxParam[gParamIndex].f = gParamScale;
            gAudioEffect->setParameter(&gVirtualxParam[gParamIndex].param);
            //gAudioEffect->getParameter(&gVirtualxParam[gParamIndex].param);
            LOG("tsx dc level is %f \n",gVirtualxParam[gParamIndex].f);
            return 0;
        case DTS_PARAM_VX_DEF_ENABLE_I32:
            if (gParamValue < 0 || gParamValue > 1) {
                LOG("Vritualx: tsx def enable gParamValue = %d invalid\n", gParamValue);
                return -1;
            }
            gVirtualxParam[gParamIndex].v = gParamValue;
            gAudioEffect->setParameter(&gVirtualxParam[gParamIndex].param);
            //gAudioEffect->getParameter(&gVirtualxParam[gParamIndex].param);
            LOG("tsx def enable is %d \n",gVirtualxParam[gParamIndex].v);
            return 0;
        case DTS_PARAM_VX_DEF_CONTROL_I32:
             if (gParamScale < 0.0 || gParamScale > 1.0) {
                LOG("Vritualx: tsx def level gParamValue = %f invalid\n", gParamScale);
                return -1;
            }
            gVirtualxParam[gParamIndex].f = gParamScale;
            gAudioEffect->setParameter(&gVirtualxParam[gParamIndex].param);
            //gAudioEffect->getParameter(&gVirtualxParam[gParamIndex].param);
            LOG("tsx def level is %f \n",gVirtualxParam[gParamIndex].f);
            return 0;
        case DTS_PARAM_LOUDNESS_CONTROL_ENABLE_I32:
            if (gParamValue < 0 || gParamValue > 1) {
                LOG("loudness control = %d invalid\n", gParamValue);
                return -1;
            }
            gVirtualxParam[gParamIndex].v = gParamValue;
            gAudioEffect->setParameter(&gVirtualxParam[gParamIndex].param);
            //gAudioEffect->getParameter(&gVirtualxParam[gParamIndex].param);
            LOG("loudness control is %d \n",gVirtualxParam[gParamIndex].v);
            return 0;
        case DTS_PARAM_LOUDNESS_CONTROL_TARGET_LOUDNESS_I32:
            if (gParamValue < -40 || gParamValue > 0) {
                LOG("loudness control target = %d invalid\n", gParamValue);
                return -1;
            }
            gVirtualxParam[gParamIndex].v = gParamValue;
            gAudioEffect->setParameter(&gVirtualxParam[gParamIndex].param);
            //gAudioEffect->getParameter(&gVirtualxParam[gParamIndex].param);
            LOG("loudness control target is %d \n",gVirtualxParam[gParamIndex].v);
            return 0;
        case DTS_PARAM_LOUDNESS_CONTROL_PRESET_I32:
            if (gParamValue < 0 || gParamValue > 2) {
                LOG("loudness control preset = %d invalid\n", gParamValue);
                return -1;
            }
            gVirtualxParam[gParamIndex].v = gParamValue;
            gAudioEffect->setParameter(&gVirtualxParam[gParamIndex].param);
            //gAudioEffect->getParameter(&gVirtualxParam[gParamIndex].param);
            LOG("loudness control preset is %d \n",gVirtualxParam[gParamIndex].v);
            return 0;
        case DTS_PARAM_TBHDX_APP_SPKSIZE_I32:
            if (gParamValue < 40 || gParamValue > 600) {
                LOG("app spksize = %d invaild\n",gParamValue);
                return -1;
            }
            gVirtualxParam[gParamIndex].v = gParamValue;
            gAudioEffect->setParameter(&gVirtualxParam[gParamIndex].param);
            LOG("app spksize %d\n",gVirtualxParam[gParamIndex].v);
            return 0;
        case DTS_PARAM_TBHDX_APP_HPRATIO_F32:
            if (gParamScale < 0.0 || gParamScale > 1.0) {
                LOG("app hpratio = %f invaild\n",gParamScale);
                return -1;
            }
            gVirtualxParam[gParamIndex].f = gParamScale;
            gAudioEffect->setParameter(&gVirtualxParam[gParamIndex].param);
            LOG("app hpratio is %f\n",gVirtualxParam[gParamIndex].f);
            return 0;
        case DTS_PARAM_TBHDX_APP_EXTBASS_F32:
            if (gParamScale < 0.0 || gParamScale > 1.0) {
                LOG("app ettbass = %f invaild\n",gParamScale);
                return -1;
            }
            gVirtualxParam[gParamIndex].f = gParamScale;
            gAudioEffect->setParameter(&gVirtualxParam[gParamIndex].param);
            LOG("app ettbass is %f\n",gVirtualxParam[gParamIndex].f);
            return 0;
        case DTS_PARAM_MBHL_APP_FRT_LOWCROSS_F32:
            if (gParamScale < 40 || gParamScale > 8000.0) {
                LOG("app low freq = %f invaild\n",gParamScale);
                return -1;
            }
             gVirtualxParam[gParamIndex].f = gParamScale;
            rc =gAudioEffect->setParameter(&gVirtualxParam[gParamIndex].param);
            LOG("rc is %d\n",rc);
            LOG("app low freq is %f\n",gVirtualxParam[gParamIndex].f);
            return 0;
        case DTS_PARAM_MBHL_APP_FRT_MIDCROSS_F32:
            if (gParamScale < 40.0 || gParamScale > 8000.0) {
                LOG("app mid freq = %f invaild\n",gParamScale);
                return -1;
            }
            gVirtualxParam[gParamIndex].f = gParamScale;
            gAudioEffect->setParameter(&gVirtualxParam[gParamIndex].param);
            LOG("app mid freq is %f\n",gVirtualxParam[gParamIndex].f);
            return 0;
    default:
        LOG("Virtualx: ParamIndex = %d invalid\n", gParamIndex);
        return -1;
    }
}

static int HPEQ_effect_func(AudioEffect* gAudioEffect, int gParamIndex, int gParamValue, signed char gParamBand[5])
{
    switch (gParamIndex) {
    case HPEQ_PARAM_ENABLE:
         if (gParamValue < 0 || gParamValue > 1) {
            LOG("HPEQ: Status gParamValue = %d invalid\n", gParamValue);
            return -1;
        }
        gHPEQParam[gParamIndex].v = gParamValue;
        gAudioEffect->setParameter(&gHPEQParam[gParamIndex].param);
        gAudioEffect->getParameter(&gHPEQParam[gParamIndex].param);
        LOG("HPEQ: Status is %d -> %s\n", gParamValue, HPEQStatusstr[gHPEQParam[gParamIndex].v]);
        return 0;
    case HPEQ_PARAM_EFFECT_MODE:
        if (gParamValue < 0 || gParamValue > 6) {
            LOG("Hpeq:gParamValue = %d invalid\n", gParamValue);
            return -1;
        }
        gHPEQParam[gParamIndex].v = gParamValue;
        gAudioEffect->setParameter(&gHPEQParam[gParamIndex].param);
        gAudioEffect->getParameter(&gHPEQParam[gParamIndex].param);
        LOG("HPEQ: mode is %d -> %d\n", gParamValue, gHPEQParam[gParamIndex].v);
        return 0;
    case HPEQ_PARAM_EFFECT_CUSTOM:
        for (int i = 0; i < 5; i++) {
           if (gParamBand[i]< -10 || gParamBand[i] >10) {
              LOG("Hpeq:gParamBand[%d] = %d invalid\n",i, gParamBand[i]);
              return -1;
           }
        }
        gHPEQParam[gParamIndex].band[0] = gParamBand[0];
        gHPEQParam[gParamIndex].band[1] = gParamBand[1];
        gHPEQParam[gParamIndex].band[2] = gParamBand[2];
        gHPEQParam[gParamIndex].band[3] = gParamBand[3];
        gHPEQParam[gParamIndex].band[4] = gParamBand[4];
        gAudioEffect->setParameter(&gHPEQParam[gParamIndex].param);
        gAudioEffect->getParameter(&gHPEQParam[gParamIndex].param);
        return 0;
    default:
        LOG("HPEQ: ParamIndex = %d invalid\n", gParamIndex);
        return -1;
    }
}

static int DBX_effect_func(AudioEffect* gAudioEffect, int gParamIndex, int gParamValue, signed char gMode[3])
{
    int rc = 0;
    switch (gParamIndex) {
    case DBX_PARAM_ENABLE:
        if (gParamValue < 0 || gParamValue > 1) {
            LOG("DBX: Status gParamValue = %d invalid\n", gParamValue);
            return -1;
        }
        gdbxtvparam[gParamIndex].v = gParamValue;
        gAudioEffect->setParameter(&gdbxtvparam[gParamIndex].param);
        LOG("DBX: Status is %d -> %s\n", gParamValue, DBXStatusstr[gdbxtvparam[gParamIndex].v]);
        return 0;
     case DBX_SET_MODE:
        gdbxtvparam[gParamIndex].mode[0] = gMode[0];
        LOG("DBX:mode[0] is %d\n",gdbxtvparam[gParamIndex].mode[0]);
        gdbxtvparam[gParamIndex].mode[1] = gMode[1];
        LOG("DBX:mode[1] is %d\n",gdbxtvparam[gParamIndex].mode[1]);
        gdbxtvparam[gParamIndex].mode[2] = gMode[2];
        LOG("DBX:mode[2] is %d\n",gdbxtvparam[gParamIndex].mode[2]);
        rc = gAudioEffect->setParameter(&gdbxtvparam[gParamIndex].param);
        LOG("rc is %d\n",rc);
        return 0;
    default:
        LOG("DBX: ParamIndex = %d invalid\n", gParamIndex);
        return -1;
    }
}
static int GEQ_effect_func(AudioEffect* gAudioEffect, int gParamIndex, int gParamValue, signed char gParamBands[9])
{
    switch (gParamIndex) {
    case GEQ_PARAM_ENABLE:
         if (gParamValue < 0 || gParamValue > 1) {
            LOG("GEQ: Status gParamValue = %d invalid\n", gParamValue);
            return -1;
        }
        gGEQParam[gParamIndex].v = gParamValue;
        gAudioEffect->setParameter(&gGEQParam[gParamIndex].param);
        gAudioEffect->getParameter(&gGEQParam[gParamIndex].param);
        LOG("GEQ: Status is %d -> %s\n", gParamValue, GEQStatusstr[gGEQParam[gParamIndex].v]);
        return 0;
    case GEQ_PARAM_EFFECT_MODE:
        if (gParamValue < 0 || gParamValue > 6) {
            LOG("Hpeq:gParamValue = %d invalid\n", gParamValue);
            return -1;
        }
        gGEQParam[gParamIndex].v = gParamValue;
        gAudioEffect->setParameter(&gGEQParam[gParamIndex].param);
        gAudioEffect->getParameter(&gGEQParam[gParamIndex].param);
        LOG("GEQ: mode is %d -> %d\n", gParamValue, gGEQParam[gParamIndex].v);
        return 0;
    case GEQ_PARAM_EFFECT_CUSTOM:
        for (int i = 0; i < 9; i++) {
           if (gParamBands[i]< -10 || gParamBands[i] >10) {
              LOG("Geq:gParamBands[%d] = %d invalid\n",i, gParamBands[i]);
              return -1;
           }
        }
        gGEQParam[gParamIndex].band[0] = gParamBands[0];
        gGEQParam[gParamIndex].band[1] = gParamBands[1];
        gGEQParam[gParamIndex].band[2] = gParamBands[2];
        gGEQParam[gParamIndex].band[3] = gParamBands[3];
        gGEQParam[gParamIndex].band[4] = gParamBands[4];
        gGEQParam[gParamIndex].band[5] = gParamBands[5];
        gGEQParam[gParamIndex].band[6] = gParamBands[6];
        gGEQParam[gParamIndex].band[7] = gParamBands[7];
        gGEQParam[gParamIndex].band[8] = gParamBands[8];
        gAudioEffect->setParameter(&gGEQParam[gParamIndex].param);
        gAudioEffect->getParameter(&gGEQParam[gParamIndex].param);
        return 0;
    default:
        LOG("GEQ: ParamIndex = %d invalid\n", gParamIndex);
        return -1;
    }
}

static int Avl_effect_func(AudioEffect* gAudioEffect, int gParamIndex, int gParamValue)
{
    switch (gParamIndex) {
    case AVL_PARAM_ENABLE:
         if (gParamValue < 0 || gParamValue > 1) {
            LOG("AVL: Status gParamValue = %d invalid\n", gParamValue);
            return -1;
        }
        gAvlParam[gParamIndex].v = gParamValue;
        gAudioEffect->setParameter(&gAvlParam[gParamIndex].param);
        gAudioEffect->getParameter(&gAvlParam[gParamIndex].param);
        LOG("Avl: Status is %d -> %s\n", gParamValue, AvlStatusstr[gAvlParam[gParamIndex].v]);
        return 0;
    case AVL_PARAM_PEAK_LEVEL:
         if (gParamValue < -40 || gParamValue > 0) {
            LOG("AVL: Status gParamValue = %d invalid\n", gParamValue);
            return -1;
        }
        gAvlParam[gParamIndex].v = gParamValue;
        gAudioEffect->setParameter(&gAvlParam[gParamIndex].param);
        gAudioEffect->getParameter(&gAvlParam[gParamIndex].param);
        LOG("Avl: peak_level is %d -> %d\n", gParamValue, gAvlParam[gParamIndex].v);
        return 0;
    case AVL_PARAM_DYNAMIC_THRESHOLD:
         if (gParamValue < -80 || gParamValue > 0) {
            LOG("AVL: dynamic_threshold = %d invalid\n", gParamValue);
            return -1;
        }
        gAvlParam[gParamIndex].v = gParamValue;
        gAudioEffect->setParameter(&gAvlParam[gParamIndex].param);
        gAudioEffect->getParameter(&gAvlParam[gParamIndex].param);
        LOG("Avl: dynamic_threshold is %d -> %d\n", gParamValue, gAvlParam[gParamIndex].v);
        return 0;
     case AVL_PARAM_NOISE_THRESHOLD:
         if (gParamValue > -10) {
            LOG("AVL: noise_threshold = %d invalid\n", gParamValue);
            return -1;
        }
         gAvlParam[gParamIndex].v = gParamValue * 48;
         gAudioEffect->setParameter(&gAvlParam[gParamIndex].param);
         gAudioEffect->getParameter(&gAvlParam[gParamIndex].param);
         LOG("Avl: noise_threshold is %d -> %d\n", gParamValue, gAvlParam[gParamIndex].v);
         return 0;
    case AVL_PARAM_RESPONSE_TIME:
         if (gParamValue < 20 || gParamValue > 2000) {
            LOG("AVL: Status gParamValue = %d invalid\n", gParamValue);
            return -1;
        }
        gAvlParam[gParamIndex].v = gParamValue;
        gAudioEffect->setParameter(&gAvlParam[gParamIndex].param);
        gAudioEffect->getParameter(&gAvlParam[gParamIndex].param);
        LOG("Avl: Status is %d -> %d\n", gParamValue, gAvlParam[gParamIndex].v);
        return 0;
    case AVL_PARAM_RELEASE_TIME:
         if (gParamValue < 20 || gParamValue > 2000) {
            LOG("AVL: Status gParamValue = %d invalid\n", gParamValue);
            return -1;
        }
        gAvlParam[gParamIndex].v = gParamValue;
        gAudioEffect->setParameter(&gAvlParam[gParamIndex].param);
        gAudioEffect->getParameter(&gAvlParam[gParamIndex].param);
        LOG("Avl: Status is %d -> %d\n", gParamValue, gAvlParam[gParamIndex].v);
        return 0;
    case AVL_PARAM_SOURCE_IN:
        if (gParamValue < 0 || gParamValue > 5) {
            LOG("Avl: source_id gParamValue = %d invalid\n", gParamValue);
            return -1;
        }
        gAvlParam[gParamIndex].v = gParamValue;
        gAudioEffect->setParameter(&gAvlParam[gParamIndex].param);
        gAudioEffect->getParameter(&gAvlParam[gParamIndex].param);
        LOG("Avl: source_id is %d -> %d\n", gParamValue, gAvlParam[gParamIndex].v);
        return 0;
   default:
        LOG("Avl: ParamIndex = %d invalid\n", gParamIndex);
        return -1;
    }
}

static void effectCallback(int32_t event, void* user, void *info)
{
    LOG("%s : %s():line:%d\n", __FILE__, __FUNCTION__, __LINE__);
}

static int create_audio_effect(AudioEffect **gAudioEffect, String16 name16, int index)
{
    status_t status = NO_ERROR;
    AudioEffect *pAudioEffect = NULL;
    audio_session_t gSessionId = AUDIO_SESSION_OUTPUT_MIX;

    if (*gAudioEffect != NULL)
        return 0;

    pAudioEffect = new AudioEffect(name16);
    if (!pAudioEffect) {
        LOG("create audio effect object failed\n");
        return -1;
    }

    status = pAudioEffect->set(NULL,
            &(gEffectStr[index]), // specific uuid
            0, // priority,
            effectCallback,
            NULL, // callback user data
            gSessionId,
            0); // default output device
    if (status != NO_ERROR) {
        LOG("set effect parameters failed\n");
        return -1;
    }

    status = pAudioEffect->initCheck();
    if (status != NO_ERROR) {
        LOG("init audio effect failed\n");
        return -1;
    }

    pAudioEffect->setEnabled(true);
    LOG("effect %d is %s\n", index, pAudioEffect->getEnabled()?"enabled":"disabled");

    *gAudioEffect = pAudioEffect;
    return 0;
}

int main(int argc,char **argv)
{
    int i;
    int ret = -1;
    int gEffectIndex = 0;
    int gParamIndex = 0;
    int gParamValue = 0;
    float gParamScale = 0.0f;
    signed char gmode[3] = {0};
    signed char gParamBand[5] = {0};
    signed char gParamBands[9] = {0};
    status_t status = NO_ERROR;
    String16 name16[EFFECT_MAX] = {String16("AudioEffectEQTest"), String16("AudioEffectSRSTest"), String16("AudioEffectHPEQTest"),
        String16("AudioEffectAVLTest"), String16("AudioEffectGEQTest"),String16("AudioEffectVirtualxTest"),String16("AudioEffectDBXTest")};
    AudioEffect* gAudioEffect[EFFECT_MAX] = {0};
    audio_session_t gSessionId = AUDIO_SESSION_OUTPUT_MIX;
    LOG("**********************************Balance***********************************\n");
    LOG("EffectIndex: 0\n");
    LOG("ParamIndex: 0 -> Level\n");
    LOG("ParamValue: 0 ~ 100\n");
    LOG("ParamIndex: 1 -> Enable\n");
    LOG("ParamValue: 0 -> Disable   1 -> Enable\n");
    LOG("ParamIndex: 4 -> Usb_Balance\n");
    LOG("ParamValue: 0 ~ 100\n");
    LOG("ParamIndex: 5 -> Bt_Banlance\n");
    LOG("ParamValue: 0 ~ 100\n");
    LOG("****************************************************************************\n\n");

    LOG("********************************TruSurround*********************************\n");
    LOG("EffectIndex: 1\n");
    LOG("ParamIndex: 0 -> Mode\n");
    LOG("ParamValue: 0 -> Standard  1 -> Music   2 -> Movie   3 -> ClearVoice   4 -> EnhancedBass   5->Custom\n");
    LOG("ParamIndex: 1 -> DiglogClarity Mode\n");
    LOG("ParamValue: 0 -> OFF 1 -> LOW 2 -> HIGH\n");
    LOG("ParamIndex: 2 -> Surround Mode\n");
    LOG("ParamValue: 0 -> ON 1 -> OFF\n");
    LOG("ParamIndex: 4 -> Enable\n");
    LOG("ParamValue: 0 -> Disable   1 -> Enable\n");
    LOG("ParamIndex: 5 -> TrueBass\n");
    LOG("ParamValue: 0 -> Disable   1 -> Enable\n");
    LOG("ParamIndex: 6 -> TrueBassSpeakSize\n");
    LOG("ParamValue: 0 ~ 7\n");
    LOG("ParamIndex: 7 -> TrueBassGain\n");
    LOG("ParamScale: 0.0 ~ 1.0\n");
    LOG("ParamIndex: 8 -> DiglogClarity\n");
    LOG("ParamValue: 0 -> Disable   1 -> Enable\n");
    LOG("ParamIndex: 9 -> DiglogClarityGain\n");
    LOG("ParamScale: 0.0 ~ 1.0\n");
    LOG("ParamIndex: 10 -> Definition\n");
    LOG("ParamValue: 0 -> Disable   1 -> Enable\n");
    LOG("ParamIndex: 11 -> DefinitionGain\n");
    LOG("ParamScale: 0.0 ~ 1.0\n");
    LOG("ParamIndex: 12 -> Surround\n");
    LOG("ParamValue: 0 -> Disable   1 -> Enable\n");
    LOG("ParamIndex: 13 -> SurroundGain\n");
    LOG("ParamScale: 0.0 ~ 1.0\n");
    LOG("ParamIndex: 14 -> InputGain\n");
    LOG("ParamScale: 0.0 ~ 1.0\n");
    LOG("ParamIndex: 15 -> OuputGain\n");
    LOG("ParamScale: 0.0 ~ 1.0\n");
    LOG("ParamIndex: 16 -> OuputGainCompensation\n");
    LOG("ParamScale: -20.0dB ~ 20.0dB\n");
    LOG("****************************************************************************\n\n");

    LOG("*********************************TrebleBass*********************************\n");
    LOG("EffectIndex: 2\n");
    LOG("ParamIndex: 0 -> Bass\n");
    LOG("ParamValue: 0 ~ 100\n");
    LOG("ParamIndex: 1 -> Treble\n");
    LOG("ParamValue: 0 ~ 100\n");
    LOG("ParamIndex: 2 -> Enable\n");
    LOG("ParamValue: 0 -> Disable   1 -> Enable\n");
    LOG("****************************************************************************\n\n");

    LOG("*********************************HPEQ*********************************\n");
    LOG("EffectIndex: 3\n");
    LOG("ParamIndex: 0 -> Enable\n");
    LOG("ParamValue: 0 -> Disable   1 -> Enable\n");
    LOG("ParamIndex: 1 -> Mode\n");
    LOG("ParamValue: 0 -> Standard  1 -> Music   2 -> news  3 -> movie   4 -> game   5->user\n");
    LOG("ParamIndex: 2 -> custom\n");
    LOG("ParamValue: -10 ~10 \n");

    LOG("*********************************Avl*********************************\n");
    LOG("EffectIndex: 4\n");
    LOG("ParamIndex: 0 -> Enable\n");
    LOG("ParamValue: 0 -> Disable   1 -> Enable\n");
    LOG("ParamIndex: 1 -> Peak Level\n");
    LOG("ParamScale: -40.0 ~ 0.0\n");
    LOG("ParamIndex: 2 -> Dynamic Threshold\n");
    LOG("ParamScale: 0 ~ -80\n");
    LOG("ParamIndex: 3 -> Noise Threshold\n");
    LOG("ParamScale: -NAN ~ -10\n");
    LOG("ParamIndex: 4 -> Response Time\n");
    LOG("ParamValue: 20 ~ 2000\n");
    LOG("ParamIndex: 5 -> Release Time\n");
    LOG("ParamValue: 20 ~ 2000\n");
    LOG("ParamIndex: 6 -> Source In\n");
    LOG("ParamValue: 0 -> DTV  1 -> ATV   2 -> AV  3 -> HDMI   4 -> SPDIF  5->REMOTE_SUBMIX  6->WIRED_HEADSET\n");
    LOG("****************************************************************************\n\n");

    LOG("*********************************GEQ*********************************\n");
    LOG("EffectIndex: 5\n");
    LOG("ParamIndex: 0 -> Enable\n");
    LOG("ParamValue: 0 -> Disable   1 -> Enable\n");
    LOG("ParamIndex: 1 -> Mode\n");
    LOG("ParamValue: 0 -> Standard  1 -> Music   2 -> news  3 -> movie   4 -> game   5->user\n");
    LOG("ParamIndex: 2 -> custom\n");
    LOG("ParamValue: -10 ~10 \n");
    LOG("****************************************************************************\n\n");

    LOG("*********************************Virtualx*********************************\n");
    LOG("EffectIndex: 6\n");
    LOG("ParamIndex: 0 -> Enable\n");
    LOG("ParamValue: 0 -> Disable   1 -> Enable\n");
    LOG("ParamIndex: 3 -> Mbhl Enable\n");
    LOG("ParamValue: 0 -> Disable   1 -> Enable\n");
    LOG("ParamIndex: 4 -> Mbhl Bypass Gain\n");
    LOG("ParamScale: 0.0 ~ 1.0\n");
    LOG("ParamIndex: 5 -> Mbhl Reference Level\n");
    LOG("ParamScale: 0.0009 ~ 1.0\n");
    LOG("ParamIndex: 6 -> Mbhl Volume\n");
    LOG("ParamScale: 0.0 ~ 1.0\n");
    LOG("ParamIndex: 7 -> Mbhl Volume Step\n");
    LOG("ParamValue: 0 ~ 100\n");
    LOG("ParamIndex: 8 -> Mbhl Balance Step\n");
    LOG("ParamValue: -10 ~ 10\n");
    LOG("ParamIndex: 9 -> Mbhl Output Gain\n");
    LOG("ParamScale: 0.0 ~ 1.0\n");
    LOG("ParamIndex: 10 -> Mbhl Mode\n");
    LOG("ParamValue: 0 ~ 4\n");
    LOG("ParamIndex: 11 -> Mbhl process Discard\n");
    LOG("ParamValue: 0 ~ 1\n");
    LOG("ParamIndex: 12 -> Mbhl Cross Low\n");
    LOG("ParamValue: 0 ~ 20\n");
    LOG("ParamIndex: 13 -> Mbhl Cross Mid\n");
    LOG("ParamValue: 0 ~ 20\n");
    LOG("ParamIndex: 14 -> Mbhl Comp Attack\n");
    LOG("ParamValue: 0 ~ 100\n");
    LOG("ParamIndex: 15 -> Mbhl Comp Low Release\n");
    LOG("ParamValue: 50 ~ 2000\n");
    LOG("ParamIndex: 16 -> Mbhl Comp Low Ratio\n");
    LOG("ParamScale: 1.0 ~ 20.0\n");
    LOG("ParamIndex: 17 -> Mbhl Comp Low Thresh\n");
    LOG("ParamScale: 0.0640 ~ 15.8479\n");
    LOG("ParamIndex: 18 -> Mbhl Comp Low Makeup\n");
    LOG("ParamScale: 0.0640 ~ 15.8479\n");
    LOG("ParamIndex: 19 -> Mbhl Comp Mid Release\n");
    LOG("ParamValue: 50 ~ 2000\n");
    LOG("ParamIndex: 20 -> Mbhl Comp Mid Ratio\n");
    LOG("ParamScale: 1.0 ~ 20.0\n");
    LOG("ParamIndex: 21 -> Mbhl Comp Mid Thresh\n");
    LOG("ParamScale: 0.0640 ~ 15.8479\n");
    LOG("ParamIndex: 22 -> Mbhl Comp Mid Makeup\n");
    LOG("ParamScale: 0.0640 ~ 15.8479\n");
    LOG("ParamIndex: 23 -> Mbhl Comp High Release\n");
    LOG("ParamValue: 50 ~ 2000\n");
    LOG("ParamIndex: 24 -> Mbhl Comp High Ratio\n");
    LOG("ParamScale: 1.0 ~ 20.0\n");
    LOG("ParamIndex: 25 -> Mbhl Comp High Thresh\n");
    LOG("ParamScale: 0.0640 ~ 15.8479\n");
    LOG("ParamIndex: 26 -> Mbhl Comp High Makeup\n");
    LOG("ParamScale: 0.0640 ~ 15.8479\n");
    LOG("ParamIndex: 27 -> Mbhl Boost\n");
    LOG("ParamScale: 0.001 ~ 1000\n");
    LOG("ParamIndex: 28 -> Mbhl Threshold\n");
    LOG("ParamScale: 0.0640 ~ 1.0\n");
    LOG("ParamIndex: 29 -> Mbhl Slow Offset\n");
    LOG("ParamScale: 0.317 ~ 3.1619\n");
    LOG("ParamIndex: 30 -> Mbhl Fast Attack\n");
    LOG("ParamScale: 0 ~ 10\n");
    LOG("ParamIndex: 31 -> Mbhl Fast Release\n");
    LOG("ParamValue: 10 ~ 500\n");
    LOG("ParamIndex: 32 -> Mbhl Slow Attack\n");
    LOG("ParamValue: 100 ~ 1000\n");
    LOG("ParamIndex: 33 -> Mbhl Slow Release\n");
    LOG("ParamValue: 100 ~ 2000\n");
    LOG("ParamIndex: 34 -> Mbhl Delay\n");
    LOG("ParamValue: 0 ~ 16\n");
    LOG("ParamIndex: 35 -> Mbhl Envelope Freq\n");
    LOG("ParamValue: 5 ~ 500\n");
    LOG("ParamIndex: 36 -> TBHDX Enable\n");
    LOG("ParamValue: 0 -> Disable   1 -> Enable\n");
    LOG("ParamIndex: 37 -> TBHDX Mono Mode\n");
    LOG("ParamValue: 0 -> Disable   1 -> Enable\n");
    LOG("ParamIndex: 38 -> TBHDX Max Gain\n");
    LOG("ParamScale: 0.0 ~ 1.0\n");
    LOG("ParamIndex: 39 -> TBHDX Spk Size\n");
    LOG("ParamValue: 0 ~ 12\n");
    LOG("ParamIndex: 40 -> TBHDX  HP Enable\n");
    LOG("ParamValue: 0 -> Disable   1 -> Enable\n");
    LOG("ParamIndex: 41 -> TBHDX Temp Gain\n");
    LOG("ParamScale: 0.0 ~ 1.0\n");
    LOG("ParamIndex: 42 -> TBHDX  Process Discard\n");
    LOG("ParamValue: 0 -> Disable   1 -> Enable\n");
    LOG("ParamIndex: 43 -> TBHDX HP Order\n");
    LOG("ParamValue: 1 ~ 8\n");
    LOG("ParamIndex: 44 -> VX Enable\n");
    LOG("ParamValue: 0 -> Disable   1 -> Enable\n");
    LOG("ParamIndex: 45 -> VX Input Mode\n");
    LOG("ParamValue: 0 ~ 4\n");
    LOG("ParamIndex: 47 -> VX Head Room Gain\n");
    LOG("ParamScale: 0.125 ~ 1.0\n");
    LOG("ParamIndex: 48 -> VX Proc Output Gain\n");
    LOG("ParamScale: 0.5 ~ 4.0\n");
    LOG("ParamIndex: 50 -> TSX Enable\n");
    LOG("ParamValue: 0 -> Disable   1 -> Enable\n");
    LOG("ParamIndex: 51 -> TSX Passive Matrix Upmixer Enable\n");
    LOG("ParamValue: 0 -> Disable   1 -> Enable\n");
    LOG("ParamIndex: 52 -> TSX Height Upmixer Enable\n");
    LOG("ParamValue: 0 -> Disable   1 -> Enable\n");
    LOG("ParamIndex: 53 -> TSX Lpr Gain\n");
    LOG("ParamScale: 0.000 ~ 2.0\n");
    LOG("ParamIndex: 54 -> TSX Center Gain\n");
    LOG("ParamScale: 1.0 ~ 2.0\n");
    LOG("ParamIndex: 55 -> TSX Horiz Vir Effect Ctrl\n");
    LOG("ParamValue: 0 -> default   1 -> mild\n");
    LOG("ParamIndex: 56 -> TSX Height Mix Coeff\n");
    LOG("ParamScale: 0.5 ~ 2.0\n");
    LOG("ParamIndex: 57 -> TSX Process Discard\n");
    LOG("ParamValue: 0 -> Disable   1 -> Enable\n");
    LOG("ParamIndex: 58 -> TSX Height Discard\n");
    LOG("ParamValue: 0 -> Disable   1 -> Enable\n");
    LOG("ParamIndex: 59 -> TSX Frnt Ctrl\n");
    LOG("ParamScale: 0.5 ~ 2.0\n");
    LOG("ParamIndex: 60 -> TSX Srnd Ctrl\n");
    LOG("ParamScale: 0.5 ~ 2.0\n");
    LOG("ParamIndex: 61 -> VX DC Enable\n");
    LOG("ParamValue: 0 -> Disable   1 -> Enable\n");
    LOG("ParamIndex: 62 -> VX DC Control\n");
    LOG("ParamScale: 0.5 ~ 2.0\n");
    LOG("ParamIndex: 63 -> VX DEF Enable\n");
    LOG("ParamValue: 0 -> Disable   1 -> Enable\n");
    LOG("ParamIndex: 64 -> VX DEF Control\n");
    LOG("ParamScale: 0.5 ~ 2.0\n");
    LOG("ParamIndex: 65 -> Loudness Control Enable\n");
    LOG("ParamValue: 0 -> Disable   1 -> Enable\n");
    LOG("ParamIndex: 66 -> Loudness Control Target Loudness\n");
    LOG("ParamValue: -40 ~ 0\n");
    LOG("ParamIndex: 67 -> Loudness Control Preset\n");
    LOG("ParamValue: 0 -> light 1 -> mid 2 -> Aggresive \n");
    LOG("ParamIndex: 69 -> tbhd app spksize\n");
    LOG("ParamValue 40 ~ 600 \n");
    LOG("ParamIndex: 70 -> tbhd app hpratio\n");
    LOG("ParamScale 0 ~ 1.0\n");
    LOG("ParamIndex: 71 -> tbhd app extbass\n");
    LOG("ParamScale 0 ~ 1.0\n");
    LOG("ParamIndex: 72 -> mbhl frt lowcross\n");
    LOG("ParamScale 40 ~ 8000\n");
    LOG("ParamIndex: 73 -> mbhl frt midcross\n");
    LOG("ParamScale 40 ~ 8000\n");
    LOG("****************************************************************************\n\n");

    LOG("**********************************DBX***********************************\n");
    LOG("EffectIndex: 7\n");
    LOG("ParamIndex: 0 -> Enable\n");
    LOG("ParamValue: 0 -> Disable   1 -> Enable\n");
    LOG("ParamIndex: 1 -> Choose mode(son_mode--vol_mode--sur_mode)\n");
    LOG("ParamValue: son_mode 0-4 vol_mode  0-2 sur_mode 0-2\n");
    LOG("****************************************************************************\n\n");

   if (argc != 4 && argc != 12 && argc != 8 && argc != 6) {
        LOG("Usage: %s <EffectIndex> <ParamIndex> <ParamValue/ParamScale/gParamBand>\n", argv[0]);
        return -1;
   } else {
        LOG("start...\n");
        sscanf(argv[1], "%d", &gEffectIndex);
        sscanf(argv[2], "%d", &gParamIndex);
        if (gEffectIndex == 1 && (gParamIndex == 7 || gParamIndex == 9 || gParamIndex == 11 || (gParamIndex >= 13 && gParamIndex <= 16)))
            sscanf(argv[3], "%f", &gParamScale);
        else if (gEffectIndex == 3 && gParamIndex == 2) {
            sscanf(argv[3], "%d", &gParamBand[0]);
            sscanf(argv[4], "%d", &gParamBand[1]);
            sscanf(argv[5], "%d", &gParamBand[2]);
            sscanf(argv[6], "%d", &gParamBand[3]);
            sscanf(argv[7], "%d", &gParamBand[4]);
          } else if (gEffectIndex == 5 && gParamIndex == 2) {
            sscanf(argv[3], "%d", &gParamBands[0]);
            sscanf(argv[4], "%d", &gParamBands[1]);
            sscanf(argv[5], "%d", &gParamBands[2]);
            sscanf(argv[6], "%d", &gParamBands[3]);
            sscanf(argv[7], "%d", &gParamBands[4]);
            sscanf(argv[8], "%d", &gParamBands[5]);
            sscanf(argv[9], "%d", &gParamBands[6]);
            sscanf(argv[10], "%d", &gParamBands[7]);
            sscanf(argv[11], "%d", &gParamBands[8]);
      } else if ((gEffectIndex == 6) && ((gParamIndex >= 4 && gParamIndex <= 6) || gParamIndex == 9 || (gParamIndex >= 16 && gParamIndex <= 18)
        ||(gParamIndex >= 20 && gParamIndex <= 22) || (gParamIndex >= 24 && gParamIndex <= 30) || gParamIndex == 38 || gParamIndex == 41
        || gParamIndex == 47 || gParamIndex == 48 || gParamIndex == 53 || gParamIndex == 54 || gParamIndex == 56 || gParamIndex == 59 ||
        gParamIndex == 60 || gParamIndex == 62 || gParamIndex == 64) || (gParamIndex >= 70 && gParamIndex <= 73))  {
            sscanf(argv[3], "%f", &gParamScale);
      }  else if (gEffectIndex == 7 && gParamIndex == 1) {
            sscanf(argv[3], "%d", &gmode[0]);
            sscanf(argv[4], "%d", &gmode[1]);
            sscanf(argv[5], "%d", &gmode[2]);
      } else
            sscanf(argv[3], "%d", &gParamValue);
    }
    if (gEffectIndex >= (int)(sizeof(gEffectStr)/sizeof(gEffectStr[0]))) {
        LOG("Effect is not exist\n");
        return -1;
    }

    if (gEffectIndex == 1 && (gParamIndex == 7 || gParamIndex == 9 || gParamIndex == 11 || (gParamIndex >= 13 && gParamIndex <= 16)))
        LOG("EffectIndex:%d, ParamIndex:%d, ParamScale:%f\n", gEffectIndex, gParamIndex, gParamScale);
    else if (gEffectIndex == 3 && gParamIndex == 2) {
          for (int i = 0; i < 5; i++)
             LOG("EffectIndex:%d, ParamIndex:%d, ParamBand:%d\n", gEffectIndex, gParamIndex, gParamBand[i]);
        } else if (gEffectIndex == 5 && gParamIndex == 2) {
           for (int i = 0; i < 9; i++)
           LOG("EffectIndex:%d, ParamIndex:%d, ParamBand:%d\n", gEffectIndex, gParamIndex, gParamBands[i]);
        } else if (gEffectIndex == 6 && ((gParamIndex >= 4 && gParamIndex <= 6) || gParamIndex == 9 || (gParamIndex >= 16 && gParamIndex <= 18)
        ||(gParamIndex >= 20 && gParamIndex <= 22) || (gParamIndex >= 24 && gParamIndex <= 30) || gParamIndex == 38 || gParamIndex == 41
        || gParamIndex == 47 || gParamIndex == 48 || gParamIndex == 53 || gParamIndex == 54 || gParamIndex == 56 || gParamIndex == 59 ||
        gParamIndex == 60 || gParamIndex == 62 || gParamIndex == 64) || (gParamIndex >= 70 && gParamIndex <= 73) )
             LOG("EffectIndex:%d, ParamIndex:%d, ParamScale:%f\n", gEffectIndex, gParamIndex, gParamScale);
         else if (gEffectIndex == 7 && gParamIndex == 1) {
            for (int i = 0; i < 3; i++)
                LOG("EffectIndex:%d, ParamIndex:%d, ParamMode:%d\n", gEffectIndex, gParamIndex, gmode[i]);
         } else
             LOG("EffectIndex:%d, ParamIndex:%d, Paramvalue:%d\n", gEffectIndex, gParamIndex, gParamValue);
    while (1) {
        switch (gEffectIndex) {
        case EFFECT_BALANCE:
            ret = create_audio_effect(&gAudioEffect[EFFECT_BALANCE], name16[EFFECT_BALANCE], EFFECT_BALANCE);
            if (ret < 0) {
                LOG("create Balance effect failed\n");
                goto Error;
            }
            //------------set Balance parameters---------------------------------------
            if (Balance_effect_func(gAudioEffect[gEffectIndex], gParamIndex, gParamValue) < 0)
                LOG("Balance Test failed\n");
            break;
        case EFFECT_SRS:
            ret = create_audio_effect(&gAudioEffect[EFFECT_SRS], name16[EFFECT_SRS], EFFECT_SRS);
            if (ret < 0) {
                LOG("create TruSurround effect failed\n");
                goto Error;
            }
            //------------set TruSurround parameters-----------------------------------
            if (SRS_effect_func(gAudioEffect[gEffectIndex], gParamIndex, gParamValue, gParamScale) < 0)
                LOG("TruSurround Test failed\n");
            break;
        case EFFECT_TREBLEBASS:
            ret = create_audio_effect(&gAudioEffect[EFFECT_TREBLEBASS], name16[EFFECT_TREBLEBASS], EFFECT_TREBLEBASS);
            if (ret < 0) {
                LOG("create TrebleBass effect failed\n");
                goto Error;
            }
            //------------set TrebleBass parameters------------------------------------
            if (TrebleBass_effect_func(gAudioEffect[gEffectIndex], gParamIndex, gParamValue) < 0)
                LOG("TrebleBass Test failed\n");
            break;
        case EFFECT_HPEQ:
            ret = create_audio_effect(&gAudioEffect[EFFECT_HPEQ], name16[EFFECT_HPEQ], EFFECT_HPEQ);
            if (ret < 0) {
                LOG("create Hpeq effect failed\n");
                goto Error;
            }
            //------------set HPEQ parameters------------------------------------------
            if (HPEQ_effect_func(gAudioEffect[gEffectIndex], gParamIndex, gParamValue,gParamBand) < 0)
                LOG("HPEQ Test failed\n");
            break;
         case EFFECT_GEQ:
             ret = create_audio_effect(&gAudioEffect[EFFECT_GEQ], name16[EFFECT_GEQ], EFFECT_GEQ);
             if (ret < 0) {
                 LOG("create Geq effect failed\n");
                  goto Error;
             }
             //------------set GEQ parameters------------------------------------------
             if (GEQ_effect_func(gAudioEffect[gEffectIndex], gParamIndex, gParamValue,gParamBands) < 0)
                LOG("GEQ Test failed\n");
             break;
         case EFFECT_AVL:
            ret = create_audio_effect(&gAudioEffect[EFFECT_AVL], name16[EFFECT_AVL], EFFECT_AVL);
            if (ret < 0) {
                LOG("create AVl effect failed\n");
                goto Error;
            }
            //------------set Avl parameters-------------------------------------------
            if (Avl_effect_func(gAudioEffect[gEffectIndex], gParamIndex, gParamValue) < 0)
                LOG("Avl Test failed\n");
            break;
         case EFFECT_VIRTUALX:
            ret = create_audio_effect(&gAudioEffect[EFFECT_VIRTUALX], name16[EFFECT_VIRTUALX], EFFECT_VIRTUALX);
            if (ret < 0) {
                LOG("create Virtualx effect failed\n");
                goto Error;
            }
            //------------set Virtualx parameters-------------------------------------------
            if (Virtualx_effect_func(gAudioEffect[gEffectIndex], gParamIndex, gParamValue,gParamScale) < 0)
                LOG("Virtualx Test failed\n");
            break;
        case EFFECT_DBX:
            ret = create_audio_effect(&gAudioEffect[EFFECT_DBX], name16[EFFECT_DBX], EFFECT_DBX);
            if (ret < 0) {
                LOG("create EFFECT_DBX effect failed\n");
                goto Error;
            }
            //------------set DBX parameters------------------------------------------------
            if (DBX_effect_func(gAudioEffect[gEffectIndex], gParamIndex, gParamValue,gmode) < 0)
                LOG("DBX Test failed\n");
            break;
        default:
            LOG("EffectIndex = %d invalid\n", gEffectIndex);
            break;
        }

        LOG("Please enter param: <27> to Exit\n");
        LOG("Please enter param: <EffectIndex> <ParamIndex> <ParamValue/ParamScale>\n");
        scanf("%d", &gEffectIndex);
        if (gEffectIndex == 27) {
            LOG("Exit...\n");
            break;
        }
        scanf("%d", &gParamIndex);
        if (gEffectIndex == 1 && (gParamIndex == 7 || gParamIndex == 9 || gParamIndex == 11 || (gParamIndex >= 13 && gParamIndex <= 16)))
            scanf("%f", &gParamScale);
        else if (gEffectIndex == 3 && gParamIndex == 2) {
             scanf("%d %d %d %d %d",&gParamBand[0],&gParamBand[1],&gParamBand[2],&gParamBand[3],&gParamBand[4]);
        } else if (gEffectIndex == 5 && gParamIndex == 2) {
            scanf("%d %d %d %d %d %d %d %d %d",&gParamBands[0],&gParamBands[1],&gParamBands[2],&gParamBands[3],&gParamBands[4],
            &gParamBands[5],&gParamBands[6],&gParamBands[7],&gParamBands[8]);
        } else if (gEffectIndex == 6 && ((gParamIndex >= 4 && gParamIndex <= 6) || gParamIndex == 9 || (gParamIndex >= 16 && gParamIndex <= 18)
        ||(gParamIndex >= 20 && gParamIndex <= 22) || (gParamIndex >= 24 && gParamIndex <= 30) || gParamIndex == 38 || gParamIndex == 41
        || gParamIndex == 47 || gParamIndex == 48 || gParamIndex == 53 || gParamIndex == 54 || gParamIndex == 56 || gParamIndex == 59 ||
        gParamIndex == 60 || gParamIndex == 62 || gParamIndex == 64) || (gParamIndex >= 70 && gParamIndex <= 73)) {
                scanf("%f", &gParamScale);
            } else if (gEffectIndex == 7 && gParamIndex == 1) {
                scanf("%d %d %d",&gmode[0],&gmode[1],&gmode[2]);
            } else
                scanf("%d", &gParamValue);
        if (gEffectIndex >= (int)(sizeof(gEffectStr)/sizeof(gEffectStr[0]))) {
            LOG("Effect is not exist\n");
            goto Error;
        }
        if (gEffectIndex == 1 && (gParamIndex == 7 || gParamIndex == 9 || gParamIndex == 11 || (gParamIndex >= 13 && gParamIndex <= 16)))
            LOG("EffectIndex:%d, ParamIndex:%d, ParamScale:%f\n", gEffectIndex, gParamIndex, gParamScale);
        else if (gEffectIndex == 6 && ((gParamIndex >= 4 && gParamIndex <= 6) || gParamIndex == 9 || (gParamIndex >= 16 && gParamIndex <= 18)
        ||(gParamIndex >= 20 && gParamIndex <= 22) || (gParamIndex >= 24 && gParamIndex <= 30) || gParamIndex == 38 || gParamIndex == 41
        || gParamIndex == 47 || gParamIndex == 48 || gParamIndex == 53 || gParamIndex == 54 || gParamIndex == 56 || gParamIndex == 59 ||
        gParamIndex == 60 || gParamIndex == 62 || gParamIndex == 64) || (gParamIndex >= 70 && gParamIndex <= 73)) {
             LOG("EffectIndex:%d, ParamIndex:%d, ParamScale:%f\n", gEffectIndex, gParamIndex, gParamScale);
            }
            else
                LOG("EffectIndex:%d, ParamIndex:%d, ParamValue:%d\n", gEffectIndex, gParamIndex, gParamValue);
    }

    ret = 0;
Error:
    for (i = 0; i < EFFECT_MAX; i++) {
        if (gAudioEffect[i] != NULL) {
            delete gAudioEffect[i];
            gAudioEffect[i] = NULL;
        }
    }
    return ret;
}
