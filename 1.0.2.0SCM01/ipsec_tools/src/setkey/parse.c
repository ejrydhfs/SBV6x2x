#ifndef lint
static char const 
yyrcsid[] = "$FreeBSD: src/usr.bin/yacc/skeleton.c,v 1.28 2000/01/17 02:04:06 bde Exp $";
#endif
#include <stdlib.h>
#define YYBYACC 1
#define YYMAJOR 1
#define YYMINOR 9
#define YYLEX yylex()
#define YYEMPTY -1
#define yyclearin (yychar=(YYEMPTY))
#define yyerrok (yyerrflag=0)
#define YYRECOVERING() (yyerrflag!=0)
static int yygrowstack();
#define YYPREFIX "yy"
#line 33 "parse.y"
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <net/pfkeyv2.h>
#ifdef HAVE_NETINET6_IPSEC
#  include <netinet6/ipsec.h>
#else 
#  include <netinet/ipsec.h>
#endif
#include <arpa/inet.h>

#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <netdb.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>

#include "libpfkey.h"
#include "vchar.h"
#include "extern.h"

#define DEFAULT_NATT_PORT	4500

#ifndef UDP_ENCAP_ESPINUDP
#define UDP_ENCAP_ESPINUDP	2
#endif

#define ATOX(c) \
  (isdigit((int)c) ? (c - '0') : \
    (isupper((int)c) ? (c - 'A' + 10) : (c - 'a' + 10)))

u_int32_t p_spi;
u_int p_ext, p_alg_enc, p_alg_auth, p_replay, p_mode;
u_int32_t p_reqid;
u_int p_key_enc_len, p_key_auth_len;
const char *p_key_enc;
const char *p_key_auth;
time_t p_lt_hard, p_lt_soft;
size_t p_lb_hard, p_lb_soft;

static u_int p_natt_type;
static struct addrinfo * p_natt_oa = NULL;

static int p_aiflags = 0, p_aifamily = PF_UNSPEC;

static struct addrinfo *parse_addr __P((char *, char *));
static int fix_portstr __P((vchar_t *, vchar_t *, vchar_t *));
static int setvarbuf __P((char *, int *, struct sadb_ext *, int, 
    const void *, int));
void parse_init __P((void));
void free_buffer __P((void));

int setkeymsg0 __P((struct sadb_msg *, unsigned int, unsigned int, size_t));
static int setkeymsg_spdaddr __P((unsigned int, unsigned int, vchar_t *,
	struct addrinfo *, int, struct addrinfo *, int));
static int setkeymsg_spdaddr_tag __P((unsigned int, char *, vchar_t *));
static int setkeymsg_addr __P((unsigned int, unsigned int,
	struct addrinfo *, struct addrinfo *, int));
static int setkeymsg_add __P((unsigned int, unsigned int,
	struct addrinfo *, struct addrinfo *));
