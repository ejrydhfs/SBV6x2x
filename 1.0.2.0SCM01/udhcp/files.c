/* 
 * files.c -- DHCP server file manipulation *
 * Rewrite by Russ Dill <Russ.Dill@asu.edu> July 2001
 */

//-------------------------------------------------------------------------------------
// Copyright 2006, Texas Instruments Incorporated
//
// This program has been modified from its original operation by Texas Instruments
// to do the following:
// 
// 1. NSP Policy Routing Framework
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
//------------------------------------------------------------------------------------- 

/* Change Description:07112006
 * 1. k_arr is modified to accomodate changes for DHCP class
 * 2. Modified functions to include classindex to indicate correct server_config
 * 3. Modified create_config for special handling of option 43 and read the class info
 */

/* Change Description:09252006
 * 1. k_arr is removed and read_config modified 
 */

#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include <netdb.h>
#if 0
#include <linux/pr.h>
#endif

#include "debug.h"
#include "dhcpd.h"
#include "files.h"
#include "options.h"
#include "leases.h"

#define LEASE_ADD		1
#define LEASE_DEL 	2

#define LEASEFILE_CLASS "/landhcps00.leases"
#define LEASEFILE_NO_CLASS "/landhcps0.leases"
#define IFID_LOCATION 9
#define CLASSID_LOCATION 10

extern void write_to_delta(u_int8_t *chaddr, u_int32_t yiaddr, u_int8_t *hname,unsigned long leasetime,u_int8_t action,int classindex);

/* on these functions, make sure you datatype matches */
static int read_ip(char *line, void *arg)
{
	struct in_addr *addr = arg;
	struct hostent *host;
	int retval = 1;
#if 0
    unsigned int pr_mark = UDHCP_PR_MARK;
#endif

	if (!inet_aton(line, addr))
	{
    /* Adding an option which allows to pass a mark value for a particular
	 * socket. 
	 */
#if 1
		if ((host = gethostbyname(line))) 
#else
		if ((host = ti_gethostbyname(line, pr_mark))) 
#endif
			addr->s_addr = *((unsigned long *) host->h_addr_list[0]);
		else retval = 0;
	}
	return retval;
}


static int read_str(char *line, void *arg)
{
	char **dest = arg;
	
	if (*dest) free(*dest);
	*dest = strdup(line);
	
	return 1;
}


static int read_u32(char *line, void *arg)
{
	u_int32_t *dest = arg;
	char *endptr;
	*dest = strtoul(line, &endptr, 0);
	return endptr[0] == '\0';
}


static int read_yn(char *line, void *arg)
{
	char *dest = arg;
	int retval = 1;

	if (!strcasecmp("yes", line))
		*dest = 1;
	else if (!strcasecmp("no", line))
		*dest = 0;
	else retval = 0;
	
	return retval;
}


