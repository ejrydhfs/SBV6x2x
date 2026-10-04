/*
 * File containing hadr wire CPPI configurations for each driver.
 * Put into a single file to (hopefully) avoid configuration
 * clashes.
 *
 * Copyright (C) Texas Instruments, Inc. 2007
 */

/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
/*************************** Global parameters **********************/
/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
/** Not configuration, hardware defined. **/

/* Strictly according to hardware
 * Total 16 Queues.
 */
#define PAL_CPPI41_FBD_Q_BASE                           128
#define PAL_CPPI41_FBD_Q_LAST                           143

/* Strictly according to hardware
 * Total 16 Queues.
 */
#define PAL_CPPI41_FD_Q_BASE                            144
#define PAL_CPPI41_FD_Q_LAST                            159

/* Hardware is having only 4 queues
 * 6 required to using unassigned queues 
 * from 122-127
 * Document change required.
 */
#define PAL_CPPI41_TX_CMPL_Q_BASE                       122
#define PAL_CPPI41_TX_CMPL_Q_LAST                       127

/* Strictly according to hardware
 * Total 16 Queues.
 */  
#define PAL_CPPI41_RX_Q_BASE                            100
#define PAL_CPPI41_RX_Q_LAST                            115

/* Strictly according to hardware
 * Total 8 Queues.
 */ 
#define PAL_CPPI41_HOST2SR_TX_Q_BASE                    196
#define PAL_CPPI41_HOST2SR_TX_Q_LAST                    203


#define PAL_ACC_CHN_BASE                                 0
#define PAL_ACC_CHN_LAST                                31


#define PAL_CPPI41_HOST2SR_DMA_CHN_NUM_BASE              0
#define PAL_CPPI41_HOST2SR_DMA_CHN_NUM_LAST              1

#define PAL_CPPI41_C552SR_DMA_CHN_NUM_BASE               2
#define PAL_CPPI41_C552SR_DMA_CHN_NUM_LAST               2

#define PAL_CPPI41_SR2C55_DMA_CHN_NUM_BASE               3
#define PAL_CPPI41_SR2C55_DMA_CHN_NUM_LAST               3                                                                                   

#define PAL_CPPI41_MPEG_DMA_CHN_NUM_BASE                13
#define PAL_CPPI41_MPEG_DMA_CHN_NUM_LAST                13

#define PAL_CPPI41_RESERVED_DMA_CHN_NUM_BASE            14
#define PAL_CPPI41_RESERVED_DMA_CHN_NUM_LAST            14

#define PAL_CPPI41_DOC_US_COP_DMA_CHN_NUM_BASE          15
#define PAL_CPPI41_DOC_US_COP_DMA_CHN_NUM_LAST          15

#define PAL_CPPI41_DMA_BLOCK0                            0
#define PAL_CPPI41_DMA_BLOCK1                            1


/* External descriptor common size */
#define PAL_CPPI_COMMON1_BD_SIZE                        64
#define PAL_CPPI_COMMON2_BD_SIZE                        64
#define PAL_CPPI_COMMON3_BD_SIZE                        64
#define PAL_CPPI_COMMON4_BD_SIZE                        64

/**
 * Queue manager configurations
 */
#define PAL_CPPI41_NUM_QUEUE_MGR                1    /**< Number of queue managers in CPPI system */
#define PAL_CPPI41_MAX_DESC_REGIONS            16    /**< Maximum descriptor regions per queue manager */
#define PAL_CPPI41_MAX_NUM_QUEUES             368    /**< Maximum number of queues per queue manager */

/**
 * Buffer manager configurations
 */
#define PAL_CPPI41_NUM_BUF_MGR                  1    /**< Number of buffer managers in the system */
#define PAL_CPPI41_BUFMGR_NUM_BUF_POOLS        14    /**< Maximum number of buffer pools per manager */

/**
 * DMA channel configuration
 */
#define PAL_CPPI41_MAX_TX_CHANNELS             22    /**< Max CPPI4.1 Tx channles that could be opened */
#define PAL_CPPI41_MAX_RX_CHANNELS             22    /**< Max CPPI4.1 Rx channles that could be opened */
#define PAL_CPPI41_NUM_TOTAL_CHAN              ((PAL_CPPI41_MAX_TX_CHANNELS) + (PAL_CPPI41_MAX_RX_CHANNELS))   /**< Number of total channels */

