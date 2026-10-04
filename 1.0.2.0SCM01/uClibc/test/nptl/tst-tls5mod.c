#include <tst-tls5.h>

#ifdef TLS_REGISTER
/* Ensure tls_registry is exported from the binary.  */
void *tst_tls5mod __attribute__ ((visibility ("hidden"))) = tls_registry;
#endif