/* read a dhcp option and add it to opt_list */
static void read_opt(char *line, void *arg, int ifid, int classindex)
{
	struct option_set **opt_list = arg;
	char *opt, *val, *endptr;
	struct dhcp_option *option = NULL;
	int retval = 0, length = 0;
    /* Fix for alignment issues seen on ARM board. ARM has stricter
       alignment rules for int. The char buffer here is passed to
       read_ip function to process options which hold IP address values 
       (eg router, dns, subnet etc), where it is cast to int and if its not 
       aligned on word boundary, will result in Bus error on ARM. Fix is
       to force the buffer to start on word boundary.
    */
	char buffer[255] __attribute__ ((aligned));
	u_int16_t result_u16;
	u_int32_t result_u32;
	int i;

	sprintf(buffer,"%s",line);
	opt = strtok(buffer," ");
	if(!opt){
		return ;
	}
	if(!strcmp(opt, "vendorid")){
		server_config[ifid][classindex].classifier.id = strdup("vendorid");
		val = strtok(NULL,"\"");
		server_config[ifid][classindex].classifier.value = strdup(val);
		return ;
	}
	if(!strcmp(opt, "userclass")){
		server_config[ifid][classindex].classifier.id = strdup("userclass");
		val = strtok(NULL,"\"");
		server_config[ifid][classindex].classifier.value = strdup(val);
		return ;
	}
	if(!strcmp(opt, "vendorinfo")){
		val = strtok(NULL," ");
		server_config[ifid][classindex].vendorinfo = strdup(val);
		return ;
	}
	if (!(opt = strtok(line, " \t=")))
		return ;
	
	for (i = 0; options[i].code; i++)
		if (!strcmp(options[i].name, opt))
			option = &(options[i]);
		
	if (!option)
		return ;
	
	do {
		val = strtok(NULL, ", \t");
		if (val) {
			length = option_lengths[option->flags & TYPE_MASK];
			retval = 0;
			switch (option->flags & TYPE_MASK) {
			case OPTION_IP:
				retval = read_ip(val, buffer);
				break;
			case OPTION_IP_PAIR:
				retval = read_ip(val, buffer);
				if (!(val = strtok(NULL, ", \t/-"))) retval = 0;
				if (retval) retval = read_ip(val, buffer + 4);
				break;
			case OPTION_STRING:
				length = strlen(val);
				if (length > 0) {
					if (length > 254) length = 254;
					memcpy(buffer, val, length);
					retval = 1;
				}
				break;
			case OPTION_BOOLEAN:
				retval = read_yn(val, buffer);
				break;
			case OPTION_U8:
				buffer[0] = strtoul(val, &endptr, 0);
				retval = (endptr[0] == '\0');
				break;
			case OPTION_U16:
				result_u16 = htons(strtoul(val, &endptr, 0));
				memcpy(buffer, &result_u16, 2);
				retval = (endptr[0] == '\0');
				break;
			case OPTION_S16:
				result_u16 = htons(strtol(val, &endptr, 0));
				memcpy(buffer, &result_u16, 2);
				retval = (endptr[0] == '\0');
				break;
			case OPTION_U32:
				result_u32 = htonl(strtoul(val, &endptr, 0));
				memcpy(buffer, &result_u32, 4);
				retval = (endptr[0] == '\0');
				break;
			case OPTION_S32:
				result_u32 = htonl(strtol(val, &endptr, 0));	
				memcpy(buffer, &result_u32, 4);
				retval = (endptr[0] == '\0');
				break;
			default:
				break;
			}
			if (retval) 
				attach_option(opt_list, option, buffer, length);
		};
	} while (val && retval && option->flags & OPTION_LIST);
	//return retval;
	return ;
}


