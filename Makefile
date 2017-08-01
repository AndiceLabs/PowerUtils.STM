# Meant to be built natively (not cross-compiled)

default: ina219

ina219:	ina219.c
	gcc -o ina219 ina219.c

power:	power.c
	gcc -o power power.c

