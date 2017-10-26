override TAG ?= vdev
override CORE_MODULE ?= 0

SDK_DIR ?= sdk

CFLAGS += -D'BC_SCHEDULER_MAX_TASKS=64'
CFLAGS += -D'BC_RADIO_MAX_DEVICES=16'
CFLAGS += -D'TAG="$(TAG)"'
CFLAGS += -D'CORE_MODULE=$(CORE_MODULE)'

-include sdk/Makefile.mk

.PHONY: all
all: sdk
	@$(MAKE) core-module

.PHONY: sdk
sdk:
	@if [ ! -f $(SDK_DIR)/Makefile.mk ]; then echo "Initializing Git submodules..."; git submodule update --init; fi

.PHONY: update
update: sdk
	@echo "Updating Git submodules..."; git submodule update --remote --merge

.PHONY: core-module
core-module:
	@$(MAKE) debug TAG=$(TAG) CORE_MODULE=1

.PHONY: core-module-release
core-module-release:
	@$(MAKE) release TAG=$(TAG) CORE_MODULE=1

.PHONY: usb-dongle
usb-dongle:
	@$(MAKE) debug TAG=$(TAG)

.PHONY: usb-dongle-release
usb-dongle-release:
	@$(MAKE) release TAG=$(TAG)
