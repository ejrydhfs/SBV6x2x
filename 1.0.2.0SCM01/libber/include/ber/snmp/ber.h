#ifndef BER_SNMP_H
#define BER_SNMP_H

/*
 * Copyright (c) Abraham vd Merwe <abz@blio.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *	  notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *	  notice, this list of conditions and the following disclaimer in the
 *	  documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of other contributors
 *	  may be used to endorse or promote products derived from this software
 *	  without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <ber/ber.h>

/*
 * Tag Name			Tag Encoding
 * ----------------------------------------------
 * IpAddress		<uint8_t>{4}
 * Counter32		<uint32_t>
 * Gauge32			<uint32_t>
 * TimeTicks		<uint32_t>		--  time measured in hundreds of a second (10ms)
 * Opaque
 * Counter64		<uint64_t>
 * GetRequest
 * GetNextRequest
 * GetResponse
 * SetRequest
 * Trap
 * GetBulkRequest
 * InformRequest
 * SNMPv2_Trap
 */

/* Primitive SNMP application types */
#define BER_IpAddress		( BER_Application | BER_Primitive | 0x00 )
#define BER_Counter32		( BER_Application | BER_Primitive | 0x01 )
#define BER_Gauge32			( BER_Application | BER_Primitive | 0x02 )
#define BER_TimeTicks		( BER_Application | BER_Primitive | 0x03 )
#define BER_Opaque			( BER_Application | BER_Primitive | 0x04 )
#define BER_Counter64		( BER_Application | BER_Primitive | 0x06 )

/* Context-specific types within an SNMP Message */
#define BER_GetRequest		( BER_ContextSpecific | BER_Constructed | 0x00 )
#define BER_GetNextRequest	( BER_ContextSpecific | BER_Constructed | 0x01 )
#define BER_GetResponse		( BER_ContextSpecific | BER_Constructed | 0x02 )
#define BER_SetRequest		( BER_ContextSpecific | BER_Constructed | 0x03 )
#define BER_Trap			( BER_ContextSpecific | BER_Constructed | 0x04 )
#define BER_GetBulkRequest	( BER_ContextSpecific | BER_Constructed | 0x05 )
#define BER_InformRequest	( BER_ContextSpecific | BER_Constructed | 0x06 )
#define BER_SNMPv2_Trap		( BER_ContextSpecific | BER_Constructed | 0x07 )

#include <ber/snmp/ber-encode.h>
#include <ber/snmp/ber-decode.h>

#endif	/* #ifndef BER_SNMP_H */
