
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

/*
#include <debug/log.h>
#include <debug/hex.h>
#include <debug/memory.h>
*/


#include <ber/ber.h>
#define ARRAYSIZE sizeof

static int oidcmp (const uint32_t *base,const uint32_t *oid)
{
   uint32_t saved = *base - *oid,len = *base < *oid ? *base : *oid;

   while (len && *++base == *++oid)
	 len--;

   return (len ? *base - *oid : saved);
}

static int oscmp (octet_string_t *a,const char *b)
{
   size_t len = strlen (b);
   if (a->len < len)
	 return (-1);
   else if (a->len > len)
	 return (1);
   else return (memcmp (a->buf,b,len));
}

static int test_decode_1 (void)
{
   static const uint32_t oid[] = { 10, 43, 6, 1, 2, 1, 2, 2, 1, 10, 1 };
   static const uint8_t buf[] =
	 {
		0x30, 0x2b, 0x02, 0x01, 0x00, 0x04, 0x06, 0x70,
		0x75, 0x62, 0x6c, 0x69, 0x63, 0xa0, 0x1e, 0x02,
		0x04, 0x4a, 0x4e, 0x03, 0xc1, 0x02, 0x01, 0x00,
		0x02, 0x01, 0x00, 0x30, 0x10, 0x30, 0x0e, 0x06,
		0x0a, 0x2b, 0x06, 0x01, 0x02, 0x01, 0x02, 0x02,
		0x01, 0x0a, 0x01, 0x05, 0x00
	 };
   ber_t d;
   int32_t version,id,errorstatus,errorindex;
   octet_string_t community = { .len = 0 };
   uint32_t *oid2 = NULL;
   int result = 0;

   d.buf = (uint8_t *) buf;
   d.size = ARRAYSIZE (buf);
printf("SIZE: %d", ARRAYSIZE(buf));
   d.offset = 0;

   if (ber_decode_sequence (&d) < 0 ||
	   ber_decode_integer (&version,&d) < 0 || version ||
	   ber_decode_octet_string (&community,&d) < 0 || oscmp (&community,"public") ||
	   ber_decode_get_request (&d) < 0 ||
	   ber_decode_integer (&id,&d) < 0 || id != 1246626753 ||
	   ber_decode_integer (&errorstatus,&d) < 0 || errorstatus ||
	   ber_decode_integer (&errorindex,&d) < 0 || errorindex ||
	   ber_decode_sequence (&d) < 0 ||
	   ber_decode_sequence (&d) < 0 ||
	   ber_decode_oid (&oid2,&d) < 0 || oidcmp (oid2,oid) ||
	   ber_decode_null (&d) < 0 ||
	   d.offset != d.size)
	 result = -1;

   if (oid2 != NULL) free (oid2);
   if (community.len) free (community.buf);

   return (result);
}

static int test_decode_2 (void)
{
   static const uint32_t oid[] = { 10, 43, 6, 1, 2, 1, 2, 2, 1, 10, 1 };
   static const uint8_t buf[] =
	 {
		0x30, 0x2f, 0x02, 0x01, 0x00, 0x04, 0x06, 0x70,
		0x75, 0x62, 0x6c, 0x69, 0x63, 0xa2, 0x22, 0x02,
		0x04, 0x4a, 0x4e, 0x03, 0xc1, 0x02, 0x01, 0x00,
		0x02, 0x01, 0x00, 0x30, 0x14, 0x30, 0x12, 0x06,
		0x0a, 0x2b, 0x06, 0x01, 0x02, 0x01, 0x02, 0x02,
		0x01, 0x0a, 0x01, 0x41, 0x04, 0x3f, 0x5e, 0xf5,
		0xf6
	 };
   ber_t d;
   int32_t version,id,errorstatus,errorindex;
   octet_string_t community = { .len = 0 };
   uint32_t *oid2 = NULL;
   int result = 0;
   uint32_t counter32;

   d.buf = (uint8_t *) buf;
   d.size = ARRAYSIZE (buf);
   d.offset = 0;

   if (ber_decode_sequence (&d) < 0 ||
	   ber_decode_integer (&version,&d) < 0 || version ||
	   ber_decode_octet_string (&community,&d) < 0 || oscmp (&community,"public") ||
	   ber_decode_get_response (&d) < 0 ||
	   ber_decode_integer (&id,&d) < 0 || id != 1246626753 ||
	   ber_decode_integer (&errorstatus,&d) < 0 || errorstatus ||
	   ber_decode_integer (&errorindex,&d) < 0 || errorindex ||
	   ber_decode_sequence (&d) < 0 ||
	   ber_decode_sequence (&d) < 0 ||
	   ber_decode_oid (&oid2,&d) < 0 || oidcmp (oid2,oid) ||
	   ber_decode_counter32 (&counter32,&d) < 0 || counter32 != 1063187958 ||
	   d.offset != d.size)
	 result = -1;

   if (oid2 != NULL) free (oid2);
   if (community.len) free (community.buf);

   return (result);
}