/** 
 * DMA scheduler entries.
 */
    


#define PAL_CPPI41_NUM_DMA_BLOCK                2    /**< Total number of DMA blocks in the system */
#define PAL_CPPI41_DMA_BLOCK0                   0
#define PAL_CPPI41_DMA_BLOCK1                   1


/* ---------------------------------------------------------------- */
/*************************** Global parameters **********************/
/* ---------------------------------------------------------------- */

/* +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
/************Shared resources allocation **************/
/* +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */

/* Free Buffer Descriptor queue allocation */
#define CPMAC_CPPI4x_FBD_QMGR                   0
#define CPMAC_CPPI4x_FBD_Q_COUNT                4  /* Actually only 1 is used, reserving 4,  idx is for buffer slot*/
#define CPMAC_CPPI4x_FBD_QNUM(pri)              (PAL_CPPI41_FBD_Q_BASE + 0 + pri)

#define USB_CPPI4x_FBD_QMGR                     0
#define USB_CPPI4x_FBD_Q_COUNT                  4
#define USB_CPPI4x_EP0_FBD_QNUM(pri)            (PAL_CPPI41_FBD_Q_BASE + 4 + pri)
#define USB_CPPI4x_EP1_FBD_QNUM(pri)            (PAL_CPPI41_FBD_Q_BASE + 5 + pri)
#define USB_CPPI4x_EP2_FBD_QNUM(pri)            (PAL_CPPI41_FBD_Q_BASE + 6 + pri)
#define USB_CPPI4x_EP3_FBD_QNUM(pri)            (PAL_CPPI41_FBD_Q_BASE + 7 + pri)

#define CNI_CPPI4x_FBD_QMGR                     0
#define CNI_CPPI4x_FBD_Q_COUNT                  4
#define CNI_CPPI4x_FBD_QNUM(pri)                (PAL_CPPI41_FBD_Q_BASE + 8 + pri)

#define CMGR_CPPI4x_FBD_QMGR                    0
#define CMGR_CPPI4x_FBD_Q_COUNT                 1
#define CMGR_CPPI4x_FBD_QNUM                    (PAL_CPPI41_FBD_Q_BASE + 12)

#if(CMGR_CPPI4x_FBD_Q_COUNT + CNI_CPPI4x_FBD_Q_COUNT + \
    CPMAC_CPPI4x_FBD_Q_COUNT + USB_CPPI4x_FBD_Q_COUNT +  \
    PAL_CPPI41_FBD_Q_BASE - 1 > PAL_CPPI41_FBD_Q_LAST)
#error  "Allocated Free buffer descriptor queues are more than Hardware defined"
#endif

/* Tx completion Queue allocation. */
#define CPMAC_CPPI4x_TX_COMP_QMGR               0
#define CPMAC_CPPI4x_TX_COMP_Q_COUNT            1
#define CPMAC_CPPI4x_TX_COMP_QNUM(pri)          (PAL_CPPI41_TX_CMPL_Q_BASE + 0 + pri)

#define USB_CPPI4x_TX_COMP_QMGR                 0
#define USB_CPPI4x_TX_COMP_Q_COUNT              1
#define USB_CPPI4x_TX_COMP_QNUM(pri)            (PAL_CPPI41_TX_CMPL_Q_BASE + 1 + pri)

#define CNI_CPPI4x_TX_COMP_QMGR                 0
#define CNI_CPPI4x_TX_COMP_Q_COUNT              3
#define CNI_CPPI4x_TX_COMP_QNUM(pri)            (PAL_CPPI41_TX_CMPL_Q_BASE + 2 + pri)

#define CMNG_CPPI4x_TX_COMP_QMGR                0
#define CMNG_CPPI4x_TX_COMP_Q_COUNT             1
#define CMNG_CPPI4x_TX_COMP_QNUM                (PAL_CPPI41_TX_CMPL_Q_BASE + 5 + pri)