int read_config(char *file)
{
	FILE *in;
    char buffer[200], orig[200], *token, *line;
    int i, index,classindex = 0,ifid = 0;
    unsigned int len = 0;
    char def[50];
    char * leasefile;
    int prev;
    for (index = 0; index < MAX_INTERFACES; index++){
        for(classindex = 0 ; classindex < MAX_CLASSES; classindex++){
            switch(index){
                case 0:
                    sprintf(def,"eth0");
                break;
                case 1:
                    sprintf(def,"eth1");
                break;
                case 2:
                case 3:
                case 4:
                case 5:
                    sprintf(def,"usbrndis");
                break;
            }
            read_str(def, &server_config[index][classindex].interface);
            switch(classindex){
                case 0:
                    sprintf(def,"192.168.%d.20",index);
                break;
                case 1:
                    sprintf(def,"192.168.%d.115",index);
                break;
                case 2:
                    sprintf(def,"192.168.%d.156",index);
                break;
                case 3:
                    sprintf(def,"192.168.%d.197",index);
                break;
            }
            read_ip(def, &server_config[index][classindex].start);
            switch(classindex){
                case 0:
                    sprintf(def,"192.168.%d.114",index);
                break;
                case 1:
                    sprintf(def,"192.168.%d.155",index);
                break;
                case 2:
                    sprintf(def,"192.168.%d.196",index);
                break;
                case 3:
                    sprintf(def,"192.168.%d.254",index);
                break;
            }
            read_ip(def, &server_config[index][classindex].end);

            sprintf(def,"/var/lib/misc/udhcpd%d%d.leases",ifid,classindex);
            read_str(def, &server_config[index][classindex].lease_file);

            sprintf(def,"604800");
            read_u32(def, &server_config[index][classindex].inflease_time);
            
            sprintf(def,"yes");
            read_yn(def, &server_config[index][classindex].remaining);

            sprintf(def,"254");
            read_u32(def, &server_config[index][classindex].max_leases);

            sprintf(def,"1");
            read_u32(def, &server_config[index][classindex].auto_time);

            sprintf(def,"3600");
            read_u32(def, &server_config[index][classindex].decline_time);

            sprintf(def,"3600");
            read_u32(def, &server_config[index][classindex].conflict_time);

            sprintf(def,"60");
            read_u32(def, &server_config[index][classindex].offer_time);

            sprintf(def,"60");
            read_u32(def, &server_config[index][classindex].min_lease);

            sprintf(def,"/var/run/udhcpd.pid");
            read_str(def, &server_config[index][classindex].pidfile);

            sprintf(def,"0.0.0.0");
            read_ip(def, &server_config[index][classindex].siaddr);
        }
    }
	if (!(in = fopen(file, "r"))) {
		LOG(LOG_ERR, "unable to open config file: %s", file);
		return 0;
	}

	index = -1;	
    prev = -1;
    ifid = -1;
    while (fgets(buffer, 200, in)) {
		if (strchr(buffer, '\n')) *(strchr(buffer, '\n')) = '\0';
        strncpy(orig, buffer, 200);
		if (strchr(buffer, '#')) *(strchr(buffer, '#')) = '\0';
		token = buffer + strspn(buffer, " \t");
		if (*token == '\0') continue;
		line = token + strcspn(token, " \t=");
		if (*line == '\0') continue;
		*line = '\0';
		line++;
		/* eat leading whitespace */
		line = line + strspn(line, " \t=");
		/* eat trailing whitespace */
		for (i = strlen(line) ; i > 0 && isspace(line[i-1]); i--);
		line[i] = '\0';
	
        if (!strcasecmp(token, "lease_file")) {
            sprintf(def,"%s",line);
            leasefile = strrchr(def,'/');
            if(!leasefile)
                return 0;
            len = strlen(leasefile);
            ifid = leasefile[IFID_LOCATION] - '0';
            if(ifid != prev)
            {
                prev = ifid;
				index++;
			}		
            if(len == strlen(LEASEFILE_CLASS))
            {
                classindex = leasefile[CLASSID_LOCATION] - '0';
            }else classindex = 0;
            sprintf(def,"/var/lib/misc/udhcpd%d%d.leases",ifid,classindex);
            if(!read_str(line, &server_config[index][classindex].lease_file))
                read_str(def, &server_config[index][classindex].lease_file);
            continue;
        }
        if (!strcasecmp(token, "start")) {
            switch(classindex){
                case 0:
                    sprintf(def,"192.168.%d.20",index);
                break;
                case 1:
                    sprintf(def,"192.168.%d.115",index);
                break;
                case 2:
                    sprintf(def,"192.168.%d.156",index);
                break;
                case 3:
                    sprintf(def,"192.168.%d.197",index);
                break;
            }
            if(!read_ip(line, &server_config[index][classindex].start))
                read_ip(def, &server_config[index][classindex].start);
            continue;
        }
        if (!strcasecmp(token, "end")) {
            switch(classindex){
                case 0:
                    sprintf(def,"192.168.%d.114",index);
                break;
                case 1:
                    sprintf(def,"192.168.%d.155",index);
                break;
                case 2:
                    sprintf(def,"192.168.%d.196",index);
                break;
                case 3:
                    sprintf(def,"192.168.%d.254",index);
                break;
            }
            if(!read_ip(line, &server_config[index][classindex].end))
                read_ip(def, &server_config[index][classindex].end);
            continue;
        }
        if (!strcasecmp(token, "interface")) {
            switch(classindex){
                case 0:
                    sprintf(def,"eth0");
                break;
                case 1:
                    sprintf(def,"eth1");
                break;
                case 2:
                case 3:
                case 4:
                case 5:
                    sprintf(def,"usbrndis");
                break;
            }
            if(!read_str(line, &server_config[index][classindex].interface))
                read_str(def, &server_config[index][classindex].interface);
			server_config[index][classindex].active=TRUE;
            continue;
        }
        if ((!strcasecmp(token, "opt")) || (!strcasecmp(token, "option"))){
            read_opt(line,&server_config[index][classindex].options,index,classindex);
            continue;
        }
        if (!strcasecmp(token, "inflease_time")) {
            sprintf(def,"604800");
            if(!read_u32(line,&server_config[index][classindex].inflease_time))
                read_u32(def,&server_config[index][classindex].inflease_time);
            continue;
        }
        if (!strcasecmp(token, "max_leases")) {
            sprintf(def,"254");
            if(!read_u32(line,&server_config[index][classindex].max_leases))
                read_u32(def,&server_config[index][classindex].max_leases);
            continue;
        }
        if (!strcasecmp(token, "remaining")) {
            sprintf(def,"yes");
            if(!read_yn(line,&server_config[index][classindex].remaining))
                read_yn(def,&server_config[index][classindex].remaining);
            continue;
        }
        if (!strcasecmp(token, "auto_time")) {
            sprintf(def,"1");
            if(!read_u32(line,&server_config[index][classindex].auto_time))
                read_u32(def,&server_config[index][classindex].auto_time);
            continue;
        }
        if (!strcasecmp(token, "decline_time")) {
            sprintf(def,"3600");
            if(!read_u32(line,&server_config[index][classindex].decline_time))
                read_u32(def,&server_config[index][classindex].decline_time);
            continue;
        }
        if (!strcasecmp(token, "conflict_time")) {
            sprintf(def,"3600");
            if(!read_u32(line,&server_config[index][classindex].conflict_time))
                read_u32(def,&server_config[index][classindex].conflict_time);
            continue;
        }
        if (!strcasecmp(token, "offer_time")) {
            sprintf(def,"60");
            if(!read_u32(line,&server_config[index][classindex].offer_time))
                read_u32(def,&server_config[index][classindex].offer_time);
            continue;
        }
        if (!strcasecmp(token, "min_lease")) {
            sprintf(def,"60");
            if(!read_u32(line,&server_config[index][classindex].min_lease))
                read_u32(def,&server_config[index][classindex].min_lease);
            continue;
        }
        if (!strcasecmp(token, "pidfile")) {
            sprintf(def,"/var/run/udhcpd.pid");
            if(!read_str(line,&server_config[index][classindex].pidfile))
                read_str(def,&server_config[index][classindex].pidfile);
            continue;
        }
        if (!strcasecmp(token, "notify_file")) {
            read_str(line,&server_config[index][classindex].notify_file);
            continue;
        }
        if (!strcasecmp(token, "siaddr")) {
            sprintf(def,"0.0.0.0");
            if(!read_ip(line,&server_config[index][classindex].siaddr))
                read_ip(def,&server_config[index][classindex].siaddr);
            continue;
        }
        if (!strcasecmp(token, "sname")) {
            read_str(line,&server_config[index][classindex].sname);
            continue;
        }
        if (!strcasecmp(token, "boot_file")) {
            read_str(line,&server_config[index][classindex].boot_file);
            continue;
        }
                      
    }
    no_of_ifaces = index + 1 ;
	fclose(in);
	return 1;
}


