
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

/*
#include <debug/log.h>
#include <debug/hex.h>
#include <debug/memory.h>
*/

#include <ber/ber.h>

static void test_makeoid_fail (const char *str)
{
   uint32_t *oid;

   if ((oid = makeoid (str)) != NULL)
	 {
		printf ("makeoid(%s) succeeded, but should've failed\n",str);
		free (oid);
		exit (EXIT_FAILURE);
	 }

   printf ("makeoid(%s) fail test succeeded: \n",str);
}

static void test_makeoid_succeed (const char *str,const uint32_t *oid,uint32_t len)
{
   uint32_t *tmp;

   if ((tmp = makeoid (str)) == NULL)
	 {
		printf ("makeoid(%s) failed, but should've succeeded\n",str);
		exit (EXIT_FAILURE);
	 }

   if (tmp[0] != oid[0] || memcmp (tmp,oid,len * sizeof (uint32_t)))
	 {
		printf ("makeoid(%s) failed: invalid answer\n",str);
		free (tmp);
		exit (EXIT_FAILURE);
	 }

   printf ("makeoid(%s) succeed test succeeded\n",str);

   free (tmp);
}

static void test_makeoid (void)
{
   static const uint32_t ccitt_0[] = { 1, 0 };
   static const uint32_t _2_39[] = { 1, 119 };
   static const uint32_t max_min[] = { 4, 42, 4294967295U, 0, 5 };
   static const uint32_t c7[] = { 7, 42, 8, 1, 2, 3, 2, 1 };
   static const uint32_t c8[] = { 8, 42, 8, 1, 2, 3, 2, 1, 1 };
   static const uint32_t c9[] = { 9, 42, 8, 1, 2, 3, 2, 1, 1, 1 };

   test_makeoid_fail (NULL);
   test_makeoid_fail ("0");
   test_makeoid_fail ("1");
   test_makeoid_fail ("4.1");
   test_makeoid_fail ("2.40");
   test_makeoid_fail ("1.2.3.4294967296.5");
   test_makeoid_fail ("1.2.3..4.5");
   test_makeoid_fail ("1.a.b.c.4");

   test_makeoid_succeed ("0.0",ccitt_0,ARRAYSIZE (ccitt_0));
   test_makeoid_succeed ("2.39",_2_39,ARRAYSIZE (_2_39));
   test_makeoid_succeed ("1.2.4294967295.0.5",max_min,ARRAYSIZE (max_min));

   test_makeoid_succeed ("1.2.8.1.2.3.2.1",c7,ARRAYSIZE (c7));
   test_makeoid_succeed ("1.2.8.1.2.3.2.1.1",c8,ARRAYSIZE (c8));
   test_makeoid_succeed ("1.2.8.1.2.3.2.1.1.1",c9,ARRAYSIZE (c9));
}

int main ()
{
/*
   mem_open (NULL);
   log_open (NULL,LOG_NOISY,LOG_HAVE_COLORS);
   atexit (log_close);
   atexit (mem_close);
*/
   test_makeoid ();

   exit (EXIT_SUCCESS);
}

