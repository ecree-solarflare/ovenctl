# Makefile for modbus (BINDER's variant MODBus protocol) tools

CC := gcc
CFLAGS := -Wall -Wextra -Werror -pedantic --std=gnu99 -g # Lots of warnings

all: nmbtest nmbdump

nmbtest: nmbtest.c modbus.o bits.o net.o net.h bits.h modbus.h mberr.h
	$(CC) $(CFLAGS) $(CPPFLAGS) $< $(LDFLAGS) -lm modbus.o bits.o net.o -o $@

nmbdump: nmbdump.c modbus.o bits.o net.o net.h modbus.h mberr.h
	$(CC) $(CFLAGS) $(CPPFLAGS) $< $(LDFLAGS) -lm modbus.o bits.o net.o -o $@

modbus.o: bits.h mberr.h

bits.o: mberr.h

%.o: %.c %.h
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

