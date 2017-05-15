####################################################################################################
## vim:set ft=make: ################################################################################
## ADS Build System ################################################################################
## Copyright (C) 2017 Maximilian Stein #############################################################
####################################################################################################
## Directory Makefile Fragment #####################################################################
## Add all files of this directory and recurse into sub dirs #######################################
####################################################################################################

ifndef __PKG_LOADED
$(error No PKG loaded. Include $$(BUILDIR)/mk/pkg.mk)
endif
ifndef MYDIR
$(error MYDIR not defined. Set with: MYDIR := $$(dir $$(lastword $$(MAKEFILE_LIST))))
endif

## Append to object file list
include $(BUILDIR)/mk/_collect.mk

# extend package-wide object file list
OBJ.$(PKG)              += $(__OBJ)

## Dependencies
# include automatic dependency files
-include $(addsuffix .d,$(__OBJ))

ifndef MK_NOMAKEFILEDEP
# let all local files depend on everything that led to its inclusion
# (but filter dependency files)
# TODO: This still contains too many dependencies (siblings in dir hierachy)
$(__OBJ): $(filter-out %.d,$(MAKEFILE_LIST))
endif

# add package dependencies
$(__OBJ): $$(foreach pkg,$$(PREDEPENDS.$(PKG)),$$(TARGET.$$(pkg)))

## Cleaning
ifneq ($(CLEAN_FILES),)
# prepend directory
__CLEAN_FILES           := $(addprefix $(MYDIR),$(CLEAN_FILES))
# get name for clean target
__CLNCNT.$(PKG)         := $(shell echo $$(($(__CLNCNT.$(PKG)) + 1)))
__CLNTARGET             := clean.$(PKG)__$(__CLNCNT.$(PKG))
.PHONY: $(__CLNTARGET)
# add local clean target to package clean dependencies
clean.$(PKG): $(__CLNTARGET)
# remember local files
$(__CLNTARGET): __CLEAN_FILES := $(__CLEAN_FILES)
# clean!
$(__CLNTARGET):
	@echo " >> (cln)  clean component [$@]"
	@$(RM) $(__CLEAN_FILES)
CLEAN_FILES             :=
endif

## Installation
ifneq ($(INST_ETC)$(INST_SHARE),)
# prepend directory
__INST_ETC              := $(addprefix $(MYDIR),$(INST_ETC))
__INST_SHARE            := $(addprefix $(MYDIR),$(INST_SHARE))
# get name for installer target
__INSTCNT.$(PKG)        := $(shell echo $$(($(__INSTCNT.$(PKG)) + 1)))
__INSTTARGET            := install.$(PKG)__$(__INSTCNT.$(PKG))
.PHONY: $(__INSTTARGET)
# add local installer to package installer dependencies
install.$(PKG): $(__INSTTARGET)
# remember local files
$(__INSTTARGET): __INST_ETC:=$(__INST_ETC)
$(__INSTTARGET): __INST_SHARE:=$(__INST_SHARE)
# install!
$(__INSTTARGET):
	@echo " >> (inst) install component [$@] to \"$(DESTDIR)\""
ifneq ($(INSTPATH_ETC.$(PKG)),)
	@arg="$(__INST_ETC)"; \
	for tuple in $$arg; do \
		IFS=">" read f t <<< "$$tuple" ; \
		install -D -m644 "$$f" "$(INSTPATH_ETC.$(PKG))/$$t" ; \
	done
endif
ifneq ($(INSTPATH_SHARE.$(PKG)),)
	@arg="$(__INST_SHARE)"; \
	for tuple in $$arg; do \
		IFS=">" read f t <<< "$$tuple" ; \
		install -D -m644 "$$f" "$(INSTPATH_SHARE.$(PKG))/$$t" ; \
	done
endif
INST_SHARE              :=
INST_ETC                :=
endif

## Recurse into subdirs and subpackages
__SUBS                  := $(patsubst %,$(MYDIR)/%/dir.mk,$(SUBDIRS)) $(patsubst %,$(MYDIR)/%/Makefile,$(PKGDIRS))
# unset variables to prevent unintended side-effects
SUBDIRS                 :=
PKGDIRS                 :=
# recurse!
include $(__SUBS)