/* the dummy var is here so this can be a signal handler */
void write_leases(int ifid, int classindex)
{
	FILE *fp;
	unsigned int i;
	char buf[255];
	time_t curr = time(0);
	unsigned long lease_time;
  int j;
  unsigned char line[100];
  struct in_addr in;
	
	if (!(fp = fopen(server_config[ifid][classindex].lease_file, "w"))) {
		LOG(LOG_ERR, "Unable to open %s for writing", server_config[ifid][classindex].lease_file);
		return;
	}
	
	for (i = 0; i < server_config[ifid][classindex].max_leases; i++) {
		if (server_config[ifid][classindex].leases[i].yiaddr != 0) {
			if (server_config[ifid][classindex].remaining) {
				if (lease_expired(&(server_config[ifid][classindex].leases[i]),ifid,classindex))
				{
					lease_time = 0;
					if( server_config[ifid][classindex].leases[i].expires != 0)
						write_to_delta(server_config[ifid][classindex].leases[i].chaddr,
								server_config[ifid][classindex].leases[i].yiaddr,
								server_config[ifid][classindex].leases[i].hostname,
								0,LEASE_DEL,classindex);
					server_config[ifid][classindex].leases[i].expires = 0;
				}
				else
				{
					if( server_config[ifid][classindex].leases[i].expires != server_config[ifid][classindex].inflease_time)
        		lease_time = server_config[ifid][classindex].leases[i].expires - curr;
					else
						lease_time = server_config[ifid][classindex].inflease_time;
				}
			}
			else
				lease_time = server_config[ifid][classindex].leases[i].expires;

			sprintf(line,"%02x:%02x:%02x:%02x:%02x:%02x ",server_config[ifid][classindex].leases[i].chaddr[0],
									server_config[ifid][classindex].leases[i].chaddr[1],
									server_config[ifid][classindex].leases[i].chaddr[2],
									server_config[ifid][classindex].leases[i].chaddr[3],
									server_config[ifid][classindex].leases[i].chaddr[4],
									server_config[ifid][classindex].leases[i].chaddr[5]);

			in.s_addr = server_config[ifid][classindex].leases[i].yiaddr;
			j = sprintf(&line[18],"%s %ld %s\n",inet_ntoa(in),lease_time,server_config[ifid][classindex].leases[i].hostname);
      fwrite( line, 18+j, 1, fp);
		}
	}
	fclose(fp);
	
	if (server_config[ifid][classindex].notify_file) {
		sprintf(buf, "%s %s", server_config[ifid][classindex].notify_file, server_config[ifid][classindex].lease_file);
		system(buf);
	}
}


