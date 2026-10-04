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

//#include <debug/memory.h>
#include <ber/ber.h>

static void decode_failed (const char *tag)
{
   printf ("failed to decode BER (ITU X.690) %s tag",tag);
}

static void out_of_memory (void)
{
   printf ("failed to allocate memory: %m");
}

static int decode_length (uint32_t *taglen,ber_t *d)
{
   if (d->offset >= d->size)
	 return (-1);

   if (d->buf[d->offset] > 127)
	 {
		uint16_t octets = d->buf[d->offset] & 0x7f;
		int i = 0;

		if (octets < 1 || octets > 4 || d->size - d->offset < octets)
		  return (-1);

		for (*taglen = i = 0; i < octets; i++)
		  *taglen += d->buf[d->offset + octets - i] << (i * 8);

		d->offset += octets + 1;

		if (*taglen > INT32_MAX)
		  return (-1);
	 }
   else *taglen = d->buf[d->offset++];

   return (0);
}

int ber_decode_null (ber_t *d)
{
   

   if (d->offset >= d->size - 1 || d->buf[d->offset] != BER_NULL || d->buf[d->offset + 1])
	 {
		decode_failed ("NULL");
		return (-1);
	 }

   d->offset += 2;

   return (0);
}

int ber_decode_sequence (ber_t *d)
{
   

   do
	 {
		uint32_t taglen;

		if (d->offset >= d->size || d->buf[d->offset] != BER_SEQUENCE)
		  break;

		d->offset++;

		if (decode_length (&taglen,d) < 0 || d->offset + taglen > d->size)
		  break;

		return (0);
	 }
   while (0);

   decode_failed ("SEQUENCE");
   return (-1);
}

#define decode_constructed(d,t) decode_constructed_stub(d,BER_##t,#t)
static int decode_constructed_stub (ber_t *d,uint8_t tag,const char *name)
{
   

   do
	 {
		uint32_t taglen;

		if (d->offset >= d->size || d->buf[d->offset] != tag)
		  break;

		d->offset++;

		if (decode_length (&taglen,d) < 0 || d->offset + taglen != d->size)
		  break;

		return (0);
	 }
   while (0);

   decode_failed (name);
   return (-1);
}

int ber_decode_get_request (ber_t *d)
{
   return (decode_constructed (d,GetRequest));
}

int ber_decode_get_next_request (ber_t *d)
{
   return (decode_constructed (d,GetNextRequest));
}

int ber_decode_get_response (ber_t *d)
{
   return (decode_constructed (d,GetResponse));
}

int ber_decode_set_request (ber_t *d)
{
   return (decode_constructed (d,SetRequest));
}

int ber_decode_trap (ber_t *d)
{
   return (decode_constructed (d,Trap));
}

int ber_decode_get_bulk_request (ber_t *d)
{
   return (decode_constructed (d,GetBulkRequest));
}

int ber_decode_inform_request (ber_t *d)
{
   return (decode_constructed (d,InformRequest));
}

int ber_decode_snmpv2_trap (ber_t *d)
{
   return (decode_constructed (d,SNMPv2_Trap));
}

int ber_decode_get_message (ber_t *d)
{
   return (decode_constructed (d,GetMessage));
}

int ber_decode_put_message (ber_t *d)
{
   return (decode_constructed (d,PutMessage));
}

int ber_decode_auth (ber_t *d)
{
   return (decode_constructed (d,Auth));
}

int ber_decode_reset (ber_t *d)
{
   return (decode_constructed (d,Reset));
}

int ber_decode_set_level (ber_t *d)
{
   return (decode_constructed (d,SetLevel));
}

#define decode_uint64(v,d,t) decode_uint64_stub(v,d,BER_##t,#t)
static int decode_uint64_stub (int64_t *value,ber_t *d,uint8_t tag,const char *name)
{
   

   do
	 {
		uint32_t taglen;

		if (d->offset >= d->size || d->buf[d->offset] != tag)
		  break;

		d->offset++;

		if (decode_length (&taglen,d) < 0 || d->offset + taglen > d->size)
		  break;

		*value = 0;

		while (taglen)
		  {
			 if (*value >= UINT64_C (0x100000000000000))
			   break;

			 *value = (*value << 8) | d->buf[d->offset++];
			 taglen--;
		  }

		if (!taglen)
		  return (0);
	 }
   while (0);

   decode_failed (name);
   return (-1);
}

int ber_decode_counter64 (uint64_t *value,ber_t *d)
{
   return (decode_uint64 (value,d,Counter64));
}

#define decode_uint32(v,d,t) decode_uint32_stub(v,d,BER_##t,#t)
static int decode_uint32_stub (uint32_t *value,ber_t *d,uint8_t tag,const char *name)
{
   

   do
	 {
		uint32_t taglen;

		if (d->offset >= d->size || d->buf[d->offset] != tag)
		  break;

		d->offset++;

		if (decode_length (&taglen,d) < 0 || d->offset + taglen > d->size)
		  break;

		*value = 0;

		while (taglen)
		  {
			 if (*value >= UINT32_C (0x1000000))
			   break;

			 *value = (*value << 8) | d->buf[d->offset++];
			 taglen--;
		  }

		if (!taglen)
		  return (0);
	 }
   while (0);

   decode_failed (name);
   return (-1);
}

int ber_decode_counter32 (uint32_t *n,ber_t *d)
{
   return (decode_uint32 (n,d,Counter32));
}

