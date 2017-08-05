JNI_PATH := $(call my-dir)


LOCAL_PATH := $(JNI_PATH)
include $(CLEAR_VARS)

TARGET_ARCH_ABI := armeabi armeabi-v7a

LOCAL_MODULE := root_automator
LOCAL_SRC_FILES := main.c linux_input_event_util.c
LOCAL_CFLAGS += -fPIE
LOCAL_LDLIBS += -L$(SYSROOT)/usr/lib -llog -fPIE -pie
include $(BUILD_EXECUTABLE)
