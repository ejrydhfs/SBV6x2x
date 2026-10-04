/*-------------------------------------------------------------------------------------
// Copyright 2007, Texas Instruments Incorporated
//
// This program has been modified from its original operation by Texas Instruments
// to do the following:
// 1. Added the below mentioned wrapper APIs to simplify things for 
//    calling application. 
// 2. Also defined ti_hostname_answer_t data structure that encapsulates TTL, query type,
//    class with the IP addresses received as answer to hostname query.
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

/*
 * TI API Wrappers for Host Name / SRV RR Query Submissions.
 *      1. ti_init_dnsclient
 *      2. ti_query_hostname
 *      3. ti_query_srvrr
 *      4. ti_exit_dnsclient
 *      5. ti_dns_start_query_scheduler
 *      6. ti_dns_parse_hostname_answer
 *      7. ti_dns_parse_srvrr_answer
 *      8. ti_dns_clean_hostname_query
 *      9. ti_dns_clean_srvrr_query
 *     10. ti_dns_get_hostname_qid
 *     11. ti_dns_get_hostname_resctx
 *     12. ti_dns_get_srvrr_qid
 *     13. ti_dns_get_srvrr_resctx
 *     14. ti_dns_get_resctx
 *     15. ti_dns_cancel_hostname_query
 *     16. ti_dns_cancel_srvrr_query
 */

/**************************************************************************
 * Constant definitions.
 *************************************************************************/
#define QBUFSZ RULI_LIMIT_DNAME_TEXT_BUFSZ


/**************************************************************************
 * Data structure definitions.
 *************************************************************************/
/*
 * Data structure to store SRV query buffers
 */
typedef struct {
    char txt_service[QBUFSZ];       /* Service string - _service._protocol */
    int  txt_service_len;           /* Service string length */
    char txt_domain[QBUFSZ];        /* Domain string - domain.com */
    int  txt_domain_len;            /* Domain string length */
} srv_qbuf_t;

/* TI DNS Error Codes */
typedef enum {
    TI_DNS_SUCCESS,             /* Success */
    TI_DNS_TIMEOUT,             /* Timed out waiting for DNS answer */
    TI_DNS_ANSWER_UNPARSEABLE,  /* DNS answer not in the correct format thus unparseable */
    TI_DNS_MISSING_ADDRESS,     /* Hostname answer doesnt contain an IP address */
    TI_DNS_SRV_CODE_UNAVAIL,    /* DNS server returned Service unavailable for the SRV RR query sent */
    TI_DNS_QUERY_FAILURE,       /* Other error */
	TI_DNS_ERROR,               /* General DNS client processing error */
	TI_DNS_ERROR_LAST, 
} TI_DNS_ERROR_CODE;

/* Data structure to store IP address received as an answer to hostname query with TTL and other additional
   information received in the answer.
*/
typedef struct {
    ruli_uint16_t      type;    /* Query type - A/SRV */
    ruli_uint16_t      qclass;  /* Query class - IN */
    ruli_uint32_t      ttl;     /* Time to live */
    ruli_list_t        addr_list;     /* IP address list (answer) for hostname */
} ti_hostname_answer_t;


/**************************************************************************
 * TI Wrapper API definitions.
 *************************************************************************/

int ti_dns_init_resolver(unsigned int dns_timeout, unsigned int dns_retry_count, char* dns_interface,
                        int serverc, char** serverv);
int ti_dns_exit_resolver(int ti_res_handle);
int ti_dns_get_resctx(int ti_res_handle);
int ti_dns_query_srvrr(int ti_res_handle, char *domain, void *(*call)(int search,void *arg));
int ti_dns_query_hostname(int ti_res_handle, char *domain, void *(*call)(int qry_buf,void *arg));
void* ti_dns_start_query_scheduler(int ti_res_handle);
int ti_dns_parse_hostname_answer(int qry, void *arg, ti_hostname_answer_t* hostname_answer);
int ti_dns_get_hostname_qid(int qry);
int ti_dns_get_hostname_resctx(int qry_buf);
int ti_dns_clean_hostname_query(int qry_buf, void *arg);
int ti_dns_parse_srvrr_answer(int search_buf, void *search_arg);
int ti_dns_get_srvrr_qid(int search_buf);
int ti_dns_get_srvrr_resctx(int search_buf);
int ti_dns_clean_srvrr_query(int search_buf, srv_qbuf_t *qbuf);
char *ti_dns_error2str(TI_DNS_ERROR_CODE error);
int ti_dns_cancel_hostname_query(int ti_res_handle, int query_id);
int ti_dns_cancel_srvrr_query(int ti_res_handle, int query_id);
