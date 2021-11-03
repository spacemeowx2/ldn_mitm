SUBFOLDERS := Atmosphere-libs/libstratosphere ldn_mitm ldnmitm_config overlay

TOPTARGETS := all clean

OUTDIR		:=	out
SD_ROOT     :=  $(OUTDIR)/sd
NRO_DIR     :=  $(SD_ROOT)/switch/ldnmitm_config
TITLE_DIR   :=  $(SD_ROOT)/atmosphere/contents/4200000000000010
OVERLAY_DIR :=  $(SD_ROOT)/switch/.overlays

$(TOPTARGETS): PACK

$(SUBFOLDERS):
	$(MAKE) -C $@ $(MAKECMDGOALS)

$(KIPS): Atmosphere-libs/libstratosphere

#---------------------------------------------------------------------------------
PACK: $(SUBFOLDERS)
	@ mkdir -p $(NRO_DIR)
	@ mkdir -p $(TITLE_DIR)/flags
	@ mkdir -p $(OVERLAY_DIR)
	@ cp ldnmitm_config/ldnmitm_config.nro $(NRO_DIR)/ldnmitm_config.nro
	@ cp ldn_mitm/ldn_mitm.nsp $(TITLE_DIR)/exefs.nsp
	@ cp overlay/overlay.ovl $(OVERLAY_DIR)/ldnmitm_config.ovl
	@ cp ldn_mitm/res/toolbox.json $(TITLE_DIR)/toolbox.json
	@ touch $(TITLE_DIR)/flags/boot2.flag
#---------------------------------------------------------------------------------

.PHONY: $(TOPTARGETS) $(SUBFOLDERS)