#line 103 "parse.y"
typedef union {
	int num;
	unsigned long ulnum;
	vchar_t val;
	struct addrinfo *res;
} YYSTYPE;
#line 93 "y.tab.c"
#define YYERRCODE 256
#define EOT 257
#define SLASH 258
#define BLCL 259
#define ELCL 260
#define ADD 261
#define GET 262
#define DELETE 263
#define DELETEALL 264
#define FLUSH 265
#define DUMP 266
#define EXIT 267
#define PR_ESP 268
#define PR_AH 269
#define PR_IPCOMP 270
#define PR_ESPUDP 271
#define PR_TCP 272
#define F_PROTOCOL 273
#define F_AUTH 274
#define F_ENC 275
#define F_REPLAY 276
#define F_COMP 277
#define F_RAWCPI 278
#define F_MODE 279
#define MODE 280
#define F_REQID 281
#define F_EXT 282
#define EXTENSION 283
#define NOCYCLICSEQ 284
#define ALG_AUTH 285
#define ALG_AUTH_NOKEY 286
#define ALG_ENC 287
#define ALG_ENC_NOKEY 288
#define ALG_ENC_DESDERIV 289
#define ALG_ENC_DES32IV 290
#define ALG_ENC_OLD 291
#define ALG_COMP 292
#define F_LIFETIME_HARD 293
#define F_LIFETIME_SOFT 294
#define F_LIFEBYTE_HARD 295
#define F_LIFEBYTE_SOFT 296
#define DECSTRING 297
#define QUOTEDSTRING 298
#define HEXSTRING 299
#define STRING 300
#define ANY 301
#define SPDADD 302
#define SPDDELETE 303
#define SPDDUMP 304
#define SPDFLUSH 305
#define F_POLICY 306
#define PL_REQUESTS 307
#define F_AIFLAGS 308
#define TAGGED 309
const short yylhs[] = {                                        -1,
    0,    0,   11,   11,   11,   11,   11,   11,   11,   11,
   11,   11,   11,   12,   14,   15,   13,   16,   17,    2,
    2,    2,    2,    2,    2,    2,   24,   24,   26,   26,
   26,   27,   27,   28,   29,   29,   30,   30,   30,   30,
   30,   31,   31,    5,    5,   25,   25,   32,   32,   32,
   32,   32,   32,   32,   32,   32,   32,   19,   19,   20,
   21,   22,   23,   23,   33,    9,   10,   10,    1,    1,
    4,    4,    4,    4,    3,    3,    3,    3,    7,    7,
    8,    6,   18,
};
const short yylen[] = {                                         2,
    0,    2,    1,    1,    1,    1,    1,    1,    1,    1,
    1,    1,    1,    9,    8,    6,    8,    3,    3,    0,
    1,    1,    1,    1,    2,    1,    1,    1,    1,    1,
    1,    4,    2,    2,    2,    3,    1,    2,    1,    2,
    2,    2,    1,    1,    1,    0,    2,    2,    2,    2,
    2,    2,    2,    2,    2,    2,    2,   12,    5,   12,
    2,    2,    0,    2,    1,    1,    1,    2,    0,    2,
    0,    3,    3,    3,    1,    1,    1,    1,    0,    1,
    2,    1,    2,
};
const short yydefred[] = {                                      1,
    0,   63,   63,   63,   63,    0,    0,    0,    0,   63,
    0,    0,    2,    3,    4,    5,    6,    7,    8,    9,
   10,   11,   12,   13,    0,    0,    0,    0,   21,   22,
   23,    0,   26,    0,    0,   83,    0,    0,    0,   61,
   62,    0,   65,    0,   64,    0,    0,   66,    0,   25,
   18,   19,    0,    0,    0,    0,   68,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,   82,   81,   59,   70,    0,    0,   73,
   74,   72,   27,   28,   46,   46,   46,   16,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,   29,   30,   31,
   47,   17,   15,    0,    0,    0,   43,   34,    0,   37,
    0,    0,   39,    0,   53,    0,   50,   51,   52,   48,
   49,   54,   55,   56,   57,   14,   77,   75,   78,   76,
    0,    0,   44,   45,   42,   38,   40,   41,    0,   36,
   80,    0,    0,   32,    0,    0,   58,   60,
};
const short yydgoto[] = {                                       1,
   65,   34,  141,   57,  145,   75,  152,   63,   49,   44,
   13,   14,   15,   16,   17,   18,   19,   20,   21,   22,
   23,   24,   25,   85,   91,  107,  108,  109,  110,  124,
  118,  111,   45,
};
const short yysindex[] = {                                      0,
 -260,    0,    0,    0,    0, -141, -141, -228, -253,    0,
 -176, -168,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0, -281, -281, -281, -280,    0,    0,
    0, -192,    0, -147, -139,    0, -157, -278, -276,    0,
    0, -117,    0, -156,    0, -156, -156,    0, -192,    0,
    0,    0, -163, -113, -113, -209,    0, -141, -141, -141,
 -141, -161, -110, -149, -117, -117, -111, -109, -108, -237,
 -237, -237, -107,    0,    0,    0,    0, -146, -145,    0,
    0,    0,    0,    0,    0,    0,    0,    0, -113, -113,
 -170, -218, -210, -117, -117, -189, -155, -144, -136, -266,
 -140, -183, -138, -137, -135, -134,  -99,    0,    0,    0,
    0,    0,    0, -207, -207, -182,    0,    0, -182,    0,
 -182, -182,    0, -106,    0, -114,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
 -133, -133,    0,    0,    0,    0,    0,    0, -189,    0,
    0, -163, -163,    0,  -92,  -91,    0,    0,
};
const short yyrindex[] = {                                      0,
    0,    0,    0,    0,    0,  -88,  -88,    0, -221,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0, -242,    0,    0,    0,    0,    0,    0,    0,    0,
    0, -259,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0, -241, -241,    0,    0, -217, -217, -217,
  -88,    0,    0,    0, -130, -130,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0, -251, -251,
    0,    0,    0, -202, -202,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,  -86,    0,  -85,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
 -132, -132,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,
};
const short yygindex[] = {                                      0,
  -22,   -7,   46,    8,  -96,    0,   31,  -31,   -1,  -10,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,  110,   66,   53,    0,    0,    0,    0,    0,
   26,    0,    0,
};
#define YYTABLESIZE 175
const short yytable[] = {                                      35,
    2,    3,    4,    5,    6,    7,    8,   69,   67,   67,
   67,   67,   67,  127,   24,   46,   47,   69,   42,   48,
   69,   54,  146,   55,  147,  148,   43,   43,   36,   43,
   50,   43,   66,   58,  128,   59,   60,   67,  112,   67,
   67,    9,   10,   11,   12,   69,  113,   61,   69,   69,
   70,   71,   72,   73,   24,   37,   24,   98,   69,   83,
  100,   84,  101,  102,  137,   98,   94,   95,  100,   71,
  101,  102,   78,   79,  103,  104,  105,  106,   63,   20,
   40,   20,  103,  104,  105,  106,   63,   67,   41,  138,
   68,   69,  139,  140,   71,  116,  117,   71,   71,  130,
  131,  114,  115,   96,   97,   98,   99,   48,  100,   51,
  101,  102,   26,   27,   28,  143,  144,   52,   38,   39,
  155,  156,  103,  104,  105,  106,   29,   30,   31,   32,
   33,  119,  120,  121,  122,  123,   86,   87,   92,   93,
   53,   56,   62,   42,   64,   74,   76,   77,   80,   88,
   81,   82,  125,   89,   90,  126,  129,  136,  132,  133,
  142,  134,  135,  150,  157,  158,  151,  149,   20,   71,
   33,   35,  153,   79,  154,
};
const short yycheck[] = {                                       7,
  261,  262,  263,  264,  265,  266,  267,  259,  268,  269,
  270,  271,  272,  280,  257,   26,   27,  259,  300,  300,
  272,  300,  119,  300,  121,  122,  308,  308,  257,  308,
   32,  308,   55,   44,  301,   46,   47,  297,  257,  299,
  300,  302,  303,  304,  305,  297,  257,   49,  300,  301,
   58,   59,   60,   61,  297,  309,  299,  276,  300,  297,
  279,  299,  281,  282,  272,  276,   89,   90,  279,  272,
  281,  282,   65,   66,  293,  294,  295,  296,  300,  297,
  257,  299,  293,  294,  295,  296,  308,  297,  257,  297,
  300,  301,  300,  301,  297,  285,  286,  300,  301,  283,
  284,   94,   95,  274,  275,  276,  277,  300,  279,  257,
  281,  282,    3,    4,    5,  298,  299,  257,    9,   10,
  152,  153,  293,  294,  295,  296,  268,  269,  270,  271,
  272,  287,  288,  289,  290,  291,   71,   72,   86,   87,
  298,  259,  306,  300,  258,  307,  257,  297,  260,  257,
  260,  260,  297,  300,  300,  292,  297,  257,  297,  297,
  115,  297,  297,  278,  257,  257,  300,  274,  257,  300,
  257,  257,  142,  306,  149,
};
#define YYFINAL 1
#ifndef YYDEBUG
#define YYDEBUG 0
#endif
#define YYMAXTOKEN 309
#if YYDEBUG
const char * const yyname[] = {
"end-of-file",0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"EOT","SLASH","BLCL","ELCL","ADD",
"GET","DELETE","DELETEALL","FLUSH","DUMP","EXIT","PR_ESP","PR_AH","PR_IPCOMP",
"PR_ESPUDP","PR_TCP","F_PROTOCOL","F_AUTH","F_ENC","F_REPLAY","F_COMP",
"F_RAWCPI","F_MODE","MODE","F_REQID","F_EXT","EXTENSION","NOCYCLICSEQ",
"ALG_AUTH","ALG_AUTH_NOKEY","ALG_ENC","ALG_ENC_NOKEY","ALG_ENC_DESDERIV",
"ALG_ENC_DES32IV","ALG_ENC_OLD","ALG_COMP","F_LIFETIME_HARD","F_LIFETIME_SOFT",
"F_LIFEBYTE_HARD","F_LIFEBYTE_SOFT","DECSTRING","QUOTEDSTRING","HEXSTRING",
"STRING","ANY","SPDADD","SPDDELETE","SPDDUMP","SPDFLUSH","F_POLICY",
"PL_REQUESTS","F_AIFLAGS","TAGGED",
};
const char * const yyrule[] = {
"$accept : commands",
"commands :",
"commands : commands command",
"command : add_command",
"command : get_command",
"command : delete_command",
"command : deleteall_command",
"command : flush_command",
"command : dump_command",
"command : exit_command",
"command : spdadd_command",
"command : spddelete_command",
"command : spddump_command",
"command : spdflush_command",
"add_command : ADD ipaddropts ipandport ipandport protocol_spec spi extension_spec algorithm_spec EOT",
"delete_command : DELETE ipaddropts ipandport ipandport protocol_spec spi extension_spec EOT",
"deleteall_command : DELETEALL ipaddropts ipaddr ipaddr protocol_spec EOT",
"get_command : GET ipaddropts ipandport ipandport protocol_spec spi extension_spec EOT",
"flush_command : FLUSH protocol_spec EOT",
"dump_command : DUMP protocol_spec EOT",
"protocol_spec :",
"protocol_spec : PR_ESP",
"protocol_spec : PR_AH",
"protocol_spec : PR_IPCOMP",
"protocol_spec : PR_ESPUDP",
"protocol_spec : PR_ESPUDP ipaddr",
"protocol_spec : PR_TCP",
"spi : DECSTRING",
"spi : HEXSTRING",
"algorithm_spec : esp_spec",
"algorithm_spec : ah_spec",
"algorithm_spec : ipcomp_spec",
"esp_spec : F_ENC enc_alg F_AUTH auth_alg",
"esp_spec : F_ENC enc_alg",
"ah_spec : F_AUTH auth_alg",
"ipcomp_spec : F_COMP ALG_COMP",
"ipcomp_spec : F_COMP ALG_COMP F_RAWCPI",
"enc_alg : ALG_ENC_NOKEY",
"enc_alg : ALG_ENC key_string",
"enc_alg : ALG_ENC_OLD",
"enc_alg : ALG_ENC_DESDERIV key_string",
"enc_alg : ALG_ENC_DES32IV key_string",
"auth_alg : ALG_AUTH key_string",
"auth_alg : ALG_AUTH_NOKEY",
"key_string : QUOTEDSTRING",
"key_string : HEXSTRING",
"extension_spec :",
"extension_spec : extension_spec extension",
"extension : F_EXT EXTENSION",
"extension : F_EXT NOCYCLICSEQ",
"extension : F_MODE MODE",
"extension : F_MODE ANY",
"extension : F_REQID DECSTRING",
"extension : F_REPLAY DECSTRING",
"extension : F_LIFETIME_HARD DECSTRING",
"extension : F_LIFETIME_SOFT DECSTRING",
"extension : F_LIFEBYTE_HARD DECSTRING",
"extension : F_LIFEBYTE_SOFT DECSTRING",
"spdadd_command : SPDADD ipaddropts STRING prefix portstr STRING prefix portstr upper_spec upper_misc_spec policy_spec EOT",
"spdadd_command : SPDADD TAGGED QUOTEDSTRING policy_spec EOT",
"spddelete_command : SPDDELETE ipaddropts STRING prefix portstr STRING prefix portstr upper_spec upper_misc_spec policy_spec EOT",
"spddump_command : SPDDUMP EOT",
"spdflush_command : SPDFLUSH EOT",
"ipaddropts :",
"ipaddropts : ipaddropts ipaddropt",
"ipaddropt : F_AIFLAGS",
"ipaddr : STRING",
"ipandport : STRING",
"ipandport : STRING portstr",
"prefix :",
"prefix : SLASH DECSTRING",
"portstr :",
"portstr : BLCL ANY ELCL",
"portstr : BLCL DECSTRING ELCL",
"portstr : BLCL STRING ELCL",
"upper_spec : DECSTRING",
"upper_spec : ANY",
"upper_spec : PR_TCP",
"upper_spec : STRING",
"upper_misc_spec :",
"upper_misc_spec : STRING",
"policy_spec : F_POLICY policy_requests",
"policy_requests : PL_REQUESTS",
"exit_command : EXIT EOT",
};
#endif
#if YYDEBUG
#include <stdio.h>
#endif
#ifdef YYSTACKSIZE
#undef YYMAXDEPTH
#define YYMAXDEPTH YYSTACKSIZE
#else
#ifdef YYMAXDEPTH
#define YYSTACKSIZE YYMAXDEPTH
#else
#define YYSTACKSIZE 10000
#define YYMAXDEPTH 10000
#endif
#endif
#define YYINITSTACKSIZE 200
int yydebug;
int yynerrs;
int yyerrflag;
int yychar;
short *yyssp;
YYSTYPE *yyvsp;
YYSTYPE yyval;
YYSTYPE yylval;
short *yyss;
short *yysslim;
YYSTYPE *yyvs;
int yystacksize;
#line 832 "parse.y"

