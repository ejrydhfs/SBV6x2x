/*-------------------------------------------------------------------------------------
// Copyright 2007, Texas Instruments Incorporated
//
// This program has been modified from its original operation by Texas Instruments
// to do the following:
// Separated all the commoon include files into this header file.
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
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/queue.h>
#include <errno.h>
#include <limits.h>
#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif
#include <net/if.h>
#ifdef __FreeBSD__
#include <net/if_var.h>
#endif
#include <netinet/in.h>
#ifdef __KAME__
#include <net/if_dl.h>
#include <netinet6/in6_var.h>
#endif

#include <arpa/inet.h>
#include <netdb.h>

#include <signal.h>
#include <stdio.h>
#include <stdarg.h>
#include <syslog.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <err.h>
#include <dlfcn.h>

#ifdef HAVE_ANSI_FUNC
#define FNAME __func__
#elif defined (HAVE_GCC_FUNCTION)
#define FNAME __FUNCTION__
#else
#define FNAME ""
#endif

typedef enum DHCP6C_ERROR
{
	TI_DHCP6C_SUCCESS, /* =0 */
	TI_DHCP6C_RFC_VIOLATION, /* If there is a RFC violation in the received packet */
    TI_DHCP6C_DUID_ERROR,   /* DUID cannot be read / generated */
    TI_DHCP6C_INITLZN_ERROR,   /* Error when initializing the DHCP client */
	TI_DHCP6C_MAX_CONFIG_ATTEMPTS_REACHED, /* If Client attempted MAX_CONFIG_ATTEMPTS times
					   to obtain configuration from server */
	TI_DHCP6C_PLUGIN_REJECT, /* Option is rejected by Plugin */
	TI_DHCP6C_PLUGIN_ERROR, /* Plugin returns an error */
	TI_DHCP6C_SHORT_PKT_RECVD, /*  packet too short */
	TI_DHCP6C_TRUNCATED_PKT_RECVD, /* packet truncated */
	TI_DHCP6C_BOGUS_PKT_RECVD, /* packet bogus - IP version incorrect, port incorrect etc */
	TI_DHCP6C_SOCKET_ERROR,  /* socket read error */	
	TI_DHCP6C_INTERNAL_ERROR, /* Error internal to Core */
	TI_DHCP6C_XID_MISMATCH, /* XID receieved by application is not same as xid sent in our last message */
	TI_DHCP6C_INVALID_SERVERID, /* No ServerID present in packets recvd from server*/
	TI_DHCP6C_INVALID_CLIENTID, /* No ClientID present / Invalid ClientID in packets recvd from server*/
	TI_DHCP6C_AUTH_FAILED, /* DHCP message could not be process auth params */
	TI_DHCP6C_STATUS_ERROR, /* DHCP message receieved contains a status which denotes an error */
	TI_DHCP6C_MALLOC_FAILURE, /* Error allocating memory */
	TI_DHCP6C_MRC_EXCEEDED, /* MRC Exceeded */
	TI_DHCP6C_MRD_EXCEEDED, /* MRD Exceeded */
	TI_DHCP6C_STATUS_NOADDRPREFIXAVAIL, /* Status code recd is No Prefix Avail/No Addr Avail */
	TI_DHCP6C_STATUS_NOTONLINK, /* Status code recd is NotOnLink */
	TI_DHCP6C_BAD_MSGTYPE_RECONF, /* Reconfigure message doesnt have msg_type option */
	TI_DHCP6C_ERROR, /* General error */
	TI_DHCP6C_ERROR_LAST, 
} DHCP6C_ERROR;

/* Data structure to hold basic authentication params */
struct auth_basic_params {
	u_int8_t authproto;
	u_int8_t authalgorithm;
	u_int8_t authrdm;
	u_int64_t authrd;
};

/* Message type */
#define DH6_SOLICIT	1
#define DH6_ADVERTISE	2
#define DH6_REQUEST	3
#define DH6_CONFIRM	4
#define DH6_RENEW	5
#define DH6_REBIND	6
#define DH6_REPLY	7
#define DH6_RELEASE	8
#define DH6_DECLINE	9
#define DH6_RECONFIGURE	10
#define DH6_INFORM_REQ	11
#define DH6_RELAY_FORW	12
#define DH6_RELAY_REPLY	13

/* DUID: DHCP unique Identifier */
struct duid {
	size_t duid_len;	/* length */
	char *duid_id;		/* variable length ID value (must be opaque) */
};

struct dhcp6_vbuf {		/* generic variable length buffer */
	unsigned int dv_len;
	caddr_t dv_buf;
};

/* option information */
struct dhcp6_ia {		/* identity association */
	u_int32_t iaid;
	u_int32_t t1;
	u_int32_t t2;
};

struct dhcp6_prefix {		/* IA_PA */
	u_int32_t pltime;
	u_int32_t vltime;
	u_int8_t plen;
	struct in6_addr addr;
};

