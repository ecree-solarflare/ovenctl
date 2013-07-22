/*
  modbus.c: Provides functions for BINDER's variant MODBus protocol
  Copyright Solarflare Communications Inc., 2012
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:
      * Redistributions of source code must retain the above copyright
        notice, this list of conditions and the following disclaimer.
      * Redistributions in binary form must reproduce the above copyright
        notice, this list of conditions and the following disclaimer in the
        documentation and/or other materials provided with the distribution.
      * Neither the name of Solarflare Communications Inc. nor the names of its
        contributors may be used to endorse or promote products derived from
        this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  DISCLAIMED. IN NO EVENT SHALL SOLARFLARE COMMUNICATIONS BE LIABLE FOR ANY
  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "modbus.h"
#include "mberr.h"
#include "bits.h"
#include <stdbool.h>
#include <math.h>

// Internal functions
int mb_setbuflen(mb_msg *buf, size_t len) // Sets /buf/'s len to /len/ after checking <= MB_MAXMSGLEN
{
	if(!buf) return(MB_ENOBUF);
	if(len<=MB_MAXMSGLEN)
	{
		buf->len=len;
		return(MB_EOK);
	}
	return(MB_EMLONG);
}

int mb_respbuflen(mb_msg *buf, size_t len) // Sets /buf/'s len to /len/ if new len <= old len <= MB_MAXMSGLEN
{
	int e;
	if(!buf) return(MB_ENOBUF);
	if(buf->len<len) return(MB_EMSHORT);
	if((e=mb_setbuflen(buf, len))) return(e);
	return(MB_EOK);
}
	

// General functions
int mb_crc16(const mb_msg *buf, uint16_t *res)
{
	if(!buf) return(MB_ENOBUF);
	// CRC16 procedure from binder-interface-technical-spec Secn 2.8
	uint16_t crc=0xffff;
	size_t i;
	for(i=0;i<buf->len-2;i++)
	{
		crc^=buf->data[i];
		for(size_t bit=0;bit<8;bit++)
		{
			bool sbit=crc&1; // rightmost bit is 1?
			crc>>=1;
			if(sbit)
				crc^=0xA001;
		}
	}
	// Swap the bytes, because the CRC, unlike everything else in this protocol, is little-endian
	crc=(crc>>8)|(crc<<8);
	if(res) *res=crc;
	return(MB_EOK);
}

int mb_apply_crc16(mb_msg *buf)
{
	int e;
	uint16_t crc;
	if((e=mb_crc16(buf, &crc))) return(e);
	if((e=write_be16(buf->data+buf->len-2, crc))) return(e);
	return(MB_EOK);
}

// Value encoders/decoders
int mb_write_float(unsigned char *buf, float val)
{
	if(!buf) return(MB_ENOBUF);
	// Convert native single-precision float into MODBus format (see 2.11.1)
	bool s=signbit(val);
	int e;
	float fm=frexpf(val, &e); // get mantissa and exponent
	e+=126; // bias the exponent
	uint32_t m=floor(fm*(1<<24)); // get mantissa bits
	// write the four (shuffled) bytes
	buf[0]=(m>>8)&0xff;
	buf[1]=m&0xff;
	buf[2]=(s?0x80:0)|((e>>1)&0x7f);
	buf[3]=((e&1)<<7)|((m>>16)&0x7f);
	return(MB_EOK);
}

float mb_read_float(const unsigned char *buf)
{
	if(!buf) return(nanf(""));
	// Convert MODBus float (see 2.11.1) into native single-precision float
	/* You are not expected to understand this */
	bool s=buf[2]&0x80;
	int8_t e=(((buf[2]&0x7f)<<1)|((buf[3]>>7)&1))-127;
	uint32_t m=(1<<23)|((buf[3]&0x7f)<<16)|(buf[0]<<8)|buf[1];
	float f=ldexpf(m, e-23);
	return(copysignf(f, s?-1:1));
}

// Request constructors (mb_ct_req_*) build a request datagram
//	First arg is always mb_msg *buf, whence the result
int mb_ct_req_readn(mb_msg *buf, uint16_t addr, size_t words)
{
	int e;
	if(!buf) return(MB_ENOBUF);
	if(words>80) return(MB_EDLONG);
	if((e=mb_setbuflen(buf, 8))) return(e);
	buf->data[0]=MB_SLAVEADDR;
	buf->data[1]=MB_FN_READN;
	if((e=write_be16(buf->data+2, addr))) return(e);
	if((e=write_be16(buf->data+4, words))) return(e);
	if((e=mb_apply_crc16(buf))) return(e);
	return(MB_EOK);
}