int
setkeymsg0(msg, type, satype, l)
	struct sadb_msg *msg;
	unsigned int type;
	unsigned int satype;
	size_t l;
{

	msg->sadb_msg_version = PF_KEY_V2;
	msg->sadb_msg_type = type;
	msg->sadb_msg_errno = 0;
	msg->sadb_msg_satype = satype;
	msg->sadb_msg_reserved = 0;
	msg->sadb_msg_seq = 0;
	msg->sadb_msg_pid = getpid();
	msg->sadb_msg_len = PFKEY_UNIT64(l);
	return 0;
}

/* XXX NO BUFFER OVERRUN CHECK! BAD BAD! */
static int
setkeymsg_spdaddr(type, upper, policy, srcs, splen, dsts, dplen)
	unsigned int type;
	unsigned int upper;
	vchar_t *policy;
	struct addrinfo *srcs;
	int splen;
	struct addrinfo *dsts;
	int dplen;
{
	struct sadb_msg *msg;
	char buf[BUFSIZ];
	int l, l0;
	struct sadb_address m_addr;
	struct addrinfo *s, *d;
	int n;
	int plen;
	struct sockaddr *sa;
	int salen;
	struct sadb_x_policy *sp;
#ifdef HAVE_POLICY_FWD
	struct sadb_x_ipsecrequest *ps = NULL;
	int saved_level, saved_id = 0;
#endif

	msg = (struct sadb_msg *)buf;

	if (!srcs || !dsts)
		return -1;

	/* fix up length afterwards */
	setkeymsg0(msg, type, SADB_SATYPE_UNSPEC, 0);
	l = sizeof(struct sadb_msg);

	sp = (struct sadb_x_policy*) (buf + l);
	memcpy(buf + l, policy->buf, policy->len);
	l += policy->len;

	l0 = l;
	n = 0;

	/* do it for all src/dst pairs */
	for (s = srcs; s; s = s->ai_next) {
		for (d = dsts; d; d = d->ai_next) {
			/* rewind pointer */
			l = l0;

			if (s->ai_addr->sa_family != d->ai_addr->sa_family)
				continue;
			switch (s->ai_addr->sa_family) {
			case AF_INET:
				plen = sizeof(struct in_addr) << 3;
				break;
#ifdef INET6
			case AF_INET6:
				plen = sizeof(struct in6_addr) << 3;
				break;
#endif
			default:
				continue;
			}

			/* set src */
			sa = s->ai_addr;
			salen = sysdep_sa_len(s->ai_addr);
			m_addr.sadb_address_len = PFKEY_UNIT64(sizeof(m_addr) +
			    PFKEY_ALIGN8(salen));
			m_addr.sadb_address_exttype = SADB_EXT_ADDRESS_SRC;
			m_addr.sadb_address_proto = upper;
			m_addr.sadb_address_prefixlen =
			    (splen >= 0 ? splen : plen);
			m_addr.sadb_address_reserved = 0;

			setvarbuf(buf, &l, (struct sadb_ext *)&m_addr,
			    sizeof(m_addr), (caddr_t)sa, salen);

			/* set dst */
			sa = d->ai_addr;
			salen = sysdep_sa_len(d->ai_addr);
			m_addr.sadb_address_len = PFKEY_UNIT64(sizeof(m_addr) +
			    PFKEY_ALIGN8(salen));
			m_addr.sadb_address_exttype = SADB_EXT_ADDRESS_DST;
			m_addr.sadb_address_proto = upper;
			m_addr.sadb_address_prefixlen =
			    (dplen >= 0 ? dplen : plen);
			m_addr.sadb_address_reserved = 0;

			setvarbuf(buf, &l, (struct sadb_ext *)&m_addr,
			    sizeof(m_addr), sa, salen);

			msg->sadb_msg_len = PFKEY_UNIT64(l);

			sendkeymsg(buf, l);

#ifdef HAVE_POLICY_FWD
			/* create extra call for FWD policy */
			if (f_rfcmode && sp->sadb_x_policy_dir == IPSEC_DIR_INBOUND) {
				sp->sadb_x_policy_dir = IPSEC_DIR_FWD;
				ps = (struct sadb_x_ipsecrequest*) (sp+1);

				/* if request level is unique, change it to
				 * require for fwd policy */
				/* XXX: currently, only first policy is updated
				 * only. Update following too... */
				saved_level = ps->sadb_x_ipsecrequest_level;
				if (saved_level == IPSEC_LEVEL_UNIQUE) {
					saved_id = ps->sadb_x_ipsecrequest_reqid;
					ps->sadb_x_ipsecrequest_reqid=0;
					ps->sadb_x_ipsecrequest_level=IPSEC_LEVEL_REQUIRE;
				}

				sendkeymsg(buf, l);
				/* restoring for next message */
				sp->sadb_x_policy_dir = IPSEC_DIR_INBOUND;
				if (saved_level == IPSEC_LEVEL_UNIQUE) {
					ps->sadb_x_ipsecrequest_reqid = saved_id;
					ps->sadb_x_ipsecrequest_level = saved_level;
				}
			}
#endif

			n++;
		}
	}

	if (n == 0)
		return -1;
	else
		return 0;
}

static int
setkeymsg_spdaddr_tag(type, tag, policy)
	unsigned int type;
	char *tag;
	vchar_t *policy;
{
	struct sadb_msg *msg;
	char buf[BUFSIZ];
	int l, l0;
#ifdef SADB_X_EXT_TAG
	struct sadb_x_tag m_tag;
#endif
	int n;

	msg = (struct sadb_msg *)buf;

	/* fix up length afterwards */
	setkeymsg0(msg, type, SADB_SATYPE_UNSPEC, 0);
	l = sizeof(struct sadb_msg);

	memcpy(buf + l, policy->buf, policy->len);
	l += policy->len;

	l0 = l;
	n = 0;

#ifdef SADB_X_EXT_TAG
	memset(&m_tag, 0, sizeof(m_tag));
	m_tag.sadb_x_tag_len = PFKEY_UNIT64(sizeof(m_tag));
	m_tag.sadb_x_tag_exttype = SADB_X_EXT_TAG;
	if (strlcpy(m_tag.sadb_x_tag_name, tag,
	    sizeof(m_tag.sadb_x_tag_name)) >= sizeof(m_tag.sadb_x_tag_name))
		return -1;
	memcpy(buf + l, &m_tag, sizeof(m_tag));
	l += sizeof(m_tag);
#endif

	msg->sadb_msg_len = PFKEY_UNIT64(l);

	sendkeymsg(buf, l);

	return 0;
}

