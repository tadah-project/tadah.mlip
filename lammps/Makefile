TADAH_MD_CONFIGURED=true
TADAH_MD_CONFIGURED=true
###############################################################################
# 
# WARNING: This compilation method is depreciated. Use CMake instead...
#
# To compile Tadah.MD just type 'make' inside the lib root directory.
###############################################################################
CC:= g++
CXXFLAGS:= -std=c++11 -g -O3 -Wall -fPIC -Wpedantic -pedantic-errors -Wextra
###############################################################################

ROOT_DIR:=$(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))
BUILD_DIR:=$(ROOT_DIR)/build

INC=-I$(ROOT_DIR)/build/core/external/cmrc-master/
INC+=-I$(ROOT_DIR)/build/core/external/toml11/
INC+=-I$(ROOT_DIR)/build/core/include
INC+=-I$(ROOT_DIR)/build/models/include
INC+=-I$(ROOT_DIR)/build/md/include

CORE:=$(ROOT_DIR)/build/core
MODELS:=$(ROOT_DIR)/build/models
MD:=$(ROOT_DIR)/build/md

CORE_SRC := $(wildcard $(CORE)/src/*.cpp)
CORE_SRC := $(filter-out $(CORE)/src/lapack.cpp, $(CORE_SRC))
MODELS_SRC := $(wildcard $(MODELS)/src/*.cpp)
MD_SRC := $(wildcard $(MD)/src/*.cpp)

CORE_OBJ := $(CORE_SRC:%.cpp=$(BUILD_DIR)/%.o)
MODELS_OBJ := $(MODELS_SRC:%.cpp=$(BUILD_DIR)/%.o)
MD_OBJ := $(MD_SRC:%.cpp=$(BUILD_DIR)/%.o)

LIB.so:=libtadah.lammps.so
LIB.a:=libtadah.lammps.a

dynamic: $(LIB.so) remove_dirs
static: $(LIB.a) remove_dirs

.DEFAULT_GOAL := static

lammps_tadah: src/lammps_tadah.cpp
	$(CC) $(CXXFLAGS) -o $(BUILD_DIR)/lammps_tadah.o -c src/lammps_tadah.cpp

$(CORE_OBJ): $(BUILD_DIR)/%.o : %.cpp 
	mkdir -p $(dir $@)
	$(CC) $(CXXFLAGS) $(INC) -c $< -o $@

$(MODELS_OBJ): $(BUILD_DIR)/%.o : %.cpp
	mkdir -p $(dir $@)
	$(CC) $(CXXFLAGS) $(INC) -c $< -o $@

$(MD_OBJ): $(BUILD_DIR)/%.o : %.cpp
	mkdir -p $(dir $@)
	$(CC) $(CXXFLAGS) $(INC) -c $< -o $@

obj: $(CORE_OBJ) $(MODELS_OBJ) $(MD_OBJ) lammps_tadah

move_obj: obj
	$(foreach f, $(shell find $(BUILD_DIR) -mindepth 2 -name "*.o"), $(shell mv $(f) $(BUILD_DIR)))

remove_dirs: move_obj
	$(shell find $(BUILD_DIR) -type d -empty -delete)

$(LIB.so): move_obj
	@echo Building $(LIB.so)...
	$(CC) $(CXXFLAGS) -shared -o $(BUILD_DIR)/$@ $(BUILD_DIR)/*.o

$(LIB.a): move_obj
	@echo Building $(LIB.a)...
	ar rcs $(BUILD_DIR)/$@ $(BUILD_DIR)/*.o

clean:
	rm -f $(BUILD_DIR)/*.o
	rm -f $(BUILD_DIR)/$(LIB.so)
	rm -f $(BUILD_DIR)/$(LIB.a)

ifndef TADAH_MD_CONFIGURED
$(error Run ./configure before invoking make)
endif
