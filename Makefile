# Meant to be built natively (not cross-compiled)

BEAGLEBONE = $(shell ./check_beagle.sh)

ifeq ($(BEAGLEBONE),TRUE)
	DEFS += -DBEAGLEBONE
endif

default: ina219 power

ina219:	ina219.c
	gcc $(DEFS) -o ina219 ina219.c

power:	power.c regs.h
	gcc $(DEFS) -o power power.c

.phony: clean
clean:
	rm -f power ina219
