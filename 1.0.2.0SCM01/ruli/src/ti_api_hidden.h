/**************************************************************************
 * FILE PURPOSE	:  	ti_api_hidden
 **************************************************************************
 * FILE NAME	:   ti_api_hidden.h
 *
 * DESCRIPTION	:
 *
 * TI API Wrappers and Data Structures for Host Name / SRV RR Query Submissions.
 *      1. ti_init_dnsclient
 *      2. ti_query_hostname
 *      3. ti_query_srvrr
 *      4. ti_exit_dnsclient
 *      5. ti_dns_start_query_scheduler
 *      6. ti_dns_parse_hostname_answer
 *      7. ti_dns_parse_srvrr_answer
 *      8. ti_dns_clean_hostname_query
 *      9. ti_dns_clean_srvrr_query
 *
 *	CALL-INs:
 *
 *	CALL-OUTs:
 *
 *	User-Configurable Items:
 *
 *	(C) Copyright 2007, Texas Instruments, Inc.
**+----------------------------------------------------------------------------------+**
**|                            ****                                                  |**
**|                            ****                                                  |**
**|                            ******o***                                            |**
**|                      ********_///_****                                           |**
**|                      ***** /_//_/ ****                                           |**
**|                       ** ** (__/ ****                                            |**
**|                           *********                                              |**
**|                            ****                                                  |**
**|                            ***                                                   |**
**|                                                                                  |**
**|         Copyright (c) 1998-2007 Texas Instruments Incorporated                   |**
**|                        ALL RIGHTS RESERVED                                       |**
**|                                                                                  |**
**| Permission is hereby granted to licensees of Texas Instruments                   |**
**| Incorporated (TI) products to use this computer program for the sole             |**
**| purpose of implementing a licensee product based on TI products.                 |**
**| No other rights to reproduce, use, or disseminate this computer                  |**
**| program, whether in part or in whole, are granted.                               |**
**|                                                                                  |**
**| TI makes no representation or warranties with respect to the                     |**
**| performance of this computer program, and specifically disclaims                 |**
**| any responsibility for any damages, special or consequential,                    |**
**| connected with the use of this program.                                          |**
**|                                                                                  |**
***************************************************************************************/


/**************************************************************************
 * Data structure definitions.
 *************************************************************************/

/* Data structure to maintain resolver settings */
typedef struct {
    char*                   interface;      /* Interface to use for sending DNS queries */

    /* RULI Settings */
    unsigned int            timeout;        /* Timeout in seconds for a DNS query */
    unsigned int            retry;          /* Number of times each server configured is tried */
    ruli_conf_handler_t*    handler;        /* CONF file handler */
    ruli_search_res_t*      search_res;     /* RULI Search context */
    ruli_res_t*             ruli_res_ctx;   /* RULI Resolver context */
    ruli_list_t*            server_list;    /* List of DNS servers to use */

    /* LIBOOP Settings */
    oop_source_sys*         source_sys;     /* OOP System Source */ 
    oop_source*             source;         /* OOP Source - Event registration interface */
} ti_resolver_ctx_t;