static int test_decode_3 (void)
{
   static const uint32_t oid1[] = { 10, 43, 6, 1, 2, 1, 2, 2, 1, 10, 1 };
   static const uint32_t oid2[] = { 10, 43, 6, 1, 2, 1, 2, 2, 1, 16, 1 };
   static const uint32_t oid3[] = { 10, 43, 6, 1, 2, 1, 2, 2, 1, 13, 1 };
   static const uint32_t oid4[] = { 10, 43, 6, 1, 2, 1, 2, 2, 1, 19, 1 };
   static const uint32_t oid5[] = { 10, 43, 6, 1, 2, 1, 2, 2, 1, 14, 1 };
   static const uint32_t oid6[] = { 10, 43, 6, 1, 2, 1, 2, 2, 1, 20, 1 };
   static const uint32_t oid7[] = { 10, 43, 6, 1, 2, 1, 2, 2, 1, 7, 1 };
   static const uint32_t oid8[] = { 10, 43, 6, 1, 2, 1, 2, 2, 1, 8, 1 };
   static const uint8_t buf[] =
	 {
		0x30, 0x81, 0x9d, 0x02, 0x01, 0x00, 0x04, 0x06,
		0x70, 0x75, 0x62, 0x6c, 0x69, 0x63, 0xa0, 0x81,
		0x8f, 0x02, 0x04, 0x34, 0xc0, 0x3a, 0xa1, 0x02,
		0x01, 0x00, 0x02, 0x01, 0x00, 0x30, 0x81, 0x80,
		0x30, 0x0e, 0x06, 0x0a, 0x2b, 0x06, 0x01, 0x02,
		0x01, 0x02, 0x02, 0x01, 0x0a, 0x01, 0x05, 0x00,
		0x30, 0x0e, 0x06, 0x0a, 0x2b, 0x06, 0x01, 0x02,
		0x01, 0x02, 0x02, 0x01, 0x10, 0x01, 0x05, 0x00,
		0x30, 0x0e, 0x06, 0x0a, 0x2b, 0x06, 0x01, 0x02,
		0x01, 0x02, 0x02, 0x01, 0x0d, 0x01, 0x05, 0x00,
		0x30, 0x0e, 0x06, 0x0a, 0x2b, 0x06, 0x01, 0x02,
		0x01, 0x02, 0x02, 0x01, 0x13, 0x01, 0x05, 0x00,
		0x30, 0x0e, 0x06, 0x0a, 0x2b, 0x06, 0x01, 0x02,
		0x01, 0x02, 0x02, 0x01, 0x0e, 0x01, 0x05, 0x00,
		0x30, 0x0e, 0x06, 0x0a, 0x2b, 0x06, 0x01, 0x02,
		0x01, 0x02, 0x02, 0x01, 0x14, 0x01, 0x05, 0x00,
		0x30, 0x0e, 0x06, 0x0a, 0x2b, 0x06, 0x01, 0x02,
		0x01, 0x02, 0x02, 0x01, 0x07, 0x01, 0x05, 0x00,
		0x30, 0x0e, 0x06, 0x0a, 0x2b, 0x06, 0x01, 0x02,
		0x01, 0x02, 0x02, 0x01, 0x08, 0x01, 0x05, 0x00
	 };
   ber_t d;
   int32_t version,id,errorstatus,errorindex;
   octet_string_t community = { .len = 0 };
   uint32_t *oid[8] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };
   int i,result = 0;

   d.buf = (uint8_t *) buf;
   d.size = ARRAYSIZE (buf);
   d.offset = 0;

   if (ber_decode_sequence (&d) < 0 ||
	   ber_decode_integer (&version,&d) < 0 || version ||
	   ber_decode_octet_string (&community,&d) < 0 || oscmp (&community,"public") ||
	   ber_decode_get_request (&d) < 0 ||
	   ber_decode_integer (&id,&d) < 0 || id != 885013153 ||
	   ber_decode_integer (&errorstatus,&d) < 0 || errorstatus ||
	   ber_decode_integer (&errorindex,&d) < 0 || errorindex ||
	   ber_decode_sequence (&d) < 0 ||

	   ber_decode_sequence (&d) < 0 ||
	   ber_decode_oid (&oid[0],&d) < 0 || oidcmp (oid[0],oid1) ||
	   ber_decode_null (&d) < 0 ||

	   ber_decode_sequence (&d) < 0 ||
	   ber_decode_oid (&oid[1],&d) < 0 || oidcmp (oid[1],oid2) ||
	   ber_decode_null (&d) < 0 ||

	   ber_decode_sequence (&d) < 0 ||
	   ber_decode_oid (&oid[2],&d) < 0 || oidcmp (oid[2],oid3) ||
	   ber_decode_null (&d) < 0 ||

	   ber_decode_sequence (&d) < 0 ||
	   ber_decode_oid (&oid[3],&d) < 0 || oidcmp (oid[3],oid4) ||
	   ber_decode_null (&d) < 0 ||

	   ber_decode_sequence (&d) < 0 ||
	   ber_decode_oid (&oid[4],&d) < 0 || oidcmp (oid[4],oid5) ||
	   ber_decode_null (&d) < 0 ||

	   ber_decode_sequence (&d) < 0 ||
	   ber_decode_oid (&oid[5],&d) < 0 || oidcmp (oid[5],oid6) ||
	   ber_decode_null (&d) < 0 ||

	   ber_decode_sequence (&d) < 0 ||
	   ber_decode_oid (&oid[6],&d) < 0 || oidcmp (oid[6],oid7) ||
	   ber_decode_null (&d) < 0 ||

	   ber_decode_sequence (&d) < 0 ||
	   ber_decode_oid (&oid[7],&d) < 0 || oidcmp (oid[7],oid8) ||
	   ber_decode_null (&d) < 0 ||

	   d.offset != d.size)
	 result = -1;

   for (i = 0; i < 8; i++) if (oid[i] != NULL) free (oid[i]);
   if (community.len) free (community.buf);

   return (result);
}

