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

//-------UUID------------------------------------------
typedef enum {
    EFFECT_BALANCE = 0,
    EFFECT_SRS,
    EFFECT_TREBLEBASS,
    EFFECT_HPEQ,
    EFFECT_AVL,
    EFFECT_GEQ,
    EFFECT_MAX,
} EFFECT_params;

effect_uuid_t gEffectStr[] = {
    {0x6f33b3a0, 0x578e, 0x11e5, 0x892f, {0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b}}, // 0:Balance
    {0x8a857720, 0x0209, 0x11e2, 0xa9d8, {0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b}}, // 1:TruSurround
    {0x76733af0, 0x2889, 0x11e2, 0x81c1, {0x08, 0x00, 0x20, 0x0c, 0x9a, 0x66}}, // 2:TrebleBass
    {0x049754aa, 0xc4cf, 0x439f, 0x897e, {0x37, 0xdd, 0x0c, 0x38, 0x11, 0x20}}, // 3:Hpeq
    {0x08246a2a, 0xb2d3, 0x4621, 0xb804, {0x42, 0xc9, 0xb4, 0x78, 0xeb, 0x9d}}, // 4:Avl
    {0x2e2a5fa6, 0xcae8, 0x45f5, 0xbb70, {0xa2, 0x9c, 0x1f, 0x30, 0x74, 0xb2}}, // 5:Geq
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
    float gParamScale = 0.0;
    signed char gParamBand[5]={0};
    signed char gParamBands[9]={0};
    status_t status = NO_ERROR;
    String16 name16[EFFECT_MAX] = {String16("AudioEffectEQTest"), String16("AudioEffectSRSTest"), String16("AudioEffectHPEQTest"),
        String16("AudioEffectAVLTest"), String16("AudioEffectGEQTest")};
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

   if (argc != 4 && argc != 12 && argc != 8) {
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
        } else
            scanf("%d", &gParamValue);
        if (gEffectIndex >= (int)(sizeof(gEffectStr)/sizeof(gEffectStr[0]))) {
            LOG("Effect is not exist\n");
            goto Error;
        }
        if (gEffectIndex == 1 && (gParamIndex == 7 || gParamIndex == 9 || gParamIndex == 11 || (gParamIndex >= 13 && gParamIndex <= 16)))
            LOG("EffectIndex:%d, ParamIndex:%d, ParamScale:%f\n", gEffectIndex, gParamIndex, gParamScale);
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
