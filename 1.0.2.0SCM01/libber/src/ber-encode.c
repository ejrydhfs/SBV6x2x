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
/*-------------------------------------------------------------------------------------
// Copyright 2007, Texas Instruments Incorporated
//
// This program has been modified from its original operation by Texas Instruments
// to do the following:
//
// 1. Removed compilation against libabz. 
//
// THIS MODIFIED SOFTWARE AND DOCUMENTATION ARE PROVIDED
// "AS IS," AND TEXAS INSTRUMENTS MAKES NO REPRESENTATIONS
// OR WARRENTIES, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
// TO, WARRANTIES OF MERCHANTABILITY OR FITNESS FOR ANY
// PARTICULAR PURPOSE OR THAT THE USE OF THE SOFTWARE OR
// DOCUMENTATION WILL NOT INFRINGE ANY THIRD PARTY PATENTS,
// COPYRIGHTS, TRADEMARKS OR OTHER RIGHTS.
//
// These changes are covered as per original license.
//-------------------------------------------------------------------------------------*/ 

#include <stdint.h>
#include <string.h>

#include <ber/ber.h>

static void encode_failed (const char *tag)
{
   printf ("failed to encode BER (ITU X.690) %s tag",tag);
}

static int encode_length (ber_t *e,uint32_t length)
{
   if (length > 127)
	 {
		int i,octets = 0;
		uint8_t tmp[13];

		while (length && octets < 12)
		  tmp[octets++] = length & 0xff, length >>= 8;

		if (octets >= 12 || octets + e->offset >= e->size)
		  return (-1);

		for (i = 0; i < octets; i++)
		  e->buf[e->size - e->offset++ - 1] = tmp[i];

		e->buf[e->size - e->offset++ - 1] = 0x80 | octets;

		return (0);
	 }

   if (e->offset < e->size)
	 {
		e->buf[e->size - e->offset++ - 1] = length;
		return (0);
	 }

   return (-1);
}

int ber_encode_null (ber_t *e)
{
   

   if (e->offset >= e->size)
	 {
		encode_failed ("NULL");
		return (-1);
	 }

   e->buf[e->size - e->offset++ - 1] = 0;
   e->buf[e->size - e->offset++ - 1] = BER_NULL;

   return (0);
}

int ber_encode_sequence (ber_t *e,uint32_t length)
{
   

   if (encode_length (e,length) || e->offset >= e->size)
	 {
		encode_failed ("SEQUENCE");
		return (-1);
	 }

   e->buf[e->size - e->offset++ - 1] = BER_SEQUENCE;

   return (0);
}

#define encode_constructed(e,t) encode_constructed_stub(e,BER_##t,#t)
static int encode_constructed_stub (ber_t *e,uint8_t tag,const char *name)
{
   

   if (encode_length (e,e->offset) || e->offset >= e->size)
	 {
		encode_failed (name);
		return (-1);
	 }

   e->buf[e->size - e->offset++ - 1] = tag;

   return (0);
}

int ber_encode_get_request (ber_t *e)
{
   return (encode_constructed (e,GetRequest));
}

int ber_encode_get_next_request (ber_t *e)
{
   return (encode_constructed (e,GetNextRequest));
}

int ber_encode_get_response (ber_t *e)
{
   return (encode_constructed (e,GetResponse));
}

int ber_encode_set_request (ber_t *e)
{
   return (encode_constructed (e,SetRequest));
}

int ber_encode_trap (ber_t *e)
{
   return (encode_constructed (e,Trap));
}

int ber_encode_get_bulk_request (ber_t *e)
{
   return (encode_constructed (e,GetBulkRequest));
}

int ber_encode_inform_request (ber_t *e)
{
   return (encode_constructed (e,InformRequest));
}

int ber_encode_snmpv2_trap (ber_t *e)
{
   return (encode_constructed (e,SNMPv2_Trap));
}

int ber_encode_get_message (ber_t *e)
{
   return (encode_constructed (e,GetMessage));
}

int ber_encode_put_message (ber_t *e)
{
   return (encode_constructed (e,PutMessage));
}

int ber_encode_auth (ber_t *e)
{
   return (encode_constructed (e,Auth));
}

int ber_encode_reset (ber_t *e)
{
   return (encode_constructed (e,Reset));
}

int ber_encode_set_level (ber_t *e)
{
   return (encode_constructed (e,SetLevel));
}

int ber_encode_octet_string (ber_t *e,const octet_string_t *str)
{
   static const char name[] = "OCTET STRING";
   int i;

   

   for (i = str->len - 1; i >= 0; i--)
	 {
		if (e->offset >= e->size)
		  {
			 encode_failed (name);
			 return (-1);
		  }

		e->buf[e->size - e->offset++ - 1] = str->buf[i];
	 }

   if (encode_length (e,str->len) || e->offset >= e->size)
	 {
		encode_failed (name);
		return (-1);
	 }

   e->buf[e->size - e->offset++ - 1] = BER_OCTET_STRING;

   return (0);
}

