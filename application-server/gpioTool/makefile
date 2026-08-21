SHELL:=/bin/bash

# use gcc for native builds, else $CC as defined by yocto sdk
ifeq ($(SDKTARGETSYSROOT),)
CC=gcc
else
BOOST_INC=$(SDKTARGETSYSROOT)/usr/include
BOOST_LIB=$(SDKTARGETSYSROOT)/usr/lib
endif

# ensure that changes to the compiler cause a rebuild (e.g. if user changes the CC environment variable or if yocto sdk sets a different CC than was previously used)
ARCH_STAMP := obj/.arch
PREV_ARCH := $(shell cat $(ARCH_STAMP) 2>/dev/null)
ifeq ($(filter $(CC),$(PREV_ARCH)),)
  $(shell rm -rf obj && mkdir -p obj && echo $(CC) > $(ARCH_STAMP))
endif

# build list of include directories
INCS=.
INCS+=../Objects
INCS+=..
INC=$(INCS:%=-I %) # prefix each include directory with '-I'

# build list of source files
CSRC+=$(wildcard *.c)
CPPSRC=$(wildcard *.cpp)
CPPSRC+=../gpio.cpp
# CPPSRC+=$(shell find $(HOME)/LEA/WebsocketAPI/Objects -type f -name 'gpio.cpp')
# VPATH+=: $(HOME)/LEA/WebsocketAPI/Objects
vpath %.cpp ..

# build list of objects and dependencies
OBJDIR=obj
CPPOBJS=$(notdir $(CPPSRC:%.cpp=%.o))         # list of c++ objects sans directories
CPPOBJ=$(patsubst %, $(OBJDIR)/%, $(CPPOBJS)) # list of c++ objects prefixed with '$(OBJDIR)/'
COBJS=$(notdir $(CSRC:%.c=%.o))               # list of c objects sans directories
COBJ=$(patsubst %, $(OBJDIR)/%, $(COBJS))     # list of c objects prefixed with '$(OBJDIR)/'
OBJ=$(CPPOBJ)
OBJ+=$(COBJ)
DEP=$(OBJ:%.o=%.d)
BIN=$(OBJDIR)/gpioTool

# compiler/linker flags
COMPFLAGS=-g -pthread -Wno-psabi -O3 -Wno-unused-result
CPPFLAGS=-x c++ -std=c++1z -DBOOST_BIND_GLOBAL_PLACEHOLDERS -DBOOST_ASIO_USE_TS_EXECUTOR_AS_DEFAULT
OUTPUT_OPTION=-MMD -MP -o $@
LDLIBS=-lm -lpthread -lgcc -lstdc++ -lcrypto -lboost_filesystem 

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
	$(CC) -c $(CPPFLAGS) $(COMPFLAGS) $(INC) $(OUTPUT_OPTION) $<

## rule to compile C files
$(COBJ): $(OBJDIR)/%.o: %.c 
	mkdir -p $(OBJDIR)
	$(CC) -c $(COMPFLAGS) $(INC) $(OUTPUT_OPTION) $<

## rule so changes to makefiles cause a rebuild 
$(OBJ): $(MAKEFILE_LIST)

# include auto-dependency files
-include $(DEP)

clean:
	echo cleaning
	rm -f $(OBJDIR)/*.* $(BIN)

# target to print variables; invoked with 'make print-VariableName'
print-%:
	@echo $* = $($*)