static int test_decode_6 (void)
{
   static const uint8_t buf[] =
	 {
		0x30, 0x82, 0x00, 0x37, 0x02, 0x01, 0x00, 0x04,
		0x06, 0x70, 0x75, 0x62, 0x6c, 0x69, 0x63, 0xa2,
		0x82, 0x00, 0x28, 0x02, 0x04, 0x3c, 0x23, 0xfd,
		0xbb, 0x02, 0x01, 0x00, 0x02, 0x01, 0x00, 0x30,
		0x82, 0x00, 0x18, 0x30, 0x82, 0x00, 0x14, 0x06,
		0x0d, 0x2b, 0x06, 0x01, 0x04, 0x01, 0x9b, 0x27,
		0x01, 0x03, 0x02, 0x01, 0x0a, 0x00, 0x02, 0x03,
		0x10, 0x20, 0xca
	 };
   ber_t d;

   d.buf = (uint8_t *) buf;
   d.size = ARRAYSIZE (buf);
   d.offset = 0;

   if (ber_decode_sequence (&d) < 0) return (-1);

   return (0);
}

static int test_decode_7 ()
{
   static const uint8_t buf[] =
	 {
		0x30, 0x81, 0xad, 0x02, 0x01, 0x00, 0x04, 0x06,
		0x70, 0x75, 0x62, 0x6c, 0x69, 0x63, 0xa2, 0x81,
		0x9f, 0x02, 0x04, 0x3c, 0x27, 0xb7, 0x7f, 0x02,
		0x01, 0x00, 0x02, 0x01, 0x00, 0x30, 0x81, 0x90,
		0x30, 0x12, 0x06, 0x0a, 0x2b, 0x06, 0x01, 0x02,
		0x01, 0x02, 0x02, 0x01, 0x0a, 0x01, 0x41, 0x04,
		0x5f, 0xcf, 0xef, 0x95, 0x30, 0x13, 0x06, 0x0a,
		0x2b, 0x06, 0x01, 0x02, 0x01, 0x02, 0x02, 0x01,
		0x10, 0x01, 0x41, 0x05, 0x00, 0xac, 0xc5, 0x82,
		0x02, 0x30, 0x0f, 0x06, 0x0a, 0x2b, 0x06, 0x01,
		0x02, 0x01, 0x02, 0x02, 0x01, 0x0d, 0x01, 0x41,
		0x01, 0x00, 0x30, 0x0f, 0x06, 0x0a, 0x2b, 0x06,
		0x01, 0x02, 0x01, 0x02, 0x02, 0x01, 0x13, 0x01,
		0x41, 0x01, 0x00, 0x30, 0x0f, 0x06, 0x0a, 0x2b,
		0x06, 0x01, 0x02, 0x01, 0x02, 0x02, 0x01, 0x0e,
		0x01, 0x41, 0x01, 0x00, 0x30, 0x10, 0x06, 0x0a,
		0x2b, 0x06, 0x01, 0x02, 0x01, 0x02, 0x02, 0x01,
		0x14, 0x01, 0x41, 0x02, 0x33, 0x36, 0x30, 0x0f,
		0x06, 0x0a, 0x2b, 0x06, 0x01, 0x02, 0x01, 0x02,
		0x02, 0x01, 0x07, 0x01, 0x02, 0x01, 0x01, 0x30,
		0x0f, 0x06, 0x0a, 0x2b, 0x06, 0x01, 0x02, 0x01,
		0x02, 0x02, 0x01, 0x08, 0x01, 0x02, 0x01, 0x01
	 };
   ber_t d;
   int32_t version,id,errorstatus,errorindex;
   octet_string_t community = { .len = 0 };
   uint32_t *oid,counter32;

   d.buf = (uint8_t *) buf;
   d.size = ARRAYSIZE (buf);
   d.offset = 0;

   if (ber_decode_sequence (&d) < 0) return (-1);
   if (ber_decode_integer (&version,&d) < 0 || version) return (-1);
   if (ber_decode_octet_string (&community,&d) < 0 || oscmp (&community,"public"))
	 {
		if (community.len) free (community.buf);
		return (-1);
	 }
   if (community.len) free (community.buf);
   if (ber_decode_get_response (&d) < 0) return (-1);
   if (ber_decode_integer (&id,&d) < 0) return (-1);
   if (ber_decode_integer (&errorstatus,&d) < 0 || errorstatus) return (-1);
   if (ber_decode_integer (&errorindex,&d) < 0 || errorindex) return (-1);
   if (ber_decode_sequence (&d) < 0) return (-1);

   /* printf (LOG_DEBUG,"ifInOctets\n"); */
   if (ber_decode_sequence (&d) < 0) return (-1);
   if (ber_decode_oid (&oid,&d) < 0) return (-1);
   free (oid);
   if (ber_decode_counter32 (&counter32,&d) < 0) return (-1);

   /* printf (LOG_DEBUG,"ifOutOctets\n"); */
   if (ber_decode_sequence (&d) < 0) return (-1);
   if (ber_decode_oid (&oid,&d) < 0) return (-1);
   free (oid);
   if (ber_decode_counter32 (&counter32,&d) < 0) return (-1);

   /* printf (LOG_DEBUG,"ifInDiscards\n"); */
   if (ber_decode_sequence (&d) < 0) return (-1);
   if (ber_decode_oid (&oid,&d) < 0) return (-1);
   free (oid);
   if (ber_decode_counter32 (&counter32,&d) < 0) return (-1);

   /* printf (LOG_DEBUG,"ifOutDiscards\n"); */
   if (ber_decode_sequence (&d) < 0) return (-1);
   if (ber_decode_oid (&oid,&d) < 0) return (-1);
   free (oid);
   if (ber_decode_counter32 (&counter32,&d) < 0) return (-1);

   /* printf (LOG_DEBUG,"ifInErrors\n"); */
   if (ber_decode_sequence (&d) < 0) return (-1);
   if (ber_decode_oid (&oid,&d) < 0) return (-1);
   free (oid);
   if (ber_decode_counter32 (&counter32,&d) < 0) return (-1);

   /* printf (LOG_DEBUG,"ifOutErrors\n"); */
   if (ber_decode_sequence (&d) < 0) return (-1);
   if (ber_decode_oid (&oid,&d) < 0) return (-1);
   free (oid);
   if (ber_decode_counter32 (&counter32,&d) < 0) return (-1);

   return (0);
}