#if ((CMNG_CPPI4x_TX_COMP_Q_COUNT + CNI_CPPI4x_TX_COMP_Q_COUNT + \
     USB_CPPI4x_TX_COMP_Q_COUNT + CPMAC_CPPI4x_TX_COMP_Q_COUNT + \
     PAL_CPPI41_TX_CMPL_Q_BASE - 1) > PAL_CPPI41_TX_CMPL_Q_LAST)
#error  "Allocated Tx completion queues are more than Hardware defined"
#endif

/* Rx queue allocation */
#define CPMAC_CPPI4x_RX_QMGR                    0
#define CPMAC_CPPI4x_RX_Q_COUNT                 1
#define CPMAC_CPPI4x_RX_QNUM(pri)               (PAL_CPPI41_RX_Q_BASE + 0 + pri)

#define USB_CPPI4x_RX_QMGR                      0
#define USB_CPPI4x_RX_Q_COUNT                   4
#define USB_CPPI4x_EP0_RX_QNUM(pri)             (PAL_CPPI41_RX_Q_BASE + 1 + pri)
#define USB_CPPI4x_EP1_RX_QNUM(pri)             (PAL_CPPI41_RX_Q_BASE + 2 + pri)
#define USB_CPPI4x_EP2_RX_QNUM(pri)             (PAL_CPPI41_RX_Q_BASE + 3 + pri)
#define USB_CPPI4x_EP3_RX_QNUM(pri)             (PAL_CPPI41_RX_Q_BASE + 4 + pri)

#define CNI_CPPI4x_RX_QMGR                      0
#define CNI_CPPI4x_RX_Q_COUNT                   4
#define CNI_CPPI4x_RX_QNUM(pri)                 (PAL_CPPI41_RX_Q_BASE + 5 + pri)

#define CMGR_CPPI4x_RX_QMGR                     0
#define CMGR_CPPI4x_RX_Q_COUNT                  4
#define CMGR_CPPI4x_RX_QNUM                     (PAL_CPPI41_RX_Q_BASE + 9)

#if ((USB_CPPI4x_RX_Q_COUNT + CPMAC_CPPI4x_RX_Q_COUNT + PAL_CPPI41_RX_Q_BASE) > PAL_CPPI41_RX_Q_LAST)
#error  "Allocated Free Buffer descriptor queueus queues are more than Hardware defined"
#endif

/* Free Descriptor queue allocation */
#define CPMAC_CPPI4x_FD_QMGR                    0
#define CPMAC_CPPI4x_FD_Q_COUNT                 1
#define CPMAC_CPPI4x_FD_QNUM(pri)               (PAL_CPPI41_FD_Q_BASE + 0 + pri)

#define USB_CPPI4x_FD_QMGR                      0
#define USB_CPPI4x_FD_Q_COUNT                   4
#define USB_CPPI4x_EP0_FD_QNUM(pri)             (PAL_CPPI41_FD_Q_BASE + 1 + pri)
#define USB_CPPI4x_EP1_FD_QNUM(pri)             (PAL_CPPI41_FD_Q_BASE + 2 + pri)
#define USB_CPPI4x_EP2_FD_QNUM(pri)             (PAL_CPPI41_FD_Q_BASE + 3 + pri)
#define USB_CPPI4x_EP3_FD_QNUM(pri)             (PAL_CPPI41_FD_Q_BASE + 4 + pri)

#define DMA_CPPI4x_TD_Q_COUNT                   2
#define DMA0_CPPI4x_FTD_QMGR                    0
#define DMA0_CPPI4x_FTD_QNUM                    (PAL_CPPI41_FD_Q_BASE + 5)
#define DMA1_CPPI4x_FTD_QMGR                    0
/* 
 *__IMPORTANT__ : The teardown queue for DMA1 is changed from 150 since it 
 * was observed during tests that Q150 was getting emptied as soon as PP 
 * firmware was downloaded. Note that this is a TEMPORARY fix till the 
 * discussion on this issue is resolved with Mike. Currently using queue 
 * number 216 since it is not used by anyone (for now).
 */
#define DMA1_CPPI4x_FTD_QNUM                    /*216*/ (PAL_CPPI41_FD_Q_BASE + 6)

#define CNI_CPPI4x_FD_QMGR                      0
#define CNI_CPPI4x_FD_Q_COUNT                   4
#define CNI_CPPI4x_FD_QNUM(pri)                 (PAL_CPPI41_FD_Q_BASE + 7 + pri)

