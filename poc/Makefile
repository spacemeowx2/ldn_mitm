#---------------------------------------------------------------------------------
# ldn_mitm, libnx-only PoC -- sysmodule build
#
# Unlike the original this links against libnx only: no Atmosphere-libs, no
# libstratosphere, no libvapours.
#
#   make            # build build/ldn_mitm_poc.elf
#   make clean
#
# Requires devkitPro's devkitA64 + libnx (see dev-env.md).
#---------------------------------------------------------------------------------
ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>/devkitpro")
endif

DEVKITA64 ?= $(DEVKITPRO)/devkitA64
LIBNX     ?= $(DEVKITPRO)/libnx

CROSS     := $(DEVKITA64)/bin/aarch64-none-elf-
CC        := $(CROSS)gcc
CXX       := $(CROSS)g++
LD        := $(CXX)

TARGET    := ldn_mitm_poc
BUILD     := build
SOURCES   := src
APP_JSON  := res/app.json

TOOLS     := $(DEVKITPRO)/tools/bin
NPDMTOOL  := $(TOOLS)/npdmtool
ELF2NSO   := $(TOOLS)/elf2nso
BUILD_PFS0:= $(TOOLS)/build_pfs0

# -fno-rtti / -fno-exceptions match what the original sysmodule was built with.
ARCH      := -march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE

INCLUDES  := -Isrc -I$(LIBNX)/include

CFLAGS    := -g -O2 -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers \
             -ffunction-sections -fdata-sections $(ARCH) $(INCLUDES) -D__SWITCH__ -D_GNU_SOURCE
CXXFLAGS  := $(CFLAGS) -std=gnu++17 -fno-rtti -fno-exceptions
LDFLAGS   := -specs=$(LIBNX)/switch.specs -L$(LIBNX)/lib -g $(ARCH) -Wl,--gc-sections -Wl,-Map,$(BUILD)/$(TARGET).map
LIBS      := -lnx

CPPFILES  := $(wildcard $(SOURCES)/*.cpp)
CFILES    := $(wildcard $(SOURCES)/*.c)
OFILES    := $(patsubst $(SOURCES)/%.cpp,$(BUILD)/%.o,$(CPPFILES)) \
             $(patsubst $(SOURCES)/%.c,$(BUILD)/%.o,$(CFILES))

.PHONY: all clean

all: $(BUILD)/$(TARGET).elf

# The deployable artifact: atmosphere/contents/<tid>/exefs.nsp
nsp: $(BUILD)/$(TARGET).nsp

$(BUILD)/$(TARGET).npdm: $(APP_JSON) | $(BUILD)
	@echo "  NPDM $@"
	@$(NPDMTOOL) $< $@

$(BUILD)/$(TARGET).nso: $(BUILD)/$(TARGET).elf
	@echo "  NSO  $@"
	@$(ELF2NSO) $< $@

$(BUILD)/$(TARGET).nsp: $(BUILD)/$(TARGET).nso $(BUILD)/$(TARGET).npdm
	@echo "  NSP  $@"
	@rm -rf $(BUILD)/exefs && mkdir -p $(BUILD)/exefs
	@cp $(BUILD)/$(TARGET).npdm $(BUILD)/exefs/main.npdm
	@cp $(BUILD)/$(TARGET).nso $(BUILD)/exefs/main
	@$(BUILD_PFS0) $(BUILD)/exefs $@

$(BUILD):
	@mkdir -p $@

$(BUILD)/%.o: $(SOURCES)/%.cpp | $(BUILD)
	@echo "  CXX  $<"
	@$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/%.o: $(SOURCES)/%.c | $(BUILD)
	@echo "  CC   $<"
	@$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/$(TARGET).elf: $(OFILES)
	@echo "  LD   $@"
	@$(LD) $(LDFLAGS) -o $@ $(OFILES) $(LIBS)

-include $(OFILES:.o=.d)

clean:
	@rm -rf $(BUILD)