static int test_decode_4 ()
{
   static const uint8_t buf[] = { 0x30, 0x82, 0x01, 0xfa };
   ber_t d;

   d.offset = 0;
   d.size = 65535;
   d.buf = (uint8_t *) buf;

   if (ber_decode_sequence (&d) < 0 ||
	   d.buf[0] != buf[0] ||
	   d.buf[1] != buf[1] ||
	   d.buf[2] != buf[2] ||
	   d.buf[3] != buf[3])
	 return (-1);

   return (0);
}

static int test_decode_5 ()
{
   static const uint32_t oid[] =
	 {
		26, 1, 1450, 2, 5, 768, 127, 128, 4294967295U, 1, 128, 256, 512,
		1024, 2048, 4096, 8192, 16384, 32768, 65536, 128000, 256000,
		512000, 1024000, 2048000, 4096000, 8192000
	 };
   static const uint8_t buf[] =
	 {
		0x06, 0x3e, 0x01, 0x8b, 0x2a, 0x02, 0x05, 0x86,
		0x00, 0x7f, 0x81, 0x00, 0x8f, 0xff, 0xff, 0xff,
		0x7f, 0x01, 0x81, 0x00, 0x82, 0x00, 0x84, 0x00,
		0x88, 0x00, 0x90, 0x00, 0xa0, 0x00, 0xc0, 0x00,
		0x81, 0x80, 0x00, 0x82, 0x80, 0x00, 0x84, 0x80,
		0x00, 0x87, 0xe8, 0x00, 0x8f, 0xd0, 0x00, 0x9f,
		0xa0, 0x00, 0xbe, 0xc0, 0x00, 0xfd, 0x80, 0x00,
		0x81, 0xfa, 0x80, 0x00, 0x83, 0xf4, 0x80, 0x00
	 };
   ber_t d;
   uint32_t *oid2;

   d.offset = 0;
   d.size = ARRAYSIZE (buf);
   d.buf = (uint8_t *) buf;

   if (ber_decode_oid (&oid2,&d) < 0) return (-1);

   if (oidcmp (oid2,oid))
	 {
		free (oid2);
		return (-1);
	 }

   free (oid2);

   return (0);
}