// <Ziv2> right I am aware of it. Let me follow up on this with Mike

#define PPFW_CPPI4x_FD_QMGR                     0      
#define PPFW_CPPI4x_FD_Q_COUNT                  1
#define PPFW_CPPI4x_FD_QNUM(pri)                (PAL_CPPI41_FD_Q_BASE + 11 + pri)

#if ((CNI_CPPI4x_FD_Q_COUNT + PPFW_CPPI4x_FD_Q_COUNT + \
     CPMAC_CPPI4x_FD_Q_COUNT + USB_CPPI4x_FD_Q_COUNT + \
     DMA_CPPI4x_TD_Q_COUNT  + PAL_CPPI41_FD_Q_BASE - 1 ) > PAL_CPPI41_FD_Q_LAST)
#error  "Allocated Free descriptor queues are more than Hardware defined"
#endif

/* Accumulator Configuration */
#define CPMAC_ACC_RX_CHNUM                    6
#define CPMAC_ACC_RX_INTV                     2
#define CPMAC_ACC_TXCMPL_CHNUM                7
#define CPMAC_ACC_TXCMPL_INTV                 3

#define USB_ACC_TXCMPL_CHNUM		      8
#define USB_ACC_TXCMPL_INTV	              4
#define USB_EP0_ACC_RX_CHNUM		      2
#define USB_EP1_ACC_RX_CHNUM		      3
#define USB_EP2_ACC_RX_CHNUM		      4
#define USB_EP3_ACC_RX_CHNUM		      5
#define USB_ALL_ACC_RX_INTV                   1

#define CNI_ACC_RX_CHNUM_0                   28
#define CNI_ACC_RX_CHNUM_1                   29
#define CNI_ACC_RX_CHNUM_2                   30
#define CNI_ACC_RX_INTV                      15
#define CNI_ACC_TXCMPL_CHNUM_0         	     24
#define CNI_ACC_TXCMPL_CHNUM_1         	     25
#define CNI_ACC_TXCMPL_INTV                  14

#define CNI_ACC_RX_CH_COUNT						3
#define CNI_ACC_RX_CHNUM(idx)					(CNI_ACC_RX_CHNUM_0 + idx)

#define CNI_ACC_TX_CH_COUNT						1
#define CNI_ACC_TXCMPL_CHNUM(idx)				(CNI_ACC_TXCMPL_CHNUM_0 + idx)

#define CMNG_ACC_RX_CHNUM                    14
#define CMNG_ACC_RX_INTV                     10


// <Ziv> we might want to move buf-region, port-num, to the share area 
// <Ziv> we also should consider to join all sram memory in to single location, but I'm not so sure if this is the right think to do, what do you think?
// <Hardik> Suraj will discuss with you on this.

/* --------------------------------------------------------------------- */
/*************Buffer pool allocation******************/
/* ---------------------------------------------------------------------- */     
#define BUF_POOL_MGR0                    0
#define BMGR0_POOL00_BUF_SIZE          256
#define BMGR0_POOL01_BUF_SIZE          256
#define BMGR0_POOL02_BUF_SIZE         1024
#define BMGR0_POOL03_BUF_SIZE          512
#define BMGR0_POOL04_BUF_SIZE          512
#define BMGR0_POOL05_BUF_SIZE         1024
#define BMGR0_POOL06_BUF_SIZE            0
#define BMGR0_POOL07_BUF_SIZE            0
#define BMGR0_POOL08_BUF_SIZE          256
#define BMGR0_POOL09_BUF_SIZE         1024
#define BMGR0_POOL10_BUF_SIZE          256
#define BMGR0_POOL11_BUF_SIZE         1024
#define BMGR0_POOL12_BUF_SIZE         1024
#define BMGR0_POOL13_BUF_SIZE          128

#define BMGR0_POOL00_REF_CNT             1
#define BMGR0_POOL01_REF_CNT             1
#define BMGR0_POOL02_REF_CNT             1
#define BMGR0_POOL03_REF_CNT             0
#define BMGR0_POOL04_REF_CNT             1
#define BMGR0_POOL05_REF_CNT             1
#define BMGR0_POOL06_REF_CNT             0
#define BMGR0_POOL07_REF_CNT             0
#define BMGR0_POOL08_REF_CNT             1
#define BMGR0_POOL09_REF_CNT             1
#define BMGR0_POOL10_REF_CNT             1
#define BMGR0_POOL11_REF_CNT             1
#define BMGR0_POOL12_REF_CNT             0
#define BMGR0_POOL13_REF_CNT             0

