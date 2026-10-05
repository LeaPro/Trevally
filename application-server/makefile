SHELL:=/bin/bash

SOUNDTRACK_DIR=./third_party/splayerapi-v5
SOUNDTRACK_INC=$(SOUNDTRACK_DIR)/include
SOUNDTRACK_LIB=$(SOUNDTRACK_DIR)/lib

# use gcc for native builds, else $CC as defined by yocto sdk
ifndef SDKTARGETSYSROOT
CC=gcc
SOUNDTRACK_ENABLED=0
# Host/dev-machine build: LVGL is built+installed natively from LEA-LVGL
# (SDL2 window backend) into ~/.local -- see LEA-LVGL/configs/trevally_host.defconfig
# and `cmake --install`. Add an rpath so the binary finds it without needing
# LD_LIBRARY_PATH set at runtime.
LVGL_INC=$(HOME)/.local/include $(HOME)/.local/include/lvgl $(HOME)/.local/include/lvgl/config
LVGL_LIB=$(HOME)/.local/lib
LVGL_BACKEND=SDL
LVGL_EXTRA_LDLIBS=$(shell pkg-config --libs sdl2 2>/dev/null || echo -lSDL2)
LVGL_EXTRA_LDFLAGS=-Wl,-rpath,$(HOME)/.local/lib
LVGL_EXTRA_CFLAGS=
else
BOOST_INC=$(SDKTARGETSYSROOT)/usr/include
BOOST_LIB=$(SDKTARGETSYSROOT)/usr/lib
SOUNDTRACK_ENABLED=1
# Target build: LVGL (lvgl-trevally recipe) ships headers/libs in the SDK
# sysroot, built with the DRM (dumb-buffer) display backend and evdev input.
LVGL_INC=$(SDKTARGETSYSROOT)/usr/include $(SDKTARGETSYSROOT)/usr/include/lvgl $(SDKTARGETSYSROOT)/usr/include/lvgl/config
LVGL_LIB=$(SDKTARGETSYSROOT)/usr/lib
LVGL_BACKEND=DRM
LVGL_EXTRA_LDLIBS=
LVGL_EXTRA_LDFLAGS=
# LV_USE_HID_ENCODER is a custom Kconfig option (not upstream LVGL), so the
# vendored lvgl submodule's generated config header has no alias from
# CONFIG_LV_USE_HID_ENCODER to the plain LV_USE_HID_ENCODER macro this file's
# own #if LV_USE_HID_ENCODER check (in lvgl_ui.cpp) relies on - define it
# directly here to match the same compiler-flag workaround used on the
# LEA-LVGL/liblvgl_linux.so side (see LEA-LVGL/CMakeLists.txt).
LVGL_EXTRA_CFLAGS=-DLV_USE_HID_ENCODER=1
endif

# ensure that changes to the compiler cause a rebuild (e.g. if user changes the CC environment variable or if yocto sdk sets a different CC than was previously used)
ARCH_STAMP := obj/.arch
PREV_ARCH := $(shell cat $(ARCH_STAMP) 2>/dev/null)
ifeq ($(filter $(CC),$(PREV_ARCH)),)
  $(shell rm -rf obj && mkdir -p obj && echo $(CC) > $(ARCH_STAMP))
endif

# build list of include directories
INCS=.
INCS+=./objects
ifeq ($(SOUNDTRACK_ENABLED),1)
INCS+=$(SOUNDTRACK_INC)
endif
INCS+=$(BOOST_INC)
INCS+=$(LVGL_INC)
INCS+=$(HOME)/LEA/WebsocketAPI/WebsocketServer
INCS+=$(HOME)/LEA/WebsocketAPI/Objects
INCS+=$(HOME)/LEA/WebsocketAPI/JSON
INC=$(INCS:%=-I %) # prefix each include directory with '-I'

# build list of source files
CSRC=$(wildcard *.c)
CPPSRC=$(wildcard *.cpp)
CPPSRC+=$(shell find ./objects -type f -name '*.cpp')
ifeq ($(SOUNDTRACK_ENABLED),1)
CSRC+=$(SOUNDTRACK_DIR)/src/audio_output_alsa.c
VPATH+=: $(SOUNDTRACK_DIR)/src
else
CPPSRC:=$(filter-out ./objects/Soundtrack.cpp,$(CPPSRC))
endif
VPATH+=: ./objects
CPPSRC+=$(shell find $(HOME)/LEA/WebsocketAPI/WebsocketServer -type f -name '*.cpp')
VPATH+=: $(HOME)/LEA/WebsocketAPI/WebsocketServer
CPPSRC+=$(shell find $(HOME)/LEA/WebsocketAPI/Objects -type f -name '*.cpp')
VPATH+=: $(HOME)/LEA/WebsocketAPI/Objects
CPPSRC+=$(shell find $(HOME)/LEA/WebsocketAPI/JSON -type f -name '*.cpp')
VPATH+=: $(HOME)/LEA/WebsocketAPI/JSON

