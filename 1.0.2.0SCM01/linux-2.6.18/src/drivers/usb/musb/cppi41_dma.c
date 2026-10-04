/*
 *
 * cppi41_dma.c
 * Description:
 * see below
 *
 *
 * Copyright (C) 2008, Texas Instruments, Incorporated
 *
 *  This program is free software; you can distribute it and/or modify it
 *  under the terms of the GNU General Public License (Version 2) as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 *
 */


/*
 * This file implements a DMA  interface using TI's CPPI DMA.
 * For now it's DaVinci-only, but CPPI isn't specific to DaVinci or USB.
 */
#include <linux/config.h>
#include <linux/usb.h>

#include "cppi41_dma.h"
#include "musbdefs.h"
#include "musb_host.h"
#include "plat_arc.h"
#include "puma5.h"
#include "dma.h"
#include <asm/arch-avalanche/puma5/puma5_cppi.h>

/* CPPI DMA status 7-mar:
 *
 * - See musb_{host,gadget}.c for more info
 *
 * - Correct RX DMA generally forces the engine into irq-per-packet mode,
 *   which can easily saturate the CPU under non-mass-storage loads.
 */

/* REVISIT now we can avoid preallocating these descriptors; or
 * more simply, switch to a global freelist not per-channel ones.
 * Note: at full speed, 64 descriptors == 4K bulk data.
 */
//#define USB_CPPI4_DEBUG
#define NUM_TXCHAN_BD       64
#define NUM_RXCHAN_BD       64
#define USB_CPPI4_DBG_LEVEL  7

#define TX_SUBMIT	1
#define TX_COMPLETE 	2

//#define  DEBUG_CPPI_TD
//#define  USBDRV_DEBUG 1

#ifdef USBDRV_DEBUG
#define dprintk(x,...) printk(x, ## __VA_ARGS__)
#else
#define dprintk(x,...)
#endif

//#define DBG(level,fmt,args...) xprintk(level,KERN_DEBUG,fmt, ## args)

usbCppiChInfo usbTxChInfo[MAX_USB_CPPI4_CHANNEL+3];
usbCppiChInfo usbRxChInfo[MAX_USB_CPPI4_CHANNEL+3];

static struct dma_controller *
cppi_controller_new(struct musb *musb, void __iomem *pCoreBase);
static void cppi_controller_destroy(struct dma_controller *pController);
static int cppi_channel_abort(struct dma_channel *pChannel);
void  cppi_abort_flushfifo(struct musb *musb, int bEnd,int flushfifo );

const struct dma_controller_factory dma_controller_factory = {
	.create	 = cppi_controller_new,
	.destroy = cppi_controller_destroy,
};


static int txCppiChannelTearDown( struct cppi_channel *txCh );
static int rxCppiChannelTearDown( struct cppi_channel *rxCh ); 

UsbCppi4HostDesc* usbGetRxBD(usbRxCppiCh * rxCppi);
UsbCppi4HostDesc *usbGetFreeTxBDList(usbTxCppiCh * txCppi, int numBDs);
UsbCppi4HostDesc *usbGetFreeRxBDList(usbRxCppiCh * rxCppi, int numBDs);
int usbTxBDProc(struct cppi * cppi, int txChannel );
Int usbTxPktProcess(struct cppi *cppi, Ptr pktArgs);
void cppi_mode_update ( struct cppi_channel *c, int bTransmit, int mode );
Int usbRxPktProcess(struct cppi *cppi, Ptr pktArgs);
int usbRxBDProc(struct cppi * cppi, int rxChannel );
UsbCppi4HostDesc *alloc_buff_descriptor(struct cppi *c, int numBDs);
int free_buff_descriptor(struct cppi *c,UsbCppi4HostDesc *BDPtr);
void printBD(UsbCppi4HostDesc *currBD, int displayData);
irqreturn_t usbCppiTxPktProcess_Isr(int irq, void *pThis, struct pt_regs *r);
irqreturn_t usbCppiRxPktProcess_Isr(int irq, void *pThis, struct pt_regs *r);


#ifdef USB_POLL_MODE
void usbCppi_tx_poll( unsigned long data );
void usbCppi_rx_poll( unsigned long data );
#endif

void usb_cppi_tx_tasklet(unsigned long data);

#ifdef USB_CPPI_RX_TASKLET_MODE
	void usb_cppi_rx_tasklet(unsigned long data);
#endif



static inline void cpu_drain_writebuffer(void)
{
	wmb();
#ifdef	CONFIG_CPU_ARM926T
	/* REVISIT this "should not be needed",
	 * but lack of it sure seemed to hurt ...
	 */
	asm("mcr p15, 0, r0, c7, c10, 4 @ drain write buffer\n");
#endif
}
#define	CAST (void *__force __iomem)
/*
 * alloc_buff_descriptor
 */
UsbCppi4HostDesc *alloc_buff_descriptor(struct cppi *c, int numBDs)
{
    UsbCppi4HostDesc *BDPtr = NULL;
    struct bdMemChunk *bdPool;
    Uint32 i;

    bdPool = c->bdPool;
    for( i = 0; i < CPPI41_MAX_USB_EPS; ++i ){
        if( bdPool[i].allocated == 0 ){
                bdPool[i].allocated = 1;
                BDPtr = (UsbCppi4HostDesc *)bdPool[i].base;
                break;
        }
    }

    return BDPtr;
}
/*
 * free_buff_descriptor
 */
int free_buff_descriptor(struct cppi *c,UsbCppi4HostDesc *BDPtr)
{

    struct bdMemChunk *bdPool;
    Uint32 i;

    bdPool = c->bdPool;
    for( i = 0; i < CPPI41_MAX_USB_EPS; ++i ){
        if( bdPool[i].allocated && ((UsbCppi4HostDesc *)bdPool[i].base == BDPtr) ){
                bdPool[i].allocated = 0;
		BDPtr = NULL;
                return 0;
        }
    }

    return 1;

}
#ifdef DEBUG_CPPI_TD
void printBDList(UsbCppi4HostDesc *bdPoolHead )
{
  UsbCppi4HostDesc* currBD;
  int cnt = 0;

  currBD = bdPoolHead;
  while( currBD != NULL ){
        if( cnt % 8 == 0 )
                dprintk("\n%02x ",cnt);
        cnt++;
        dprintk(" %p",currBD);
        currBD = (UsbCppi4HostDesc*)currBD->nextSwBDPtr;
  }
  dprintk("\n");

}
#endif
/**
 *  usbCppi4InitTxChannel
 *    - Allocates memory for TX Ch Control structure, Buffer descriptors
 *    - Initializes the above data structures as per channel configuration
 *    - Stores the cppi specific configuration provided in 'chInfo' for quick
 *    access at txCppi level.
 *    - Sets up accumulator list memory for tx complete as per the number of
 *    pages.  - Initializes each descriptor and chain the TX BD list ready to
 *    be given to hardware
 *
 */
PRIVATE int usbCppi4InitTxChannel(struct cppi * cppi,
	struct cppi_channel *txCppiChannel,usbCppiChInfo * chInfo)
{
    PAL_Result retCode;
    Uint32 cnt;
    UsbCppi4HostDesc* currBD;
    usbTxCppiCh *txCppi = NULL;
    Cppi4AccumulatorCfg* accChCfg;

    DBG(USB_CPPI4_DBG_LEVEL,"\n usbCppi4InitTxChannel:%d", chInfo->chNum);

    /* allocate memory for the usbTxCppiCh structure */
    retCode = PAL_osMemAlloc(0, sizeof(usbTxCppiCh), 0, (Ptr *) & txCppi);
    if (retCode != PAL_SOK) {
        DBG(USB_CPPI4_DBG_LEVEL,
	"\nERROR:usbInitTxChannel:Failed to allocate memory for usbTxCppi Ch %d",
	 chInfo->chNum);
        return (retCode);
    }
    PAL_osMemSet(txCppi, 0, sizeof(usbTxCppiCh));

    txCppiChannel->txCppi = txCppi;
    txCppiChannel->pController = cppi->Controller.pPrivateData;

    txCppi->chInfo = *chInfo;
    txCppi->chInfo.chState = USB_CPPI4_CH_INITIALIZED;

    txCppi->numBD = chInfo->numBD;
    txCppi->numTxQs = chInfo->numTxQs;
    /*TODO numTxQs is set to one, in usb cppi component */
    for (cnt = 0; cnt < chInfo->numTxQs; cnt++) {
        txCppi->txQueue [cnt] = chInfo->cppi4TxChInfo.txInQueue [cnt];
    }
    txCppi->txCmplQueue = chInfo->cppi4TxChInfo.txCompQueue;

    /* accumulator channel configuration data associated with tx channel*/
    accChCfg = &txCppi->chInfo.txAccChInfo;

    accChCfg->queue.qMgr	  = USB_CPPI41_TXCMPL_QMGR;
    accChCfg->queue.qNum	  = USB_CPPI41_TXCMPL_QNUM;
    accChCfg->pacingTickCnt 	  = chInfo->txAccChInfo.pacingTickCnt;
    accChCfg->list.maxPageEntry   = chInfo->txAccChInfo.list.maxPageEntry ;
    accChCfg->list.pacingMode     = chInfo->txAccChInfo.list.pacingMode;
    accChCfg->list.stallAvoidance = chInfo->txAccChInfo.list.stallAvoidance ;

    accChCfg->list.listCountMode  = chInfo->txAccChInfo.list.listCountMode;
    accChCfg->list.listEntrySize  = chInfo->txAccChInfo.list.listEntrySize;
    accChCfg->list.maxPageCnt     = chInfo->txAccChInfo.list.maxPageCnt;

    txCppi->accChNum    = accChCfg->accChanNum;
    txCppi->accVecNum   = chInfo->accVecNum;

    /* !@0 TODO: Do we need to specify alignment for list region as 
     * USB_ACC_ENTRY_SIZE ?
     */
    txCppi->accListSize = (USB_ACC_LIST_DIV) * ((USB_ACC_TX_MAXENTRIES)
                            * (USB_ACC_ENTRY_SIZE));
    retCode = PAL_osMemAlloc(0, txCppi->accListSize, USB_ACC_TX_MAXENTRIES, &accChCfg->list.listBase);
    if (retCode != PAL_SOK) {
        DBG(1, "\nusbCppi4InitTxChannel:Failed to allocate "
                "%d bytes for accumulator list for ch %d", 
                txCppi->accListSize, chInfo->chNum);

        /* !@0 TODO: Before returning we should do the cleanup for previous
         * successful allocations (applicable to most such error cases!)
         */

        return (retCode);
    }

    for (cnt = 0; cnt < USB_ACC_LIST_DIV; cnt++)
        txCppi->listBuffBase[cnt] = (Ptr) ((Uint32)accChCfg->list.listBase +
                    (cnt * (USB_ACC_TX_MAXENTRIES) * (USB_ACC_ENTRY_SIZE)));


     for (cnt  = 0; cnt < USB_ACC_LIST_DIV; ++cnt)
        *(Uint32*)txCppi->listBuffBase[cnt] = 0;

    PAL_CPPI4_CACHE_WRITEBACK_INVALIDATE(accChCfg->list.listBase, txCppi->accListSize);


    /* teardown descriptor will be pushed to txCompletion Queue of associated 
       txCppi Channel */
    txCppi->chInfo.cppi4TxChInfo.tdQueue = txCppi->txCmplQueue;

    txCppiChannel->txTeardownPending = False;  /* Clear the TX teardown pending flag */

    /* TODO: We may need to check numBD here since CPPI4.1 requires the
     * desc count to be * multiple of 2 power 5 (ie 32). Similarly, the desc
     * size should also be multiple of 32.
     */

    txCppi->bdMem = (Char *)alloc_buff_descriptor( cppi, chInfo->numBD);

    if (txCppi->bdMem == NULL) {
        DBG (1, "\nERROR:usbInitTxChannel: Failed to allocate Tx descriptors");
        return (USB_ERR_CPPI_DESC_REGN_FAIL); /* descriptor region is not available */
    }
    txCppi->allocSize = sizeof(UsbCppi4HostDesc) * chInfo->numBD;

    currBD = (UsbCppi4HostDesc *) txCppi->bdMem;
    txCppi->bdPoolHead = 0;
    for (cnt = 0; cnt < chInfo->numBD; cnt++) {
        currBD->descInfo    = CPPI4_DESC_TYPE_HOST << CPPI4_DESC_TYPE_SHIFT_HOST;
        currBD->tagInfo     = 0;
        currBD->pktInfo     = (CPPI4_PKT_TYPE_USB << CPPI4_PKT_TYPE_SHIFT)
                                | (CPPI4_PKT_RETPLCY_FULL << CPPI4_PKT_RETPLCY_SHIFT)
                                | (CPPI4_DESC_LOC_OFFCHIP << CPPI4_DESC_LOC_SHIFT)
                                | (txCppi->txCmplQueue.qMgr << CPPI4_PKT_RETQMGR_SHIFT)
                                | (txCppi->txCmplQueue.qNum << CPPI4_PKT_RETQ_SHIFT);
        currBD->buffLen     = 0;
        currBD->bufPtr      = 0;
        currBD->nextBDPtr   = 0;
        currBD->orgBuffLen  = 0;
        currBD->orgBufPtr   = 0;

        currBD->nextSwBDPtr = (Ptr) txCppi->bdPoolHead;
        txCppi->bdPoolHead = currBD;
        currBD = (UsbCppi4HostDesc*) ((Uint32)currBD + txCppi->chInfo.descAlignment);
    }

    txCppiChannel->txIsCreated = True;

    DBG(USB_CPPI4_DEBUG_LEVEL,
                     "\n- cppi4InitTxChannel: Ch=%d",chInfo->chNum);

    return (USB_SUCCESS);
}
/**
 *  usbUnInitTxChannel
 *    - Frees memory previously allocated for Ch Control structure, buffer
 *      descriptors
 *
 *  \note 1. This function assumes that the channel number passed is valid and
 *           this function will not do any error check to avoid duplicate error
 *           checks (done in caller function).
 */
PRIVATE PAL_Result usbUnInitTxChannel(struct cppi *cppi, Uint32 channel)
{
    PAL_Result retCode;
    usbTxCppiCh *txCppi;

    DBG(USB_CPPI4_DEBUG_LEVEL,
                     "\n+ usbUnInitTxChannel: Ch=%d",channel);

    /* Check if channel structure is already de-allocated */
    if (cppi->txCppiCh[channel].txIsCreated == False) {
       DBG(1,
	"\nERROR:usbUnInitTxChannel:TX CPPI Ch %d structure already freed",channel);
        return (USB_ERR_TX_CH_ALREADY_CLOSED);
    }

    txCppi = cppi->txCppiCh[channel].txCppi;

    retCode = free_buff_descriptor( cppi, (UsbCppi4HostDesc *)txCppi->bdMem);

    txCppi->bdMem = NULL; 
    if (retCode != 0) {
       DBG (USB_CPPI4_DEBUG_LEVEL,
	"\nERROR:usbUnInitTxChannel:Failed to free tx desc region for Ch %d",
              channel);
    }
    txCppi->numBD = 0;

#ifdef USB_USE_ACC_LIST 
    retCode = PAL_osMemFree(0, txCppi->chInfo.txAccChInfo.list.listBase,
                            txCppi->accListSize);
    if (retCode != PAL_SOK) {
     	DBG(USB_CPPI4_DEBUG_LEVEL,
	"\nERROR:iusbUnInitTxChannel: Failed to free TX accumulator list for Ch %d",
              channel);
    }
#endif

    /* Free the TX Channel structure */
    retCode = PAL_osMemFree(0, txCppi, sizeof(usbTxCppiCh));
    if (retCode != PAL_SOK) {
        DBG(USB_CPPI4_DEBUG_LEVEL,
	"\nERROR:usbUnInitTxChannel: Failed to free TX CPPI channel structure for Ch %d",
              channel);
    }

    cppi->txCppiCh[channel].txCppi = NULL;
    cppi->txCppiCh[channel].txIsCreated = False;

    DBG(USB_CPPI4_DEBUG_LEVEL,"\n- usbUnInitTxChannel:Ch=%d",
                     channel);

    return (USB_SUCCESS);
}

