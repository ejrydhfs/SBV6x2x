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

#include <sys/types.h>
#include <stdint.h>
#include <string.h>

//#include <debug/memory.h>
#include <ber/ber.h>

/* initial guess of how many nodes are in an ObjectID string. must be >= 2 */
#define OIDLEN 8

/* we're not interested in locale support in this case */
#define ISDIGIT(x) ((x) >= '0' && (x) <= '9')

struct oid
{
   uint32_t *buf;
   uint32_t offset;
   uint32_t length;
};

static void out_of_memory (void)
{
   printf ("failed to allocate memory: %m");
}

static int next_oid_value (uint32_t *result,const char **oid)
{
   int64_t value = 0,j;
   char stack[10];
   int i,len = 0;

   do
	 {
		if (len >= 10 || !ISDIGIT (**oid))
		  {
			 printf ("invalid node");
			 return (-1);
		  }

		stack[len++] = **oid;
		(*oid)++;
	 }
   while (**oid != '.' && **oid != '\0');

   if (**oid == '.') (*oid)++;

   for (i = len - 1, j = 1; i >= 0; i--, j *= 10)
	 value += (stack[i] - '0') * j;

   if (value > UINT32_MAX)
	 {
		printf ("node too big");
		return (-1);
	 }

   *result = (uint32_t) value;

   return (0);
}

static int decode_first (const char *filename,int line,const char *function,
						 struct oid *oid,const char **str)
{
   uint32_t first,second;

   oid->offset = 1;
   oid->length = OIDLEN;

   if ((oid->buf = malloc (oid->length * sizeof (uint32_t))) == NULL)
	 {
		out_of_memory ();
		return (-1);
	 }

   if (next_oid_value (&first,str) || first > 2)
	 {
		printf ("first node neither ccitt(0), iso(1), or joint-iso-ccitt(2)");
		return (-1);
	 }

   /*
	* if first == joint-iso-ccitt(2), then second may actually be
	* bigger than 39, but we don't take that into account
	*/
   if (**str == '\0' || next_oid_value (&second,str) || second > 39)
	 {
		printf ("second node missing or bigger than 39");
		return (-1);
	 }

   oid->buf[1] = first * 40 + second;

   return (0);
}

static int decode_rest (const char *filename,int line,const char *function,
						struct oid *oid,const char **str)
{
   uint32_t value;

   while (**str != '\0')
	 {
		if (next_oid_value (&value,str))
		  return (-1);

		if (oid->length == oid->offset + 1)
		  {
			 void *ptr;

			 oid->length += OIDLEN;

			 if ((ptr = realloc(oid->buf,oid->length * sizeof (uint32_t))) == NULL)
			   {
				  out_of_memory ();
				  return (-1);
			   }

			 oid->buf = ptr;
		  }

		oid->buf[++oid->offset] = value;
	 }

   /* detect ..'s */
   if ((*str)[-1] == '.')
	 {
		printf ("multiple dots between nodes");
		return (-1);
	 }

   return (0);
}

uint32_t *makeoid_stub (const char *filename,int line,const char *function,const char *oid)
{
   void *ptr;
   struct oid data;

   

   if (oid == NULL || *oid == '\0')
	 {
		printf ("invalid arguments");
		return (NULL);
	 }

   if (decode_first (filename,line,function,&data,&oid) ||
	   decode_rest (filename,line,function,&data,&oid))
	 {
		if (data.buf != NULL)
		  free (data.buf);

		return (NULL);
	 }

   if ((ptr = realloc(data.buf,(data.offset + 1) * sizeof (uint32_t))) != NULL)
	 data.buf = ptr;

   data.buf[0] = data.offset;

   return (data.buf);
}


