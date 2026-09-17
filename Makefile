# One-command build for Pulse Designer.
#
#   make                 Release tests + plugin formats for this OS
#   make debug           Same, Debug
#   make test            Tests only
#   make vst3|au|standalone
#   make run             Open Standalone
#   make where           Print artefact paths
#   make clean           Remove generated build artefacts
#
# CONFIG=debug|release (default: release)

CONFIG ?= release
PRESET := $(CONFIG)
BUILD_DIR := build/$(CONFIG)
VERSION := $(shell tr -d ' \n' < VERSION)

ifeq ($(CONFIG),debug)
ARTEFACT_CONFIG := Debug
else
ARTEFACT_CONFIG := Release
endif

ARTEFACT_DIR := $(BUILD_DIR)/src/plugin/PulseDesigner_artefacts/$(ARTEFACT_CONFIG)
PLUGIN_NAME := Pulse Designer

ifeq ($(OS),Windows_NT)
HOST := windows
PLUGIN_TARGETS := PulseDesigner_VST3 PulseDesigner_Standalone
STANDALONE_BIN := $(ARTEFACT_DIR)/Standalone/$(PLUGIN_NAME).exe
else
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
HOST := macos
PLUGIN_TARGETS := PulseDesigner_VST3 PulseDesigner_AU PulseDesigner_Standalone
STANDALONE_BIN := $(ARTEFACT_DIR)/Standalone/$(PLUGIN_NAME).app
else
HOST := linux
PLUGIN_TARGETS := PulseDesigner_VST3 PulseDesigner_Standalone
STANDALONE_BIN := $(ARTEFACT_DIR)/Standalone/$(PLUGIN_NAME)
endif
endif

.PHONY: all debug plugin test vst3 au standalone run where version clean help

all: test plugin where

debug:
	$(MAKE) CONFIG=debug all

configure:
	cmake --preset $(PRESET)

plugin: configure
	cmake --build --preset $(PRESET) --target $(PLUGIN_TARGETS)

test: configure
	cmake --build --preset $(PRESET) --target pulse_tests pulse_plugin_tests
	ctest --test-dir $(BUILD_DIR) --output-on-failure

vst3: configure
	cmake --build --preset $(PRESET) --target PulseDesigner_VST3

au: configure
ifeq ($(HOST),macos)
	cmake --build --preset $(PRESET) --target PulseDesigner_AU
else
	$(error AU is macOS-only)
endif

standalone: configure
	cmake --build --preset $(PRESET) --target PulseDesigner_Standalone

run: standalone
ifeq ($(HOST),macos)
	open "$(STANDALONE_BIN)"
else
	"$(STANDALONE_BIN)"
endif

where:
	@echo "version:   $(VERSION)"
	@echo "host:      $(HOST)"
	@echo "build:     $(BUILD_DIR)"
	@echo "artefacts: $(ARTEFACT_DIR)"
	@echo "VST3:      $(ARTEFACT_DIR)/VST3/$(PLUGIN_NAME).vst3"
ifeq ($(HOST),macos)
	@echo "AU:        $(ARTEFACT_DIR)/AU/$(PLUGIN_NAME).component"
	@echo "standalone: $(STANDALONE_BIN)"
else
	@echo "standalone: $(STANDALONE_BIN)"
endif

version:
	@echo $(VERSION)

clean:
	rm -rf build/debug build/release dist

help:
	@echo "make              Release tests + plugins for $(HOST)"
	@echo "make debug        Debug build"
	@echo "make test         Tests"
	@echo "make vst3|au|standalone"
	@echo "make run          Open Standalone"
	@echo "make where        Print artefact paths"
	@echo "make clean        Remove generated build artefacts"