/* cppi dma channel resources used for usb OTG controller module 

USB       DMA   DMA   QMgr  QNum  Src 
          TX    RX                Port
-------------------------------------
EP0_TX    4     4      0    204    1
-------------------------------------
EP1_TX    5     5      0    206    2
-------------------------------------
EP2_TX    6     6      0    208    3
-------------------------------------
EP3_TX    7     7      0    210    4
-------------------------------------
*/
/* usbCppi4InitChInfo
 * Initialize the cppi4 tx/rx channel configuration information.
 * TODO Note : all configuration data are hardcoded.
 */
static void usbCppi4InitChInfo(void)
{
	int i;
	/* configuration data for usb cppi channel 0 */
	for(i=0; i < MAX_USB_CPPI4_CHANNEL; ++i)
	{
		/* usb Cppi Channel 1/2/3/4 configuration for TX/RX endpoint */
    		PAL_osMemSet(&usbTxChInfo[i], 0, sizeof(usbCppiChInfo));
		usbTxChInfo[i].chNum      = i+1;
		usbTxChInfo[i].chDir      = 0;
		usbTxChInfo[i].chState    = 0;
		usbTxChInfo[i].numBD      = USB_CPPI4_EPNUM_BD;
		usbTxChInfo[i].bufSize    = 0;
		usbTxChInfo[i].numTxQs    = 1;
		usbTxChInfo[i].serviceMax = 1;
		usbTxChInfo[i].descAlignment = USB_CPPI4_HOST_DESC_ALGN;

    		PAL_osMemSet(&usbRxChInfo[i], 0, sizeof(usbCppiChInfo));
		usbRxChInfo[i].chNum      = i+1;
		usbRxChInfo[i].chDir	  = 1;
		usbRxChInfo[i].chState    = 0;
		usbRxChInfo[i].numBD      = USB_CPPI4_EPNUM_BD;
		usbRxChInfo[i].bufSize    = 0;
		usbRxChInfo[i].serviceMax = 1;
		usbRxChInfo[i].descAlignment = USB_CPPI4_HOST_DESC_ALGN;

	}

	/* Tx channel 0 configuration */
	usbTxChInfo[0].accVecNum 			= USB_ACCTX_INTD_VEC;
	usbTxChInfo[0].cppi4TxChInfo.chNum		= USB_CPPI41DMA_EP1_TXCH; 
	usbTxChInfo[0].cppi4TxChInfo.dmaNum		= USB_CPPI41_DMA_NUM;
	usbTxChInfo[0].cppi4TxChInfo.txInQueue[0].qMgr 	= USB_CPPI41_TX1_QMGR;
	usbTxChInfo[0].cppi4TxChInfo.txInQueue[0].qNum	= USB_CPPI41_TX1_QNUM;
	usbTxChInfo[0].cppi4TxChInfo.txCompQueue.qMgr   = USB_CPPI41_TXCMPL_QMGR;
	usbTxChInfo[0].cppi4TxChInfo.txCompQueue.qNum   = USB_CPPI41_TXCMPL_QNUM;
    
	/* Tx channel 1 configuration */
	usbTxChInfo[1].accVecNum 			=  USB_ACCTX_INTD_VEC;
	usbTxChInfo[1].cppi4TxChInfo.chNum 		= USB_CPPI41DMA_EP2_TXCH; 
	usbTxChInfo[1].cppi4TxChInfo.dmaNum 		= USB_CPPI41_DMA_NUM;
	usbTxChInfo[1].cppi4TxChInfo.txInQueue[0].qMgr 	= USB_CPPI41_TX2_QMGR;
	usbTxChInfo[1].cppi4TxChInfo.txInQueue[0].qNum 	= USB_CPPI41_TX2_QNUM;
	usbTxChInfo[1].cppi4TxChInfo.txCompQueue.qMgr   = USB_CPPI41_TXCMPL_QMGR ;
	usbTxChInfo[1].cppi4TxChInfo.txCompQueue.qNum   = USB_CPPI41_TXCMPL_QNUM ; 
	
	/* Tx Channel 2 configuration */
	usbTxChInfo[2].accVecNum 			=  USB_ACCTX_INTD_VEC;
	usbTxChInfo[2].cppi4TxChInfo.chNum 		= USB_CPPI41DMA_EP3_TXCH; 
	usbTxChInfo[2].cppi4TxChInfo.dmaNum 		= USB_CPPI41_DMA_NUM; 
	usbTxChInfo[2].cppi4TxChInfo.txInQueue[0].qMgr  = USB_CPPI41_TX3_QMGR;
	usbTxChInfo[2].cppi4TxChInfo.txInQueue[0].qNum	= USB_CPPI41_TX3_QNUM;
	usbTxChInfo[2].cppi4TxChInfo.txCompQueue.qMgr   = USB_CPPI41_TXCMPL_QMGR;
	usbTxChInfo[2].cppi4TxChInfo.txCompQueue.qNum   = USB_CPPI41_TXCMPL_QNUM; 

	/* Tx Channel 3 configuration */	
	usbTxChInfo[3].accVecNum 			=  USB_ACCTX_INTD_VEC;
	usbTxChInfo[3].cppi4TxChInfo.chNum 		= USB_CPPI41DMA_EP4_TXCH;
	usbTxChInfo[3].cppi4TxChInfo.dmaNum 		= USB_CPPI41_DMA_NUM;
	usbTxChInfo[3].cppi4TxChInfo.txInQueue[0].qMgr  = USB_CPPI41_TX4_QMGR;
	usbTxChInfo[3].cppi4TxChInfo.txInQueue[0].qNum	= USB_CPPI41_TX4_QNUM;
	usbTxChInfo[3].cppi4TxChInfo.txCompQueue.qMgr   = USB_CPPI41_TXCMPL_QMGR;
	usbTxChInfo[3].cppi4TxChInfo.txCompQueue.qNum   = USB_CPPI41_TXCMPL_QNUM;

        /* Tx channel 0,1,2,3 : Accumulator channel configuration */
        for( i = 0; i < 4; ++i ) {
                usbTxChInfo[i].txAccChInfo.accChanNum = USB_ACC_TX_CHNUM ;
                usbTxChInfo[i].txAccChInfo.mode         = USB_ACC_MODE_LIST_MODE;
                usbTxChInfo[i].txAccChInfo.queue.qMgr   = USB_CPPI41_TXCMPL_QMGR;
                usbTxChInfo[i].txAccChInfo.queue.qNum   = USB_CPPI41_TXCMPL_QNUM;
                usbTxChInfo[i].txAccChInfo.pacingTickCnt = USB_ACC_INTR_DELAY ;
                usbTxChInfo[i].txAccChInfo.list.listBase = 0 ;
                usbTxChInfo[i].txAccChInfo.list.maxPageEntry = USB_ACC_TX_MAXENTRIES ;
                usbTxChInfo[i].txAccChInfo.list.pacingMode = USB_ACC_INTR_MODE ;
                usbTxChInfo[i].txAccChInfo.list.stallAvoidance = USB_ACC_STALL_AVOID ;
                usbTxChInfo[i].txAccChInfo.list.listCountMode  = USB_ACC_LIST_MODE ;
                usbTxChInfo[i].txAccChInfo.list.listEntrySize  = USB_ACC_ENTRY_SIZE_VAL;
                usbTxChInfo[i].txAccChInfo.list.maxPageCnt     = USB_ACC_LIST_DIV;

		/* monitor configuration */
		usbTxChInfo[i].txAccChInfo.monitor.pktCountThresh = 0; /* TBD */
		usbTxChInfo[i].txAccChInfo.monitor.pacingMode 	  = 0;
        }

	
	/* Rx channel 0 configuration */
	usbRxChInfo[0].accVecNum 			=  USB_ACCRX_INTD_VEC;
	usbRxChInfo[0].cppi4RxChInfo.chNum 		= USB_CPPI41DMA_EP1_RXCH; 
	usbRxChInfo[0].cppi4RxChInfo.dmaNum 		= USB_CPPI41_DMA_NUM;
	usbRxChInfo[0].cppi4RxChInfo.rxCompQueue.qMgr   = USB_CPPI41_RX1_QMGR;
	usbRxChInfo[0].cppi4RxChInfo.rxCompQueue.qNum	= USB_CPPI41_RX1_QNUM;
	usbRxChInfo[0].cppi4RxChInfo.sopOffset	        = 0;
	usbRxChInfo[0].cppi4RxChInfo.defDescType        = 1; /* Host Packet descriptor */
        usbRxChInfo[0].rxAccChInfo.accChanNum = USB_ACC_RX1_CHNUM ;
	usbRxChInfo[0].cppi4RxChInfo.u.hostPktCfg.fdbQueue[0].qMgr = USB_CPPI41_RX1_FDB_QMGR;
	usbRxChInfo[0].cppi4RxChInfo.u.hostPktCfg.fdbQueue[0].qNum = USB_CPPI41_RX1_FDB_QNUM;

	/* Rx channel 1 configuration */
	usbRxChInfo[1].accVecNum 			=  USB_ACCRX_INTD_VEC;
	usbRxChInfo[1].cppi4RxChInfo.chNum 		= USB_CPPI41DMA_EP2_RXCH; 
	usbRxChInfo[1].cppi4RxChInfo.dmaNum 		= USB_CPPI41_DMA_NUM; 
	usbRxChInfo[1].cppi4RxChInfo.rxCompQueue.qMgr   = USB_CPPI41_RX2_QMGR;
	usbRxChInfo[1].cppi4RxChInfo.rxCompQueue.qNum	= USB_CPPI41_RX2_QNUM;
	usbRxChInfo[1].cppi4RxChInfo.sopOffset	        = 0;
	usbRxChInfo[1].cppi4RxChInfo.defDescType        = 1; /* Host Packet descriptor */
        usbRxChInfo[1].rxAccChInfo.accChanNum = USB_ACC_RX2_CHNUM ;
	usbRxChInfo[1].cppi4RxChInfo.u.hostPktCfg.fdbQueue[0].qMgr = USB_CPPI41_RX2_FDB_QMGR;
	usbRxChInfo[1].cppi4RxChInfo.u.hostPktCfg.fdbQueue[0].qNum = USB_CPPI41_RX2_FDB_QNUM;

	/* Rx channel 2 configuration */
	usbRxChInfo[2].accVecNum 			=  USB_ACCRX_INTD_VEC;
	usbRxChInfo[2].cppi4RxChInfo.chNum 		= USB_CPPI41DMA_EP3_RXCH; 
	usbRxChInfo[2].cppi4RxChInfo.dmaNum 		= USB_CPPI41_DMA_NUM; 
	usbRxChInfo[2].cppi4RxChInfo.rxCompQueue.qMgr   = USB_CPPI41_RX3_QMGR;
	usbRxChInfo[2].cppi4RxChInfo.rxCompQueue.qNum	= USB_CPPI41_RX3_QNUM;
	usbRxChInfo[2].cppi4RxChInfo.sopOffset	        = 0;
	usbRxChInfo[2].cppi4RxChInfo.defDescType        = 1; /* Host Packet descriptor */
	usbRxChInfo[2].rxAccChInfo.accChanNum	= USB_ACC_RX3_CHNUM;
	usbRxChInfo[2].cppi4RxChInfo.u.hostPktCfg.fdbQueue[0].qMgr = USB_CPPI41_RX3_FDB_QMGR;
	usbRxChInfo[2].cppi4RxChInfo.u.hostPktCfg.fdbQueue[0].qNum = USB_CPPI41_RX3_FDB_QNUM;
	
	/* Rx channel 3 configuration */
	usbRxChInfo[3].accVecNum 			=  USB_ACCRX_INTD_VEC;
	usbRxChInfo[3].cppi4RxChInfo.chNum 		= USB_CPPI41DMA_EP4_RXCH; 
	usbRxChInfo[3].cppi4RxChInfo.dmaNum 		= USB_CPPI41_DMA_NUM; 
	usbRxChInfo[3].cppi4RxChInfo.rxCompQueue.qMgr   = USB_CPPI41_RX4_QMGR;
	usbRxChInfo[3].cppi4RxChInfo.rxCompQueue.qNum	= USB_CPPI41_RX4_QNUM;
	usbRxChInfo[3].cppi4RxChInfo.sopOffset	        = 0;
	usbRxChInfo[3].cppi4RxChInfo.defDescType        = 1; /* Host Packet descriptor */
	usbRxChInfo[3].rxAccChInfo.accChanNum	= USB_ACC_RX4_CHNUM;
	usbRxChInfo[3].cppi4RxChInfo.u.hostPktCfg.fdbQueue[0].qMgr = USB_CPPI41_RX4_FDB_QMGR;
	usbRxChInfo[3].cppi4RxChInfo.u.hostPktCfg.fdbQueue[0].qNum = USB_CPPI41_RX4_FDB_QNUM;

        for( i = 0; i < 4; ++i ) {

		usbRxChInfo[i].cppi4RxChInfo.retryOnStarvation  = 1;

	/* Rx channel 0,1,2,3 : Accumulator channel configuration */
                usbRxChInfo[i].rxAccChInfo.mode         = USB_ACC_MODE_LIST_MODE;
                usbRxChInfo[i].rxAccChInfo.queue = usbRxChInfo[i].cppi4RxChInfo.rxCompQueue ; 
		usbRxChInfo[i].rxAccChInfo.queue.qMgr   = usbRxChInfo[i].cppi4RxChInfo.rxCompQueue.qMgr;
                usbRxChInfo[i].rxAccChInfo.queue.qNum   = usbRxChInfo[i].cppi4RxChInfo.rxCompQueue.qNum; 
               
                usbRxChInfo[i].rxAccChInfo.pacingTickCnt = USB_ACC_INTR_DELAY ;
                usbRxChInfo[i].rxAccChInfo.list.listBase = 0 ;
                usbRxChInfo[i].rxAccChInfo.list.maxPageEntry = USB_ACC_RX_MAXENTRIES ;
                usbRxChInfo[i].rxAccChInfo.list.pacingMode = USB_ACC_INTR_MODE ;
                usbRxChInfo[i].rxAccChInfo.list.stallAvoidance = USB_ACC_STALL_AVOID ;
                usbRxChInfo[i].rxAccChInfo.list.listCountMode  = USB_ACC_LIST_MODE ;
                usbRxChInfo[i].rxAccChInfo.list.listEntrySize  = USB_ACC_ENTRY_SIZE_VAL;
                usbRxChInfo[i].rxAccChInfo.list.maxPageCnt     = USB_ACC_LIST_DIV;
		
                /* monitor configuration */
                usbRxChInfo[i].rxAccChInfo.monitor.pktCountThresh = 0; /* TBD */
                usbRxChInfo[i].rxAccChInfo.monitor.pacingMode     = 0;
        }


	
}
/**
 *  usbInitRxChannel
 *    - Allocates memory for RX Ch structure and other internal structs
 *    - Stores the cppi specific configuration provided in 'chInfo' for quick
 *    access at rxCppi level.
 *    - Sets up accumulator list memory for rx as per the number of pages.
 *    - prepares the rx packet list
 *
 *  \note
 *        1. This function assumes that the channel number passed is valid and
 *           the  >rxCppi pointer is NULL.
 *        2. This function will not do any error check on these initialization
 *        parameters to avoid duplicate error checks (done in caller function
 *        or init parse functions). Also numBD value and alignment requirement
 *        should be validated as per the specs during init time
 *        
 */
