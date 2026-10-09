# Fixed source-production commands; ICK is the only owned-C frontend.
ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/..)
ICK ?= ick
ICK_FLAGS ?= -fno-link-libatomic
BUILD ?= $(ROOT)/build/icky-host
JAVA_HOME ?=
JNI_PLATFORM ?= $(if $(filter Darwin,$(shell uname -s)),darwin,linux)
MODEL_TESTS = compact_acceleration_test error_study accelerometer_snapshot_test inspection_fixture
MODEL_FLAGS = -std=c17 -O2 -Wall -Wextra -Werror -Wpedantic -Wshadow
PICKER_FLAGS = -std=c17 -Wall -Wextra -Werror -Wpedantic -Wconversion -Wshadow -g -fsanitize=address,undefined -fno-omit-frame-pointer
MODEL_HEADERS = $(wildcard $(ROOT)/accelerometer/model/*.h $(ROOT)/accelerometer/command/*.h $(ROOT)/geometry/*.h)
PICKER_HEADERS = $(wildcard $(ROOT)/math-characters/app/src/main/c/*.h $(ROOT)/math-characters/app/src/main/c/*.inc)
.PHONY: model-tests picker-tests clipboard-tests
model-tests: $(BUILD)/compact_unit_direction_test $(addprefix $(BUILD)/,$(MODEL_TESTS))
	"$(BUILD)/compact_unit_direction_test"
	"$(BUILD)/compact_acceleration_test"
	"$(BUILD)/error_study"
	"$(BUILD)/accelerometer_snapshot_test"
	"$(BUILD)/inspection_fixture" > "$(BUILD)/inspection.tsv"
	grep -Fqx "$$(printf 'android.x_m_per_s2\t1.25')" "$(BUILD)/inspection.tsv"
	grep -Fqx "$$(printf 'compact.encode_status\tok')" "$(BUILD)/inspection.tsv"
	grep -Fq 'What the screen shows' "$(BUILD)/inspection.tsv"

$(BUILD)/compact_unit_direction_test: $(ROOT)/geometry/tests/compact_unit_direction_test.c $(MODEL_HEADERS)
	mkdir -p "$(@D)"
	$(ICK) $(ICK_FLAGS) $(MODEL_FLAGS) -I"$(ROOT)/geometry" "$<" -lm -o "$@"

$(BUILD)/%: $(ROOT)/accelerometer/tests/%.c $(MODEL_HEADERS)
	mkdir -p "$(@D)"
	$(ICK) $(ICK_FLAGS) $(MODEL_FLAGS) -I"$(ROOT)/accelerometer/model" -I"$(ROOT)/accelerometer/command" "$<" -lm -o "$@"

picker-tests: $(BUILD)/pad_model_test $(BUILD)/pad_ui_test
	ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 "$(BUILD)/pad_model_test"
	ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 "$(BUILD)/pad_ui_test"

$(BUILD)/pad_model_test: $(ROOT)/math-characters/app/src/main/c/pad_model.c $(ROOT)/math-characters/app/src/test-c/pad_model_test.c $(PICKER_HEADERS)
	mkdir -p "$(@D)"
	$(ICK) $(ICK_FLAGS) $(PICKER_FLAGS) -I"$(ROOT)/math-characters/app/src/main/c" $(filter %.c,$^) -o "$@"

$(BUILD)/pad_ui_test: $(ROOT)/math-characters/app/src/main/c/pad_model.c $(ROOT)/math-characters/app/src/main/c/pad_ui.c $(ROOT)/math-characters/app/src/test-c/pad_ui_test.c $(PICKER_HEADERS)
	mkdir -p "$(@D)"
	$(ICK) $(ICK_FLAGS) $(PICKER_FLAGS) -I"$(ROOT)/math-characters/app/src/main/c" $(filter %.c,$^) -o "$@"

clipboard-tests: $(BUILD)/test_utf8 $(BUILD)/clipboard_jni.o
	"$(BUILD)/test_utf8"
	printf '%s\n' 'PASS: JNI bridge compiles against host JNI headers'

$(BUILD)/test_utf8: $(ROOT)/android-clipboard/utf8.c $(ROOT)/android-clipboard/test_utf8.c $(ROOT)/android-clipboard/utf8.h
	mkdir -p "$(@D)"
	$(ICK) $(ICK_FLAGS) -std=c11 -Wall -Wextra -Werror -pedantic -I"$(ROOT)/android-clipboard" $(filter %.c,$^) -o "$@"

$(BUILD)/clipboard_jni.o: $(ROOT)/android-clipboard/clipboard_jni.c $(ROOT)/android-clipboard/clipboard.h
	test -f "$(JAVA_HOME)/include/jni.h"
	test -f "$(JAVA_HOME)/include/$(JNI_PLATFORM)/jni_md.h"
	mkdir -p "$(@D)"
	$(ICK) $(ICK_FLAGS) -std=c11 -Wall -Wextra -Werror -pedantic -I"$(ROOT)/android-clipboard" -I"$(JAVA_HOME)/include" -I"$(JAVA_HOME)/include/$(JNI_PLATFORM)" -c "$<" -o "$@"