int ber_decode_gauge32 (uint32_t *n,ber_t *d)
{
   return (decode_uint32 (n,d,Gauge32));
}

int ber_decode_timeticks (uint32_t *n,ber_t *d)
{
   return (decode_uint32 (n,d,TimeTicks));
}

#define decode_int32(v,d,t) decode_int32_stub(v,d,BER_##t,#t)
static int decode_int32_stub (int32_t *value,ber_t *d,uint8_t tag,const char *name)
{
   

   do
	 {
		uint32_t taglen;

		if (d->offset >= d->size || d->buf[d->offset] != tag)
		  break;

		d->offset++;

		if (decode_length (&taglen,d) < 0 || d->offset + taglen > d->size)
		  break;

		*value = 0;

		if (taglen)
		  {
			 *value = (int8_t) d->buf[d->offset++];

			 while (--taglen)
			   {
				  if (*value >= INT32_C (0x1000000))
					break;

				  *value = (*value << 8) | d->buf[d->offset++];
			   }
		  }

		if (!taglen)
		  return (0);
	 }
   while (0);

   decode_failed (name);
   return (-1);
}

int ber_decode_integer (int32_t *value,ber_t *d)
{
   return (decode_int32 (value,d,INTEGER));
}

int ber_decode_octet_string_stub (const char *filename,int line,const char *function,octet_string_t *str,ber_t *d)
{
   

   do
	 {
		uint32_t taglen;

		str->len = 0;

		if (d->offset >= d->size || d->buf[d->offset] != BER_OCTET_STRING)
		  break;

		d->offset++;

		if (decode_length (&taglen,d) < 0 || d->offset + taglen > d->size)
		  break;

		if ((str->len = taglen))
		  {
#ifdef CNET_SWITCH_WORKAROUND
			 const uint8_t *buf = d->buf + d->offset + str->len - 1;

			 while (str->len && *buf == '\0')
			   str->len--, buf--;
#endif	/* #ifdef CNET_SWITCH_WORKAROUND */

			 if (str->len)
			   {
				  if ((str->buf = malloc (str->len)) == NULL)
					{
					   out_of_memory ();
					   return (-1);
					}

				  memcpy (str->buf,d->buf + d->offset,str->len);
			   }

			 d->offset += taglen;
		  }

		return (0);
	 }
   while (0);

   decode_failed ("OCTET STRING");
   return (-1);
}

int ber_decode_string_stub (const char *filename,int line,const char *function,char **str,ber_t *d)
{
   octet_string_t os;

   if (ber_decode_octet_string_stub (filename,line,function,&os,d))
     return (-1);

   if ((*str = realloc (os.buf,os.len + 1)) == NULL)
	 {
		out_of_memory ();

		if (os.len)
		  free (os.buf);

		return (-1);
	 }

   (*str)[os.len] = '\0';

   return (0);
}

int ber_decode_ipaddress (uint32_t *n,ber_t *d)
{
   

   do
	 {
		uint32_t taglen;

		if (d->offset >= d->size || d->buf[d->offset] != BER_IpAddress)
		  break;

		d->offset++;

		if (decode_length (&taglen,d) < 0 || d->offset + taglen > d->size || taglen != 4)
		  break;

		*n = *((uint32_t *) (d->buf + d->offset));

		d->offset += taglen;

		return (0);
	 }
   while (0);

   decode_failed ("IpAddress");
   return (-1);
}

static int decode_oid_value (uint32_t *value,const uint8_t *buf,uint32_t *offset,uint32_t size)
{
   int64_t tmp = 0;
   int32_t i;
   uint32_t len;

   for (len = 0; len + *offset < size && buf[len + *offset] & 0x80; len++) ;

   if (buf[len++ + *offset] & 0x80)
	 return (-1);

   for (i = 0; i < len; i++)
	 {
		tmp += (buf[*offset + i] & 0x7f) << ((len - i - 1) * 7);

		if (tmp > UINT32_MAX)
		  return (-1);
	 }

   *offset += len;
   *value = tmp;

   return (0);
}

int ber_decode_oid_stub (const char *file,int line,const char *function,uint32_t **oid,ber_t *d)
{
   static const char name[] = "OBJECT IDENTIFIER";
   uint32_t taglen,*ptr;
   int i = 0;

   

   *oid = NULL;

   if (d->offset >= d->size || d->buf[d->offset] != BER_OID)
	 {
		decode_failed (name);
		return (-1);
	 }

   d->offset++;

   if (decode_length (&taglen,d) < 0 || d->offset + taglen > d->size)
	 {
		decode_failed (name);
		return (-1);
	 }

   if ((*oid = malloc ((taglen + 1) * sizeof (uint32_t))) == NULL)
	 {
		out_of_memory ();
		return (-1);
	 }

   taglen += d->offset;

   while (d->offset < taglen)
	 {
		if (decode_oid_value (&(*oid)[++i],d->buf,&d->offset,taglen) < 0)
		  {
			 decode_failed (name);
			 free (*oid);
			 *oid = NULL;
			 return (-1);
		  }
	 }

   if (d->offset != taglen)
	 {
		decode_failed (name);
		free (*oid);
		*oid = NULL;
		return (-1);
	 }

   **oid = i;

   if ((ptr = realloc (*oid,(i + 1) * sizeof (uint32_t))) != NULL)
	 *oid = ptr;

   return (0);
}


