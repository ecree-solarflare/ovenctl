/*
	bits.h: packing bytes in words in various ways
	by Edward Cree
	for Solarflare
	(C) 2012
*/

#ifndef HAVE_BITS_H
#define HAVE_BITS_H

#include <stdint.h>

int write_be16(unsigned char *buf, uint16_t value); // Writes /value/ into buf[0:1] as big-endian
uint16_t read_be16(const unsigned char *data);

#endif // !def HAVE_BITS_H