PRIVATE PAL_Result usbCppi4InitRxChannel(struct cppi *cppi,
	struct cppi_channel *rxCppiChannel,usbCppiChInfo * chInfo)
{
    PAL_Result retCode;
    usbRxCppiCh *rxCppi = NULL;
    UsbCppi4HostDesc *currBD;
    Cppi4AccumulatorCfg* accChCfg;
    Uint32 cnt;

    DBG(USB_CPPI4_DEBUG_LEVEL,
         "\n+ usbInitRxChannel:Ch=%d", chInfo->chNum);

    retCode = PAL_osMemAlloc(0, sizeof(usbRxCppiCh), 0, (Ptr *) & rxCppi);
    if (retCode != PAL_SOK) {
       DBG(1,"\nERROR:usbInitRxChannel: Failed to allocate memory for RX CPPI Ch %d",
             chInfo->chNum);
        goto allocRxCppiFail;
    }
    PAL_osMemSet(rxCppi, 0, sizeof(usbRxCppiCh));

    rxCppiChannel->rxCppi = rxCppi;
    rxCppiChannel->pController = cppi->Controller.pPrivateData;

    rxCppi->chInfo = *chInfo;
    rxCppi->chInfo.chState = USB_CPPI4_CH_INITIALIZED;

    rxCppi->rxQueue = chInfo->cppi4RxChInfo.rxCompQueue;

    for (cnt = 0; cnt < USBOTG_NUM_RXFDB_QS; cnt++) {
        rxCppi->fdbQueue [cnt] = chInfo->cppi4RxChInfo.u.hostPktCfg.fdbQueue [cnt];
    }


    accChCfg = &rxCppi->chInfo.rxAccChInfo;
    accChCfg->queue.qMgr          = chInfo->rxAccChInfo.queue.qMgr;
    accChCfg->queue.qNum          = chInfo->rxAccChInfo.queue.qNum;
   
    accChCfg->pacingTickCnt       = chInfo->rxAccChInfo.pacingTickCnt;
    accChCfg->list.maxPageEntry   = chInfo->rxAccChInfo.list.maxPageEntry ;
    accChCfg->list.pacingMode     = chInfo->rxAccChInfo.list.pacingMode;
    accChCfg->list.stallAvoidance = chInfo->rxAccChInfo.list.stallAvoidance ;

    accChCfg->list.listCountMode  = chInfo->rxAccChInfo.list.listCountMode;
    accChCfg->list.listEntrySize  = chInfo->rxAccChInfo.list.listEntrySize;
    accChCfg->list.maxPageCnt     = chInfo->rxAccChInfo.list.maxPageCnt;

    rxCppi->accChNum    = accChCfg->accChanNum;
    rxCppi->accVecNum   = chInfo->accVecNum;

    /* allocate memory for accumulator buf list */
    rxCppi->accListSize = (USB_ACC_LIST_DIV) * ((USB_ACC_RX_MAXENTRIES)
                            * (USB_ACC_ENTRY_SIZE));
    retCode = PAL_osMemAlloc(0, rxCppi->accListSize, USB_ACC_RX_MAXENTRIES, &accChCfg->list.listBase);
    if (retCode != PAL_SOK) {
        DBG(1, "\nERROR:cpmacInitTxChannel:Failed to allocate "
                "%d bytes for accumulator list for ch %d", 
                rxCppi->accListSize, chInfo->chNum);

	goto errorOut;
    }

    /* assign the accumulator page base address, it is
       based on number of pages in list buffer */
    for (cnt = 0; cnt < USB_ACC_LIST_DIV; cnt++)
        rxCppi->listBuffBase[cnt] = (Ptr) ((Uint32)accChCfg->list.listBase +
                    (cnt * (USB_ACC_RX_MAXENTRIES) * (USB_ACC_ENTRY_SIZE)));

    /* activeListRgn indicates anticipated page index for next interrupt,
     * Incremented on each interrupt and rolls over at USB_ACC_LIST_DIV..
     */

    *(Uint32*)rxCppi->listBuffBase[0] = 0;


    PAL_CPPI4_CACHE_WRITEBACK_INVALIDATE(accChCfg->list.listBase, rxCppi->accListSize);

//	printk("\ncppi41-r04");
    rxCppiChannel->rxTeardownPending = False;    /* Clear the RX teardown pending flag */

    /* TODO: We may need to check numBD here since CPPI4.1 requires the
     * desc count to be * multiple of 2 power 5 (ie 32). Similarly, the desc
     * size should also be multiple of 32.
     */
    rxCppi->bdMem = (Char *)alloc_buff_descriptor( cppi, chInfo->numBD);
    rxCppi->numBD = chInfo->numBD;

    if (rxCppi->bdMem == NULL) {
        DBG (1,"\nERROR:usbInitRxChannel: Failed to allocate Rx descriptor");
        retCode = USB_ERR_CPPI_DESC_REGN_FAIL;
        goto errorOut;
    }
    rxCppi->allocSize = (sizeof(UsbCppi4HostDesc) * chInfo->numBD);

    rxCppi->numBD = chInfo->numBD;
    rxCppi->numOfBdInFdbQueue = 0;

    currBD = (UsbCppi4HostDesc *) rxCppi->bdMem;
    rxCppi->bdPoolHead = 0;

    /* "Slice" BD's one-by-one from the chunk, allocate a buffer and token, */
    for (cnt = 0; cnt < chInfo->numBD; cnt++) {

        /* Update the hardware descriptor */
        currBD->descInfo    = CPPI4_DESC_TYPE_HOST << CPPI4_DESC_TYPE_SHIFT_HOST;
        currBD->tagInfo     = 0;
        currBD->pktInfo     = (CPPI4_PKT_TYPE_USB << CPPI4_PKT_TYPE_SHIFT)
                                | (CPPI4_PKT_RETPLCY_FULL << CPPI4_PKT_RETPLCY_SHIFT)
                                | (CPPI4_DESC_LOC_OFFCHIP << CPPI4_DESC_LOC_SHIFT)
                                | (rxCppi->fdbQueue[0].qMgr << CPPI4_PKT_RETQMGR_SHIFT)
                                | (rxCppi->fdbQueue[0].qNum << CPPI4_PKT_RETQ_SHIFT);
        currBD->buffLen     = 0;
        currBD->bufPtr      = 0;
        currBD->nextBDPtr   = 0;
        currBD->orgBuffLen  = 0; 
        currBD->orgBufPtr   = 0; 
        PAL_CPPI4_CACHE_WRITEBACK(currBD, CPPI4_BD_LENGTH_FOR_CACHE);
	currBD->nextSwBDPtr = (Ptr)rxCppi->bdPoolHead;
	rxCppi->bdPoolHead = currBD;
        currBD = (UsbCppi4HostDesc*) ((Uint32)currBD + rxCppi->chInfo.descAlignment);
	
    }


    rxCppiChannel->rxIsCreated = True;

    return (USB_SUCCESS);

  errorOut:
    /* Free rx cppi channel memory */
    PAL_osMemFree(0, rxCppi, sizeof(usbRxCppiCh));
  allocRxCppiFail:
  DBG(USB_CPPI4_DEBUG_LEVEL,
                     "\n- usbInitRxChannel:Ch=%d",chInfo->chNum);

    return (retCode);
}
#ifdef CONFIG_MACH_PUMA5
static int initRxBDChain(usbRxCppiCh *rxCppi )
{
	UsbCppi4HostDesc *currBD;
	u32 cnt;

	if( rxCppi->numBD < CPPI41_USB_BD_SIZE ){

		dprintk("%s : done, NumOfBD=%d\n",__FUNCTION__, CPPI41_USB_BD_SIZE);
		currBD = (UsbCppi4HostDesc *) rxCppi->bdMem;
    		rxCppi->bdPoolHead = 0;
		rxCppi->numBD = CPPI41_USB_BD_SIZE;

    	/* "Slice" BD's one-by-one from the chunk, allocate a buffer and token, */
	    	for (cnt = 0; cnt < rxCppi->numBD; cnt++) {

        /* Update the hardware descriptor */
        		currBD->descInfo    = CPPI4_DESC_TYPE_HOST << CPPI4_DESC_TYPE_SHIFT_HOST;
		        currBD->tagInfo     = 0;
        		currBD->pktInfo     = (CPPI4_PKT_TYPE_USB << CPPI4_PKT_TYPE_SHIFT)
		                        | (CPPI4_PKT_RETPLCY_FULL << CPPI4_PKT_RETPLCY_SHIFT)
                	                | (CPPI4_DESC_LOC_OFFCHIP << CPPI4_DESC_LOC_SHIFT)
                        	        | (rxCppi->fdbQueue[0].qMgr << CPPI4_PKT_RETQMGR_SHIFT)
                                	| (rxCppi->fdbQueue[0].qNum << CPPI4_PKT_RETQ_SHIFT);
	        	currBD->buffLen     = 0;
	        	currBD->bufPtr      = 0;
		        currBD->nextBDPtr   = 0;
        		currBD->orgBuffLen  = 0;
		        currBD->orgBufPtr   = 0;
        		PAL_CPPI4_CACHE_WRITEBACK(currBD, CPPI4_BD_LENGTH_FOR_CACHE);
	        	currBD->nextSwBDPtr = (Ptr)rxCppi->bdPoolHead;
	        	rxCppi->bdPoolHead = currBD;
		        currBD = (UsbCppi4HostDesc*) ((Uint32)currBD + rxCppi->chInfo.descAlignment);
    		}
		return 0;
	}
	return 1;
}
#endif
/**
 *  usbUnInitRxChannel
 *    - Frees memory previously allocated for Ch Control structure, buffer
 *      descriptors
 *
 *  \note 1. This function assumes that the channel number passed is valid and
 *           this function will not do any error check to avoid duplicate error
 *           checks (done in caller function).
 */
PRIVATE PAL_Result usbUnInitRxChannel(struct cppi *cppi, Uint32 channel)
{
    PAL_Result retCode;
    usbRxCppiCh *rxCppi;

    DBG(USB_CPPI4_DEBUG_LEVEL,
                     "\n+ usbUnInitRxChannel: Ch=%d",channel);

    /* Check if channel structure is already de-allocated */
    if (cppi->rxCppiCh[channel].rxIsCreated == False) {
       DBG(1,
        "\nERROR:usbUnInitRxChannel:RX CPPI Ch %d structure already freed",channel);
        return (USB_ERR_RX_CH_ALREADY_CLOSED);
    }

    rxCppi = cppi->rxCppiCh[channel].rxCppi;

    retCode = free_buff_descriptor( cppi, (UsbCppi4HostDesc *)rxCppi->bdMem);

    rxCppi->bdMem = NULL;

    if (retCode != 0) {
       DBG (USB_CPPI4_DEBUG_LEVEL,
        "\nERROR:usbUnInitRxChannel:Failed to free RX desc region for Ch %d",
              channel);
    }

#ifdef USB_USE_ACC_LIST
    retCode = PAL_osMemFree(0, rxCppi->chInfo.rxAccChInfo.list.listBase,
                            rxCppi->accListSize);
    if (retCode != PAL_SOK) {
        DBG(USB_CPPI4_DEBUG_LEVEL,
        "\nERROR:iusbUnInitRxChannel: Failed to free RX accumulator list for Ch %d",
              channel);
    }
#endif

   /* Free the RX Channel structure */
    retCode = PAL_osMemFree(0, rxCppi, sizeof(usbRxCppiCh));
    if (retCode != PAL_SOK) {
        DBG(USB_CPPI4_DEBUG_LEVEL,
        "\nERROR:usbUnInitRxChannel: Failed to free RX CPPI channel structure for Ch %d",
              channel);
    }

    cppi->rxCppiCh[channel].rxCppi = NULL;
    cppi->rxCppiCh[channel].rxIsCreated = False;

    DBG(USB_CPPI4_DEBUG_LEVEL,"\n- usbUnInitRxChannel:Ch=%d",
                     channel);

    return (USB_SUCCESS);

}
/*
 * cppi_controller_start
 * @pPrivateData : driver's private data & control structure
 * This function initializes the cppi4 tx/rx channel,
 */