/* XXX NO BUFFER OVERRUN CHECK! BAD BAD! */
static int
setkeymsg_addr(type, satype, srcs, dsts, no_spi)
	unsigned int type;
	unsigned int satype;
	struct addrinfo *srcs;
	struct addrinfo *dsts;
	int no_spi;
{
	struct sadb_msg *msg;
	char buf[BUFSIZ];
	int l, l0, len;
	struct sadb_sa m_sa;
	struct sadb_x_sa2 m_sa2;
	struct sadb_address m_addr;
	struct addrinfo *s, *d;
	int n;
	int plen;
	struct sockaddr *sa;
	int salen;

	msg = (struct sadb_msg *)buf;

	if (!srcs || !dsts)
		return -1;

	/* fix up length afterwards */
	setkeymsg0(msg, type, satype, 0);
	l = sizeof(struct sadb_msg);

	if (!no_spi) {
		len = sizeof(struct sadb_sa);
		m_sa.sadb_sa_len = PFKEY_UNIT64(len);
		m_sa.sadb_sa_exttype = SADB_EXT_SA;
		m_sa.sadb_sa_spi = htonl(p_spi);
		m_sa.sadb_sa_replay = p_replay;
		m_sa.sadb_sa_state = 0;
		m_sa.sadb_sa_auth = p_alg_auth;
		m_sa.sadb_sa_encrypt = p_alg_enc;
		m_sa.sadb_sa_flags = p_ext;

		memcpy(buf + l, &m_sa, len);
		l += len;

		len = sizeof(struct sadb_x_sa2);
		m_sa2.sadb_x_sa2_len = PFKEY_UNIT64(len);
		m_sa2.sadb_x_sa2_exttype = SADB_X_EXT_SA2;
		m_sa2.sadb_x_sa2_mode = p_mode;
		m_sa2.sadb_x_sa2_reqid = p_reqid;

		memcpy(buf + l, &m_sa2, len);
		l += len;
	}

	l0 = l;
	n = 0;

	/* do it for all src/dst pairs */
	for (s = srcs; s; s = s->ai_next) {
		for (d = dsts; d; d = d->ai_next) {
			/* rewind pointer */
			l = l0;

			if (s->ai_addr->sa_family != d->ai_addr->sa_family)
				continue;
			switch (s->ai_addr->sa_family) {
			case AF_INET:
				plen = sizeof(struct in_addr) << 3;
				break;
#ifdef INET6
			case AF_INET6:
				plen = sizeof(struct in6_addr) << 3;
				break;
#endif
			default:
				continue;
			}

			/* set src */
			sa = s->ai_addr;
			salen = sysdep_sa_len(s->ai_addr);
			m_addr.sadb_address_len = PFKEY_UNIT64(sizeof(m_addr) +
			    PFKEY_ALIGN8(salen));
			m_addr.sadb_address_exttype = SADB_EXT_ADDRESS_SRC;
			m_addr.sadb_address_proto = IPSEC_ULPROTO_ANY;
			m_addr.sadb_address_prefixlen = plen;
			m_addr.sadb_address_reserved = 0;

			setvarbuf(buf, &l, (struct sadb_ext *)&m_addr,
			    sizeof(m_addr), sa, salen);

			/* set dst */
			sa = d->ai_addr;
			salen = sysdep_sa_len(d->ai_addr);
			m_addr.sadb_address_len = PFKEY_UNIT64(sizeof(m_addr) +
			    PFKEY_ALIGN8(salen));
			m_addr.sadb_address_exttype = SADB_EXT_ADDRESS_DST;
			m_addr.sadb_address_proto = IPSEC_ULPROTO_ANY;
			m_addr.sadb_address_prefixlen = plen;
			m_addr.sadb_address_reserved = 0;

			setvarbuf(buf, &l, (struct sadb_ext *)&m_addr,
			    sizeof(m_addr), sa, salen);

			msg->sadb_msg_len = PFKEY_UNIT64(l);

			sendkeymsg(buf, l);

			n++;
		}
	}

	if (n == 0)
		return -1;
	else
		return 0;
}

#ifdef SADB_X_EXT_NAT_T_TYPE
static u_int16_t get_port (struct addrinfo *addr)
{
	struct sockaddr *s = addr->ai_addr;
	u_int16_t port = 0;

	switch (s->sa_family) {
	case AF_INET:
	  {
		struct sockaddr_in *sin4 = (struct sockaddr_in *)s;
		port = ntohs(sin4->sin_port);
		break;
	  }
	case AF_INET6:
	  {
		struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)s;
		port = ntohs(sin6->sin6_port);
		break;
	  }
	}

	if (port == 0)
		port = DEFAULT_NATT_PORT;

	return port;
}
#endif

/* XXX NO BUFFER OVERRUN CHECK! BAD BAD! */
static int
setkeymsg_add(type, satype, srcs, dsts)
	unsigned int type;
	unsigned int satype;
	struct addrinfo *srcs;
	struct addrinfo *dsts;
{
	struct sadb_msg *msg;
	char buf[BUFSIZ];
	int l, l0, len;
	struct sadb_sa m_sa;
	struct sadb_x_sa2 m_sa2;
	struct sadb_address m_addr;
	struct addrinfo *s, *d;
	int n;
	int plen;
	struct sockaddr *sa;
	int salen;

	msg = (struct sadb_msg *)buf;

	if (!srcs || !dsts)
		return -1;

	/* fix up length afterwards */
	setkeymsg0(msg, type, satype, 0);
	l = sizeof(struct sadb_msg);

	/* set encryption algorithm, if present. */
	if (satype != SADB_X_SATYPE_IPCOMP && p_key_enc) {
		union {
			struct sadb_key key;
			struct sadb_ext ext;
		} m;

		m.key.sadb_key_len =
			PFKEY_UNIT64(sizeof(m.key)
				   + PFKEY_ALIGN8(p_key_enc_len));
		m.key.sadb_key_exttype = SADB_EXT_KEY_ENCRYPT;
		m.key.sadb_key_bits = p_key_enc_len * 8;
		m.key.sadb_key_reserved = 0;

		setvarbuf(buf, &l, &m.ext, sizeof(m.key),
			p_key_enc, p_key_enc_len);
	}

	/* set authentication algorithm, if present. */
	if (p_key_auth) {
		union {
			struct sadb_key key;
			struct sadb_ext ext;
		} m;

		m.key.sadb_key_len =
			PFKEY_UNIT64(sizeof(m.key)
				   + PFKEY_ALIGN8(p_key_auth_len));
		m.key.sadb_key_exttype = SADB_EXT_KEY_AUTH;
		m.key.sadb_key_bits = p_key_auth_len * 8;
		m.key.sadb_key_reserved = 0;

		setvarbuf(buf, &l, &m.ext, sizeof(m.key),
			p_key_auth, p_key_auth_len);
	}

	/* set lifetime for HARD */
	if (p_lt_hard != 0 || p_lb_hard != 0) {
		struct sadb_lifetime m_lt;
		u_int slen = sizeof(struct sadb_lifetime);

		m_lt.sadb_lifetime_len = PFKEY_UNIT64(slen);
		m_lt.sadb_lifetime_exttype = SADB_EXT_LIFETIME_HARD;
		m_lt.sadb_lifetime_allocations = 0;
		m_lt.sadb_lifetime_bytes = p_lb_hard;
		m_lt.sadb_lifetime_addtime = p_lt_hard;
		m_lt.sadb_lifetime_usetime = 0;

		memcpy(buf + l, &m_lt, slen);
		l += slen;
	}

	/* set lifetime for SOFT */
	if (p_lt_soft != 0 || p_lb_soft != 0) {
		struct sadb_lifetime m_lt;
		u_int slen = sizeof(struct sadb_lifetime);

		m_lt.sadb_lifetime_len = PFKEY_UNIT64(slen);
		m_lt.sadb_lifetime_exttype = SADB_EXT_LIFETIME_SOFT;
		m_lt.sadb_lifetime_allocations = 0;
		m_lt.sadb_lifetime_bytes = p_lb_soft;
		m_lt.sadb_lifetime_addtime = p_lt_soft;
		m_lt.sadb_lifetime_usetime = 0;

		memcpy(buf + l, &m_lt, slen);
		l += slen;
	}

	len = sizeof(struct sadb_sa);
	m_sa.sadb_sa_len = PFKEY_UNIT64(len);
	m_sa.sadb_sa_exttype = SADB_EXT_SA;
	m_sa.sadb_sa_spi = htonl(p_spi);
	m_sa.sadb_sa_replay = p_replay;
	m_sa.sadb_sa_state = 0;
	m_sa.sadb_sa_auth = p_alg_auth;
	m_sa.sadb_sa_encrypt = p_alg_enc;
	m_sa.sadb_sa_flags = p_ext;

	memcpy(buf + l, &m_sa, len);
	l += len;

	len = sizeof(struct sadb_x_sa2);
	m_sa2.sadb_x_sa2_len = PFKEY_UNIT64(len);
	m_sa2.sadb_x_sa2_exttype = SADB_X_EXT_SA2;
	m_sa2.sadb_x_sa2_mode = p_mode;
	m_sa2.sadb_x_sa2_reqid = p_reqid;

