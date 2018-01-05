# Meant to be built natively (not cross-compiled)

default: ina219 power

ina219:	ina219.c
	gcc -o ina219 ina219.c

power:	power.c regs.h
	gcc -o power power.c

.phony: clean
clean:
	rm -f power ina219