static int test_decode_8 ()
{
   static const uint32_t oid[14] = { 13, 43, 6, 1, 2, 1, 4, 21, 1, 1, 66, 8, 28, 17 };
   static const uint8_t buf[] =
	 {
		0x30, 0x32, 0x02, 0x01, 0x00, 0x04, 0x06, 0x70,
		0x75, 0x62, 0x6c, 0x69, 0x63, 0xa2, 0x25, 0x02,
		0x04, 0x2a, 0xa1, 0xac, 0x76, 0x02, 0x01, 0x00,
		0x02, 0x01, 0x00, 0x30, 0x17, 0x30, 0x15, 0x06,
		0x0d, 0x2b, 0x06, 0x01, 0x02, 0x01, 0x04, 0x15,
		0x01, 0x01, 0x42, 0x08, 0x1c, 0x11, 0x40, 0x04,
		0x42, 0x08, 0x1c, 0x11
	 };
   ber_t d;
   uint32_t *oid2,ip;
   int32_t version,id,errorstatus,errorindex;
   octet_string_t community = { .len = 0 };

   d.offset = 0;
   d.size = ARRAYSIZE (buf);
   d.buf = (uint8_t *) buf;

   if (ber_decode_sequence (&d) < 0) return (-1);
   if (ber_decode_integer (&version,&d) < 0 || version) return (-1);
   if (ber_decode_octet_string (&community,&d) < 0 || oscmp (&community,"public"))
	 {
		if (community.len) free (community.buf);
		return (-1);
	 }
   if (community.len) free (community.buf);
   if (ber_decode_get_response (&d) < 0) return (-1);
   if (ber_decode_integer (&id,&d) < 0) return (-1);
   if (ber_decode_integer (&errorstatus,&d) < 0 || errorstatus) return (-1);
   if (ber_decode_integer (&errorindex,&d) < 0 || errorindex) return (-1);
   if (ber_decode_sequence (&d) < 0) return (-1);

   if (ber_decode_sequence (&d) < 0) return (-1);
   if (ber_decode_oid (&oid2,&d) < 0) return (-1);
   if (oidcmp (oid,oid2))
	 {
		free (oid2);
		return (-1);
	 }
   free (oid2);
   if (ber_decode_ipaddress (&ip,&d) < 0) return (-1);
   if (ip != 0x111c0842) return (-1);

   return (0);
}