int mb_ct_req_write(mb_msg *buf, uint16_t addr, uint16_t val)
{
	int e;
	if(!buf) return(MB_ENOBUF);
	if((e=mb_setbuflen(buf, 8))) return(e);
	buf->data[0]=MB_SLAVEADDR;
	buf->data[1]=MB_FN_WRITE;
	if((e=write_be16(buf->data+2, addr))) return(e);
	if((e=write_be16(buf->data+4, val))) return(e);
	if((e=mb_apply_crc16(buf))) return(e);
	return(MB_EOK);
}

int mb_ct_req_writen(mb_msg *buf, uint16_t addr, size_t words, uint16_t *vals)
{
	int e;
	if(!buf) return(MB_ENOBUF);
	if(!vals) return(MB_EINVAL);
	if(words>80) return(MB_EDLONG);
	if((e=mb_setbuflen(buf, 9+(words<<1)))) return(e);
	buf->data[0]=MB_SLAVEADDR;
	buf->data[1]=MB_FN_WRITEN;
	if((e=write_be16(buf->data+2, addr))) return(e);
	if((e=write_be16(buf->data+4, words))) return(e);
	buf->data[6]=words<<1; // won't overflow since words<=80
	for(size_t i=0;i<words;i++)
	{
		if((e=write_be16(buf->data+7+(i<<1), vals[i]))) return(e);
	}
	if((e=mb_apply_crc16(buf))) return(e);
	return(MB_EOK);
}

// Response parsers (mb_pa_resp_*) interrogate a response datagram
//  First arg is always const mb_msg *buf, the datagram to decode
int mb_pa_resp_checkcrc(const mb_msg *buf)
{
	if(!buf) return(MB_ENOBUF);
	if(buf->len<2) return(MB_EMSHORT);
	int e;
	uint16_t crc;
	if((e=mb_crc16(buf, &crc))) return(e);
	uint16_t read_crc=read_be16(buf->data+buf->len-2);
	if(crc!=read_crc) return(MB_EBADBUF);
	return(MB_EOK);
}

int mb_pa_resp_fn(const mb_msg *buf, uint8_t *fn)
{
	if(!buf) return(MB_ENOBUF);
	if(buf->len<2) return(MB_EMSHORT);
	uint8_t val=buf->data[1];
	if(fn) *fn=val;
	if(val&0x80)
	{
		val&=0x7F;
		if(fn) *fn=val;
		if(buf->len<3) return(MB_EMSHORT);
		uint8_t ec=buf->data[2];
		if(ec&0x80)
			return(MB_EMERR);
		return(ec|0x80);
	}
	return(MB_EOK);
}

int mb_pa_resp_readn(mb_msg *buf, size_t words, size_t *awords, uint16_t *vals)
{
	int e;
	if(!buf) return(MB_ENOBUF);
	uint8_t fn;
	if((e=mb_pa_resp_fn(buf, &fn))) return(e);
	if(!mb_is_fn_readn(fn)) return(MB_EBADBUF); // not a readn message
	if(buf->len<3) return(MB_EMSHORT);
	size_t nbytes=buf->data[2];
	if((e=mb_respbuflen(buf, nbytes+5))) return(e);
	if((e=mb_pa_resp_checkcrc(buf))) return(e);
	if(nbytes&1) return(MB_EBADBUF); // nbytes should always be even
	size_t rwords=nbytes/2;
	if(awords) *awords=rwords;
	for(size_t i=0;(i<words)&&(i<rwords);i++)
	{
		if(vals) vals[i]=read_be16(buf->data+3+(i<<1));
	}
	if(words<rwords) return(MB_EDLONG);
	return(MB_EOK);
}

int mb_pa_resp_write(mb_msg *buf, uint16_t *addr, uint16_t *val)
{
	int e;
	if(!buf) return(MB_ENOBUF);
	uint8_t fn;
	if((e=mb_pa_resp_fn(buf, &fn))) return(e);
	if(fn!=MB_FN_WRITE) return(MB_EBADBUF); // not a write message
	if((e=mb_respbuflen(buf, 8))) return(e);
	if((e=mb_pa_resp_checkcrc(buf))) return(e);
	if(addr) *addr=read_be16(buf->data+2);
	if(val) *val=read_be16(buf->data+4);
	return(MB_EOK);
}

int mb_pa_resp_writen(mb_msg *buf, uint16_t *addr, size_t *awords)
{
	int e;
	if(!buf) return(MB_ENOBUF);
	uint8_t fn;
	if((e=mb_pa_resp_fn(buf, &fn))) return(e);
	if(fn!=MB_FN_WRITEN) return(MB_EBADBUF); // not a writen message
	if((e=mb_respbuflen(buf, 8))) return(e);
	if((e=mb_pa_resp_checkcrc(buf))) return(e);
	if(addr) *addr=read_be16(buf->data+2);
	if(awords) *awords=read_be16(buf->data+4);
	return(MB_EOK);
}
