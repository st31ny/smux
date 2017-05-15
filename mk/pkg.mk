####################################################################################################
## vim:set ft=make: ################################################################################
## ADS Build System ################################################################################
## Copyright (C) 2017 Maximilian Stein #############################################################
####################################################################################################
## Package Makefile Fragment #######################################################################
## Define variables and targets for a package ######################################################
####################################################################################################

ifndef PKG
$(error PKG not defined)
endif
ifndef BUILDIR
$(error BUILDIR not defined)
endif

## Is another package loaded?
ifdef __PKG_LOADED
# remember parent package
__PARENT.$(PKG)         := $(__PKG_LOADED)
endif
# remember that a package was loaded
__PKG_LOADED            := $(PKG)

# needed for later update of prerequisite list of the target
.SECONDEXPANSION:

## Programs
SHELL                   := bash
MKDIR                   := mkdir -p
RM                      := rm -f
INSTALL                 := install

## Active build configuration
BUILD                   ?= main

## Package configuration
# directories
OBJDIR.$(PKG)           := $(BUILDIR)/obj/$(BUILD)/$(PKG)
TARGETDIR.$(PKG)        := $(BUILDIR)/bin/$(BUILD)/$(PKG)

# target binaries
ifdef TARGETNAME.$(PKG)_$(BUILD)
TARGETNAME.$(PKG)       := $(TARGETNAME.$(PKG)_$(BUILD))
endif
$(eval TARGETNAME.$(PKG) ?= $(PKG))
TARGET.$(PKG)           := $(TARGETDIR.$(PKG))/$(TARGETNAME.$(PKG))

# complete object file list
OBJ.$(PKG)              :=

# installation setup
$(eval INSTPATH_BIN.$(PKG)         ?= $(DESTDIR)/usr/bin/$(TARGETNAME.$(PKG)))
$(eval INSTPATH_ETC.$(PKG)         ?= $(DESTDIR)/etc/$(PKG)/)
$(eval INSTPATH_SHARE.$(PKG)       ?= $(DESTDIR)/usr/share/$(PKG)/)

## Targets
all: $(TARGET.$(PKG))

ifeq ($(BUILD),test)
# build and run test target(s)
test:: $(TARGET.$(PKG))
	@echo " >> (run)  [$<]"
	@$<
else
# otherwise, build normal target first (if other packages depend on this),
# then re-run make with BUILD config
test: $(TARGET.$(PKG))
ifndef __TESTBUILDHELPER
	@$(MAKE) BUILD=test test
__TESTBUILDHELPER       := ""
endif
endif

clean: clean.$(PKG)

install: install.$(PKG)

# make all PHONY
.PHONY: all test clean install doc

## Compilation and linking
$(TARGET.$(PKG)): PKG:=$(PKG)

include $(BUILDIR)/mk/_compile.mk

## Package dependencies
$(TARGET.$(PKG)): $$(foreach pkg,$$(DEPENDS.$(PKG)),$$(TARGET.$$(pkg)))

## Cleaning
__CLNCNT.$(PKG)         := 0
.PHONY: clean.$(PKG)
clean.$(PKG): PKG:=$(PKG)
clean.$(PKG):
	@echo " >> (cln)  cleaning [$(PKG)]"
	@$(RM) -r $(BUILDIR)/obj
	@$(RM) -r $(BUILDIR)/bin

## Installation
__INSTCNT.$(PKG)        := 0
.PHONY: install.$(PKG)
install.$(PKG): PKG:=$(PKG)
ifneq ($(INSTPATH_BIN.$(PKG)),)
install.$(PKG): $(TARGET.$(PKG))
	@echo " >> (inst) install binary of [$(PKG)] to \"$(DESTDIR)\""
	@$(INSTALL) -D -m755 $(TARGET.$(PKG)) $(INSTPATH_BIN.$(PKG))
endif

## Add this directory (and potentially subdirs and sub packages)
include $(BUILDIR)/mk/dir.mk

## Restore parent package
PKG                     := $(__PARENT.$(PKG))
__PKG_LOADED            := $(PKG)
