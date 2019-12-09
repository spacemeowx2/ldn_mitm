KIPS := ldn_mitm
NROS := ldnmitm_config

SUBFOLDERS := libstratosphere $(KIPS) $(NROS)

TOPTARGETS := all clean

OUTDIR		:=	out
SD_ROOT     :=  $(OUTDIR)/sd
NRO_DIR     :=  $(SD_ROOT)/switch/ldnmitm_config
TITLE_DIR   :=  $(SD_ROOT)/atmosphere/4200000000000010

$(TOPTARGETS): PACK

$(SUBFOLDERS):
	$(MAKE) -C $@ $(MAKECMDGOALS)

$(KIPS): libstratosphere

PACK: $(SUBFOLDERS)
	@[ -d $(NRO_DIR) ] || mkdir -p $(NRO_DIR)
	@[ -d $(TITLE_DIR)/flags ] || mkdir -p $(TITLE_DIR)/flags
	@ cp ldnmitm_config/ldnmitm_config.nro $(NRO_DIR)/ldnmitm_config.nro
	@ cp ldn_mitm/out/ldn_mitm.nsp $(TITLE_DIR)/exefs.nsp
	@ touch $(TITLE_DIR)/flags/boot2.flag

.PHONY: $(TOPTARGETS) $(SUBFOLDERS)
