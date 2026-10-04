#ifndef BER_ASN1_H
#define BER_ASN1_H

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

#include <stdint.h>
#include <ber/ber.h>

/*
 * Tag Name         Tag Encoding
 * ----------------------------------------------
 * INTEGER			<int32_t>
 * OCTET STRING		<uint8_t> [<uint8_t> [...]]
 * NULL
 * OID              <length> <uint32_t>{<length>}
 */

/* Primitive ASN.1 Types */
#define BER_INTEGER			( BER_Universal | BER_Primitive | 0x02 )
#define BER_OCTET_STRING	( BER_Universal | BER_Primitive | 0x04 )
#define BER_NULL			( BER_Universal | BER_Primitive | 0x05 )
#define BER_OID				( BER_Universal | BER_Primitive | 0x06 )

/* Constructed ASN.1 Types */
#define BER_SEQUENCE		( BER_Universal | BER_Constructed | 0x10 )

typedef struct
{
   uint32_t len;
   uint8_t *buf;
} octet_string_t;

#include <ber/asn1/ber-encode.h>
#include <ber/asn1/ber-decode.h>
#include <ber/asn1/ber-utils.h>

#endif	/* #ifndef BER_ASN1_H */
