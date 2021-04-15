LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_ARM_MODE := arm
LOCAL_MODULE_TAGS := optional

LOCAL_MODULE    := AudioEffectTool

LOCAL_SYSTEM_EXT_MODULE := true
LOCAL_SRC_FILES := main.cpp

LOCAL_SHARED_LIBRARIES := \
    libcutils \
    libutils \
    libaudioclient \
    libmedia \
    libmedia_helper \
    libmediaplayerservice \

# libaudiofoundation support only R and above
ifeq ($(shell test $(PLATFORM_SDK_VERSION) -ge 30 && echo OK), OK)
LOCAL_SHARED_LIBRARIES += \
    libaudiofoundation
endif

ifeq ($(PLATFORM_VERSION), S)
LOCAL_CFLAGS += -DUSE_IDENTITY_CREATE_AUDIOEFFECT
endif

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/../VirtualX \

include $(BUILD_EXECUTABLE)

#for tuning tool
include $(CLEAR_VARS)

LOCAL_ARM_MODE := arm
LOCAL_MODULE_TAGS := optional

LOCAL_MODULE    := TuningTool

LOCAL_SYSTEM_EXT_MODULE := true
LOCAL_SRC_FILES := main2.cpp

LOCAL_SHARED_LIBRARIES := \
    libcutils \
    libutils \
    libaudioclient \
    libmedia \
    libmedia_helper \
    libmediaplayerservice \

# libaudiofoundation support only R and above
ifeq ($(shell test $(PLATFORM_SDK_VERSION) -ge 30 && echo OK), OK)
LOCAL_SHARED_LIBRARIES += \
    libaudiofoundation
endif

ifeq ($(PLATFORM_VERSION), S)
LOCAL_CFLAGS += -DUSE_IDENTITY_CREATE_AUDIOEFFECT
endif
include $(BUILD_EXECUTABLE)