static int  cppi_controller_start(struct dma_controller *c) 
{
	struct cppi	*pController ;
	struct cppi 	*cppi ;
	void		*__iomem regBase;
	UsbCppi4HostDesc *BDPtr;
	struct bdMemChunk *bdPool;
	int		i,retVal,cnt;

	pController = container_of(c, struct cppi, Controller);
	cppi = pController;

	/* initialize the cppi4PAL object */
	pController->cppi4PAL = (Cppi4PALObj *)PAL_cppi4Init(NULL, NULL); 

	/* initialzie the tx/rx channel configuration data */
	usbCppi4InitChInfo();

	/* allocate free Buffer Descriptor pool for all tx/rx endpoints */
	pController->bdMem = PAL_cppi4AllocDesc(pController->cppi4PAL,
			     USB_CPPI4_QMGR_NUM,  USB_CPPI4_MAX_BD , USB_CPPI4_HOST_DESC_ALGN);
	if( NULL == pController->bdMem ){
		DBG(USB_CPPI4_DEBUG_LEVEL, "\nERROR:BD allocation fail%s","");
		return 0;
	}
	pController->numBD = USB_CPPI4_MAX_BD;
	pController->txdma_IntrLine = USB_CPPI_TXDMA_INTR; 
	pController->rxdma_IntrLine = USB_CPPI_RXDMA_INTR;
	pController->txIsrAttachedFlag = pController->rxIsrAttachedFlag  = 0;

	/* initialize the buffer pool memry */
	bdPool = pController->bdPool;
	BDPtr = (UsbCppi4HostDesc *)pController->bdMem;
	for( cnt = 0; cnt < CPPI41_MAX_USB_EPS; ++cnt){
		bdPool[cnt].base      = (Char *)BDPtr;
		bdPool[cnt].numBD     = CPPI41_USB_BD_SIZE; 
		bdPool[cnt].allocated = 0;
		BDPtr += bdPool[cnt].numBD;
	}


	for (i = 0; i < ARRAY_SIZE(pController->txCppiCh); i++) {
		PAL_osMemSet(&pController->txCppiCh[i], 0, sizeof(struct cppi_channel));
		pController->txCppiCh[i].bTransmit = TRUE;
		pController->txCppiCh[i].chNo = i;
		pController->txCppiCh[i].txIsOpen = 0;
	}
	for (i = 0; i < ARRAY_SIZE(pController->rxCppiCh); i++) {
		PAL_osMemSet(&pController->rxCppiCh[i], 0, sizeof(struct cppi_channel));
		pController->rxCppiCh[i].bTransmit = FALSE;
		pController->rxCppiCh[i].chNo = i;
		pController->rxCppiCh[i].rxIsOpen = 0;
	}


	/* configure the tx channel */
	for (i = 0; i < ARRAY_SIZE(pController->txCppiCh); i++){
		retVal = usbCppi4InitTxChannel(pController, &pController->txCppiCh[i],
			  		       &usbTxChInfo[i]);
		if( USB_SUCCESS != retVal ){
			DBG(USB_CPPI4_DEBUG_LEVEL, "\n usbCppi4InitTxChannel failed for tx %d",i);
			printk("\n usbCppi4InitTxChannel failed for tx %d",i);
		}
	}

	/* configure the rx channel  */
	for (i = 0; i < ARRAY_SIZE(pController->rxCppiCh); i++){
		retVal = usbCppi4InitRxChannel(pController, &pController->rxCppiCh[i], 
					&usbRxChInfo[i]);
		if( USB_SUCCESS != retVal ){
			DBG(USB_CPPI4_DEBUG_LEVEL, "\n usbCppi4InitRxChannel failed for rx %d",i);
		}
		
	}

#ifdef USB_CPPI_TX_TASKLET_MODE
	tasklet_init(&cppi->tx_tasklet, usb_cppi_tx_tasklet, pController->musb);
#endif

#ifdef USB_CPPI_RX_TASKLET_MODE
	tasklet_init(&cppi->rx_tasklet, usb_cppi_rx_tasklet, pController->musb);
#endif

#ifdef USB_POLL_MODE
	/* initialize for periodic task to handle cppi tx packet processsing */
	init_timer( &cppi->tx_poll_timer );
	cppi->tx_poll_timer.function = usbCppi_tx_poll;
	cppi->tx_poll_timer.data = (Uint32)cppi;
	cppi->tx_poll_timer.expires = jiffies + 8; 
	cppi->tx_state = 0;

	/* initialize for periodic task to handle cppi rx packet processing */
	init_timer( &cppi->rx_poll_timer );
	cppi->rx_poll_timer.function = usbRxBDProc_tasklet;
	cppi->rx_poll_timer.data = (u32)pController->musb;
	cppi->rx_poll_timer.expires = jiffies + HZ;
#endif

	/* Do Necessary configuartion in H/w to get started */
	regBase =  pController->pCoreBase - PUMA5_BASE_OFFSET;

	INIT_LIST_HEAD(&pController->tx_complete);

	/* disable auto request mode */
	musb_writel(regBase, PUMA5_AUTOREQ_REG, 0);
	
	/* disable the CDC/RNDIS modes */
	musb_writel(regBase, PUMA5_MODE_TGCR_REG,  0);


	return TRUE;
}

/*
 *  Stop Dma controller
 *
 *  De-Init the Dma Controller as necessary.
 */

static int  cppi_controller_stop(struct dma_controller *c)
{
	struct cppi		*pController ;
	void __iomem		*regBase; 
	struct musb *pThis;
	int			i, retVal;

	pController = container_of(c, struct cppi, Controller);
        pThis = pController->musb;

	/* DISABLE INDIVIDUAL CHANNEL Interrupts */
	if( pController->txIsrAttachedFlag ){
		free_irq(  pController->txdma_IntrLine , pThis);
		pController->txIsrAttachedFlag = 0;
	}

	if( pController->rxIsrAttachedFlag ){
		free_irq(  pController->rxdma_IntrLine, pThis );
		pController->rxIsrAttachedFlag = 0;
	}

	DBG(USB_CPPI4_DEBUG_LEVEL, "Tearing down RX and TX Channels\n");
	for (i = 0; i < ARRAY_SIZE(pController->txCppiCh); i++) {
		usbUnInitTxChannel(pController, i );
#ifdef USB_CPPI4_GETSTATS
		pController->txIntCount[i] = 0;
#endif
	}

	/* un-initialize the RxChannel */
	for (i = 0; i < ARRAY_SIZE(pController->rxCppiCh); i++) {
		usbUnInitRxChannel(pController, i );
#ifdef USB_CPPI4_GETSTATS
		pController->rxIntCount[i] = 0;
#endif
	}

	/* free the host descriptor allocated for all tx/rx channels */
	retVal = PAL_cppi4DeallocDesc(pController->cppi4PAL, USB_CPPI4_QMGR_NUM, pController->bdMem); 
	if (retVal != 0) {
	       DBG (USB_CPPI4_DEBUG_LEVEL,"\nERROR:cppi_controller_stop:Failed to free cppihost desc");
    	}

	pController->txIsrAttachedFlag = 0;
	pController->rxIsrAttachedFlag = 0;


	regBase =  pController->pCoreBase - PUMA5_BASE_OFFSET;

	/* TODO */
	INIT_LIST_HEAD(&pController->tx_complete);

	/* disable auto request mode */
	musb_writel(regBase, PUMA5_AUTOREQ_REG, 0);
	
	/* disable the CDC/RNDIS modes */
	musb_writel(regBase, PUMA5_MODE_TGCR_REG,  0);

	return TRUE;
}

/*
 * Allocate a CPPI Channel for DMA.  With CPPI, channels are bound to
 * each transfer direction of a non-control endpoint, so allocating
 * (and deallocating) is mostly a way to notice bad housekeeping on
 * the software side.  We assume the irqs are always active.
 */
static struct dma_channel *
cppi_channel_allocate(struct dma_controller *c, 
		 	struct musb_hw_ep *ep,
			u8 bTransmit )
{
	
	struct cppi		*pController;
	u8			chNum,indx;
	struct cppi_channel	*txCh;
	struct cppi_channel     *rxCh;
	usbTxCppiCh *txCppi;
	usbRxCppiCh *rxCppi;
	Ptr cppi4TxChHnd,cppi4RxChHnd;
	PAL_Cppi4AccChHnd txAccChHnd;
	PAL_Cppi4AccChHnd rxAccChHnd;
	struct musb		*pThis ;
	Int32			retVal;
	int 	bLocalEnd = ep->bLocalEnd;

	pController = (struct cppi *)c->pPrivateData; 
	pThis = pController->musb;

	/* remember bLocalEnd: 1..Max_EndPt, and cppi ChNum:0..Max_EndPt-1 */
	chNum = bLocalEnd - 1;

	/* as of now, just return the corresponding CPPI Channel Handle */
	if (bTransmit) {
		if (bLocalEnd > ARRAY_SIZE(pController->txCppiCh)) {
			DBG(1, "no %cX DMA channel for ep%d\n", 'T', bLocalEnd);
			return NULL;
		}
		txCh = &pController->txCppiCh[chNum];
		
		if( !txCh->txIsCreated ){
			DBG(1,"Cppi Tx channel is not created chnum=%d",chNum);
			return NULL;
		}
	
		if (txCh->pEndPt)
			DBG(1, "re-allocating DMA%d TX channel %p\n",chNum,txCh);

		txCppi = txCh->txCppi;
		txCppi->palCppi4Hnd = pController->cppi4PAL;

		for(indx = 0; indx < txCppi->numTxQs; indx++){
			txCppi->txQueueHnd[indx] =  PAL_cppi4QueueOpen(pController->cppi4PAL,
							txCppi->txQueue[indx]);
			if( NULL == txCppi->txQueueHnd[indx] ){
				DBG(1,"Error:cppiChAlloc, QueueOpen fail txQueueHnd%s","");
				return NULL;
			}
		}
		txCppi->txCmplQueueHnd = PAL_cppi4QueueOpen (pController->cppi4PAL, 
							txCppi->txCmplQueue);

		if( NULL == txCppi->txCmplQueueHnd ){
				DBG(1,"Error:cppiChAlloc, QueueOpen Fail,txCmplQueue%s","");

				goto txCppiAlloc_fail1;
		}
		/* open the cppi4 dma channel in the CPPI4 PAL */
		cppi4TxChHnd = PAL_cppi4TxChOpen(pController->cppi4PAL, 
						&txCppi->chInfo.cppi4TxChInfo,NULL);
		if( NULL == cppi4TxChHnd ){
			DBG(1, "\ncppi_alloc_channel: Error cppiTxChOpen for channel %d",chNum);
			goto txCppiAlloc_fail2; 
		}
		txCppi->cppi4TxChHnd = cppi4TxChHnd;

		txAccChHnd = PAL_cppi4AccChOpen(pController->cppi4PAL, &txCppi->chInfo.txAccChInfo);
	        if (txAccChHnd == NULL) {
		        DBG(1,"\nERROR:ncppi_alloc_channel: Error CPPI4 Tx PAL_cppi4AccChOpen()");
	            	goto txCppiAlloc_fail2;;
        	}
	        txCppi->txAccChHnd = txAccChHnd;

		txCppi->listEntryPtr = PAL_cppi4AccChGetNextList(txCppi->txAccChHnd);

		/* enable the cppi4 dma channel */
		retVal = PAL_cppi4EnableTxChannel(txCppi->cppi4TxChHnd, NULL);
		if( PAL_SOK != retVal ){
			DBG(1, "\nCppi_alloc_channel: Error in EnableTxChannel");
			goto txCppiAlloc_fail3;	
		}

	       	if( !pController->txIsrAttachedFlag ){

			/* attach accumulator - cppi tx dma interrupt service routine */
			request_irq( pController->txdma_IntrLine, usbCppiTxPktProcess_Isr, 
					SA_INTERRUPT, pThis->controller->bus_id, pThis); 

			/* enable the interrupt corresponding to accumulator channel */
			retVal = enableTxIntr(txCppi);
			if( retVal ){
				DBG(1, "\nError:cppi_alloc_channel: unable to enable interrupt ");
				goto txCppiAlloc_fail3;	
			}
			pController->txIsrAttachedFlag = 1;
		}

		/* set the all the tx endpoint to Generic mode */
		cppi_mode_update(txCh, 1, DEFAULT_TX_ENDPOINT_MODE );  

		txCh->pEndPt = ep;
		txCh->Channel.bStatus = MGC_DMA_STATUS_FREE;
		txCh->Channel.pPrivateData = txCh;
		txCh->chNo = chNum;

		txCh->txIsOpen = True;
		txCppi->chInfo.chState = USB_CPPI4_CH_OPENED;

		DBG(USB_CPPI4_DEBUG_LEVEL, "Allocate CPPI4 TxCh%d for ep%d\n",chNum, bLocalEnd);

#ifdef	USB_GET_STATS
		rxCppi->outOfTxBD = 0;
#endif
		
		return &txCh->Channel;

	txCppiAlloc_fail3 : 
		PAL_cppi4TxChClose( txCppi->cppi4TxChHnd, NULL );

	txCppiAlloc_fail2 : 
		PAL_cppi4QueueClose( pController->cppi4PAL, txCppi->txCmplQueueHnd );

	txCppiAlloc_fail1 :
		PAL_cppi4QueueClose( pController->cppi4PAL, txCppi->txQueueHnd[0] );

		return (NULL);

	} else {
   
		if (bLocalEnd > ARRAY_SIZE(pController->rxCppiCh)) {
			DBG(1, "no %cX DMA channel for ep%d\n", 'R', bLocalEnd);
			return NULL;
		}

		rxCh = &pController->rxCppiCh[chNum];
		rxCppi = rxCh->rxCppi;

		if( NULL == rxCppi ){
			DBG(1, "\n+-cppi_channel_allocate: rxCppi structure NULL:%d",chNum);
			return (NULL) ; 
		}
		if( !rxCh->rxIsCreated ){
			DBG(1,"Cppi Rx channel is not created chnum=%d",chNum);
			return NULL;
		}

		rxCppi->palCppi4Hnd = pController->cppi4PAL; 

		rxCppi->rxQueueHnd = PAL_cppi4QueueOpen(pController->cppi4PAL, 
							rxCppi->rxQueue);

		if( NULL == rxCppi->rxQueueHnd ){
			DBG(1, "\ncppi_channel_alloc:QueuOpen Error for rxQueue");
			return NULL;
		}

		rxCppi->fdbQueueHnd[0] = PAL_cppi4QueueOpen(pController->cppi4PAL,
							rxCppi->fdbQueue[0]);

		if( NULL == rxCppi->fdbQueueHnd[0] ){
			DBG(1, "\ncppi_channel_alloc:QueuOpen Error for rxQueue");
			goto rxCppiAlloc_fail1 ;
		}
	        /* open the channel in the CPPI4 PAL */
		cppi4RxChHnd = PAL_cppi4RxChOpen(pController->cppi4PAL, 
					&rxCppi->chInfo.cppi4RxChInfo, NULL);

		if( NULL == cppi4RxChHnd ){
			DBG(1, 
			"\ncppi_channel_alloc:Error cppi4RxChOpen");
			goto rxCppiAlloc_fail2 ;
		}		
		rxCppi->cppi4RxChHnd = cppi4RxChHnd;

	        rxAccChHnd = PAL_cppi4AccChOpen(pController->cppi4PAL, &rxCppi->chInfo.rxAccChInfo);
	        if (rxAccChHnd == NULL) {
        	    DBG(1,"\nERROR:ncppi_channel_alloc: CPPI4 Rx PAL_cppi4AccChOpen()");
	             goto rxCppiAlloc_fail2 ;
	        }
        	rxCppi->rxAccChHnd = rxAccChHnd;

		rxCppi->listEntryPtr = PAL_cppi4AccChGetNextList(rxCppi->rxAccChHnd);

		/* enable the rx channel */
		retVal = PAL_cppi4EnableRxChannel ( rxCppi->cppi4RxChHnd, NULL );
		if( PAL_SOK != retVal ){
			DBG(1,
			"\nERROR:cppi_alloc_channel,cppi4EnableRxChannel failed %d",
			retVal);
			goto rxCppiAlloc_fail3;
		}

                if( !pController->rxIsrAttachedFlag ){

                        /* attach accumulator - cppi tx dma interrupt service routine */
                        request_irq( pController->rxdma_IntrLine,usbCppiRxPktProcess_Isr,
                                        SA_INTERRUPT, pThis->controller->bus_id, pThis);

	                retVal = enableRxIntr( rxCppi );

          	        if( PAL_SOK != retVal ){
                	        DBG(1,
                        	"\nERROR:cppi_alloc_channel,enableRxIntr failed %d",
	                        retVal);
        	                goto rxCppiAlloc_fail3;
                	}
                        pController->rxIsrAttachedFlag = 1;
                }
			
		if (rxCh->pEndPt)
			DBG(1, "re-allocating DMA%d RX channel %p\n",chNum,rxCh);

		/* set the all the rx endpoint to Generic mode */	
		cppi_mode_update(rxCh, 0, DEFAULT_RX_ENDPOINT_MODE );  

		rxCh->pEndPt = ep;
		rxCh->Channel.bStatus = MGC_DMA_STATUS_FREE;
		rxCh->Channel.pPrivateData = rxCh;
		DBG(USB_CPPI4_DEBUG_LEVEL,"Allocate CPPI RxCh%d for ep%d\n",chNum,bLocalEnd);
		
		rxCh->rxIsOpen = True;
		rxCppi->chInfo.chState = USB_CPPI4_CH_OPENED;
		rxCh->chNo = chNum;

#ifdef	USB_GET_STATS
		rxCppi->outOfRxBD = 0;
#endif

		return &rxCh->Channel;

	rxCppiAlloc_fail3 :
		PAL_cppi4RxChClose( rxCppi->cppi4RxChHnd , NULL);

	rxCppiAlloc_fail2 :
		PAL_cppi4QueueClose( pController->cppi4PAL, rxCppi->fdbQueueHnd[0] );
		
	rxCppiAlloc_fail1 :
		PAL_cppi4QueueClose( pController->cppi4PAL, rxCppi->rxQueueHnd );
	
		return NULL;
		
	}
}

