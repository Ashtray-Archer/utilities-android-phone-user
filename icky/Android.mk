# ICK owns maintained C; NDK tools own upstream glue, assembly and linking.
ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/..)
PROFILE ?= accelerometer
AICI_ICK_ROOT ?= $(ROOT)/.ai-ci-ick
ICK_STAGE_ROOT ?= $(ROOT)/build/ick
ICK_STAGE ?= $(ICK_STAGE_ROOT)/$(ABI)
BUILD ?= $(ROOT)/build/icky-$(PROFILE)/$(ABI)
OUT ?= $(BUILD)/output/$(PROFILE)
PROFILE_API_accelerometer = 26
PROFILE_API_picker = 26
PROFILE_API_hardware = 21
PROFILE_API_clipboard = 24
API ?= $(PROFILE_API_$(PROFILE))
ifeq ($(API),)
$(error PROFILE must be accelerometer, picker, hardware, or clipboard)
endif
ifneq ($(filter $(PROFILE),accelerometer picker),)
FORTIFY_SOURCE = 2
HARDENING = -fstack-protector-strong
endif
include $(AICI_ICK_ROOT)/ick-android/Makefile
.DEFAULT_GOAL := native
NDK_CC = $(NDK_BIN)/$(NDK_TARGET_$(ABI))$(API)-clang
GLUE = $(NDK)/sources/android/native_app_glue
MODE_armeabi-v7a = -marm -march=armv7-a -mfpu=neon -mfloat-abi=softfp
ifeq ($(PROFILE),accelerometer)
MODE_armeabi-v7a = -mthumb -march=armv7-a -mfpu=neon -mfloat-abi=softfp
endif
MODE_arm64-v8a = -ffixed-x18
MODE_x86_64 = -march=x86-64-v2 -mno-avx -mno-movbe
TARGET_FLAGS = $(MODE_$(ABI))
COMMON = -std=c17 -O2 -g -gdwarf-4 -gno-variable-location-views -fPIC -ffunction-sections -fdata-sections
WARN = -Wall -Wextra -Werror -Wpedantic -Wshadow
ifeq ($(PROFILE),picker)
WARN += -Wconversion
endif
SOURCE_accelerometer = accelerometer/app/src/main/c/native_main.c accelerometer/android/android_accelerometer.c
SOURCE_picker = math-characters/app/src/main/c/native_main.c math-characters/app/src/main/c/pad_model.c math-characters/app/src/main/c/pad_ui.c
SOURCE_hardware = hardware/android-native/hardware_sensor.c accelerometer/android/android_accelerometer.c
SOURCE_clipboard = android-clipboard/device-smoke/smoke.c android-clipboard/clipboard_jni.c android-clipboard/utf8.c
INCLUDE_accelerometer = -I"$(ROOT)/accelerometer/app/src/main/c" -I"$(ROOT)/accelerometer/model" -I"$(ROOT)/accelerometer/android"
INCLUDE_picker = -I"$(ROOT)/math-characters/app/src/main/c"
INCLUDE_hardware = -I"$(ROOT)/accelerometer/android" -I"$(ROOT)/accelerometer/model" -I"$(ROOT)/accelerometer/command"
INCLUDE_clipboard = -I"$(ROOT)/android-clipboard"
SOURCES = $(SOURCE_$(PROFILE))
INCLUDES = $(INCLUDE_$(PROFILE)) -isystem "$(GLUE)"
OBJECTS = $(addprefix $(BUILD)/,$(SOURCES:.c=.o))
LIBS_accelerometer = -landroid -llog -lm
LIBS_picker = -landroid -llog
LIBS_hardware = -landroid -lm
LIBS_clipboard = -landroid -llog
LINK_FLAGS = -shared -Wl,--no-undefined -Wl,--gc-sections -Wl,-z,relro,-z,now
ifneq ($(filter $(PROFILE),accelerometer picker),)
OBJECTS += $(BUILD)/upstream/native_app_glue.o
endif
ifeq ($(PROFILE),accelerometer)
LINK_FLAGS += -Wl,-u,ANativeActivity_onCreate
endif
ifeq ($(PROFILE),hardware)
COMMON = -std=c17 -O2 -fPIC
WARN = -Wall -Wextra -Wpedantic -Wno-deprecated-declarations
LINK_FLAGS = -fPIE -pie
endif
ifeq ($(PROFILE),clipboard)
WARN = -Wall -Wextra -Werror -Wpedantic
endif
.PHONY: native check-owned-producer
native: $(OUT)
	"$(NDK_READELF)" -h -r "$(OUT)" > "$(OUT).elf"
	! grep -Eq 'R_[A-Z0-9_]+_COPY' "$(OUT).elf"
	cat "$(OUT).elf"
	sha256sum "$(OUT)"

check-owned-producer:
	test -x "$(ICK_COMPILER)"
	test "$$("$(ICK_COMPILER)" $(ICK_EXTRA_FLAGS) -dumpmachine)" = "$(EXPECTED_TARGET)"
	test -f "$(ICK_BUILTIN_INCLUDE)/stdatomic.h"
	test -f "$(ICK_BUILTIN_INCLUDE)/stddef.h"
	test -x "$(NDK_CC)"

$(BUILD)/%.s: $(ROOT)/%.c FORCE | check-owned-producer
	mkdir -p "$(@D)"
	"$(ICK_COMPILER)" $(ICK_EXTRA_FLAGS) $(ICK_NDK_FLAGS) $(COMMON) $(WARN) $(HARDENING) -D__ANDROID_API__=$(API) -D__ANDROID_MIN_SDK_VERSION__=$(API) $(INCLUDES) -S "$<" -o "$@"

$(BUILD)/%.o: $(BUILD)/%.s
	"$(NDK_CC)" $(TARGET_FLAGS) --sysroot="$(NDK_SYSROOT)" -c "$<" -o "$@"

$(BUILD)/upstream/native_app_glue.o: FORCE | check-owned-producer
	mkdir -p "$(@D)"
	"$(NDK_CC)" $(TARGET_FLAGS) --sysroot="$(NDK_SYSROOT)" -std=c17 -O2 -g -fPIC -ffunction-sections -fdata-sections -isystem "$(GLUE)" -c "$(GLUE)/android_native_app_glue.c" -o "$@"

$(OUT): $(OBJECTS)
	mkdir -p "$(@D)"
	"$(NDK_CC)" $(TARGET_FLAGS) --sysroot="$(NDK_SYSROOT)" $(LINK_FLAGS) $(OBJECTS) $(LIBS_$(PROFILE)) -o "$@"