	memcpy(buf + l, &m_sa2, len);
	l += len;

#ifdef SADB_X_EXT_NAT_T_TYPE
	if (p_natt_type) {
		struct sadb_x_nat_t_type natt_type;

		len = sizeof(struct sadb_x_nat_t_type);
		memset(&natt_type, 0, len);
		natt_type.sadb_x_nat_t_type_len = PFKEY_UNIT64(len);
		natt_type.sadb_x_nat_t_type_exttype = SADB_X_EXT_NAT_T_TYPE;
		natt_type.sadb_x_nat_t_type_type = p_natt_type;

		memcpy(buf + l, &natt_type, len);
		l += len;

		if (p_natt_oa) {
			sa = p_natt_oa->ai_addr;
			switch (sa->sa_family) {
			case AF_INET:
				plen = sizeof(struct in_addr) << 3;
				break;
#ifdef INET6
			case AF_INET6:
				plen = sizeof(struct in6_addr) << 3;
				break;
#endif
			default:
				return -1;
			}
			salen = sysdep_sa_len(sa);
			m_addr.sadb_address_len = PFKEY_UNIT64(sizeof(m_addr) +
			    PFKEY_ALIGN8(salen));
			m_addr.sadb_address_exttype = SADB_X_EXT_NAT_T_OA;
			m_addr.sadb_address_proto = IPSEC_ULPROTO_ANY;
			m_addr.sadb_address_prefixlen = plen;
			m_addr.sadb_address_reserved = 0;

			setvarbuf(buf, &l, (struct sadb_ext *)&m_addr,
			    sizeof(m_addr), sa, salen);
		}
	}
#endif

	l0 = l;
	n = 0;

	/* do it for all src/dst pairs */
	for (s = srcs; s; s = s->ai_next) {
		for (d = dsts; d; d = d->ai_next) {
			/* rewind pointer */
			l = l0;

			if (s->ai_addr->sa_family != d->ai_addr->sa_family)
				continue;
			switch (s->ai_addr->sa_family) {
			case AF_INET:
				plen = sizeof(struct in_addr) << 3;
				break;
#ifdef INET6
			case AF_INET6:
				plen = sizeof(struct in6_addr) << 3;
				break;
#endif
			default:
				continue;
			}

			/* set src */
			sa = s->ai_addr;
			salen = sysdep_sa_len(s->ai_addr);
			m_addr.sadb_address_len = PFKEY_UNIT64(sizeof(m_addr) +
			    PFKEY_ALIGN8(salen));
			m_addr.sadb_address_exttype = SADB_EXT_ADDRESS_SRC;
			m_addr.sadb_address_proto = IPSEC_ULPROTO_ANY;
			m_addr.sadb_address_prefixlen = plen;
			m_addr.sadb_address_reserved = 0;

			setvarbuf(buf, &l, (struct sadb_ext *)&m_addr,
			    sizeof(m_addr), sa, salen);

			/* set dst */
			sa = d->ai_addr;
			salen = sysdep_sa_len(d->ai_addr);
			m_addr.sadb_address_len = PFKEY_UNIT64(sizeof(m_addr) +
			    PFKEY_ALIGN8(salen));
			m_addr.sadb_address_exttype = SADB_EXT_ADDRESS_DST;
			m_addr.sadb_address_proto = IPSEC_ULPROTO_ANY;
			m_addr.sadb_address_prefixlen = plen;
			m_addr.sadb_address_reserved = 0;

			setvarbuf(buf, &l, (struct sadb_ext *)&m_addr,
			    sizeof(m_addr), sa, salen);

#ifdef SADB_X_EXT_NAT_T_TYPE
			if (p_natt_type) {
				struct sadb_x_nat_t_port natt_port;

				/* NATT_SPORT */
				len = sizeof(struct sadb_x_nat_t_port);
				memset(&natt_port, 0, len);
				natt_port.sadb_x_nat_t_port_len = PFKEY_UNIT64(len);
				natt_port.sadb_x_nat_t_port_exttype =
					SADB_X_EXT_NAT_T_SPORT;
				natt_port.sadb_x_nat_t_port_port = htons(get_port(s));
				
				memcpy(buf + l, &natt_port, len);
				l += len;

				/* NATT_DPORT */
				natt_port.sadb_x_nat_t_port_exttype =
					SADB_X_EXT_NAT_T_DPORT;
				natt_port.sadb_x_nat_t_port_port = htons(get_port(d));
				
				memcpy(buf + l, &natt_port, len);
				l += len;
			}
#endif
			msg->sadb_msg_len = PFKEY_UNIT64(l);

			sendkeymsg(buf, l);

			n++;
		}
	}

	if (n == 0)
		return -1;
	else
		return 0;
}

static struct addrinfo *
parse_addr(host, port)
	char *host;
	char *port;
{
	struct addrinfo hints, *res = NULL;
	int error;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = p_aifamily;
	hints.ai_socktype = SOCK_DGRAM;		/*dummy*/
	hints.ai_protocol = IPPROTO_UDP;	/*dummy*/
	hints.ai_flags = p_aiflags;
	error = getaddrinfo(host, port, &hints, &res);
	if (error != 0) {
		yyerror(gai_strerror(error));
		return NULL;
	}
	return res;
}

static int
fix_portstr(spec, sport, dport)
	vchar_t *spec, *sport, *dport;
{
	const char *p, *p2 = "0";
	char *q;
	u_int l;

	l = 0;
	for (q = spec->buf; *q != ',' && *q != '\0' && l < spec->len; q++, l++)
		;
	if (*q != '\0') {
		if (*q == ',') {
			*q = '\0';
			p2 = ++q;
		}
		for (p = p2; *p != '\0' && l < spec->len; p++, l++)
			;
		if (*p != '\0' || *p2 == '\0') {
			yyerror("invalid an upper layer protocol spec");
			return -1;
		}
	}

	sport->buf = strdup(spec->buf);
	if (!sport->buf) {
		yyerror("insufficient memory");
		return -1;
	}
	sport->len = strlen(sport->buf);
	dport->buf = strdup(p2);
	if (!dport->buf) {
		yyerror("insufficient memory");
		return -1;
	}
	dport->len = strlen(dport->buf);

	return 0;
}

static int
setvarbuf(buf, off, ebuf, elen, vbuf, vlen)
	char *buf;
	int *off;
	struct sadb_ext *ebuf;
	int elen;
	const void *vbuf;
	int vlen;
{
	memset(buf + *off, 0, PFKEY_UNUNIT64(ebuf->sadb_ext_len));
	memcpy(buf + *off, (caddr_t)ebuf, elen);
	memcpy(buf + *off + elen, vbuf, vlen);
	(*off) += PFKEY_ALIGN8(elen + vlen);

	return 0;
}

void
parse_init()
{
	p_spi = 0;

	p_ext = SADB_X_EXT_CYCSEQ;
	p_alg_enc = SADB_EALG_NONE;
	p_alg_auth = SADB_AALG_NONE;
	p_mode = IPSEC_MODE_ANY;
	p_reqid = 0;
	p_replay = 0;
	p_key_enc_len = p_key_auth_len = 0;
	p_key_enc = p_key_auth = 0;
	p_lt_hard = p_lt_soft = 0;
	p_lb_hard = p_lb_soft = 0;

	p_aiflags = 0;
	p_aifamily = PF_UNSPEC;

	/* Clear out any natt OA information */
	if (p_natt_oa)
		freeaddrinfo (p_natt_oa);
	p_natt_oa = NULL;
	p_natt_type = 0;

	return;
}

void
free_buffer()
{
	/* we got tons of memory leaks in the parser anyways, leave them */

	return;
}
#line 1118 "y.tab.c"
/* allocate initial stack or double stack size, up to YYMAXDEPTH */
static int yygrowstack()
{
    int newsize, i;
    short *newss;
    YYSTYPE *newvs;

    if ((newsize = yystacksize) == 0)
        newsize = YYINITSTACKSIZE;
    else if (newsize >= YYMAXDEPTH)
        return -1;
    else if ((newsize *= 2) > YYMAXDEPTH)
        newsize = YYMAXDEPTH;
    i = yyssp - yyss;
    newss = yyss ? (short *)realloc(yyss, newsize * sizeof *newss) :
      (short *)malloc(newsize * sizeof *newss);
    if (newss == NULL)
        return -1;
    yyss = newss;
    yyssp = newss + i;
    newvs = yyvs ? (YYSTYPE *)realloc(yyvs, newsize * sizeof *newvs) :
      (YYSTYPE *)malloc(newsize * sizeof *newvs);
    if (newvs == NULL)
        return -1;
    yyvs = newvs;
    yyvsp = newvs + i;
    yystacksize = newsize;
    yysslim = yyss + newsize - 1;
    return 0;
}

#define YYABORT goto yyabort
#define YYREJECT goto yyabort
#define YYACCEPT goto yyaccept
#define YYERROR goto yyerrlab

