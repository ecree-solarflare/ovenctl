/*
	modbus.h: Functions for BINDER's variant MODBus protocol
	by Edward Cree
	for Solarflare
	(C) 2012
*/

#ifndef HAVE_MODBUS_H
#define HAVE_MODBUS_H

/*
	NOTES:
	1) This protocol is not the same as standard MODBus RTU or MODBus TCP
	2) The "Slave address" is always set to 0x01 (MB_SLAVEADDR), as this code is for use with the XPort TCP/IP-to-MODBus adaptor
*/

#include <stdint.h>
#include <stddef.h>

#define MB_SLAVEADDR	0x01

#ifndef MB_MAXMSGLEN
#define MB_MAXMSGLEN	256
#endif // !def MB_MAXMSGLEN
typedef struct
{
	size_t len;
	unsigned char data[MB_MAXMSGLEN];
}
mb_msg; // single modbus datagram

#define MB_FN_READN		0x03	// Docs say "Function 0x03 or 0x04" with no further explanation
#define MB_FN_READN_ALT	0x04
#define MB_FN_WRITE		0x06
#define MB_FN_WRITEN	0x10

#define mb_is_fn_readn(fn)	(((fn)==MB_FN_READN)||((fn)==MB_FN_READN_ALT))	// see note on MB_FN_READN

// General functions
int mb_crc16(const mb_msg *buf, uint16_t *res); // Compute the CRC16 checksum for buf (excl. last two bytes, as that's where the checksum value goes).  Store result in res

// Value encoders/decoders
int mb_write_float(unsigned char *buf, float val); // Encodes a float in MODBus format (4 bytes)
float mb_read_float(const unsigned char *buf); // Decodes a float from MODBus format (4 bytes)

// Request constructors (mb_ct_req_*) build a request datagram
//	First arg is always mb_msg *buf, whence the result
int mb_ct_req_readn(mb_msg *buf, uint16_t addr, size_t words); // Build a read request (Reading n words) to read /words/ words starting from /addr/
int mb_ct_req_write(mb_msg *buf, uint16_t addr, uint16_t val); // Build a write request (Writing one word) to write /val/ to /addr/
int mb_ct_req_writen(mb_msg *buf, uint16_t addr, size_t words, uint16_t *vals); // Build a write request (Writing n words) to write /words/ words from /vals/ to /addr/

// Response parsers (mb_pa_resp_*) interrogate a response datagram
//	First arg is always mb_msg *buf, the datagram to decode
//	On a successful parse, buf->len will be changed to the actual number of bytes consumed
//	Exceptions are helper functions like _checkcrc and _fn, which only do part of a parse, don't update buf->len, and thus the buf parameter is const
int mb_pa_resp_checkcrc(const mb_msg *buf); // Check the CRC16 of /buf/; if bad, return MB_EBADBUF
int mb_pa_resp_fn(const mb_msg *buf, uint8_t *fn); // Decode the function code from /buf/ and put it in /fn/
int mb_pa_resp_readn(mb_msg *buf, size_t words, size_t *awords, uint16_t *vals); // Parse a read response (Reading n words) and, if successful, store up to /words/ words in /vals/.  Actual number of words read (which may be more or fewer than /words/) is then written to /awords/.  If this exceeds /words/ the function will return MB_EDLONG but will otherwise complete
int mb_pa_resp_write(mb_msg *buf, uint16_t *addr, uint16_t *val); // Parse a write response (Writing one word) and, if successful, store the word address and word value into /addr/ and /val/ respectively
int mb_pa_resp_writen(mb_msg *buf, uint16_t *addr, size_t *awords); // Parse a write response (Writing more than one words[sic]) and, if successful, store the word address and number of words written into /addr/ and /awords/ respectively

#endif // !def HAVE_MODBUS_H
