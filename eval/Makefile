SPECS := $(notdir $(wildcard specs/*))

all:
	for spec in $(SPECS); do \
		make -C specs/$$spec; \
	done
