/*	$KAME: addrconf.c,v 1.8 2005/09/16 11:30:13 suz Exp $	*/

/*
 * Copyright (C) 2002 WIDE Project.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*-------------------------------------------------------------------------------------
// Copyright 2007, Texas Instruments Incorporated
//
// This program has been modified from its original operation by Texas Instruments
// to do the following:
//      - Made changes to the code at various places to add plugin hooks.
//      - Changed the return type of na_ifaddrconf to return the error status from 
//        ioctls on the interface.
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
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/queue.h>
#include <sys/ioctl.h>

#include <net/if.h>
#ifdef __FreeBSD__
#include <net/if_var.h>
#endif

#include <netinet/in.h>

#ifdef __KAME__
#include <netinet6/in6_var.h>
#include <netinet6/nd6.h>
#endif

#include <errno.h>
#include <syslog.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "dhcp6c.h"
#include "dhcp6.h"
#include "config.h"
#include "common.h"
#include "timer.h"
#include "dhcp6c_ia.h"
#include "prefixconf.h"
#include "addrconf.h"

TAILQ_HEAD(statefuladdr_list, statefuladdr);
struct iactl_na {
	struct iactl common;
	struct statefuladdr_list statefuladdr_head;
};
#define iacna_ia common.iactl_ia
#define iacna_callback common.callback
#define iacna_isvalid common.isvalid
#define iacna_duration common.duration
#define iacna_renew_data common.renew_data
#define iacna_rebind_data common.rebind_data
#define iacna_reestablish_data common.reestablish_data
#define iacna_release_data common.release_data
#define iacna_cleanup common.cleanup

struct statefuladdr {
	TAILQ_ENTRY (statefuladdr) link;

	struct dhcp6_statefuladdr addr;
	time_t updatetime;
	struct dhcp6_timer *timer;
	struct iactl_na *ctl;
	struct dhcp6_if *dhcpif;
};

static struct statefuladdr *find_addr __P((struct statefuladdr_list *,
    struct dhcp6_statefuladdr *));
static void remove_addr __P((struct statefuladdr *));
static int isvalid_addr __P((struct iactl *));
static u_int32_t duration_addr __P((struct iactl *));
static void cleanup_addr __P((struct iactl *));
static int renew_addr __P((struct iactl *, struct dhcp6_ia *,
    struct dhcp6_eventdata **, struct dhcp6_eventdata *));
static void na_renew_data_free __P((struct dhcp6_eventdata *));

static struct dhcp6_timer *addr_timo __P((void *));

static int na_ifaddrconf __P((ifaddrconf_cmd_t, struct statefuladdr *));

extern struct dhcp6_timer *client6_timo __P((void *));

/**************************************************************************
 * FUNCTION NAME : update_address
 **************************************************************************
 * DESCRIPTION   :
 * Does the following:
 *      - Validates the "valid" and "preferred" lifetimes for this 
 *        stateful address.
 *      - Sets the IPv6 address to the interface.
 *      - Starts a timer on "valid lifetime" of the address. 
 *************************************************************************/
