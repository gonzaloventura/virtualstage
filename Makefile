# Attempt to load a config.make file.
# If none is found, project defaults in config.project.make will be used.
ifneq ($(wildcard config.make),)
	include config.make
endif

# make sure the the OF_ROOT location is defined
ifndef OF_ROOT
	OF_ROOT=../../..
endif

# call the project makefile!
include $(OF_ROOT)/libs/openFrameworksCompiled/project/makefileCommon/compile.project.mk

# Embed Syphon.framework into .app bundle after build (macOS)
SYPHON_FW = $(OF_ROOT)/addons/ofxSyphon/libs/Syphon/lib/osx/Syphon.framework

.PHONY: bundle
bundle: Release
	@if [ -d "$(SYPHON_FW)" ]; then \
		for APP in bin/*.app; do \
			if [ -d "$$APP" ]; then \
				mkdir -p "$$APP/Contents/Frameworks"; \
				cp -R "$(SYPHON_FW)" "$$APP/Contents/Frameworks/"; \
				echo "     Embedded Syphon.framework into $$APP"; \
			fi; \
		done; \
	fi
