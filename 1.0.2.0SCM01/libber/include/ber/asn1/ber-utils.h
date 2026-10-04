#ifndef BER_ASN1_UTILS_H
#define BER_ASN1_UTILS_H

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

#ifndef BER_ASN1_H
#error "Don't include this file directly. Include asn1/ber.h instead"
#endif	/* #ifndef BER_ASN1_H */

#include <stdint.h>
#include <sys/cdefs.h>

/*
 * Takes an Object Identifier (dot-delimited string) as input and
 * convert it to the Object Identifier format used by the BER
 * routines.
 *
 * Returns the newly created Object Identifier if successful, or
 * NULL if some error occurred. Call abz_get_error() to retrieve
 * the error message.
 *
 * Example:
 *
 *      uint32_t *oid = makeoid ("1.3.6.1.2.1.2.2.1.10.1");
 *
 * Unless the function failed, oid should now be:
 *
 *      oid == { 11, 1, 3, 6, 1, 2, 1, 2, 2, 1, 10, 1 }
 *
 * Notice that the first integer specify how many integers
 * will follow (i.e. 11 for that particular OID).
 */
#define makeoid(oid) makeoid_stub(__FILE__,__LINE__,__FUNCTION__,oid)
uint32_t *makeoid_stub (const char *filename,int line,const char *function,const char *oid)
  __attribute_malloc__;

#endif	/* #ifndef BER_ASN1_UTILS_H */
