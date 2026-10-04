/*----------------------------------------------------------------------------
// Copyright 2007, Texas Instruments Incorporated
//
// This program has been modified from its original operation by Texas Instruments
// to do the following:
//
// 1. PKTSIZE
//
// THIS MODIFIED SOFTWARE AND DOCUMENTATION ARE PROVIDED
// "AS IS," AND TEXAS INSTRUMENTS MAKES NO REPRESENTATIONS
// OR WARRENTIES, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
// TO, WARRANTIES OF MERCHANTABILITY OR FITNESS FOR ANY
// PARTICULAR PURPOSE OR THAT THE USE OF THE SOFTWARE OR
// DOCUMENTATION WILL NOT INFRINGE ANY THIRD PARTY PATENTS,
// COPYRIGHTS, TRADEMARKS OR OTHER RIGHTS.
// 
// These changes are covered as per original license
//-----------------------------------------------------------------------------*/
#define PKTSIZE 65464       /* max tftp pkt size */

int synchnet(int);
struct tftphdr *r_init(void);
struct tftphdr *w_init(void);
int readit(FILE *file, struct tftphdr **dpp, int convert);
int writeit(FILE *file, struct tftphdr **dpp, int ct, int convert);
void read_ahead(FILE *file, int convert /* if true, convert to ascii */);
int write_behind(FILE *file, int convert);

void mysignal(int, void (*func)(int));