/* CPPI source port numbers - not configuration, hardware defined */
#define CPPI41_SRCPORT_USBEP0                   1
#define CPPI41_SRCPORT_USBEP1                   2
#define CPPI41_SRCPORT_USBEP2                   3
#define CPPI41_SRCPORT_USBEP3                   4
#define CPPI41_SRCPORT_CPMAC0                   5
#define CPPI41_SRCPORT_DOCSISMACPHY             6 /* only one CNI port 6 */
#define CPPI41_SRCPORT_DOCSISMACPHY1            6
#define CPPI41_SRCPORT_DOCSISMACPHY2            6
#define CPPI41_SRCPORT_DOCSISMACPHY3            6
#define CPPI41_SRCPORT_DOCSISMACPHY_HIGHPRIO    6
#define CPPI41_SRCPORT_DOCSISMACPHY_MGMT        6
#define CPPI41_SRCPORT_DOCSISUSCOP              0

/* +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
/************Packet processor Resource Allocation **************/
/* +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */

#define PAL_CPPI41_APDSP_IP_Q_BASE             56
#define PAL_CPPI41_APDSP_IP_Q_LAST             63

#define PAL_CPPI41_QPDSP_IP_Q_BASE             48
#define PAL_CPPI41_QPDSP_IP_Q_LAST             55

#define HOST2PP_INFRA_CHN(pri)                  (PAL_CPPI41_HOST2SR_DMA_CHN_NUM_BASE + pri)
#define HOST2P_INFRA_RX_QMGR                    0
#define HOST2PP_INFRA_RX_Q(pri)                 (PAL_CPPI41_HOST2SR_TX_Q_BASE + (4 * pri))

#define PPFW_CPPI4x_QMGR                        0
#define PPFW_CPPI4x_RX_INGRESS_Q_COUNT          4
#define PPFW_CPPI4x_RX_INGRESS_QNUM(pri)        (PAL_CPPI41_APDSP_IP_Q_BASE + PPFW_CPPI4x_RX_INGRESS_Q_COUNT - 1 - pri) 

#define PPFW_CPPI4x_RX_HOST_QMGR                0
#define PPFW_CPPI4x_RX_HOST_Q_COUNT             4
#define PPFW_CPPI4x_RX_HOST_QNUM(pri)           (PAL_CPPI41_QPDSP_IP_Q_BASE + 2*(PPFW_CPPI4x_RX_HOST_Q_COUNT - 1 - pri)) 

/* PP-FW parameters */
#define PPFW_CPPI4x_BD_BASE                     0x03110040
#define PPFW_CPPI4x_BD_SIZE                      64
#define PPFW_CPPI4x_BUF_BD_BASE                 0x03167000
#define PPFW_CPPI4x_BUF_BD_SIZE                 128
#define PPFW_CPPI4x_APDSP_FD_QMGR                 0
#define PPFW_CPPI4x_APDSP_FD_QNUM               120
#define PPFW_CPPI4x_APDSP_PREBUF_QMGR             0
#define PPFW_CPPI4x_APDSP_PREBUF_QNUM            63


/* +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
/******************CPMAC Driver Resource Allocation **********************/
/* +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
/* Total 2 queues 2 used */
#define PAL_CPPI41_MAC_TX_Q_BASE                212
#define PAL_CPPI41_MAC_TX_Q_LAST                213

#define PAL_CPPI41_ETH_TO_HOST_PRXY_BASE        230
#define PAL_CPPI41_ETH_TO_HOST_PRXY_LAST        231

#define PAL_CPPI41_ETH_DMA_CHN_NUM_BASE           8
#define PAL_CPPI41_ETH_DMA_CHN_NUM_LAST           8

#define PAL_CPPI41_ETH2HOST_DMA_CHN_NUM_BASE     21
#define PAL_CPPI41_ETH2HOST_DMA_CHN_NUM_LAST     21