# build list of objects and dependencies
OBJDIR=obj
CPPOBJS=$(notdir $(CPPSRC:%.cpp=%.o))         # list of c++ objects sans directories
CPPOBJ=$(patsubst %, $(OBJDIR)/%, $(CPPOBJS)) # list of c++ objects prefixed with '$(OBJDIR)/'
COBJS=$(notdir $(CSRC:%.c=%.o))               # list of c objects sans directories
COBJ=$(patsubst %, $(OBJDIR)/%, $(COBJS))     # list of c objects prefixed with '$(OBJDIR)/'
OBJ=$(CPPOBJ)
OBJ+=$(COBJ)
DEP=$(OBJ:%.o=%.d)
BIN=$(OBJDIR)/application-server
 
# compiler/linker flags
COMPFLAGS=-g -pthread -Wno-psabi -O3 -Wno-unused-result
COMPFLAGS+=-DSAMPLE_RATE=96e3f
COMPFLAGS+=-DSOUNDTRACK_ENABLED=$(SOUNDTRACK_ENABLED)
COMPFLAGS+=-DLV_LINUX_BACKEND=\"$(LVGL_BACKEND)\"
COMPFLAGS+=$(LVGL_EXTRA_CFLAGS)

# Prefer ALSA backend on target when headers are available in SDK sysroot.
ifeq ($(SOUNDTRACK_ENABLED),1)
ifdef SDKTARGETSYSROOT
ifneq ($(wildcard $(SDKTARGETSYSROOT)/usr/include/alsa/asoundlib.h),)
COMPFLAGS+=-DSOUNDTRACK_USE_ALSA=1
else
$(warning ALSA headers not found in $(SDKTARGETSYSROOT); building Soundtrack with dummy audio backend)
endif
endif
endif
CPPFLAGS=-x c++ -std=c++1z -DBOOST_BIND_GLOBAL_PLACEHOLDERS -DBOOST_ASIO_USE_TS_EXECUTOR_AS_DEFAULT
OUTPUT_OPTION=-MMD -MP -o $@
LDLIBS=-lm -lpthread -lgcc -lstdc++ -lrt -L$(BOOST_LIB) -lboost_system -lboost_filesystem -llmdb -lssl -lcrypto -ldl -lcurl
LDLIBS+=-L$(LVGL_LIB) -llvgl_linux -llvgl $(LVGL_EXTRA_LDLIBS)
LDFLAGS+=$(LVGL_EXTRA_LDFLAGS)

ifeq ($(SOUNDTRACK_ENABLED),1)
LDLIBS+=-L$(SOUNDTRACK_LIB) -lsplayer-1
ifdef SDKTARGETSYSROOT
ifneq ($(wildcard $(SDKTARGETSYSROOT)/usr/include/alsa/asoundlib.h),)
LDLIBS+=-lasound
endif
endif
LDFLAGS+=-Wl,-rpath,'$$ORIGIN/../third_party/splayerapi-v5/lib'
endif

# targets and rules 
.PHONY: default all clean
default: $(BIN)
all: default

## rule to build the output binary 
$(BIN): $(OBJ) 
	echo $(LDFLAGS)
	$(CC) -o $@ $^ $(LDFLAGS) $(LDLIBS) 

## rule to compile C++ files
$(CPPOBJ): $(OBJDIR)/%.o: %.cpp 
	mkdir -p $(OBJDIR)
	$(CC) -c $(CPPFLAGS) $(COMPFLAGS) $(INC) $(OUTPUT_OPTION) $(realpath $<)

## rule to compile C files
$(COBJ): $(OBJDIR)/%.o: %.c 
	mkdir -p $(OBJDIR)
	$(CC) -c $(COMPFLAGS) $(INC) $(OUTPUT_OPTION) $(realpath $<)

## rule so changes to makefiles cause a rebuild 
$(OBJ): $(MAKEFILE_LIST)

# include auto-dependency files
-include $(DEP)

clean:
	echo cleaning
	rm -f $(OBJDIR)/*.*

# target to print variables; invoked with 'make print-VariableName'
print-%:
	@echo $* = $($*)