struct dhcp6_statefuladdr {	/* IA_NA */
	struct in6_addr addr;
	u_int32_t pltime;
	u_int32_t vltime;
};

/* options */
#define DH6OPT_CLIENTID	1
#define DH6OPT_SERVERID	2
#define DH6OPT_IA_NA 3
#define DH6OPT_IA_TA 4
#define DH6OPT_IAADDR 5
#define DH6OPT_ORO 6
#define DH6OPT_PREFERENCE 7
#  define DH6OPT_PREF_UNDEF -1
#  define DH6OPT_PREF_MAX 255
#define DH6OPT_ELAPSED_TIME 8
#  define DH6OPT_ELAPSED_TIME_UNDEF -1
#define DH6OPT_RELAY_MSG 9
/* #define DH6OPT_SERVER_MSG 10: deprecated */
#define DH6OPT_AUTH 11
#  define DH6OPT_AUTH_PROTO_DELAYED 2
#  define DH6OPT_AUTH_RRECONFIGURE 3
#  define DH6OPT_AUTH_ALG_HMACMD5 1
#define DH6OPT_UNICAST 12
#define DH6OPT_STATUS_CODE 13
#  define DH6OPT_STCODE_SUCCESS 0
#  define DH6OPT_STCODE_UNSPECFAIL 1
#  define DH6OPT_STCODE_NOADDRSAVAIL 2
#  define DH6OPT_STCODE_NOBINDING 3
#  define DH6OPT_STCODE_NOTONLINK 4
#  define DH6OPT_STCODE_USEMULTICAST 5
#  define DH6OPT_STCODE_NOPREFIXAVAIL 6

#define DH6OPT_RAPID_COMMIT 14
#define DH6OPT_USER_CLASS 15
#define DH6OPT_VENDOR_CLASS 16
#define DH6OPT_VENDOR_OPTS 17
#define DH6OPT_INTERFACE_ID 18
#define DH6OPT_RECONF_MSG 19
#   define DH6OPT_RECONF_MSG_TYPE_UNDEF -1

#define DH6OPT_RECONF_ACCEPT 20

#define DH6OPT_SIP_SERVER_D 21
#define DH6OPT_SIP_SERVER_A 22
#define DH6OPT_DNS 23
#define DH6OPT_DNSNAME 24
#define DH6OPT_IA_PD 25
#define DH6OPT_IA_PD_PREFIX 26
#define DH6OPT_NIS_SERVERS 27
#define DH6OPT_NISP_SERVERS 28
#define DH6OPT_NIS_DOMAIN_NAME 29
#define DH6OPT_NISP_DOMAIN_NAME 30
#define DH6OPT_NTP 31
#define DH6OPT_REFRESHTIME 32
 #define DH6OPT_REFRESHTIME_UNDEF -1
#define DH6OPT_BCMCS_SERVER_D 33
#define DH6OPT_BCMCS_SERVER_A 34
#define DH6OPT_GEOCONF_CIVIC 36
#define DH6OPT_REMOTE_ID 37
#define DH6OPT_SUBSCRIBER_ID 38
#define DH6OPT_CLIENT_FQDN 39

#define DH6_AUTH_RECONF_TYPE_KEY 1
#define DH6_AUTH_RECONF_TYPE_HMACMD5 2 

enum { DHCP6_AUTHPROTO_UNDEF = -1, DHCP6_AUTHPROTO_DELAYED = 2,
       DHCP6_AUTHPROTO_RECONFIG = 3 };
enum { DHCP6_AUTH_RECONF_TYPE_UNDEF = -1, DHCP6_AUTH_RECONF_TYPE_KEY = 1,
       DHCP6_AUTH_RECONF_TYPE_HMACMD5 = 2 };
enum { DHCP6_AUTHALG_UNDEF = -1, DHCP6_AUTHALG_HMACMD5 = 1 };
enum { DHCP6_AUTHRDM_UNDEF = -1, DHCP6_AUTHRDM_MONOCOUNTER = 0 };

struct dhcp6opt {
	u_int16_t dh6opt_type;
	u_int16_t dh6opt_len;
	/* type-dependent data follows */
} __attribute__ ((__packed__));

/* secret key information for delayed authentication */
struct keyinfo {
	struct keyinfo *next;

	char *name;		/* key name */

	char *realm;		/* DHCP realm */
	size_t realmlen;	/* length of realm */
	u_int32_t keyid;	/* key ID */
	char *secret;		/* binary key */
	size_t secretlen;	/* length of the key */
	time_t expire;		/* expiration time (0 means forever) */
};

extern void dprintf __P((int, const char *, const char *, ...));

extern char *dhcp6optstr __P((int));
extern char *dhcp6msgstr __P((int));
extern char *dhcp6_stcodestr __P((u_int16_t));
extern char *duidstr __P((struct duid *));
extern char *in6addr2str __P((struct in6_addr *, int));

/* TI defined functions */
extern int ti_dhcp6c_copy_option __P((u_int16_t, u_int16_t, void *, struct dhcp6opt **, struct dhcp6opt *, int *, int));