#define CPMAC_CPPI4x_TX_QMGR                      0
#define CPMAC_CPPI4x_TX_Q_BASE                    (PAL_CPPI41_MAC_TX_Q_BASE)
#define CPMAC_CPPI4x_TX_Q_COUNT                   2
#define CPMAC_CPPI4x_TX_QNUM(pri)                 (CPMAC_CPPI4x_TX_Q_BASE + pri)

#define CPMAC_CPPI4x_ETH_TO_HOST_PRXY_QMGR        0
#define CPMAC_CPPI4x_ETH_TO_HOST_PRXY_Q_BASE      PAL_CPPI41_ETH_TO_HOST_PRXY_BASE
#define CPMAC_CPPI4x_ETH_TO_HOST_PRXY_Q_COUNT     2
#define CPMAC_CPPI4x_ETH_TO_HOST_PRXY_QNUM(pri)   (CPMAC_CPPI4x_ETH_TO_HOST_PRXY_Q_BASE + pri) 


#define CPMAC_CPPI4x_RX_DMA_CH_COUNT              1
#define CPMAC_CPPI4x_RX_DMA_CHNUM(idx)            (PAL_CPPI41_ETH_DMA_CHN_NUM_BASE + idx)
#define CPMAC_CPPI4x_RX_HOST_BD_SIZE             64 /* sizeof Cppi4HostDesc + align */
#define CPMAC_CPPI4x_TX_DMA_CH_COUNT              1
#define CPMAC_CPPI4x_TX_DMA_CHNUM(idx)            (PAL_CPPI41_ETH_DMA_CHN_NUM_BASE + idx)
#define CPMAC_CPPI4x_TX_HOST_BD_SIZE             64 /* sizeof Cppi4HostDesc + align */

#define CPMAC_CPPI4x_RX_EMB_BD_SIZE              64 /* sizeof Cppi4HostDesc + align */
#define CPMAC_CPPI4x_ETH2HOST_PROXY_CH_COUNT      1
#define CPMAC_CPPI4x_ETH2HOST_PROXY_CHNUM(idx)    (PAL_CPPI41_ETH2HOST_DMA_CHN_NUM_BASE + idx)
#define CPMAC_CPPI4x_POOL_MGR                     0
#define CPMAC_CPPI4x_POOL_COUNT                   2
#define CPMAC_CPPI4x_POOL_NUM(sltcnt)             (10 + sltcnt)


/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
/************USB Driver Resource Allocation **************/
/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
/* Total 8 queues 4 used */                             
#define PAL_CPPI41_USB_TX_Q_BASE                204
#define PAL_CPPI41_USB_TX_Q_LAST                211

#define PAL_CPPI41_USB_DMA_CHN_NUM_BASE           4
#define PAL_CPPI41_USB_DMA_CHN_NUM_LAST           7

#define PAL_CPPI41_USB2HOST_DMA_CHN_NUM_BASE     20
#define PAL_CPPI41_USB2HOST_DMA_CHN_NUM_LAST     20

#define USB_CPPI4x_TX_QMGR                        0
#define USB_CPPI4x_TX_Q_BASE                      PAL_CPPI41_USB_TX_Q_BASE
#define USB_CPPI4x_TX_Q_COUNT                     8
#define USB_CPPI4x_EP0_TX_QNUM(pri)               (USB_CPPI4x_TX_Q_BASE + pri + 0)
#define USB_CPPI4x_EP1_TX_QNUM(pri)               (USB_CPPI4x_TX_Q_BASE + pri + 2)
#define USB_CPPI4x_EP2_TX_QNUM(pri)               (USB_CPPI4x_TX_Q_BASE + pri + 4)
#define USB_CPPI4x_EP3_TX_QNUM(pri)               (USB_CPPI4x_TX_Q_BASE + pri + 6)

#define USB_CPPI4x_RX_EMB_BD_SIZE                64 /* sizeof Cppi4HostDesc + align */
#define USB_CPPI4x_USB2HOST_PROXY_CH_COUNT        1
#define USB_CPPI4x_USB2HOST_PROXY_CHNUM(idx)      (PAL_CPPI41_USB2HOST_DMA_CHN_NUM_BASE + idx)
#define USB_CPPI4x_POOL_COUNT                     2
#define USB_CPPI4x_POOL_NUM(sltcnt)               (10 + sltcnt)

