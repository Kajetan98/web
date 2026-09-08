CC      ?= cc
CFLAGS  ?= -std=c99 -Wall -Wextra -Werror -O1 -I../lib
LDLIBS  := -lm
SRC     := test_epi.c ../lib/epi_led.c ../lib/epi_battery.c \
           ../lib/epi_button.c ../lib/epi_detector.c

test: test_epi
	./test_epi

test_epi: $(SRC) ../lib/*.h
	$(CC) $(CFLAGS) -o $@ $(SRC) $(LDLIBS)

clean:
	rm -f test_epi

.PHONY: test clean
