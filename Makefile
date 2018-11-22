KIPS := ldn_mitm
NROS := ldn_example

SUBFOLDERS := libstratosphere $(KIPS) $(NROS)

TOPTARGETS := all clean

$(TOPTARGETS): $(SUBFOLDERS)

$(SUBFOLDERS):
	$(MAKE) -C $@ $(MAKECMDGOALS)

$(KIPS): libstratosphere

.PHONY: $(TOPTARGETS) $(SUBFOLDERS)
