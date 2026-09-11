CC      ?= cc
CFLAGS  ?= -std=c99 -Wall -Wextra -Werror -O1 -I../lib -I../arduino/EpiNano
LDLIBS  := -lm

CORE    := ../lib/epi_led.c ../lib/epi_battery.c ../lib/epi_button.c \
           ../lib/epi_detector.c
NANO    := ../arduino/EpiNano/epi_logic.c

test: test_epi test_nano
	./test_epi
	./test_nano

test_epi: test_epi.c $(CORE) ../lib/*.h
	$(CC) $(CFLAGS) -o $@ test_epi.c $(CORE) $(LDLIBS)

test_nano: test_nano.c nano_ref.c $(NANO) $(CORE) ../lib/*.h nano_ref.h
	$(CC) $(CFLAGS) -o $@ test_nano.c nano_ref.c $(NANO) $(CORE) $(LDLIBS)

clean:
	rm -f test_epi test_nano

.PHONY: test clean