#define USB_CPPI4x_USB_BD_SIZE	                 64
#define USB_MAX_EPS                               8
#define USB_CPPI4x_MAX_USB_DESC                   (64 * USB_MAX_EPS) /* number of Descriptors for all tx/rx endpoints */

/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
/************CNI and CMGR Driver Resource Allocation **************/
/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
#define PAL_CPPI41_CNI_US_Q_BASE                180
#define PAL_CPPI41_CNI_US_Q_LAST                189

#define PAL_CPPI41_DOC_US_DMA_CHN_NUM_BASE        9
#define PAL_CPPI41_DOC_US_DMA_CHN_NUM_LAST       12

#define PAL_CPPI41_DOC2HOST_DMA_CHN_NUM_BASE     16
#define PAL_CPPI41_DOC2HOST_DMA_CHN_NUM_LAST     19

#define CNI_CPPI4x_DOC2HOST_PROXY_QBASE         222 /* Input Q to Channel 16 */
#define CNI_CPPI4x_DOC2HOST_PROXY_QNUM(idx)     (CNI_CPPI4x_DOC2HOST_PROXY_QBASE + idx)

#define CNI_CPPI4x_DOC2HOST_PROXY_CH_COUNT      3
#define CNI_CPPI4x_DOC2HOST_PROXY_CHNUM(idx)    (PAL_CPPI41_DOC2HOST_DMA_CHN_NUM_BASE + idx)

#define PAL_CPPI41_CNI_DMA_CHN_NUM_BASE         9
#define CNI_CPPI4x_RX_DMA_CH_COUNT              5
#define CNI_CPPI4x_RX_DMA_CHNUM(idx)            (PAL_CPPI41_CNI_DMA_CHN_NUM_BASE + idx)

#define CMGR_CPPI4x_TX_QMGR                       0
#define CMGR_CPPI4x_TX_Q_BASE                     (PAL_CPPI41_CNI_US_Q_BASE)
#define CMGR_CPPI4x_TX_Q_COUNT                    1
#define CMGR_CPPI4x_TX_QNUM                       (CMGR_CPPI4x_TX_Q_BASE)

#define CNI_CPPI4x_TX_QMGR                        0
#define CNI_CPPI4x_TX_Q_BASE                      (PAL_CPPI41_CNI_US_Q_BASE + CMGR_CPPI4x_TX_Q_COUNT)
#define CNI_CPPI4x_TX_Q_COUNT                     9 /* 9 from Q-181, 10 from Q-180 */
#define CNI_CPPI4x_TX_QNUM(pri)                   (CNI_CPPI4x_TX_Q_BASE + pri)

#define CNI_CPPI4x_POOL_MGR                     0
#define CNI_CPPI4x_POOL_COUNT                   2
#define CNI_CPPI4x_POOL_NUM(sltcnt)             (4 + sltcnt)

#define CNI_CPPI4x_HOST_BD_SIZE                   64
#define CNI_CPPI4x_RX_EMB_BD_SIZE                 64
#define CNI_CPPI4x_S0_POOL_NUM_HI                 -1
#define CNI_CPPI4x_S1_POOL_NUM_HI                  4
#define CNI_CPPI4x_S2_POOL_NUM_HI                  4
#define CNI_CPPI4x_S3_POOL_NUM_HI                  4
#define CNI_CPPI4x_S0_POOL_NUM_MD                 -1
#define CNI_CPPI4x_S1_POOL_NUM_MD                  0
#define CNI_CPPI4x_S2_POOL_NUM_MD                  1
#define CNI_CPPI4x_S3_POOL_NUM_MD                  2
#define CNI_CPPI4x_S0_POOL_NUM_LO                  2 //-1
#define CNI_CPPI4x_S1_POOL_NUM_LO                  2 //0
#define CNI_CPPI4x_S2_POOL_NUM_LO                  2 //1
#define CNI_CPPI4x_S3_POOL_NUM_LO                  2 //2
#define CNI_CPPI4x_RX_NUM_OF_CHN                   3

