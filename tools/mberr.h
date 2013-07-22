/*
  mberr.h: Error codes for modbus.h API
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

#ifndef HAVE_MBERR_H
#define HAVE_MBERR_H	1

// Error codes
#define MB_EOK		0 // Success
#define MB_ENOBUF	1 // Supplied buffer could not be written/read (eg. buf==NULL)
#define MB_EMLONG	2 // Message too long for buffer (perhaps MB_MAXMSGLEN needs to be increased)
#define MB_EDLONG	3 // Data too long (eg. too many words for a single read/write)
#define MB_EBADBUF	4 // Message is invalid (eg. CRC error) or not of the required type
#define MB_EMERR	5 // Message is an error response but was otherwise readable, and the error code has the high bit set.  (Otherwise, the high bit will be set and the error code returned; see 'Error codes from bus' below, MB_EE_* macros)
#define MB_EMSHORT	6 // Message is too short
#define MB_EINVAL	7 // Bad parameters (eg. a pointer param is NULL and must be read)
#define MB_ETIMEOUT	8 // Communication timed out (remote device state may have been affected)
#define MB_EMATCH	9 // Response parameters don't match the request sent
#define MB_ESAFETY	0x7e // The requested operation was denied by a safety software interlock
#define MB_EERROR	0x7f // A system call returned an error; details in errno

// Error codes from bus (ORed with 0x80)
#define MB_EE_FN		0x81 // "invalid function"
#define MB_EE_ADDR		0x82 // "invalid parameter address"
#define MB_EE_RANGE		0x83 // "parameter value outside range of values"
#define MB_EE_BUSY		0x84 // "slave not ready" (should never happen: "Error code 4 (slave not ready) is not implemented in the controller since the controller always responds within 250 ms to a valid data request.")
#define MB_EE_ACCESS	0x85 // "write access to parameter denied"

#endif // !def HAVE_MBERR_H