#ifndef YYPARSE_PARAM
#if defined(__cplusplus) || __STDC__
#define YYPARSE_PARAM_ARG void
#define YYPARSE_PARAM_DECL
#else	/* ! ANSI-C/C++ */
#define YYPARSE_PARAM_ARG
#define YYPARSE_PARAM_DECL
#endif	/* ANSI-C/C++ */
#else	/* YYPARSE_PARAM */
#ifndef YYPARSE_PARAM_TYPE
#define YYPARSE_PARAM_TYPE void *
#endif
#if defined(__cplusplus) || __STDC__
#define YYPARSE_PARAM_ARG YYPARSE_PARAM_TYPE YYPARSE_PARAM
#define YYPARSE_PARAM_DECL
#else	/* ! ANSI-C/C++ */
#define YYPARSE_PARAM_ARG YYPARSE_PARAM
#define YYPARSE_PARAM_DECL YYPARSE_PARAM_TYPE YYPARSE_PARAM;
#endif	/* ANSI-C/C++ */
#endif	/* ! YYPARSE_PARAM */

int
yyparse (YYPARSE_PARAM_ARG)
    YYPARSE_PARAM_DECL
{
    register int yym, yyn, yystate;
#if YYDEBUG
    register const char *yys;

    if ((yys = getenv("YYDEBUG")))
    {
        yyn = *yys;
        if (yyn >= '0' && yyn <= '9')
            yydebug = yyn - '0';
    }
#endif

    yynerrs = 0;
    yyerrflag = 0;
    yychar = (-1);

    if (yyss == NULL && yygrowstack()) goto yyoverflow;
    yyssp = yyss;
    yyvsp = yyvs;
    *yyssp = yystate = 0;

yyloop:
    if ((yyn = yydefred[yystate])) goto yyreduce;
    if (yychar < 0)
    {
        if ((yychar = yylex()) < 0) yychar = 0;
#if YYDEBUG
        if (yydebug)
        {
            yys = 0;
            if (yychar <= YYMAXTOKEN) yys = yyname[yychar];
            if (!yys) yys = "illegal-symbol";
            printf("%sdebug: state %d, reading %d (%s)\n",
                    YYPREFIX, yystate, yychar, yys);
        }
#endif
    }
    if ((yyn = yysindex[yystate]) && (yyn += yychar) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == yychar)
    {
#if YYDEBUG
        if (yydebug)
            printf("%sdebug: state %d, shifting to state %d\n",
                    YYPREFIX, yystate, yytable[yyn]);
#endif
        if (yyssp >= yysslim && yygrowstack())
        {
            goto yyoverflow;
        }
        *++yyssp = yystate = yytable[yyn];
        *++yyvsp = yylval;
        yychar = (-1);
        if (yyerrflag > 0)  --yyerrflag;
        goto yyloop;
    }
    if ((yyn = yyrindex[yystate]) && (yyn += yychar) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == yychar)
    {
        yyn = yytable[yyn];
        goto yyreduce;
    }
    if (yyerrflag) goto yyinrecovery;
#if defined(lint) || defined(__GNUC__)
    goto yynewerror;
#endif
yynewerror:
    yyerror("syntax error");
#if defined(lint) || defined(__GNUC__)
    goto yyerrlab;
#endif
yyerrlab:
    ++yynerrs;
yyinrecovery:
    if (yyerrflag < 3)
    {
        yyerrflag = 3;
        for (;;)
        {
            if ((yyn = yysindex[*yyssp]) && (yyn += YYERRCODE) >= 0 &&
                    yyn <= YYTABLESIZE && yycheck[yyn] == YYERRCODE)
            {
#if YYDEBUG
                if (yydebug)
                    printf("%sdebug: state %d, error recovery shifting\
 to state %d\n", YYPREFIX, *yyssp, yytable[yyn]);
#endif
                if (yyssp >= yysslim && yygrowstack())
                {
                    goto yyoverflow;
                }
                *++yyssp = yystate = yytable[yyn];
                *++yyvsp = yylval;
                goto yyloop;
            }
            else
            {
#if YYDEBUG
                if (yydebug)
                    printf("%sdebug: error recovery discarding state %d\n",
                            YYPREFIX, *yyssp);
#endif
                if (yyssp <= yyss) goto yyabort;
                --yyssp;
                --yyvsp;
            }
        }
    }
    else
    {
        if (yychar == 0) goto yyabort;
#if YYDEBUG
        if (yydebug)
        {
            yys = 0;
            if (yychar <= YYMAXTOKEN) yys = yyname[yychar];
            if (!yys) yys = "illegal-symbol";
            printf("%sdebug: state %d, error recovery discards token %d (%s)\n",
                    YYPREFIX, yystate, yychar, yys);
        }
#endif
        yychar = (-1);
        goto yyloop;
    }
yyreduce:
#if YYDEBUG
    if (yydebug)
        printf("%sdebug: state %d, reducing by rule %d (%s)\n",
                YYPREFIX, yystate, yyn, yyrule[yyn]);
#endif
    yym = yylen[yyn];
    yyval = yyvsp[1-yym];
    switch (yyn)
    {
case 2:
#line 146 "parse.y"
{
			free_buffer();
			parse_init();
		}
break;
case 14:
#line 170 "parse.y"
{
			int status;

			status = setkeymsg_add(SADB_ADD, yyvsp[-4].num, yyvsp[-6].res, yyvsp[-5].res);
			if (status < 0)
				return -1;
		}
break;
case 15:
#line 182 "parse.y"
{
			int status;

			if (yyvsp[-5].res->ai_next || yyvsp[-4].res->ai_next) {
				yyerror("multiple address specified");
				return -1;
			}
			if (p_mode != IPSEC_MODE_ANY)
				yyerror("WARNING: mode is obsolete");

			status = setkeymsg_addr(SADB_DELETE, yyvsp[-3].num, yyvsp[-5].res, yyvsp[-4].res, 0);
			if (status < 0)
				return -1;
		}
break;
case 16:
#line 201 "parse.y"
{
			int status;

			status = setkeymsg_addr(SADB_DELETE, yyvsp[-1].num, yyvsp[-3].res, yyvsp[-2].res, 1);
			if (status < 0)
				return -1;
		}
break;
case 17:
#line 213 "parse.y"
{
			int status;

			if (p_mode != IPSEC_MODE_ANY)
				yyerror("WARNING: mode is obsolete");

			status = setkeymsg_addr(SADB_GET, yyvsp[-3].num, yyvsp[-5].res, yyvsp[-4].res, 0);
			if (status < 0)
				return -1;
		}
break;
case 18:
#line 228 "parse.y"
{
			struct sadb_msg msg;
			setkeymsg0(&msg, SADB_FLUSH, yyvsp[-1].num, sizeof(msg));
			sendkeymsg((char *)&msg, sizeof(msg));
		}
break;
case 19:
#line 238 "parse.y"
{
			struct sadb_msg msg;
			setkeymsg0(&msg, SADB_DUMP, yyvsp[-1].num, sizeof(msg));
			sendkeymsg((char *)&msg, sizeof(msg));
		}
break;
case 20:
#line 247 "parse.y"
{
			yyval.num = SADB_SATYPE_UNSPEC;
		}
break;
case 21:
#line 251 "parse.y"
{
			yyval.num = SADB_SATYPE_ESP;
			if (yyvsp[0].num == 1)
				p_ext |= SADB_X_EXT_OLD;
			else
				p_ext &= ~SADB_X_EXT_OLD;
		}
break;
case 22:
#line 259 "parse.y"
{
			yyval.num = SADB_SATYPE_AH;
			if (yyvsp[0].num == 1)
				p_ext |= SADB_X_EXT_OLD;
			else
				p_ext &= ~SADB_X_EXT_OLD;
		}
break;
case 23:
#line 267 "parse.y"
{
			yyval.num = SADB_X_SATYPE_IPCOMP;
		}
break;
case 24:
#line 271 "parse.y"
{
			yyval.num = SADB_SATYPE_ESP;
			p_ext &= ~SADB_X_EXT_OLD;
			p_natt_oa = 0;
			p_natt_type = UDP_ENCAP_ESPINUDP;
		}
break;
case 25:
#line 278 "parse.y"
{
			yyval.num = SADB_SATYPE_ESP;
			p_ext &= ~SADB_X_EXT_OLD;
			p_natt_oa = yyvsp[0].res;
			p_natt_type = UDP_ENCAP_ESPINUDP;
		}
break;
case 26:
#line 285 "parse.y"
{
#ifdef SADB_X_SATYPE_TCPSIGNATURE
			yyval.num = SADB_X_SATYPE_TCPSIGNATURE;
#endif
		}
break;
case 27:
#line 293 "parse.y"
{ p_spi = yyvsp[0].ulnum; }
break;
case 28:
#line 295 "parse.y"
{
			char *ep;
			unsigned long v;

			ep = NULL;
			v = strtoul(yyvsp[0].val.buf, &ep, 16);
			if (!ep || *ep) {
				yyerror("invalid SPI");
				return -1;
			}
			if (v & ~0xffffffff) {
				yyerror("SPI too big.");
				return -1;
			}

			p_spi = v;
		}
break;
case 35:
#line 331 "parse.y"
{
			if (yyvsp[0].num < 0) {
				yyerror("unsupported algorithm");
				return -1;
			}
			p_alg_enc = yyvsp[0].num;
		}
break;
case 36:
#line 339 "parse.y"
{
			if (yyvsp[-1].num < 0) {
				yyerror("unsupported algorithm");
				return -1;
			}
			p_alg_enc = yyvsp[-1].num;
			p_ext |= SADB_X_EXT_RAWCPI;
		}
break;
case 37:
#line 350 "parse.y"
{
			if (yyvsp[0].num < 0) {
				yyerror("unsupported algorithm");
				return -1;
			}
			p_alg_enc = yyvsp[0].num;

			p_key_enc_len = 0;
			p_key_enc = "";
			if (ipsec_check_keylen(SADB_EXT_SUPPORTED_ENCRYPT,
			    p_alg_enc, PFKEY_UNUNIT64(p_key_enc_len)) < 0) {
				yyerror(ipsec_strerror());
				return -1;
			}
		}
break;
case 38:
#line 365 "parse.y"
{
			if (yyvsp[-1].num < 0) {
				yyerror("unsupported algorithm");
				return -1;
			}
			p_alg_enc = yyvsp[-1].num;

			p_key_enc_len = yyvsp[0].val.len;
			p_key_enc = yyvsp[0].val.buf;
			if (ipsec_check_keylen(SADB_EXT_SUPPORTED_ENCRYPT,
			    p_alg_enc, PFKEY_UNUNIT64(p_key_enc_len)) < 0) {
				yyerror(ipsec_strerror());
				return -1;
			}
		}
break;
case 39:
#line 380 "parse.y"
{
			if (yyvsp[0].num < 0) {
				yyerror("unsupported algorithm");
				return -1;
			}
			yyerror("WARNING: obsolete algorithm");
			p_alg_enc = yyvsp[0].num;

			p_key_enc_len = 0;
			p_key_enc = "";
			if (ipsec_check_keylen(SADB_EXT_SUPPORTED_ENCRYPT,
			    p_alg_enc, PFKEY_UNUNIT64(p_key_enc_len)) < 0) {
				yyerror(ipsec_strerror());
				return -1;
			}
		}
break;
case 40:
#line 397 "parse.y"
{
			if (yyvsp[-1].num < 0) {
				yyerror("unsupported algorithm");
				return -1;
			}
			p_alg_enc = yyvsp[-1].num;
			if (p_ext & SADB_X_EXT_OLD) {
				yyerror("algorithm mismatched");
				return -1;
			}
			p_ext |= SADB_X_EXT_DERIV;

			p_key_enc_len = yyvsp[0].val.len;
			p_key_enc = yyvsp[0].val.buf;
			if (ipsec_check_keylen(SADB_EXT_SUPPORTED_ENCRYPT,
			    p_alg_enc, PFKEY_UNUNIT64(p_key_enc_len)) < 0) {
				yyerror(ipsec_strerror());
				return -1;
			}
		}
break;
case 41:
#line 418 "parse.y"
{
			if (yyvsp[-1].num < 0) {
				yyerror("unsupported algorithm");
				return -1;
			}
			p_alg_enc = yyvsp[-1].num;
			if (!(p_ext & SADB_X_EXT_OLD)) {
				yyerror("algorithm mismatched");
				return -1;
			}
			p_ext |= SADB_X_EXT_IV4B;

			p_key_enc_len = yyvsp[0].val.len;
			p_key_enc = yyvsp[0].val.buf;
			if (ipsec_check_keylen(SADB_EXT_SUPPORTED_ENCRYPT,
			    p_alg_enc, PFKEY_UNUNIT64(p_key_enc_len)) < 0) {
				yyerror(ipsec_strerror());
				return -1;
			}
		}
break;
case 42:
#line 441 "parse.y"
{
			if (yyvsp[-1].num < 0) {
				yyerror("unsupported algorithm");
				return -1;
			}
			p_alg_auth = yyvsp[-1].num;

			p_key_auth_len = yyvsp[0].val.len;
			p_key_auth = yyvsp[0].val.buf;
#ifdef SADB_X_AALG_TCP_MD5
			if (p_alg_auth == SADB_X_AALG_TCP_MD5) {
				if ((p_key_auth_len < 1) || 
				    (p_key_auth_len > 80))
					return -1;
			} else 
#endif
			{
				if (ipsec_check_keylen(SADB_EXT_SUPPORTED_AUTH,
				    p_alg_auth, 
				    PFKEY_UNUNIT64(p_key_auth_len)) < 0) {
					yyerror(ipsec_strerror());
					return -1;
				}
			}
		}
break;
case 43:
#line 466 "parse.y"
{
			if (yyvsp[0].num < 0) {
				yyerror("unsupported algorithm");
				return -1;
			}
			p_alg_auth = yyvsp[0].num;

			p_key_auth_len = 0;
			p_key_auth = NULL;
		}
break;
case 44:
#line 480 "parse.y"
{
			yyval.val = yyvsp[0].val;
		}
break;
case 45:
#line 484 "parse.y"
{
			caddr_t pp_key;
			caddr_t bp;
			caddr_t yp = yyvsp[0].val.buf;
			int l;

			l = strlen(yp) % 2 + strlen(yp) / 2;
			if ((pp_key = malloc(l)) == 0) {
				yyerror("not enough core");
				return -1;
			}
			memset(pp_key, 0, l);

			bp = pp_key;
			if (strlen(yp) % 2) {
				*bp = ATOX(yp[0]);
				yp++, bp++;
			}
			while (*yp) {
				*bp = (ATOX(yp[0]) << 4) | ATOX(yp[1]);
				yp += 2, bp++;
			}

			yyval.val.len = l;
			yyval.val.buf = pp_key;
		}
break;
case 48:
#line 518 "parse.y"
{ p_ext |= yyvsp[0].num; }
break;
case 49:
#line 519 "parse.y"
{ p_ext &= ~SADB_X_EXT_CYCSEQ; }
break;
case 50:
#line 520 "parse.y"
{ p_mode = yyvsp[0].num; }
break;
case 51:
#line 521 "parse.y"
{ p_mode = IPSEC_MODE_ANY; }
break;
case 52:
#line 522 "parse.y"
{ p_reqid = yyvsp[0].ulnum; }
break;
case 53:
#line 524 "parse.y"
{
			if ((p_ext & SADB_X_EXT_OLD) != 0) {
				yyerror("replay prevention cannot be used with "
				    "ah/esp-old");
				return -1;
			}
			p_replay = yyvsp[0].ulnum;
		}
break;
case 54:
#line 532 "parse.y"
{ p_lt_hard = yyvsp[0].ulnum; }
break;
case 55:
#line 533 "parse.y"
{ p_lt_soft = yyvsp[0].ulnum; }
break;
case 56:
#line 534 "parse.y"
{ p_lb_hard = yyvsp[0].ulnum; }
break;
case 57:
#line 535 "parse.y"
{ p_lb_soft = yyvsp[0].ulnum; }
break;
case 58:
#line 542 "parse.y"
{
			int status;
			struct addrinfo *src, *dst;

#ifdef HAVE_PFKEY_POLICY_PRIORITY
			last_msg_type = SADB_X_SPDADD;
#endif

			/* fixed port fields if ulp is icmpv6 */
			if (yyvsp[-2].val.buf != NULL) {
				if (yyvsp[-3].num != IPPROTO_ICMPV6)
					return -1;
				free(yyvsp[-7].val.buf);
				free(yyvsp[-4].val.buf);
				if (fix_portstr(&yyvsp[-2].val, &yyvsp[-7].val, &yyvsp[-4].val))
					return -1;
			}

			src = parse_addr(yyvsp[-9].val.buf, yyvsp[-7].val.buf);
			dst = parse_addr(yyvsp[-6].val.buf, yyvsp[-4].val.buf);
			if (!src || !dst) {
				/* yyerror is already called */
				return -1;
			}
			if (src->ai_next || dst->ai_next) {
				yyerror("multiple address specified");
				freeaddrinfo(src);
				freeaddrinfo(dst);
				return -1;
			}

			status = setkeymsg_spdaddr(SADB_X_SPDADD, yyvsp[-3].num, &yyvsp[-1].val,
			    src, yyvsp[-8].num, dst, yyvsp[-5].num);
			freeaddrinfo(src);
			freeaddrinfo(dst);
			if (status < 0)
				return -1;
		}
break;
case 59:
#line 581 "parse.y"
{
			int status;

			status = setkeymsg_spdaddr_tag(SADB_X_SPDADD,
			    yyvsp[-2].val.buf, &yyvsp[-1].val);
			if (status < 0)
				return -1;
		}
break;
case 60:
#line 593 "parse.y"
{
			int status;
			struct addrinfo *src, *dst;

			/* fixed port fields if ulp is icmpv6 */
			if (yyvsp[-2].val.buf != NULL) {
				if (yyvsp[-3].num != IPPROTO_ICMPV6)
					return -1;
				free(yyvsp[-7].val.buf);
				free(yyvsp[-4].val.buf);
				if (fix_portstr(&yyvsp[-2].val, &yyvsp[-7].val, &yyvsp[-4].val))
					return -1;
			}

			src = parse_addr(yyvsp[-9].val.buf, yyvsp[-7].val.buf);
			dst = parse_addr(yyvsp[-6].val.buf, yyvsp[-4].val.buf);
			if (!src || !dst) {
				/* yyerror is already called */
				return -1;
			}
			if (src->ai_next || dst->ai_next) {
				yyerror("multiple address specified");
				freeaddrinfo(src);
				freeaddrinfo(dst);
				return -1;
			}

			status = setkeymsg_spdaddr(SADB_X_SPDDELETE, yyvsp[-3].num, &yyvsp[-1].val,
			    src, yyvsp[-8].num, dst, yyvsp[-5].num);
			freeaddrinfo(src);
			freeaddrinfo(dst);
			if (status < 0)
				return -1;
		}
break;
case 61:
#line 631 "parse.y"
{
			struct sadb_msg msg;
			setkeymsg0(&msg, SADB_X_SPDDUMP, SADB_SATYPE_UNSPEC,
			    sizeof(msg));
			sendkeymsg((char *)&msg, sizeof(msg));
		}
break;
case 62:
#line 642 "parse.y"
{
			struct sadb_msg msg;
			setkeymsg0(&msg, SADB_X_SPDFLUSH, SADB_SATYPE_UNSPEC,
			    sizeof(msg));
			sendkeymsg((char *)&msg, sizeof(msg));
		}
break;
case 65:
#line 657 "parse.y"
{
			char *p;

			for (p = yyvsp[0].val.buf + 1; *p; p++)
				switch (*p) {
				case '4':
					p_aifamily = AF_INET;
					break;
#ifdef INET6
				case '6':
					p_aifamily = AF_INET6;
					break;
#endif
				case 'n':
					p_aiflags = AI_NUMERICHOST;
					break;
				default:
					yyerror("invalid flag");
					return -1;
				}
		}
break;
case 66:
#line 682 "parse.y"
{
			yyval.res = parse_addr(yyvsp[0].val.buf, NULL);
			if (yyval.res == NULL) {
				/* yyerror already called by parse_addr */
				return -1;
			}
		}
break;
case 67:
#line 693 "parse.y"
{
			yyval.res = parse_addr(yyvsp[0].val.buf, NULL);
			if (yyval.res == NULL) {
				/* yyerror already called by parse_addr */
				return -1;
			}
		}
break;
case 68:
#line 701 "parse.y"
{
			yyval.res = parse_addr(yyvsp[-1].val.buf, yyvsp[0].val.buf);
			if (yyval.res == NULL) {
				/* yyerror already called by parse_addr */
				return -1;
			}
		}
break;
case 69:
#line 711 "parse.y"
{ yyval.num = -1; }
break;
case 70:
#line 712 "parse.y"
{ yyval.num = yyvsp[0].ulnum; }
break;
case 71:
#line 717 "parse.y"
{
			yyval.val.buf = strdup("0");
			if (!yyval.val.buf) {
				yyerror("insufficient memory");
				return -1;
			}
			yyval.val.len = strlen(yyval.val.buf);
		}
break;
case 72:
#line 726 "parse.y"
{
			yyval.val.buf = strdup("0");
			if (!yyval.val.buf) {
				yyerror("insufficient memory");
				return -1;
			}
			yyval.val.len = strlen(yyval.val.buf);
		}
break;
case 73:
#line 735 "parse.y"
{
			char buf[20];
			snprintf(buf, sizeof(buf), "%lu", yyvsp[-1].ulnum);
			yyval.val.buf = strdup(buf);
			if (!yyval.val.buf) {
				yyerror("insufficient memory");
				return -1;
			}
			yyval.val.len = strlen(yyval.val.buf);
		}
break;
case 74:
#line 746 "parse.y"
{
			yyval.val = yyvsp[-1].val;
		}
break;
case 75:
#line 752 "parse.y"
{ yyval.num = yyvsp[0].ulnum; }
break;
case 76:
#line 753 "parse.y"
{ yyval.num = IPSEC_ULPROTO_ANY; }
break;
case 77:
#line 754 "parse.y"
{ 
				yyval.num = IPPROTO_TCP; 
			}
break;
case 78:
#line 758 "parse.y"
{
			struct protoent *ent;

			ent = getprotobyname(yyvsp[0].val.buf);
			if (ent)
				yyval.num = ent->p_proto;
			else {
				if (strcmp("icmp6", yyvsp[0].val.buf) == 0) {
					yyval.num = IPPROTO_ICMPV6;
				} else if(strcmp("ip4", yyvsp[0].val.buf) == 0) {
					yyval.num = IPPROTO_IPV4;
				} else {
					yyerror("invalid upper layer protocol");
					return -1;
				}
			}
			endprotoent();
		}
break;
case 79:
#line 780 "parse.y"
{
			yyval.val.buf = NULL;
			yyval.val.len = 0;
		}
break;
case 80:
#line 785 "parse.y"
{
			yyval.val.buf = strdup(yyvsp[0].val.buf);
			if (!yyval.val.buf) {
				yyerror("insufficient memory");
				return -1;
			}
			yyval.val.len = strlen(yyval.val.buf);
		}
break;
case 81:
#line 797 "parse.y"
{
			char *policy;
#ifdef HAVE_PFKEY_POLICY_PRIORITY
			struct sadb_x_policy *xpl;
#endif

			policy = ipsec_set_policy(yyvsp[0].val.buf, yyvsp[0].val.len);
			if (policy == NULL) {
				yyerror(ipsec_strerror());
				return -1;
			}

			yyval.val.buf = policy;
			yyval.val.len = ipsec_get_policylen(policy);

#ifdef HAVE_PFKEY_POLICY_PRIORITY
			xpl = (struct sadb_x_policy *) yyval.val.buf;
			last_priority = xpl->sadb_x_policy_priority;
#endif
		}
break;
case 82:
#line 820 "parse.y"
{ yyval.val = yyvsp[0].val; }
break;
case 83:
#line 826 "parse.y"
{
			exit_now = 1;
			YYACCEPT;
		}
break;
#line 2010 "y.tab.c"
    }
    yyssp -= yym;
    yystate = *yyssp;
    yyvsp -= yym;
    yym = yylhs[yyn];
    if (yystate == 0 && yym == 0)
    {
#if YYDEBUG
        if (yydebug)
            printf("%sdebug: after reduction, shifting from state 0 to\
 state %d\n", YYPREFIX, YYFINAL);
#endif
        yystate = YYFINAL;
        *++yyssp = YYFINAL;
        *++yyvsp = yyval;
        if (yychar < 0)
        {
            if ((yychar = yylex()) < 0) yychar = 0;
#if YYDEBUG
            if (yydebug)
            {
                yys = 0;
                if (yychar <= YYMAXTOKEN) yys = yyname[yychar];
                if (!yys) yys = "illegal-symbol";
                printf("%sdebug: state %d, reading %d (%s)\n",
                        YYPREFIX, YYFINAL, yychar, yys);
            }
#endif
        }
        if (yychar == 0) goto yyaccept;
        goto yyloop;
    }
    if ((yyn = yygindex[yym]) && (yyn += yystate) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == yystate)
        yystate = yytable[yyn];
    else
        yystate = yydgoto[yym];
#if YYDEBUG
    if (yydebug)
        printf("%sdebug: after reduction, shifting from state %d \
to state %d\n", YYPREFIX, *yyssp, yystate);
#endif
    if (yyssp >= yysslim && yygrowstack())
    {
        goto yyoverflow;
    }
    *++yyssp = yystate;
    *++yyvsp = yyval;
    goto yyloop;
yyoverflow:
    yyerror("yacc stack overflow");
yyabort:
    return (1);
yyaccept:
    return (0);
}