int
update_address(ia, addr, dhcpifp, ctlp, callback)
	struct ia *ia;
	struct dhcp6_statefuladdr *addr;
	struct dhcp6_if *dhcpifp;
	struct iactl **ctlp;
	void (*callback)__P((struct ia *));
{
	struct iactl_na *iac_na = (struct iactl_na *)*ctlp;
	struct statefuladdr *sa;
	int sacreate = 0;
	struct timeval timo, now;

	/*
	 * A client discards any addresses for which the preferred
         * lifetime is greater than the valid lifetime.
	 * [RFC3315 22.6] 
	 */
	if (addr->vltime != DHCP6_DURATION_INFINITE &&
	    (addr->pltime == DHCP6_DURATION_INFINITE ||
	    addr->pltime > addr->vltime)) {
		dprintf(LOG_INFO, FNAME, "invalid address %s: "
		    "pltime (%lu) is larger than vltime (%lu)",
		    in6addr2str(&addr->addr, 0),
		    addr->pltime, addr->vltime);
		return (-1);
	}

	if (iac_na == NULL) {
		if ((iac_na = malloc(sizeof(*iac_na))) == NULL) {
			dprintf(LOG_NOTICE, FNAME, "memory allocation failed");
			return (-1);
		}
		memset(iac_na, 0, sizeof(*iac_na));
		iac_na->iacna_ia = ia;
		iac_na->iacna_callback = callback;
		iac_na->iacna_isvalid = isvalid_addr;
		iac_na->iacna_duration = duration_addr;
		iac_na->iacna_cleanup = cleanup_addr;
		iac_na->iacna_renew_data =
		    iac_na->iacna_rebind_data =
		    iac_na->iacna_release_data =
		    iac_na->iacna_reestablish_data = renew_addr;

		TAILQ_INIT(&iac_na->statefuladdr_head);
		*ctlp = (struct iactl *)iac_na;
	}

	/* search for the given address, and make a new one if it fails */
	if ((sa = find_addr(&iac_na->statefuladdr_head, addr)) == NULL) {
		if ((sa = malloc(sizeof(*sa))) == NULL) {
			dprintf(LOG_NOTICE, FNAME, "memory allocation failed");
			return (-1);
		}
		memset(sa, 0, sizeof(*sa));
		sa->addr.addr = addr->addr;
		sa->ctl = iac_na;
		TAILQ_INSERT_TAIL(&iac_na->statefuladdr_head, sa, link);
		sacreate = 1;
	}

	/* update the timestamp of update */
	get_sysUpTime(&now);
    sa->updatetime = now.tv_sec;

	/* update the prefix according to addr */
	sa->addr.pltime = addr->pltime;
	sa->addr.vltime = addr->vltime;
	sa->dhcpif = dhcpifp;
	dprintf(LOG_DEBUG, FNAME, "%s an address %s pltime=%lu, vltime=%lu",
	    sacreate ? "create" : "update",
	    in6addr2str(&addr->addr, 0), addr->pltime, addr->vltime);

#ifdef TI_DHCP6C_CONFIG_INTF
	if (sa->addr.vltime != 0)
		na_ifaddrconf(IFADDRCONF_ADD, sa);
#endif

	/*
	 * If the new vltime is 0, this address immediately expires.
	 * Otherwise, set up or update the associated timer.
	 */
	switch (sa->addr.vltime) {
	case 0:
		remove_addr(sa);
		break;
	case DHCP6_DURATION_INFINITE:
		if (sa->timer)
			dhcp6_remove_timer(&sa->timer);
		break;
	default:
		if (sa->timer == NULL) {
			sa->timer = dhcp6_add_timer(addr_timo, sa);
			if (sa->timer == NULL) {
				dprintf(LOG_NOTICE, FNAME,
				    "failed to add stateful addr timer");
				remove_addr(sa); /* XXX */
				return (-1);
			}
		}
		/* update the timer */
		timo.tv_sec = sa->addr.vltime;
		timo.tv_usec = 0;

		dhcp6_set_timer(&timo, sa->timer);
		break;
	}

	return (0);
}

static struct statefuladdr *
find_addr(head, addr)
	struct statefuladdr_list *head;
	struct dhcp6_statefuladdr *addr;
{
	struct statefuladdr *sa;

	for (sa = TAILQ_FIRST(head); sa; sa = TAILQ_NEXT(sa, link)) {
		if (!IN6_ARE_ADDR_EQUAL(&sa->addr.addr, &addr->addr))
			continue;
		return (sa);
	}

	return (NULL);
}

/**************************************************************************
 * FUNCTION NAME : remove_addr
 **************************************************************************
 * DESCRIPTION   :
 * Called when an address expires, i.e. not renewed withing its valid 
 * lifetime. This function essentially cleans up the address and its
 * associated timers.
 * Does the following:
 *      - Removes the timer associated with this address.
 *      - Removes the IPv6 address from the interface.
 *      - Removes the address from the list of valid addresses.
 *************************************************************************/
static void
remove_addr(sa)
	struct statefuladdr *sa;
{
	dprintf(LOG_DEBUG, FNAME, "remove an address %s",
	    in6addr2str(&sa->addr.addr, 0));

	if (sa->timer)
		dhcp6_remove_timer(&sa->timer);

	TAILQ_REMOVE(&sa->ctl->statefuladdr_head, sa, link);
	na_ifaddrconf(IFADDRCONF_REMOVE, sa);
	free(sa);
}

/**************************************************************************
 * FUNCTION NAME : isvalid_addr
 **************************************************************************
 * DESCRIPTION   :
 * Checks if the IPv6 address is a valid address.
 *************************************************************************/
static int
isvalid_addr(iac)
	struct iactl *iac;
{
	struct iactl_na *iac_na = (struct iactl_na *)iac;

	if (TAILQ_EMPTY(&iac_na->statefuladdr_head))
		return (0);	/* this IA is invalid */
	return (1);
}

/**************************************************************************
 * FUNCTION NAME : duration_addr
 **************************************************************************
 * DESCRIPTION   :
 * Determines the amount of time left for this address in its preferred 
 * lifetime.
 *************************************************************************/
static u_int32_t
duration_addr(iac)
	struct iactl *iac;
{
	struct iactl_na *iac_na = (struct iactl_na *)iac;
	struct statefuladdr *sa;
	u_int32_t base = DHCP6_DURATION_INFINITE, pltime, passed;
	struct timeval now;

	/* Determine the smallest period until pltime expires. */
	get_sysUpTime(&now);
	for (sa = TAILQ_FIRST(&iac_na->statefuladdr_head); sa;
	    sa = TAILQ_NEXT(sa, link)) {
		passed = now.tv_sec > sa->updatetime ?
		    (u_int32_t)(now.tv_sec - sa->updatetime) : 0;
		pltime = sa->addr.pltime > passed ?
		    sa->addr.pltime - passed : 0;

		if (base == DHCP6_DURATION_INFINITE || pltime < base)
			base = pltime;
	}