#define CMGR_CPPI4x_RX_HOST_BD_SIZE               64
#define CMGR_CPPI4x_TX_HOST_BD_SIZE               64                                 
#define CMGR_CPPI4x_RX_POOLING_QMGR                0
#define CMGR_CPPI4x_RX_POOLING_QNUM              195

#define DMAC_US_BD_SIZE                     	 128

/* ---------------------------------------------------------------- */
/********************** Shared resources parameters *****************/
/* ---------------------------------------------------------------- */

/* DOCSIS MAC parameters */
#define DMAC_US_BD_BASE                     0x01119000
#define DMAC_US_BD_SIZE                     128

/* ---------------------------------------------------------------- */
/*********************** Ports private parameters *******************/
/* ---------------------------------------------------------------- */

/* USB legacy parameters  
 *  This will be removed once PSP team integrates USB driver
 *   with this header file.
 */

#define  USB_CPPI41_TX1_QMGR                0
#define USB_CPPI41_TX1_QNUM                 USB_CPPI4x_EP0_TX_QNUM(0)

#define USB_CPPI41_TX2_QMGR                 0    
#define USB_CPPI41_TX2_QNUM                 USB_CPPI4x_EP1_TX_QNUM(0)
#define USB_CPPI41_TX3_QMGR                 0                            
#define USB_CPPI41_TX3_QNUM                 USB_CPPI4x_EP2_TX_QNUM(0)
#define USB_CPPI41_TX4_QMGR                 0                            
#define USB_CPPI41_TX4_QNUM                 USB_CPPI4x_EP3_TX_QNUM(0)

#define USB_CPPI41_TXCMPL_QMGR              USB_CPPI4x_TX_COMP_QMGR
#define USB_CPPI41_TXCMPL_QNUM              USB_CPPI4x_TX_COMP_QNUM(0)
                                                   
#define USB_CPPI41_RX1_QMGR                 0
#define USB_CPPI41_RX1_QNUM                 USB_CPPI4x_EP0_RX_QNUM(0)
#define USB_CPPI41_RX2_QMGR                 0                            
#define USB_CPPI41_RX2_QNUM                 USB_CPPI4x_EP1_RX_QNUM(0)
#define USB_CPPI41_RX3_QMGR                 0                            
#define USB_CPPI41_RX3_QNUM                 USB_CPPI4x_EP2_RX_QNUM(0)
#define USB_CPPI41_RX4_QMGR                 0                            
#define USB_CPPI41_RX4_QNUM                 USB_CPPI4x_EP3_RX_QNUM(0)
       
#define USB_CPPI41_RX1_FDB_QMGR             0
#define USB_CPPI41_RX1_FDB_QNUM             USB_CPPI4x_EP0_FBD_QNUM(0)
#define USB_CPPI41_RX2_FDB_QMGR             0 
#define USB_CPPI41_RX2_FDB_QNUM             USB_CPPI4x_EP1_FBD_QNUM(0)
#define USB_CPPI41_RX3_FDB_QMGR		    0
#define USB_CPPI41_RX3_FDB_QNUM		    USB_CPPI4x_EP2_FBD_QNUM(0)
#define USB_CPPI41_RX4_FDB_QMGR		    0
#define USB_CPPI41_RX4_FDB_QNUM		    USB_CPPI4x_EP3_FBD_QNUM(0)
#define CPPI41_USB_BD_SIZE	            USB_CPPI4x_USB_BD_SIZE
#define CPPI41_MAX_USB_EPS                  8
#define CPPI41_MAX_USB_DESC                 (64 * CPPI41_MAX_USB_EPS) /* number of Descriptors for all tx/rx endpoints */
#define USB_ACC_RX1_CHNUM		    USB_EP0_ACC_RX_CHNUM
#define USB_ACC_RX2_CHNUM		    USB_EP1_ACC_RX_CHNUM
#define USB_ACC_RX3_CHNUM		    USB_EP2_ACC_RX_CHNUM
#define USB_ACC_RX4_CHNUM		    USB_EP3_ACC_RX_CHNUM
#define USB_ACCTX_INTD_VEC		    USB_ACC_TXCMPL_INTV
#define USB_ACCRX_INTD_VEC		    USB_ALL_ACC_RX_INTV
#define USB_ACC_TX_CHNUM                    USB_ACC_TXCMPL_CHNUM