/* Release a CPPI Channel.  */
static void cppi_channel_release(struct dma_channel *channel)
{
	struct cppi_channel	*c;
	struct cppi_channel	*otgChannel = channel->pPrivateData;

	/* REVISIT:  for paranoia, check state and abort if needed... */
	c = container_of(channel, struct cppi_channel, Channel);
	if (!c->pEndPt)
		DBG(1, "releasing idle DMA channel %p\n", c);

	/* but for now, not its IRQ */
	c->pEndPt = NULL;
	channel->bStatus = MGC_DMA_STATUS_UNKNOWN;
	otgChannel->Channel.pPrivateData = 0;
        otgChannel->chNo = 0;
}


void cppi_mode_update ( struct cppi_channel *c, int bTransmit, int mode )
{
        struct cppi *pController; // = c->pController;
        Uint32 chNum = c->chNo,regVal;
        void                    *__iomem mbase;
        void                    *__iomem regBase;

	pController = c->pController;
	
        mbase = pController->pCoreBase;
        regBase = mbase - PUMA5_BASE_OFFSET;

	if( bTransmit ){
		c->bLastMode = mode & 0x3;
		regVal = musb_readl(regBase, PUMA5_MODE_TGCR_REG);
		regVal |= (c->bLastMode << (chNum * 4));
	}
	else{
		c->bLastMode = mode & 0x3;
		regVal = musb_readl(regBase, PUMA5_MODE_TGCR_REG);
		regVal |= ((c->bLastMode << (chNum * 4)) << 16);
	}

	musb_writel(regBase, PUMA5_MODE_TGCR_REG,regVal); 

}
 
void set_rndis_epsize ( struct cppi_channel *c, Uint32 pktsize)
{
        struct cppi *pController = c->pController;
        Uint32 chNum = c->chNo,offset;
        void                    *__iomem mbase;
        void                    *__iomem regBase;


        mbase = pController->pCoreBase;
        regBase = mbase - PUMA5_BASE_OFFSET;

	offset = PUMA5_GRNDIS_EP1SIZE_REG + 4 * chNum;

        musb_writel(regBase, offset, pktsize);
}

static inline int cppi_autoreq_update(struct cppi_channel *rx,
                                      void *__iomem tibase, int shortpkt, u32 length)
{
        u32 tmp, val;

        /* assert(is_host_active(musb)) */
        /* start from "AutoReq never" */
        tmp = musb_readl(tibase, PUMA5_AUTOREQ_REG);
        val = tmp & ~((0x3) << (rx->chNo * 2));
        /* HCD arranged reqpkt for packet #1.  we arrange int
         * for all but the last one, maybe in two segments.
         */

        if ( !shortpkt || ((length % 512 == 0) && (length > 512)) ) {
                rx->autoReq = 1;
                val |= (0x1 << (rx->chNo * 2));
        }else{
                rx->autoReq = 0;
                val &= ~((0x3) << (rx->chNo * 2));
        }

        if (val != tmp)
                musb_writel(tibase, PUMA5_AUTOREQ_REG, val);

        return rx->autoReq;
}

void printBD(UsbCppi4HostDesc *currBD, int displayData)
{
	printk("\n currBD = %p", currBD);
	if( currBD == NULL ){
		printk("---NULL BD !!");
		return ;
	}

	printk("\ntag=%x  descInfo=%x  bufPtr=%x  orgBufPtr=%x\nbufLen=%d OrgBLen=%d  tagId=%x",
	currBD->tagInfo, currBD->descInfo, currBD->bufPtr, currBD->orgBufPtr, 
        currBD->buffLen, currBD->orgBuffLen,currBD->tagId); 

     if( displayData ){
	u8 *ptr;
	u32 i;

	ptr = (u8 *)PAL_CPPI4_PHYS_2_VIRT((char *)currBD->bufPtr);
	for(i=0; i<currBD->buffLen; ++i){
		if( i % 32 == 0)
			printk("\n %04x:",i);
		printk(" %02x", ptr[i]);
	}
	
     }

}

/*
 * CPPI TX:
 * ========
 * cppi_next_tx_segment - dma write for the next chunk of a buffer
 * @musb: the controller
 * @tx: dma channel
 * @rndis : zero bit in the usb_request is set. Zero length packet 
 *	   has to be transmitted,if transmit buffer length is exact
  *	   multiple of USB tx endpoint size.
 * Context: controller irqlocked
 *
 * the cppi rx channels are configured to work in Generic rndis mode
 */
static void
cppi_next_tx_segment(struct musb *musb, struct cppi_channel *tx, int rndis)
{
	unsigned		maxpacket = tx->pktSize;  
	size_t			length = tx->transferSize - tx->currOffset;
	unsigned		lockKey,zlp,n_bds;
	usbTxCppiCh *txCppi = tx->txCppi;	
	UsbCppi4HostDesc *currBD,*sopBD, *nextBD;
	struct  cppi *cppi;
        struct dma_controller *dma;

        dma = musb->pDmaController;
        cppi = (struct cppi *)dma->pPrivateData;

	/* check if the usb link is up */
	
	 /*
	  * \note: The critical section is required to protect the "txCppi" data
	  * structure which is being used by this function and by usbTxBDProc().
	  * Since the function requires to protect the resources in many places in the
          * function below, it would be wise to get and release the crit section only
          * once rather than doing it in many places below. Interrupt protection is
          * sought here so that this function or the usbTxBDProc() function can be
          * invoked in any context and still will be safe to do so.
          */

	/* if length of transmit buffer is multiple of usb endpoint size
	   then send the zero length packet */
	zlp = 0;
	n_bds = 1; 
	if( rndis && (length % maxpacket == 0) ){
		zlp = 1; 
		n_bds = 2;
	}
	

    	PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);
	
	/* get the free Transmit Host buffer descriptor from TX BD pool */
	currBD = usbGetFreeTxBDList(txCppi, n_bds);

	PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
	
	if( currBD == NULL ){
#ifdef	USB_GET_STATS
		txCppi->outOfTxBD++;
#endif
		/* retVal = USB_ERR_TX_OUT_OF_BD;  TODO */
		DBG(1,"\n tx NULL BD ");
		return ;
	}
	
	sopBD = currBD;

	currBD->tagInfo   = /*txCppi->chInfo.chNum  << 16 */ 0; 
			    /* TODO source TagNumber(Bit31:27), 
				ChannelNumber(Bit26:21), SubChannel Number(Bit20:16)
				Destination tag (Bit 16:0) */
	currBD->descInfo  = (CPPI4_DESC_TYPE_HOST << CPPI4_DESC_TYPE_SHIFT_HOST) 
			     | length;

	currBD->bufPtr      = PAL_CPPI4_VIRT_2_PHYS((Int32 *)tx->startAddr);
	currBD->orgBufPtr   = currBD->bufPtr;
	currBD->buffLen     = length;
	currBD->orgBuffLen  = length;
	currBD->zlp 	    = 0;
	currBD->tagId       = ( tx->chNo << 4)| (tx->pEndPt->bLocalEnd);
	PAL_CPPI4_CACHE_WRITEBACK(currBD, CPPI4_BD_LENGTH_FOR_CACHE);
      	

	if( zlp ){
		nextBD = currBD->nextBDPtr;
	        nextBD->tagInfo   = /*txCppi->chInfo.chNum  << 16 */ 0;
                            /* TODO source TagNumber(Bit31:27),
                                ChannelNumber(Bit26:21), SubChannel Number(Bit20:16)
                                Destination tag (Bit 16:0) */
	        nextBD->descInfo  = (CPPI4_DESC_TYPE_HOST << CPPI4_DESC_TYPE_SHIFT_HOST);
	        nextBD->bufPtr     = 0;
        	nextBD->orgBufPtr  = 0;
	        nextBD->buffLen    = 0;
	        nextBD->orgBuffLen  = 0;
        	nextBD->nextBDPtr =  NULL ;  /* (Ptr) PAL_CPPI4_VIRT_2_PHYS(currBD->nextSwBDPtr); */
		nextBD->zlp = 1;
	}else
		currBD->nextBDPtr   =  NULL ;  /* (Ptr) PAL_CPPI4_VIRT_2_PHYS(currBD->nextSwBDPtr); */
	

	/* TODO Add tx priority support . */
	PAL_cppi4QueuePush(txCppi->txQueueHnd[0], 
			   (Ptr)PAL_CPPI4_VIRT_2_PHYS((u32)sopBD),
			   txCppi->chInfo.descAlignment, length);

}

/*
 * CPPI RX:
 * ========
 */

/**
 * cppi_next_rx_segment - dma read for the next chunk of a buffer
 * @musb: the controller
 * @rx: dma channel
 * @onepacket: true unless caller treats short reads as errors, and
 * 	performs fault recovery above usbcore.
 * Context: controller irqlocked
 *
 * the cppi rx channels are configured to work in Generic rndis mode
 * this mode is identical to the normal RNDIS mode in neraly all respects
 * except for the exception case where data is being received or transmitted
 * and the last packet of the segment exaclty matches the max USB packet size.
 * 	This mode will receive the USB packets in the same manner as RNDIS mode
 * closing the CPPI packets when a USB packet is rececieved that is less than
 * the endpoint size. otherwise the packet will be closed when the value in 
 * Generic RNDIS EP Size register is reached. 
 * 	This mode will work for network class driver (CDC/RNDIS) as well as 
 * for mass storage class, where there is no short packet.
 *
 */
static void
cppi_next_rx_segment(struct musb *musb, struct cppi_channel *rx, int shortpkt)
{
	dma_addr_t		addr = rx->startAddr + rx->currOffset;
	size_t			length= rx->transferSize - rx->currOffset;
	usbRxCppiCh *rxCppi = rx->rxCppi;
	UsbCppi4HostDesc *currBD ;
	unsigned 	lockKey;
	struct  cppi *cppi;
        struct dma_controller *dma;

        dma = musb->pDmaController;
        cppi = (struct cppi *)dma->pPrivateData;

	PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);

	/* get the free Rx Buffer Descriptor from free Rx BD Pool */
	currBD = usbGetFreeRxBDList(rxCppi, 1);

	PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);

	if( currBD == NULL ){
#ifdef	USB_GET_STATS
		rxCppi->outOfRxBD++;
#endif
		DBG(1,"\n rx NULL BD  ");
		return ;
	}
	

	
	currBD->bufPtr     = PAL_CPPI4_VIRT_2_PHYS( (Int32 *)addr );
	currBD->buffLen    = length;
	currBD->orgBufPtr  = currBD->bufPtr;
	currBD->orgBuffLen = length;
	currBD->nextBDPtr  = NULL ; /* (Ptr) PAL_CPPI4_VIRT_2_PHYS(currBD->nextSwBDPtr); */
	currBD->tagId       = ( rx->chNo << 4)| (rx->pEndPt->bLocalEnd);
	PAL_CPPI4_CACHE_WRITEBACK(currBD, CPPI4_BD_LENGTH_FOR_CACHE);


	/* set received data length in Generic RNDIS ep1/2/3/4 size register */
	set_rndis_epsize ( rx, length);

	/* push the free rx Buffer descripotr to the fdbQueue */
        PAL_cppi4QueuePush(rxCppi->fdbQueueHnd[0], (Ptr)PAL_CPPI4_VIRT_2_PHYS((u32)currBD),
			   rxCppi->chInfo.descAlignment, 0);
	
	rxCppi->numOfBdInFdbQueue++;

}

/**
 * cppi_channel_program - program channel for data transfer
 * @pChannel: the channel
 * @wPacketSz: max packet size
 * @mode: For RX, 1 unless the usb protocol driver promised to treat
 * 	all short reads as errors and kick in high level fault recovery.
 * 	For TX, 0 unless the protocol driver _requires_ short-packet
 * 	termination mode.
 * @dma_addr: dma address of buffer
 * @dwLength: length of buffer
 * Context: controller irqlocked
 */
static int  cppi_channel_program(struct dma_channel *pChannel,
		u16 wPacketSz, u8 mode,
		dma_addr_t dma_addr, u32 dwLength)
{
	struct cppi_channel	*otgChannel = pChannel->pPrivateData;
	struct cppi		*pController = otgChannel->pController;
	struct musb		*musb = pController->musb;

	switch (pChannel->bStatus) {
	case MGC_DMA_STATUS_BUS_ABORT:
	case MGC_DMA_STATUS_CORE_ABORT:
		/* fault irq handler should have handled cleanup */
		WARN("%cX DMA%d not cleaned up after abort!\n",
				otgChannel->bTransmit ? 'T' : 'R',
				otgChannel->chNo);
		//WARN_ON(1);
		break;
	case MGC_DMA_STATUS_BUSY:
		WARN("program active channel?  %cX DMA%d\n",
				otgChannel->bTransmit ? 'T' : 'R',
				otgChannel->chNo);
		//WARN_ON(1);
		break;
	case MGC_DMA_STATUS_UNKNOWN:
		DBG(1, "%cX DMA%d not allocated!\n",
				otgChannel->bTransmit ? 'T' : 'R',
				otgChannel->chNo);
		/* FALLTHROUGH */
	case MGC_DMA_STATUS_FREE:
		break;
	}

	pChannel->bStatus = MGC_DMA_STATUS_BUSY;

	/* set transfer parameters, then queue up its first segment */
	otgChannel->startAddr = dma_addr;
	otgChannel->currOffset = 0;
	otgChannel->pktSize = wPacketSz;
	otgChannel->actualLen = 0;
	otgChannel->transferSize = dwLength;
  	otgChannel->rxMode = mode;
  	otgChannel->reqcomplete = 0;
	otgChannel->autoReq = 0;

	/* TX channel? or RX? */
	if (otgChannel->bTransmit)
		cppi_next_tx_segment(musb, otgChannel, mode);
	else
		cppi_next_rx_segment(musb, otgChannel, mode);

	return TRUE;
}
struct musb *gpThis;
/* Instantiate a software object representing a DMA controller. */
static struct dma_controller *
cppi_controller_new(struct musb *musb, void __iomem *pCoreBase)
{
	struct dma_controller	*pResult = NULL;
	struct cppi		*pController;

	pController = kzalloc(sizeof *pController, GFP_KERNEL);
	if (!pController)
		return NULL;

	/* Initialize the Cppi DmaController  structure */
	//pController->dma_completed = dma_completed;
	pController->pCoreBase = pCoreBase;
	pController->musb = musb;
	pController->Controller.pPrivateData = pController;
	pController->Controller.start = cppi_controller_start;
	pController->Controller.stop = cppi_controller_stop;
	pController->Controller.channel_alloc = cppi_channel_allocate;
	pController->Controller.channel_release = cppi_channel_release;
	pController->Controller.channel_program = cppi_channel_program;
	pController->Controller.channel_abort = cppi_channel_abort;
	pResult = &(pController->Controller);
	gpThis = musb;
	return pResult;
}

/*
 *  Destroy a previously-instantiated DMA controller.
 */
static void cppi_controller_destroy(struct dma_controller *pController)
{
	struct cppi	*cpController = pController->pPrivateData;

	/* free the cppi object */
	if (cpController) {
		cpController->Controller.pPrivateData = NULL;
		kfree(cpController);
	}

}