int ber_encode_string (ber_t *e,const char *str)
{
   octet_string_t os;

   os.len = strlen (str);
   os.buf = (uint8_t *) str;

   return (ber_encode_octet_string (e,&os));
}

int ber_encode_ipaddress (ber_t *e,uint32_t n)
{
   static const char name[] = "IpAddress";
   int i;
   uint8_t *buf = (uint8_t *) &n;

   

   for (i = 3; i >= 0; i--)
	 {
		if (e->offset >= e->size)
		  {
			 encode_failed (name);
			 return (-1);
		  }

		e->buf[e->size - e->offset++ - 1] = buf[i];
	 }

   if (encode_length (e,4) || e->offset >= e->size)
	 {
		encode_failed (name);
		return (-1);
	 }

   e->buf[e->size - e->offset++ - 1] = BER_IpAddress;

   return (0);
}

#define encode_uint64(e,t,v) encode_uint64_stub(e,BER_##t,#t,v)
static int encode_uint64_stub (ber_t *e,uint8_t tag,const char *name,uint64_t value)
{
   uint32_t length = 1;

   

   if (e->offset >= e->size)
	 {
		encode_failed (name);
		return (-1);
	 }

   e->buf[e->size - e->offset++ - 1] = value & 0xff;

   while (value >>= 7)
	 {
		if (e->offset >= e->size)
		  {
			 encode_failed (name);
			 return (-1);
		  }

		e->buf[e->size - e->offset++ - 1] = (value >>= 1) & 0xff;
		length++;
	 }

   if (encode_length (e,length) || e->offset >= e->size)
	 {
		encode_failed (name);
		return (-1);
	 }

   e->buf[e->size - e->offset++ - 1] = tag;

   return (0);
}

int ber_encode_gauge32 (ber_t *e,uint32_t n)
{
   return (encode_uint64 (e,Gauge32,n));
}

int ber_encode_timeticks (ber_t *e,uint32_t n)
{
   return (encode_uint64 (e,TimeTicks,n));
}

int ber_encode_counter64 (ber_t *e,uint64_t n)
{
   return (encode_uint64 (e,Counter64,n));
}

int ber_encode_counter32 (ber_t *e,uint32_t n)
{
   return (encode_uint64 (e,Counter32,n));
}

#define encode_int32(e,t,v) encode_int32_stub(e,BER_##t,#t,v)
static int encode_int32_stub (ber_t *e,uint8_t tag,const char *name,int32_t value)
{
   uint32_t length = 1;
   uint32_t ctrl = (value < 0 ? -value : value) >> 7;

   

   if (e->offset >= e->size)
	 {
		encode_failed (name);
		return (-1);
	 }

   e->buf[e->size - e->offset++ - 1] = value & 0xff;

   while (ctrl)
	 {
		if (e->offset >= e->size)
		  {
			 encode_failed (name);
			 return (-1);
		  }

		e->buf[e->size - e->offset++ - 1] = (value >>= 8) & 0xff;
		length++;
		ctrl >>= 8;
	 }

   if (encode_length (e,length) || e->offset >= e->size)
	 {
		encode_failed (name);
		return (-1);
	 }

   e->buf[e->size - e->offset++ - 1] = tag;

   return (0);
}

int ber_encode_integer (ber_t *e,int32_t n)
{
   return (encode_int32 (e,INTEGER,n));
}

static int encode_oid_value (ber_t *e,uint32_t value)
{
   int i,len = 0;
   uint8_t stack[15];

   do
	 {
		stack[len++] = value & 0x7f;
		value >>= 7;
	 }
   while (value);

   for (i = 1; i <= len; i++)
	 stack[i] |= 0x80;

   if (len + e->offset >= e->size - 1)
	 return (-1);

   for (i = 0; i < len; i++)
	 e->buf[e->size - e->offset++ - 1] = stack[i];

   return (0);
}

int ber_encode_oid (ber_t *e,const uint32_t *oid)
{
   static const char name[] = "OBJECT IDENTIFIER";
   int i;
   uint32_t orig = e->offset;

   

   for (i = oid[0]; i >= 1; i--)
	 if (encode_oid_value (e,oid[i]))
	   {
		  encode_failed (name);
		  return (-1);
	   }

   if (encode_length (e,e->offset - orig) || e->offset >= e->size)
	 {
		encode_failed (name);
		return (-1);
	 }

   e->buf[e->size - e->offset++ - 1] = BER_OID;

   return (0);
}


