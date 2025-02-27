SPECS := $(notdir $(wildcard specs/*))

all:
	@make -C .. > /dev/null
	@for spec in $(SPECS); do \
		make -C specs/$$spec; \
	done
