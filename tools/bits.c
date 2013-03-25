/*
	bits.c: packing bytes in words in various ways
	by Edward Cree
	for Solarflare
	(C) 2012
*/

#include "bits.h"
#include "mberr.h"

int write_be16(unsigned char *buf, uint16_t value)
{
	if(!buf) return(MB_ENOBUF);
	buf[0]=(value>>8)&0xFF;
	buf[1]=value&0xFF;
	return(MB_EOK);
}

uint16_t read_be16(const unsigned char *data)
{
	if(!data) return(0); // we have to return something
	return(((data[0]&0xFF)<<8)|(data[1]&0xFF));
}