/*
 * Context: controller irqlocked, endpoint selected
 */
static int cppi_channel_abort(struct dma_channel *pChannel)
{
	struct cppi_channel	*otgCh = pChannel->pPrivateData;
	struct cppi		*pController = otgCh->pController;
	int			chNum = otgCh->chNo;
	void			*__iomem mbase;
	void			*__iomem regBase;
	u32			regVal,indx;
        u32			csr;
	usbTxCppiCh *txCppi;
        usbRxCppiCh *rxCppi;
	
	switch (pChannel->bStatus) {

		case MGC_DMA_STATUS_BUS_ABORT:
		case MGC_DMA_STATUS_CORE_ABORT:
		/* from RX or TX fault irq handler */
		case MGC_DMA_STATUS_BUSY:
		/* the hardware needs shutting down */
			dprintk("\n channel_release:dma status busy = %x",pChannel->bStatus);
			break;
		case MGC_DMA_STATUS_UNKNOWN:
			DBG(8, "%cX DMA%d not allocated\n",
				otgCh->bTransmit ? 'T' : 'R',
				otgCh->chNo);
			/* FALLTHROUGH */
		case MGC_DMA_STATUS_FREE:
	
		//	printk("\n channel_release:dma status free = %x",pChannel->bStatus);
			break;
	}


	mbase = pController->pCoreBase;
	regBase = mbase - PUMA5_BASE_OFFSET;

	/* REVISIT should rely on caller having done this,
	 * and caller should rely on us not changing it.
	 * peripheral code is safe ... check host too.
	 */
	MGC_SelectEnd(mbase, chNum + 1);

	if (otgCh->bTransmit) {

		dprintk("tx channel teardown, otgCh = %p\n",otgCh);
		txCppi = otgCh->txCppi;

        	otgCh->txTeardownPending = True;

		/* tear down the tx channel */
		txCppiChannelTearDown( otgCh );

        	otgCh->txTeardownPending = False;

		for (indx = 0; indx < txCppi->numTxQs; indx++) {
        	    PAL_cppi4QueueClose (pController->cppi4PAL, txCppi->txQueueHnd[indx]);
	        }
        	PAL_cppi4QueueClose (pController->cppi4PAL, txCppi->txCmplQueueHnd);

	        /* Mark channel closed */
        	otgCh->txIsOpen = False;
		txCppi->chInfo.chState = USB_CPPI4_CH_CLOSED;

	
		/* FIXME clean up the transfer state ... here?
		 * the completion routine should get called with
		 * an appropriate status code.
		 */
		regVal = MGC_ReadCsr16(mbase, MGC_O_HDRC_TXCSR, chNum + 1);
		regVal |= MGC_M_TXCSR_FLUSHFIFO;
		MGC_WriteCsr16(mbase, MGC_O_HDRC_TXCSR, chNum + 1, regVal);
		MGC_WriteCsr16(mbase, MGC_O_HDRC_TXCSR, chNum + 1, regVal);

	} else /* RX */ {

		dprintk("rx channel teardown, otgCh = %p\n",otgCh);
                rxCppi = otgCh->rxCppi;

        	otgCh->rxTeardownPending = True;

		/*	
		regVal = MGC_ReadCsr16(mbase, MGC_O_HDRC_RXCSR, chNum + 1);
                regVal &= ~MGC_M_RXCSR_DMAENAB;
                MGC_WriteCsr16(mbase, MGC_O_HDRC_RXCSR, chNum + 1, regVal);
		*/

                //dprintk("\n mbase=%p, chnum = %d",mbase,chNum+1);
		/* flush the fifo of endpoint */
                regVal = MGC_ReadCsr16(mbase, MGC_O_HDRC_RXCSR, chNum + 1);
                regVal |= MGC_M_RXCSR_FLUSHFIFO | MGC_M_RXCSR_P_WZC_BITS;
                MGC_WriteCsr16(mbase, MGC_O_HDRC_RXCSR, chNum + 1, regVal);
                MGC_WriteCsr16(mbase, MGC_O_HDRC_RXCSR, chNum + 1, regVal);

		/* tear down the rx channel */
		rxCppiChannelTearDown( otgCh );

        	otgCh->rxTeardownPending = False;

	      /* Mark channel closed */
        	otgCh->rxIsOpen = False;

	        PAL_cppi4QueueClose (pController->cppi4PAL, rxCppi->rxQueueHnd);
        	PAL_cppi4QueueClose (pController->cppi4PAL, rxCppi->fdbQueueHnd[0]);
 
		rxCppi->chInfo.chState = USB_CPPI4_CH_CLOSED;
 
		/* NOTE: docs don't guarantee any of this works ...  we
		 * expect that if the usb core stops telling the cppi core
		 * to pull more data from it, then it'll be safe to flush
		 * current RX DMA state iff any pending fifo transfer is done.
		 */

		/* for host, ensure ReqPkt is never set again */
		if (is_host_active(otgCh->pController->musb)) {
			regVal = musb_readl(regBase, PUMA5_AUTOREQ_REG);
			regVal &= ~((0x3) << (otgCh->chNo * 2));
			musb_writel(regBase, PUMA5_AUTOREQ_REG, regVal);
		}

		csr = MGC_ReadCsr16(mbase, MGC_O_HDRC_RXCSR, otgCh->chNo +1);

		/* for host, clear (just) ReqPkt at end of current packet(s) */
		if (is_host_active(otgCh->pController->musb)) {
			csr |= MGC_M_RXCSR_H_WZC_BITS;
			csr &= ~MGC_M_RXCSR_H_REQPKT;
		} else
			csr |= MGC_M_RXCSR_P_WZC_BITS;
		
		/* clear dma enable */
		csr &= ~(MGC_M_RXCSR_DMAENAB);
		MGC_WriteCsr16(mbase, MGC_O_HDRC_RXCSR, otgCh->chNo + 1, csr);

		 /* flush the fifo of endpoint */
                regVal = MGC_ReadCsr16(mbase, MGC_O_HDRC_RXCSR, chNum + 1);
                regVal |= MGC_M_RXCSR_FLUSHFIFO | MGC_M_RXCSR_P_WZC_BITS;
                MGC_WriteCsr16(mbase, MGC_O_HDRC_RXCSR, chNum + 1, regVal);
                MGC_WriteCsr16(mbase, MGC_O_HDRC_RXCSR, chNum + 1, regVal);

		pChannel->bStatus = MGC_DMA_STATUS_FREE;
		/* quiesce: wait for current dma to finish (if not cleanup)
		 * we can't use bit zero of stateram->sopDescPtr since that
		 * refers to an entire "DMA packet" not just emptying the
		 * current fifo, and most segements need multiple fifos.
		 */
		if (pChannel->bStatus == MGC_DMA_STATUS_BUSY)
			udelay(50);
	}

	pChannel->bStatus = MGC_DMA_STATUS_FREE;
	otgCh->startAddr = 0;
	otgCh->currOffset = 0;
	otgCh->transferSize = 0;
	otgCh->pktSize = 0;

	return 0;
}

inline UsbCppi4HostDesc *usbGetTxBD(usbTxCppiCh * txCppi)
{
    UsbCppi4HostDesc *currBD;

#ifdef USB_USE_ACC_LIST

    currBD = (UsbCppi4HostDesc *) (*txCppi->listEntryPtr);
    currBD = (UsbCppi4HostDesc *)((Uint32) currBD & QMGR_QUEUE_N_REG_D_DESC_ADDR_MASK);
#else
    currBD = (UsbCppi4HostDesc *) PAL_cppi4QueuePop (txCppi->txCmplQueueHnd);
#endif

    if (currBD) {
#ifdef USB_USE_ACC_LIST
        txCppi->listEntryPtr = (Uint32 *)((Uint32)txCppi->listEntryPtr
                                        + USB_ACC_ENTRY_SIZE);
#endif
        currBD = PAL_CPPI4_PHYS_2_VIRT(currBD);

#ifdef USB_CPPI4_DEBUG
//        printk("processed BD %x\n", (Uint32)currBD);
#endif

        PAL_CPPI4_CACHE_INVALIDATE(currBD, CPPI4_BD_LENGTH_FOR_CACHE);
    }
    return (currBD);
}

/*
 * txCppiChannelTearDown
 */
static int txCppiChannelTearDown( struct cppi_channel *txCh )
{

	struct cppi *pController = txCh->pController;
	usbTxCppiCh *txCppi = txCh->txCppi,*ntxCppi;
	int retVal,ret,bLocalEnd ;
	UsbCppi4HostDesc *currBD,*tempBD;
	Uint32 lockKey,chNum = txCh->chNo,tdreg;
	void			*__iomem mbase;
	void			*__iomem regBase;
	struct musb *pThis = pController->musb;
	struct musb_ep *pEnd;
	struct dma_channel *dma;
	int nPages,done,timeout = 100;

	mbase = pController->pCoreBase;
	regBase = mbase - PUMA5_BASE_OFFSET;
	
        /* disable the associated accumulator channel interrupt line
           through intd enable/disable interrupt register */
        retVal = disableTxIntr(txCppi);
        if( retVal ){
               DBG(1, "\nERROR:dmaAbortChannel:disableTxIntr retVal = %d",
                                retVal);
                        return retVal;
         }

        retVal = PAL_cppi4AccChClose(txCppi->txAccChHnd, NULL);
        if( retVal ){
               DBG(1, "\nERROR:dmaAbortChannel:acc close ch retVal = %d",
                              retVal);
                        return retVal;
         }
#ifdef DEBUG_CPPI_TD
	printk(" before tear down \n");	
	printBDList(txCppi->bdPoolHead);
#endif
         /* disable the associated accumulator channel and
            initiate the channel teardown for txCppiDma channel */
         retVal = PAL_cppi4TxChClose(txCppi->cppi4TxChHnd, NULL);
         if( retVal ){
        	 DBG(1, "\nERROR:dmaAbortChannel:PalTxChClose retVal = %d",
                                retVal);
                        return retVal;
         }

	/* issue tear down for tx channel -- mentor core register */
	 /*tdreg = musb_readl(regBase, PUMA5_TEARDOWN_REG);
	 tdreg |= ( (1 << chNum) << TX_TEARDOWN_SHIFT_CNT );
       	 musb_writel(regBase, PUMA5_TEARDOWN_REG, tdreg);
	 */

         /* There might be packets on the Tx Queue that were not sent,
            they need to be recycled properly */
         PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);
         while( (currBD = (UsbCppi4HostDesc *)PAL_cppi4QueuePop(txCppi->txQueueHnd[0])) ){
         	currBD = PAL_CPPI4_PHYS_2_VIRT(currBD);
		chNum = (currBD->tagId & 0xFF) ;
	        bLocalEnd = chNum & 0xF;
        	chNum = chNum >> 4;
		txCh->actualLen = currBD->buffLen;
                txCh->Channel.dwActualLength = txCh->actualLen;
                txCh->Channel.bStatus = MGC_DMA_STATUS_FREE;

		dprintk(" usbtxBD (%p) popped from txQueue\n",currBD);
		/* TODO */
	        pEnd = &pThis->aLocalEnd[bLocalEnd*2-1].ep_in;
                dma = pEnd->dma;
                dma->bStatus = MGC_DMA_STATUS_FREE;
                    
                //musb_dma_completion(pController->musb, bLocalEnd, 1);

                /* Return TX BD's to the software list - this is protected by critical
                   section */
		ntxCppi = pController->txCppiCh[chNum].txCppi;
                currBD->nextSwBDPtr = (Ptr)ntxCppi->bdPoolHead;
                ntxCppi->bdPoolHead = currBD;
                ntxCppi->numBD++;

          }/* End of while loop */
          PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);

	 ret = False;
do{
	 //cppi_abort_flushfifo(pThis, txCh->chNo+1, 0);

	 /* issue tear down for tx channel -- mentor core register */
         tdreg = musb_readl(regBase, PUMA5_TEARDOWN_REG);
         tdreg |= ( (1 << chNum) << TX_TEARDOWN_SHIFT_CNT );
         musb_writel(regBase, PUMA5_TEARDOWN_REG, tdreg);


#ifdef USB_USE_ACC_LIST

	  nPages = avalanche_intd_get_interrupt_count (USB_HOST, txCppi->accChNum);
	  while(nPages) {
            /* only one tx completion queue is used for all tx channel 1,2,3,4
               hence each buffer descriptor from txcompletionQ has to be interpreted
               for soruce tx channel number by using tagId field of currBD */
                done = 0;
                while  ( (currBD = usbGetTxBD(txCppi)) ) {
           /* get the currBD from transmit completion queue
              get the tx channel number from tagId of currBD
            */

			if( (((currBD->descInfo & CPPI4_DESC_TYPE_MASK_TD)>> CPPI4_DESC_TYPE_SHIFT_TD) == CPPI4_DESC_TYPE_TD)) {
                                        PAL_cppi4GetTdInfo (txCppi->palCppi4Hnd, (PAL_Cppi4BD*)currBD, NULL, NULL, NULL);
                                        ret = True;
					dprintk(" tx TD-BD (%p) popped from accumulator\n",currBD);	
            				txCppi = pController->txCppiCh[0].txCppi;
				        avalanche_intd_set_interrupt_count (USB_HOST, txCppi->accChNum, 1);
					break;
                        }

			dprintk(" txBD (%p) popped from accumulator\n",currBD);

                        chNum = (currBD->tagId & 0xFF) ;
                        bLocalEnd = chNum & 0xF;
                        chNum = chNum >> 4;
                        txCh->actualLen = currBD->buffLen;
                        txCh->Channel.dwActualLength = txCh->actualLen;
                        txCh->Channel.bStatus = MGC_DMA_STATUS_FREE;

                        /* TODO tx completion routine call back */
                        pEnd = &pThis->aLocalEnd[bLocalEnd*2-1].ep_in;
                        dma = pEnd->dma;
                        dma->bStatus = MGC_DMA_STATUS_FREE;
                        dma->dwActualLength = currBD->buffLen;

//                        musb_dma_completion(cppi->musb, bLocalEnd, 1);

                        /* Return TX BD's to the software list - this is protected by critical
                           section */
                        ntxCppi = pController->txCppiCh[chNum].txCppi;
                        currBD->nextSwBDPtr = (Ptr)ntxCppi->bdPoolHead;
                        ntxCppi->bdPoolHead = currBD;
                        ntxCppi->numBD++;
                        done = 1;
                }

        	if( done )
                   txCppi->listEntryPtr = PAL_cppi4AccChGetNextList(txCppi->txAccChHnd);

            	txCppi = pController->txCppiCh[0].txCppi;
	        avalanche_intd_set_interrupt_count (USB_HOST, txCppi->accChNum, 1);
		doTxEOI( txCppi); 
        	nPages = avalanche_intd_get_interrupt_count (USB_HOST, txCppi->accChNum);
	}

#endif /* USB_USE_ACC_CH */

	while ((tempBD = (UsbCppi4HostDesc *) PAL_cppi4QueuePop(txCppi->txCmplQueueHnd))) {

	       	currBD = PAL_CPPI4_PHYS_2_VIRT(tempBD);

                /* check for teardown condition here.  This allows packets to
                   be processed normally up until the teardown descriptor shows up */

                 PAL_CPPI4_CACHE_INVALIDATE(currBD, CPPI4_BD_LENGTH_FOR_CACHE);
                 if (((currBD->descInfo & CPPI4_DESC_TYPE_MASK_TD)
                 	>> CPPI4_DESC_TYPE_SHIFT_TD) == CPPI4_DESC_TYPE_TD) {
                                /* TODO what has to done with TearDown descriptor , whether
                                        TD has to queued to default free teardown queue */
			       dprintk(" tx TD-BD (%p) popped from txCmplQ\n",currBD);
			       PAL_cppi4GetTdInfo (txCppi->palCppi4Hnd, (PAL_Cppi4BD*)currBD, NULL, NULL, NULL);
                               ret = True;
                               continue; /* TODO break; ?? */
                 }
		 dprintk(" txBD (%p) popped from txCmplQueue\n",currBD);

                 /* Return TX BD's to the software list - this is protected by critical
                    section */
                 chNum = (currBD->tagId & 0xFF) ;
                 bLocalEnd = chNum & 0xF;
                 chNum = chNum >> 4;
		 txCh->actualLen = currBD->buffLen;
                 txCh->Channel.dwActualLength = txCh->actualLen;
                 txCh->Channel.bStatus = MGC_DMA_STATUS_FREE;

                 /* TODO tx completion routine call back */
                 pEnd = &pThis->aLocalEnd[bLocalEnd*2-1].ep_in;
                 dma = pEnd->dma;
                 dma->bStatus = MGC_DMA_STATUS_FREE;
                 dma->dwActualLength = currBD->buffLen;
                 //musb_dma_completion(cppi->musb, bLocalEnd, 1);

                 ntxCppi = pController->txCppiCh[chNum].txCppi;
                 currBD->nextSwBDPtr = (Ptr)ntxCppi->bdPoolHead;
                 ntxCppi->bdPoolHead = currBD;
                 ntxCppi->numBD++;
        }
	if ( ret == True ){
		dprintk("tear down descriptor received\n");
		break;
	}
}while(--timeout);