static int test_decode_9 ()
{
   static const uint32_t oid[14] = { 13, 43, 6, 1, 2, 1, 4, 21, 1, 4, 0, 0, 0, 0 };
   static const uint8_t buf[] =
	 {
		0x30, 0x2f, 0x02, 0x01, 0x00, 0x04, 0x06, 0x70,
		0x75, 0x62, 0x6c, 0x69, 0x63, 0xa2, 0x22, 0x02,
		0x04, 0x64, 0x62, 0x75, 0x33, 0x02, 0x01, 0x00,
		0x02, 0x01, 0x00, 0x30, 0x14, 0x30, 0x12, 0x06,
		0x0d, 0x2b, 0x06, 0x01, 0x02, 0x01, 0x04, 0x15,
		0x01, 0x04, 0x00, 0x00, 0x00, 0x00, 0x02, 0x01,
		0xff
	 };
   ber_t d;
   uint32_t *oid2;
   int32_t version,id,errorstatus,errorindex,n;
   octet_string_t community = { .len = 0 };

   d.offset = 0;
   d.size = ARRAYSIZE (buf);
   d.buf = (uint8_t *) buf;

   if (ber_decode_sequence (&d) < 0) return (-1);
   if (ber_decode_integer (&version,&d) < 0 || version) return (-1);
   if (ber_decode_octet_string (&community,&d) < 0 || oscmp (&community,"public"))
	 {
		if (community.len) free (community.buf);
		return (-1);
	 }
   if (community.len) free (community.buf);
   if (ber_decode_get_response (&d) < 0) return (-1);
   if (ber_decode_integer (&id,&d) < 0) return (-1);
   if (ber_decode_integer (&errorstatus,&d) < 0 || errorstatus) return (-1);
   if (ber_decode_integer (&errorindex,&d) < 0 || errorindex) return (-1);
   if (ber_decode_sequence (&d) < 0) return (-1);

   if (ber_decode_sequence (&d) < 0) return (-1);
   if (ber_decode_oid (&oid2,&d) < 0) return (-1);
   if (oidcmp (oid,oid2))
	 {
		free (oid2);
		return (-1);
	 }
   free (oid2);
   if (ber_decode_integer (&n,&d) < 0) return (-1);
   if (n != -1) return (-1);

   return (0);
}

static int test_decode_10 ()
{
   ber_t e;
   uint8_t buf[1024];
   uint64_t u64;
   uint32_t u32;
   int32_t s32;

   e.buf = buf;
   e.size = sizeof (buf);
   e.offset = 0;

   if (ber_encode_integer (&e,0) < 0 ||
	   ber_encode_integer (&e,-1) < 0 ||
	   ber_encode_integer (&e,INT32_MAX) < 0 ||
	   ber_encode_integer (&e,INT32_MIN) < 0 ||
	   ber_encode_counter32 (&e,UINT32_MAX) < 0 ||
	   ber_encode_counter64 (&e,UINT64_MAX) < 0)
	 return (-1);

   e.buf = e.buf + e.size - e.offset;
   e.size = e.offset;
   e.offset = 0;

   if (ber_decode_counter64 (&u64,&e) < 0 || u64 != UINT64_MAX)
	 return (-1);

   if (ber_decode_counter32 (&u32,&e) < 0 || u32 != UINT32_MAX)
	 return (-1);

   if (ber_decode_integer (&s32,&e) < 0 || s32 != INT32_MIN)
	 return (-1);

   if (ber_decode_integer (&s32,&e) < 0 || s32 != INT32_MAX)
	 return (-1);

   if (ber_decode_integer (&s32,&e) < 0 || s32 != -1)
	 return (-1);

   if (ber_decode_integer (&s32,&e) < 0 || s32)
	 return (-1);

   return (0);
}