	return (base);
}

/**************************************************************************
 * FUNCTION NAME : cleanup_addr
 **************************************************************************
 * DESCRIPTION   :
 * Cleansup all the addresses associated with an IA.
 *************************************************************************/
static void
cleanup_addr(iac)
	struct iactl *iac;
{
	struct iactl_na *iac_na = (struct iactl_na *)iac;
	struct statefuladdr *sa;

	while ((sa = TAILQ_FIRST(&iac_na->statefuladdr_head)) != NULL) {
		TAILQ_REMOVE(&iac_na->statefuladdr_head, sa, link);
		remove_addr(sa);
	}

	free(iac);
}

/**************************************************************************
 * FUNCTION NAME : renew_addr
 **************************************************************************
 * DESCRIPTION   :
 * Builds up the event data, i.e makes a list of all the addresses within 
 * a given IA that need to be renewed/rebound
 *************************************************************************/
static int
renew_addr(iac, iaparam, evdp, evd)
	struct iactl *iac;
	struct dhcp6_ia *iaparam;
	struct dhcp6_eventdata **evdp, *evd;
{
	struct iactl_na *iac_na = (struct iactl_na *)iac;
	struct statefuladdr *sa;
	struct dhcp6_list *ial = NULL, pl;

	TAILQ_INIT(&pl);
	for (sa = TAILQ_FIRST(&iac_na->statefuladdr_head); sa;
	    sa = TAILQ_NEXT(sa, link)) {
		if (dhcp6_add_listval(&pl, DHCP6_LISTVAL_STATEFULADDR6,
		    &sa->addr, NULL) == NULL)
			goto fail;
	}

	if ((ial = malloc(sizeof(*ial))) == NULL)
		goto fail;
	TAILQ_INIT(ial);
	if (dhcp6_add_listval(ial, DHCP6_LISTVAL_IANA, iaparam, &pl) == NULL)
		goto fail;
	dhcp6_clear_list(&pl);

	evd->type = DHCP6_EVDATA_IANA;
	evd->data = (void *)ial;
	evd->privdata = (void *)evdp;
	evd->destructor = na_renew_data_free;

	return (0);

  fail:
	dhcp6_clear_list(&pl);
	if (ial)
		free(ial);
	return (-1);
}

/**************************************************************************
 * FUNCTION NAME : na_renew_data_free
 **************************************************************************
 * DESCRIPTION   :
 * Destructor for the event data. Cleans up.
 *************************************************************************/
static void
na_renew_data_free(evd)
	struct dhcp6_eventdata *evd;
{
	struct dhcp6_list *ial;

	if (evd->type != DHCP6_EVDATA_IANA) {
		dprintf(LOG_ERR, FNAME, "assumption failure");
		exit(1);
	}

	if (evd->privdata)
		*(struct dhcp6_eventdata **)evd->privdata = NULL;
	ial = (struct dhcp6_list *)evd->data;
	dhcp6_clear_list(ial);
	free(ial);
}

/**************************************************************************
 * FUNCTION NAME : addr_timo
 **************************************************************************
 * DESCRIPTION   :
 * Timeout handler. This function is called when the valid lifetime of
 * an IPv6 address expires. This function calls a bunch of functions
 * to clean up the address and remove it permanently for use by the device.
 *************************************************************************/
static struct dhcp6_timer *
addr_timo(arg)
	void *arg;
{
	struct statefuladdr *sa = (struct statefuladdr *)arg;
	struct ia *ia;
	void (*callback)__P((struct ia *));

	dprintf(LOG_DEBUG, FNAME, "address timeout for %s",
	    in6addr2str(&sa->addr.addr, 0));

	ia = sa->ctl->iacna_ia;
	callback = sa->ctl->iacna_callback;

	if (sa->timer)
		dhcp6_remove_timer(&sa->timer);

	remove_addr(sa);

	(*callback)(ia);

	return (NULL);
}

/**************************************************************************
 * FUNCTION NAME : na_ifaddrconf
 **************************************************************************
 * DESCRIPTION   :
 * Called to add/updated/remove an IPv6 address from the interface.
 *************************************************************************/
static int
na_ifaddrconf(cmd, sa)
	ifaddrconf_cmd_t cmd;
	struct statefuladdr *sa;
{
	struct dhcp6_statefuladdr *addr;
	struct sockaddr_in6 sin6;

	addr = &sa->addr;
	memset(&sin6, 0, sizeof(sin6));
	sin6.sin6_family = AF_INET6;
#ifdef HAVE_SA_LEN
	sin6.sin6_len = sizeof(sin6);
#endif
	sin6.sin6_addr = addr->addr;

	return (ifaddrconf(cmd, sa->dhcpif->ifname, &sin6, 128,
	    addr->pltime, addr->vltime));
}