void read_leases(char *file, int ifid,int classindex)
{
	FILE *fp;
	int n;
	struct dhcpOfferedAddr lease;
	
  char ipaddress[17],macaddr[18];
  char hname[50];
  unsigned int leasetime;
  struct in_addr ipaddr;

  /* Added as a part of Alignment problem fixes for ARM */
  int j = 0;
  char mac_str[18] __attribute__ ((aligned));
  char* mac_byte __attribute__ ((aligned));
  u_int8_t mac_hex __attribute__ ((aligned));
  char *endptr;

/*  mac ipaddress leasetime hostname */
#define	readentry(fp) \
	fscanf((fp), "%s %s %u %s\n", \
		 macaddr, ipaddress, &leasetime, hname)

	if (!(fp = fopen(file, "r"))) {
		LOG(LOG_ERR, "Unable to open %s for reading", file);
		return;
	}

	for (;;)
  {
		if ((n = readentry(fp)) == EOF)
			break;
		if (!inet_aton(ipaddress, &ipaddr))
			continue;
    lease.yiaddr = ipaddr.s_addr;
    memset(lease.chaddr,0x00,16);

    /* Commented out as this was causing an alignment problems on ARM.
       All the array elements lease.chaddr[x] neednt be word aligned 
       and thus when type-casted to int will lead to Bus error on ARM
       as ARM requires word alignment on int.
    */
    /*
    sscanf(macaddr,"%x:%x:%x:%x:%x:%x", 
				(int *)&lease.chaddr[0],
				(int *)&lease.chaddr[1],
				(int *)&lease.chaddr[2],
				(int *)&lease.chaddr[3],
				(int *)&lease.chaddr[4],
				(int *)&lease.chaddr[5]);
    */
    /* Fix for ARM Alignment problem. Since not all elements of the array
       lease.chaddr are going to be word aligned, typecasting as shown above
       is not possible. So transfer byte-by-byte of the MAC address to the lease.chaddr
       array in hex int format.
    */
    j = 0;
    mac_byte = strtok(macaddr, ":");
    while(mac_byte != NULL)
    {
        /* mac_byte need not be byte aligned, so copy to a buffer which is aligned first */
        strcpy(mac_str, mac_byte);
        /* convert the word aligned string buffer to hex int */
        mac_hex = strtoul(mac_str, &endptr, 16);
        /* finally, copy the hex int to dest array */
        memcpy(&lease.chaddr[j], &mac_hex, sizeof(u_int8_t));
        /* Repeat the same for the next byte of MAC address till done */
        mac_byte = strtok(NULL, ":");
        j++;
    }

    lease.expires = leasetime;
    strcpy(lease.hostname,hname);
		if (lease.yiaddr >= server_config[ifid][classindex].start && lease.yiaddr <= server_config[ifid][classindex].end)
		{
			if (!server_config[ifid][classindex].remaining) lease.expires -= time(0);
			if (!(add_lease(lease.chaddr, lease.yiaddr, lease.expires, ifid, classindex,lease.hostname)))
      {
				LOG(LOG_WARNING, "Too many leases while loading %s\n", file);
				break;
			}
    }				
	}
	fclose(fp);
}