#ifdef CNET_SWITCH_WORKAROUND
static int test_decode_11 ()
{
   static const uint32_t oid[11] = { 10, 43, 6, 1, 2, 1, 2, 2, 1, 2, 2001 };
   static const uint8_t buf[] =
	 {
		0x30, 0x82, 0x01, 0x34, 0x02, 0x01, 0x00, 0x04,
		0x06, 0x70, 0x75, 0x62, 0x6c, 0x69, 0x63, 0xa2,
		0x82, 0x01, 0x25, 0x02, 0x04, 0x3f, 0xdf, 0xe4,
		0xf7, 0x02, 0x01, 0x00, 0x02, 0x01, 0x00, 0x30,
		0x82, 0x01, 0x15, 0x30, 0x82, 0x01, 0x11, 0x06,
		0x0b, 0x2b, 0x06, 0x01, 0x02, 0x01, 0x02, 0x02,
		0x01, 0x02, 0x8f, 0x51, 0x04, 0x82, 0x01, 0x00,
		0x56, 0x69, 0x72, 0x74, 0x75, 0x61, 0x6c, 0x20,
		0x52, 0x6f, 0x75, 0x74, 0x69, 0x6e, 0x67, 0x20,
		0x49, 0x6e, 0x74, 0x65, 0x72, 0x66, 0x61, 0x63,
		0x65, 0x20, 0x30, 0x2f, 0x31, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	 };
   ber_t d =
	 {
		.offset	= 0,
		.size	= ARRAYSIZE (buf),
		.buf	= (uint8_t *) buf
	 };
   uint32_t *oid2;
   int32_t version,id,status,index;
   octet_string_t community = { .len = 0 };
   octet_string_t str;

   if (ber_decode_sequence (&d) ||
	   ber_decode_integer (&version,&d) || version ||
	   ber_decode_octet_string (&community,&d))
	 return (-1);

   if (oscmp (&community,"public"))
	 {
		if (community.len)
		  free (community.buf);

		return (-1);
	 }

   if (community.len)
	 free (community.buf);

   if (ber_decode_get_response (&d) ||
	   ber_decode_integer (&id,&d) || id != 0x3fdfe4f7 ||
	   ber_decode_integer (&status,&d) || status ||
	   ber_decode_integer (&index,&d) || index ||
	   ber_decode_sequence (&d) ||
	   ber_decode_sequence (&d) ||
	   ber_decode_oid (&oid2,&d))
	 return (-1);

   if (oidcmp (oid,oid2))
	 {
		free (oid2);
		return (-1);
	 }

   free (oid2);

   if (ber_decode_octet_string (&str,&d))
	 return (-1);

   if (oscmp (&str,"Virtual Routing Interface 0/1"))
	 {
		if (str.len)
		  free (str.buf);

		return (-1);
	 }

   if (str.len)
	 free (str.buf);

   return (0);
}
#endif	/* #ifdef CNET_SWITCH_WORKAROUND */

typedef int (*test_decode_t)();

static void test_decode ()
{
   int i;
   static const test_decode_t test[] =
	 {
		test_decode_1,
		test_decode_2,
		test_decode_3,
		test_decode_4,
		test_decode_5,
		test_decode_6,
		test_decode_7,
		test_decode_8,
		test_decode_9,
		test_decode_10,
#ifdef CNET_SWITCH_WORKAROUND
		test_decode_11
#endif	/* #ifdef CNET_SWITCH_WORKAROUND */
	 };

   for (i = 0; i < 11; i++)
	 {
		if (test[i] () < 0)
		  {
			 printf ("decoding test %d failed\n",i + 1);
			 exit (EXIT_FAILURE);
		  }

		printf ("decoding test %d succeeded\n",i + 1);
	 }
}

int main ()
{
/*
   mem_open (NULL);
   log_open (NULL,LOG_NOISY,LOG_HAVE_COLORS);
   atexit (log_close);
   atexit (mem_close);
*/
   test_decode ();

   exit (EXIT_SUCCESS);
}