#ifdef DEBUG_CPPI_TD
	printk(" after teardown\n");
	printBDList(txCppi->bdPoolHead);
#endif
	if( ret != True ){
		/*DBG(1,*/ printk("\n ERROR: TearDown BD not found.");
	}

	/* issue tear down for tx channel -- mentor core register */
        tdreg = musb_readl(regBase, PUMA5_TEARDOWN_REG);
        tdreg |= ( (1 << chNum) << TX_TEARDOWN_SHIFT_CNT );
        musb_writel(regBase, PUMA5_TEARDOWN_REG, tdreg);

        retVal = PAL_cppi4DisableTxChannel(txCppi->cppi4TxChHnd, NULL);
        if (retVal != USB_SUCCESS) {
                    DBG(1,
                        "\nERROR:AbortDmaCh: Error %08X from PalDisableTxChannel()",
                         retVal);
                    return (retVal);
        }

	retVal = PAL_cppi4TxChDestroy(txCppi->cppi4TxChHnd, NULL);
        if (retVal != USB_SUCCESS) {
                    DBG(1,
                        "\nERROR:AbortDmaCh: Error %08X from PalDisableTxChannel()",
                         retVal);
                    return (retVal);
        }

	return USB_SUCCESS;
	
}
/*
 * rxCppiChannelTearDown
 */
static int rxCppiChannelTearDown( struct cppi_channel *rxCh )
{

        struct cppi *pController = rxCh->pController;
        usbRxCppiCh *rxCppi = rxCh->rxCppi, *nrxCppi;
        int retVal,ret,bLocalEnd;
        UsbCppi4HostDesc *currBD,*tempBD;
        Uint32 lockKey,chNum = rxCh->chNo,tdreg;
        void                    *__iomem mbase;
        void                    *__iomem regBase;
	int rPages, done;


        mbase = pController->pCoreBase;
        regBase = mbase - PUMA5_BASE_OFFSET;


	/* issue tear down for rx channel -- otg teardown register */
        tdreg = musb_readl(regBase, PUMA5_TEARDOWN_REG);
        tdreg |= ( (1 << chNum) << RX_TEARDOWN_SHIFT_CNT );
        musb_writel(regBase, PUMA5_TEARDOWN_REG, tdreg);
	
      /* disable the associated accumulator channel interrupt line
           through intd enable/disable interrupt register */
	retVal = disableRxIntr(rxCppi);
        if( retVal ){
        	DBG(1, "\nERROR:dmaAbortChannel:disableRxIntr retVal = %d",
                       retVal);
                 return retVal;
        }

        retVal = PAL_cppi4AccChClose(rxCppi->rxAccChHnd, NULL);
        if( retVal ){
               DBG(1, "\nERROR:dmaAbortChannel:acc close rxch retVal = %d",
                              retVal);
                        return retVal;
         }

#ifdef DEBUG_CPPI_TD
        printk(" before tear down \n");
        printBDList(rxCppi->bdPoolHead);
#endif
	/* issue rx dma channel teardown */
        retVal = PAL_cppi4RxChClose(rxCppi->cppi4RxChHnd, NULL);
        if( retVal ){
        	 DBG(1, "\nERROR:dmaAbortChannel:PalRxChClose retVal = %d",
                         retVal);
                 return retVal;
        }


        PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);
        while( (currBD = (UsbCppi4HostDesc *)PAL_cppi4QueuePop(rxCppi->fdbQueueHnd[0])) ){

	        currBD = PAL_CPPI4_PHYS_2_VIRT(currBD);
		PAL_CPPI4_CACHE_INVALIDATE(currBD, CPPI4_BD_LENGTH_FOR_CACHE);

		chNum = (currBD->tagId & 0xFF) ;
	        bLocalEnd = chNum & 0xF;
        	chNum = chNum >> 4;
		rxCh->Channel.bStatus = MGC_DMA_STATUS_FREE;

                dprintk ("Rx:BD(%p) popped from  fdbQueue\n",currBD);
//                musb_dma_completion(pController->musb, bLocalEnd, 0); 

                /* Return RX BD's to the software list - this is protected by critical
                   section */
		nrxCppi = pController->rxCppiCh[chNum].rxCppi;
                currBD->nextSwBDPtr = (Ptr)nrxCppi->bdPoolHead;
                nrxCppi->bdPoolHead = currBD;
                nrxCppi->numBD++;
	}
	PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);

#ifdef USB_USE_ACC_LIST

	rPages = avalanche_intd_get_interrupt_count (USB_HOST, rxCppi->accChNum);
    	while(rPages) {
            done = 0;
            while ( (currBD = usbGetRxBD(rxCppi)) ) {

			 if (((currBD->descInfo & CPPI4_DESC_TYPE_MASK_TD) >> CPPI4_DESC_TYPE_SHIFT_TD) == CPPI4_DESC_TYPE_TD) {
                            PAL_cppi4GetTdInfo (rxCppi->palCppi4Hnd, (PAL_Cppi4BD*)currBD, NULL, NULL, NULL);
                            ret = True;
                            dprintk ("Rx:TD BD(%p) popped from acc list\n",currBD);
                            continue ; /* break; TODO */
                        }

                /* extract the data from recevied pakcet descriptor */
                rxCh->actualLen = currBD->buffLen;
                rxCh->Channel.dwActualLength = rxCh->actualLen;
                rxCh->actStartAddr = (Uint32)PAL_CPPI4_PHYS_2_VIRT( (char *)currBD->bufPtr);
                rxCh->Channel.bStatus = MGC_DMA_STATUS_FREE;

                chNum = (currBD->tagId & 0xFF) ;
                bLocalEnd = chNum & 0xF;
                chNum = chNum >> 4;
                dprintk ("Rx:BD(%p) popped from accumulator\n",currBD);

                /* rx completion routine call back */
//                musb_dma_completion(cppi->musb, bLocalEnd, 0);

                done =1;
                /* Return RX BD's to the software list - this is protected by critical
                   section */
                currBD->buffLen = currBD->orgBuffLen = 0;
                currBD->bufPtr  = currBD->orgBufPtr  = 0;

                nrxCppi = pController->rxCppiCh[chNum].rxCppi;
                currBD->nextSwBDPtr = (Ptr)nrxCppi->bdPoolHead;
                nrxCppi->bdPoolHead = currBD;
                nrxCppi->numBD++;
            }/* End of while loop */

            if( done ) {
                rxCppi->listEntryPtr = PAL_cppi4AccChGetNextList(rxCppi->rxAccChHnd);
                avalanche_intd_set_interrupt_count (USB_HOST, rxCppi->accChNum, 1);
            }
        rPages = avalanche_intd_get_interrupt_count (USB_HOST, rxCppi->accChNum);
    	}
 	doRxEOI( rxCppi );	

	while ( (tempBD = (UsbCppi4HostDesc *) PAL_cppi4QueuePop(rxCppi->rxQueueHnd)) ){
	        if (tempBD == NULL)
        	        break;

      		currBD = PAL_CPPI4_PHYS_2_VIRT(tempBD);

            /* check for teardown condition here.  This allows packets to
               be processed normally up until the teardown descriptor shows up */


	        if (((currBD->descInfo & CPPI4_DESC_TYPE_MASK_TD)
                        >> CPPI4_DESC_TYPE_SHIFT_TD) == CPPI4_DESC_TYPE_TD) {
                        dprintk ("Rx:TD BD (%p) popped from rxQueue\n",currBD);
                	PAL_cppi4GetTdInfo (rxCppi->palCppi4Hnd, (PAL_Cppi4BD*)currBD, NULL, NULL, NULL);
	                ret = True;
        	        continue ; /* break; */
            	}
	
		chNum = (currBD->tagId & 0xFF) ;
                bLocalEnd = chNum & 0xF;
                chNum = chNum >> 4;
	
                dprintk ("Rx:BD (%p) popped from rxQueue\n",currBD);
                /* Return RX BD's to the software list - this is protected by critical
                   section */
		nrxCppi = pController->rxCppiCh[chNum].rxCppi;
                currBD->nextSwBDPtr = (Ptr)rxCppi->bdPoolHead;
                rxCppi->bdPoolHead = currBD;
                rxCppi->numBD++;

        }
#else /* ifndef USB_USE_ACC_LIST */

        while ((tempBD = (UsbCppi4HostDesc *) PAL_cppi4QueuePop(rxCppi->rxQueueHnd))) {

            currBD = PAL_CPPI4_PHYS_2_VIRT(tempBD);
	    chNum = (currBD->tagId & 0xFF) ;
            bLocalEnd = chNum & 0xF;
            chNum = chNum >> 4;


            /* check for teardown condition here.  This allows packets to
               be processed normally up until the teardown descriptor shows up */

            PAL_CPPI4_CACHE_INVALIDATE(currBD, CPPI4_BD_LENGTH_FOR_CACHE);
            if (((currBD->descInfo & CPPI4_DESC_TYPE_MASK_TD)
                        >> CPPI4_DESC_TYPE_SHIFT_TD) == CPPI4_DESC_TYPE_TD) {
                ret = True;
		PAL_cppi4GetTdInfo (rxCppi->palCppi4Hnd, (PAL_Cppi4BD*)currBD, NULL, NULL, NULL);
                continue ; /* break; */
            }

            /* Return RX BD's to the software list - this is protected by critical
               section */
            currBD->nextSwBDPtr = (Ptr)rxCppi->bdPoolHead;
            rxCppi->bdPoolHead = currBD;
            rxCppi->numBD++;

        }
#endif /* USB_USE_ACC_LIST */

#ifdef CONFIG_MACH_PUMA5
	PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);
	initRxBDChain(rxCppi);
	PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
#endif

#ifdef DEBUG_CPPI_TD
	printk(" after tear down \n");
        printBDList(rxCppi->bdPoolHead);
#endif


        if (ret != True) 
        	DBG(1,"\nERROR:Teardown buffer descriptor not found.");

        retVal = PAL_cppi4DisableRxChannel(rxCppi->cppi4RxChHnd, NULL);
        if (retVal != USB_SUCCESS) {
       		DBG(1,
                   "\nERROR:DisableChannel: Error %08X from CPPI4 PAL DisableRxChannel()",
                   retVal);
                    return (retVal);
        }

        retVal = PAL_cppi4RxChDestroy(rxCppi->cppi4RxChHnd, NULL);
        if (retVal != USB_SUCCESS) {
                DBG(1,
                   "\nERROR:DisableChannel: Error %08X from CPPI4 PAL DisableRxChannel()",
                   retVal);
                    return (retVal);
        }

	return retVal ;
        
}
/* ***************   TODO     T x      P R O C E S S  ****************/
/* TBD Queries:
 *
 */
UsbCppi4HostDesc *usbGetFreeTxBDList(usbTxCppiCh * txCppi, int numBDs)
{
    UsbCppi4HostDesc *currBD, *tempBD;

    if (numBDs > txCppi->numBD)
        return NULL;

    txCppi->numBD -= numBDs;

    currBD = txCppi->bdPoolHead;
    tempBD = currBD;

    while (--numBDs) {
        tempBD = (UsbCppi4HostDesc *)tempBD->nextSwBDPtr;
    }

    txCppi->bdPoolHead = (UsbCppi4HostDesc *)tempBD->nextSwBDPtr;
    tempBD->nextSwBDPtr = 0;
    return (currBD);
}
/*
 * usbGetFreeRxBD
 */
UsbCppi4HostDesc *usbGetFreeRxBDList(usbRxCppiCh * rxCppi, int numBDs)
{
    UsbCppi4HostDesc *currBD, *tempBD;

    if (numBDs > rxCppi->numBD){
	printk("\n no free BD ");
        return NULL;
	}

    rxCppi->numBD -= numBDs;

    currBD = rxCppi->bdPoolHead;
    tempBD = currBD;

    while (--numBDs) {
        tempBD = (UsbCppi4HostDesc *)tempBD->nextSwBDPtr;
    }

    rxCppi->bdPoolHead = (UsbCppi4HostDesc *)tempBD->nextSwBDPtr;
    tempBD->nextSwBDPtr = 0;

    return (currBD);
}
/*
 * usbPutFreeRxBD
 */
void usbPutFreeRxBD(usbRxCppiCh *rxCppi, UsbCppi4HostDesc *freeRxBD)
{
        UsbCppi4HostDesc *currBD;

        currBD = freeRxBD;
        currBD->tagInfo = 0;
        currBD->buffLen = 0;
        currBD->bufPtr = 0;
        currBD->orgBuffLen = 0;
        currBD->orgBufPtr = 0;
        PAL_CPPI4_CACHE_WRITEBACK(currBD, CPPI4_BD_LENGTH_FOR_CACHE);
        currBD->nextSwBDPtr = (Ptr)rxCppi->bdPoolHead;
        rxCppi->bdPoolHead = currBD;
        rxCppi->numBD++;
}
/*
 * usbGetRxBD
 * Get the buffer descriptor from queue.
 */
UsbCppi4HostDesc* usbGetRxBD(usbRxCppiCh * rxCppi)
{
    UsbCppi4HostDesc *currBD = NULL;

#ifdef USB_USE_ACC_LIST
    currBD = (UsbCppi4HostDesc *) (*rxCppi->listEntryPtr);
    currBD = (UsbCppi4HostDesc *)((Uint32) currBD & QMGR_QUEUE_N_REG_D_DESC_ADDR_MASK);
#else
    currBD = (UsbCppi4HostDesc *) PAL_cppi4QueuePop (rxCppi->rxQueueHnd);
#endif

    if (currBD) {
#ifdef USB_USE_ACC_LIST
        rxCppi->listEntryPtr = (Uint32 *)((Uint32)rxCppi->listEntryPtr
                                        + USB_ACC_ENTRY_SIZE);
#endif
        currBD = PAL_CPPI4_PHYS_2_VIRT(currBD);
#ifdef USB_CPPI4_DEBUG
//       printk ("Virt addr of currBD = %x\n", (Uint32)currBD);
#endif
       PAL_CPPI4_CACHE_INVALIDATE(currBD, CPPI4_BD_LENGTH_FOR_CACHE);
    }

    return (currBD);
}
/*
 * usbTxPktProcesss
 * process all the txCompleteQueue for tx endpoint 1 to 4.
 */

Int usbTxPktProcess(struct cppi *cppi, Ptr pktArgs)
{

#if defined USB_USE_ACC_LIST || !defined USB_POLL_MODE
    Uint32 intd_status,tx,i;
#endif


#if defined USB_USE_ACC_LIST || !defined USB_POLL_MODE
    intd_status = *(volatile Uint32 *)(INTDSTATUS);

    #ifdef USB_CPPI4_DEBUG
  	  DBG (USB_CPPI4_DEBUG_LEVEL,"usbTxPktProcess: INTD status is %#lx\n", intd_status);
    #endif

    intd_status = (intd_status & USB_ACC_TXCH_MASK) >> USB_ACC_TXCH_SHIFT_CNT;
    for( i = 0, tx = intd_status;  tx; tx = tx >> 1, i++ ){
	if( (tx & 1) == 0 )
		continue ;

        /* process the txBuffer Descriptor from tx completed Queue */	
	usbTxBDProc(cppi, i );

	    /* give EOI */
	doTxEOI( cppi->txCppiCh[i].txCppi); 

    #ifdef USB_CPPI4_GETSTATS
	    cppi->txIntCount[i]++;
    #endif
    }
#else
/* polled mode code here */

#endif
    return (USB_SUCCESS);
}

/**
 *  usbTxBDProc
 */
int usbTxBDProc(struct cppi * cppi, int txChannel )
{
    UsbCppi4HostDesc *currBD;
    usbTxCppiCh *txCppi,*ntxCppi;
    Uint32 chNum = 0 ;
    Uint32 bLocalEnd;
    struct musb *pThis = cppi->musb;
    struct musb_ep *pEnd;
    struct dma_channel *dma;
    int done = 0,nPages;
    struct cppi_channel     *txCh;


    do {

	    for( txChannel  = 0; txChannel < 4 ; ++txChannel ) {

        	txCh  = &cppi->txCppiCh[txChannel];
	        if( !txCh->txIsOpen )
        	        continue;
	        txCppi = txCh->txCppi;

	    /* only one tx completion queue is used for all tx channel 1,2,3,4
	       hence each buffer descriptor from txcompletionQ has to be interpreted
	       for soruce tx channel number by using tagId field of currBD */
		done = 0;
	  	while  ( (currBD = usbGetTxBD(txCppi)) ) {
	   /* get the currBD from transmit completion queue
	      get the tx channel number from tagId of currBD
	    */
			chNum = (currBD->tagId & 0xFF) ;
	        	bLocalEnd = chNum & 0xF;
	        	chNum = chNum >> 4;
			txCh->actualLen = currBD->buffLen;
		        txCh->Channel.dwActualLength = txCh->actualLen;
        		txCh->Channel.bStatus = MGC_DMA_STATUS_FREE;

	        	/* TODO tx completion routine call back */
	        	pEnd = &pThis->aLocalEnd[bLocalEnd*2-1].ep_in;
		        dma = pEnd->dma;
        		dma->bStatus = MGC_DMA_STATUS_FREE;
			dma->dwActualLength = currBD->buffLen;
		        musb_dma_completion(cppi->musb, bLocalEnd, 1);

        		/* Return TX BD's to the software list - this is protected by critical
		           section */
        		ntxCppi = cppi->txCppiCh[chNum].txCppi;
	        	currBD->nextSwBDPtr = (Ptr)ntxCppi->bdPoolHead;
	        	ntxCppi->bdPoolHead = currBD;
		        ntxCppi->numBD++;
			done = 1;
	    	}

#ifdef USB_USE_ACC_LIST
		if( done )
		   txCppi->listEntryPtr = PAL_cppi4AccChGetNextList(txCppi->txAccChHnd);
#endif
	    }/* end for */ 
        
	    txCppi = cppi->txCppiCh[0].txCppi; 
	    avalanche_intd_set_interrupt_count (USB_HOST, txCppi->accChNum, 1);
	    nPages = avalanche_intd_get_interrupt_count (USB_HOST, txCppi->accChNum);
    }while(nPages);

    return USB_SUCCESS;
}


#ifdef USB_CPPI_TX_TASKLET_MODE
void usb_cppi_tx_tasklet(unsigned long data)
{
	//struct musb *musb = (struct musb *)data;
	//struct cppi *cppi = musb->pDmaController->pPrivateData;
	static int cnt = 0;

	printk("\n usb_cppi_tx_tasklet [<%d>]",cnt++);


#if 1
#ifdef USB_CPPI4_DEBUG
        printk ("%s: strart CppiTx (TaskLet)\n", __FUNCTION__);
#endif
	usbTxPktProcess(cppi, NULL );

        /* after the processing all the list buffer pages for all
        the tx channels[1..4] associate with interupt , it must then perform
        an EOI by writing the accumulator interrupt vector(0-15) to
        the INTD eoi register */
/*        *(Uint32 *)INTD_EOI_REG = USB_EP_TX_ACC_VEC_NUM ; 
	TODO eoi already given in TxIsr function, is this is right? */
#endif
}
#endif

irqreturn_t usbCppiTxPktProcess_Isr(int irq, void *pThis, struct pt_regs *r)
{
        struct musb *musb = pThis;
        unsigned long flags;
        //struct cppi *cppi = musb->pDmaController->pPrivateData;
        irqreturn_t retval = IRQ_NONE;
        struct  cppi *cppi;
        struct dma_controller *dmaCtrl;
        //usbTxCppiCh *txCppi;

        spin_lock_irqsave(&musb->Lock, flags);

        dmaCtrl = musb->pDmaController;
        cppi = (struct cppi *)dmaCtrl->pPrivateData;

        /* FIXME */

#ifdef USB_CPPI_TX_TASKLET_MODE
        tasklet_schedule(&cppi->tx_tasklet);
#else
	/* interrupt mode context */
#ifdef USB_CPPI4_DEBUG
//        printk ("%s: strart CppiTx (Isr)\n", __FUNCTION__);
#endif

        usbTxPktProcess(cppi, NULL );
#endif

        /* after the processing all the list buffer pages for all
        the tx channels[1..4] associate with interupt , it must then perform
        an EOI by writing the accumulator interrupt vector(0-15) to
        the INTD eoi register */

        spin_unlock_irqrestore(&musb->Lock, flags);

        retval = IRQ_HANDLED;
        return (retval);

}

/* 
 ************* R X    P A C K E T    P R O C E S S      ***************
*/
Int usbRxPktProcess(struct cppi *cppi, Ptr pktArgs)
{
    Uint32 i=0;

#if defined USB_USE_ACC_LIST || !defined USB_POLL_MODE
    Uint32 intd_status,rx;
#endif

#if defined USB_USE_ACC_LIST || !defined USB_POLL_MODE 
    intd_status = *(volatile Uint32 *)(INTDSTATUS);
    #ifdef USB_CPPI4_DEBUG
    	DBG (USB_CPPI4_DEBUG_LEVEL,"usbRxPktProcess: INTD status is %#lx\n", intd_status);
    #endif
    intd_status = (intd_status & USB_ACC_RXCH_MASK) >> USB_ACC_RXCH_SHIFT_CNT;
    for( i = 0, rx = intd_status;  rx; rx = rx >> 1, i++ ){
        if( (rx & 1) == 0 )
                continue ;
   /* process the txBuffer Descriptor from tx completed Queue */
        usbRxBDProc(cppi,i);

     #ifdef USB_CPPI4_GETSTATS
            cppi->rxIntCount[i]++;
     #endif
    }
#endif

    return (USB_SUCCESS);
}

/**
 *  usbRxBDProc
 */
int usbRxBDProc(struct cppi * cppi, int rxChannel )
{
    UsbCppi4HostDesc *currBD;
    usbRxCppiCh *rxCppi,*nrxCppi;
    struct cppi_channel *rxCppiCh;
    Uint32 chNum,bLocalEnd;
    int done = 0,rPages;


    rxCppiCh = &cppi->rxCppiCh[rxChannel];

    if( !rxCppiCh->rxIsOpen )
           return USB_ERR_RXCH_INVALID;

    rxCppi = rxCppiCh->rxCppi;

    do {
	    done = 0;
	    while ( (currBD = usbGetRxBD(rxCppi)) ) {

        	/* extract the data from recevied pakcet descriptor */
	    	rxCppiCh->actualLen = currBD->buffLen;
        	rxCppiCh->Channel.dwActualLength = rxCppiCh->actualLen;
	        rxCppiCh->actStartAddr = (Uint32)PAL_CPPI4_PHYS_2_VIRT( (char *)currBD->bufPtr);
        	rxCppiCh->Channel.bStatus = MGC_DMA_STATUS_FREE;

	        chNum = (currBD->tagId & 0xFF) ;
        	bLocalEnd = chNum & 0xF;
	        chNum = chNum >> 4;

                /* tx completion routine call back */
        	musb_dma_completion(cppi->musb, bLocalEnd, 0);
	        done =1;
        	/* Return RX BD's to the software list - this is protected by critical
	           section */
        	currBD->buffLen = currBD->orgBuffLen = 0;
	        currBD->bufPtr  = currBD->orgBufPtr  = 0;

        	nrxCppi = cppi->rxCppiCh[chNum].rxCppi;
	        currBD->nextSwBDPtr = (Ptr)nrxCppi->bdPoolHead;
        	nrxCppi->bdPoolHead = currBD;
	        nrxCppi->numBD++;
	    }/* End of while loop */

#ifdef USB_USE_ACC_LIST
	    if( done ) {
    		rxCppi->listEntryPtr = PAL_cppi4AccChGetNextList(rxCppi->rxAccChHnd);
	        avalanche_intd_set_interrupt_count (USB_HOST, rxCppi->accChNum, 1);
    	    }
#endif
	rPages = avalanche_intd_get_interrupt_count (USB_HOST, rxCppi->accChNum);
    }while( rPages);

    return USB_SUCCESS;
}


#ifdef USB_CPPI_RX_TASKLET_MODE
void usb_cppi_rx_tasklet(unsigned long data)
{
        struct musb *musb = (struct musb *)data;
        struct cppi *cppi = (struct cppi *)musb->pDmaController->pPrivateData;

#ifdef USB_CPPI4_DEBUG
        printk ("%s: strart CppiRx (TaskLet)\n", __FUNCTION__);
#endif
        usbRxPktProcess(cppi, NULL );

}
#endif

/*
 * usbCppiRxPktProcess_Isr
 */
irqreturn_t usbCppiRxPktProcess_Isr(int irq, void *pThis, struct pt_regs *r)
{
        struct musb *musb = pThis;
        unsigned long flags;
        irqreturn_t retval = IRQ_NONE;
        int rxChannel = 0;
        struct  cppi *cppi;
        struct dma_controller *dmaCtrl;
        usbRxCppiCh *rxCppi;

        spin_lock_irqsave(&musb->Lock, flags);

        dmaCtrl = musb->pDmaController;
        cppi = (struct cppi *)dmaCtrl->pPrivateData;

        rxCppi = cppi->rxCppiCh[rxChannel].rxCppi;


#ifdef USB_CPPI_RX_TASKLET_MODE
        tasklet_schedule(&cppi->rx_tasklet);
#else

#ifdef USB_CPPI4_DEBUG
        printk ("%s: strart CppiRx (Isr)\n", __FUNCTION__);
#endif
        usbRxPktProcess(cppi, NULL );
#endif

        /* after the processing all the list buffer pages for all
        the tx channels[1..4] associate with interupt , it must then perform
        an EOI by writing the accumulator interrupt vector(0-15) to
        the INTD eoi register */
        doRxEOI( rxCppi );

        spin_unlock_irqrestore(&musb->Lock, flags);

        retval = IRQ_HANDLED;
        return (retval);

}

/*
 * usbCppiPolling_Task
 */
#ifdef USB_POLL_MODE

void usbCppi_tx_poll( unsigned long data )
{
	struct cppi *cppi = (struct cppi *)data;
	usbTxPktProcess( cppi , NULL );
	cppi->tx_poll_timer.expires = jiffies + 2; // * 10;
	add_timer( &cppi->tx_poll_timer );
		
}

void usbCppi_rx_poll( unsigned long data )
{
	struct cppi *cppi = (struct cppi *)data;
	
        usbRxBDProc(cppi, 0 );
	cppi->rx_poll_timer.expires = jiffies + 3; //HZ ;
	add_timer ( &cppi->rx_poll_timer ); 
}

#endif

#ifdef DEBUG_CPPI_TD
void  cppi_abort_flushfifo(struct musb *musb, int bEnd,int flushfifo )
{
        //struct musb_ep          *pEnd = &musb->aLocalEnd[bEnd * 2 -1].ep_in;
        void __iomem            *epio = musb->aLocalEnd[bEnd * 2 -1].regs;
        u8 __iomem              *pBase = musb->pRegs;
        u16 wCsrVal;

        MGC_SelectEnd(pBase, bEnd);

        wCsrVal = musb_readw(epio, MGC_O_HDRC_TXCSR);
        printk("{ep%d:[%p],TXCSR:%x}",bEnd,epio,wCsrVal);

        if( flushfifo ){

                wCsrVal |= MGC_M_TXCSR_TXPKTRDY;
                musb_writew(epio, MGC_O_HDRC_TXCSR, wCsrVal);
                printk("w1:%x",wCsrVal);

		wCsrVal = musb_readw(epio, MGC_O_HDRC_TXCSR);
                wCsrVal |= MGC_M_TXCSR_FLUSHFIFO;
                musb_writew(epio, MGC_O_HDRC_TXCSR, wCsrVal);
                printk("w2:%x",wCsrVal);
//		mdelay(2);

		wCsrVal = musb_readw(epio, MGC_O_HDRC_TXCSR);
                wCsrVal |= MGC_M_TXCSR_FLUSHFIFO;
                musb_writew(epio, MGC_O_HDRC_TXCSR, wCsrVal);
                printk("w3:%x",wCsrVal);
  //              mdelay(2);

                wCsrVal = musb_readw(epio, MGC_O_HDRC_TXCSR);
                printk("r2:%x",wCsrVal);
        }
}

void  cppi_print_txState(int bEnd )
{
	struct musb *musb = gpThis;
        //struct musb_ep          *pEnd = &musb->aLocalEnd[bEnd * 2 -1].ep_in;
        void __iomem            *epio = musb->aLocalEnd[bEnd * 2 -1].regs;
        u8 __iomem              *pBase = musb->pRegs;
        u16 wCsrVal;
        //u32 timeout = 0;

        MGC_SelectEnd(pBase, bEnd);

        wCsrVal = musb_readw(epio, MGC_O_HDRC_TXCSR);
        printk("{ep%d:[%p],TXCSR:%x}",bEnd,epio,wCsrVal);

}
#endif
