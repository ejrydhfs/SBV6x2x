/*
 *
 * psp_trace.c 
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


/** \file   psp_trace.c
    \brief  PSP tracer C file

    This file contains implementation for trace service:

    \author     PSP Team
    \version    1.0
 */
#include <linux/config.h>
#include <linux/kernel.h> 
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/init.h>
#include <linux/errno.h>    /* Miscellaneous error codes */
#include <linux/interrupt.h>

#include <asm/mach-avalanche/generic/pal.h>

#include <linux/psp_trace.h>
#include <asm/uaccess.h>

/** \brief TRACE_GET_TIMESTAMP - assigns a value of the CPU's count register
		to a tmp variable.Resolution of the count register is a half of CPU frequency.
*/ 
#define TRACE_GET_TIMESTAMP(tmp) __asm__ __volatile__ ( "mfc0 %0,$9"  : "=r" (tmp))

/** \brief TRACE_MAX_EVENT_NAME_LENGTH - defines a maximum length of the event's 
		description.
*/ 
#define TRACE_MAX_EVENT_NAME_LENGTH 30

#define GET_EVENT(event_id)	((event_id) & 0xFFFF)
#define GET_GROUP(event_id)	((event_id >> 16) & 0xFFFF)


/** \brief trace_entry_t - defines the content of the single line in the trace buffer.
*/ 
typedef struct {
      unsigned int event_id;	/* trace entry id as defined in enums in h file */
      unsigned long ts; 		/* timestamp of the event*/
	  int param;					
	  int sec_param;
} trace_entry_t;
 
/** \brief g_position - defines the current position in the trace buffer.
		it doesn't get zeroed at the end of the buffer, so in fact current position
		should be calculated :  g_position % TRACE_NUM_ENTRIES
*/ 
static unsigned int g_position = 0;

/** \brief trace_control_t - tracer's control block.
*/ 
typedef struct {
	  int init_done; 
	  int enable;
} trace_control_t;
 
trace_control_t trace_control = {False,True}; /* init_done = False, enable = True */
 
/** \brief trace_entry - buffer to the trace lines..
*/ 
trace_entry_t   trace_entry[TRACE_NUM_ENTRIES];

/** \brief event_entry_t - defines the event entity.
*/ 
typedef struct {
	  event_state state;
      unsigned int event_id;/* trace entry identification */
      char* event_name; 		/* printed string for this event */
} event_entry_t;

 
typedef struct {
	event_state state;
	event_entry_t* event_list;
	char* group_name; 		/* printed string for this event */
} group_entry_t;

/* dummy event id to indicate end of the group */
#define TRACE_EMPTY_EVENT_ID (-1)

/** \brief those marks might be used for post processing tools.
I am using "<-" symbol to mark enter to the function and "->" symbol to mark exit for
main EXCEL sheet.
*/ 
#define ENTER_MARK " <-"
#define EXIT_MARK " ->"
#define EMPTY_STRING ""

/** \brief REG_NEW_EVENT - add new line to the event container.Event description set to be 
the same as event id.
	ev - event id; For example GNR_EXAMPLE_EVENT
	state - event state;	Event_Enable\Event_Disable
	add_str - string to add to event description.

	REG_NEW_EVENT(GNR_EXAMPLE_EVENT,Event_Enable,"_1") will expend to the line 
					{Event_Enable,GNR_EXAMPLE_EVENT,"GNR_EXAMPLE_EVENT_1"}

*/
#define REG_NEW_EVENT(ev,state,add_str)  {state,ev,#ev add_str}

/** \brief REG_EVENT - add new line to the event container, with event description same as 
event id.
	ev - event id; For example GNR_EXAMPLE_EVENT
	state - event state;	Event_Enable\Event_Disable

	REG_EVENT(GNR_EXAMPLE_EVENT,Event_Enable) will expend to the line 
					{Event_Enable,GNR_EXAMPLE_EVENT,"GNR_EXAMPLE_EVENT"}
*/ 
#define REG_EVENT(ev,state) REG_NEW_EVENT(ev,state,EMPTY_STRING)

/** \brief REG_ENTER_EVENT,REG_EXIT_EVENT - those macros add special marks 
to the events description, to be recognized by off line post processing tools
	ev - event id; For example GNR_EXAMPLE_EVENT
	state - event state;	Event_Enable\Event_Disable

	REG_ENTER_EVENT(GNR_EXAMPLE_EVENT,Event_Enable) will expend to the line 
					{Event_Enable,GNR_EXAMPLE_EVENT,"GNR_EXAMPLE_EVENT <-"}

	REG_EXIT_EVENT(GNR_EXAMPLE_EVENT,Event_Enable) will expend to the line 
					{Event_Enable,GNR_EXAMPLE_EVENT,"GNR_EXAMPLE_EVENT ->"}
*/ 
#define REG_ENTER_EVENT(ev,state) REG_NEW_EVENT(ev,state,ENTER_MARK)
#define REG_EXIT_EVENT(ev,state) REG_NEW_EVENT(ev,state,EXIT_MARK)


/** \brief REG_EVENT_FUNCTION_TRACE - add two new lines to the event container.
	ev - event id; For example GNR_EXAMPLE_EVENT
	state - event state;	Event_Enable\Event_Disable

	REG_ENTER_EVENT(GNR_EXAMPLE_EVENT,Event_Enable) will expend to the line 
					{Event_Enable,GNR_EXAMPLE_EVENT_ENTER,"GNR_EXAMPLE_EVENT_ENTER <-"},
					{Event_Enable,GNR_EXAMPLE_EVENT_EXIT,"GNR_EXAMPLE_EVENT_EXIT ->"},
*/ 
#define REG_EVENT_FUNCTION_TRACE(ev,state) REG_ENTER_EVENT(ev##_ENTER,state),REG_EXIT_EVENT(ev##_EXIT,state)

#define REG_LAST_EVENT() REG_EVENT(TRACE_EMPTY_EVENT_ID,Event_Disable)

/** \brief REG_GROUP - add new line to the group container.
*/ 
#define REG_GROUP(gr,list,state) {state,list,#gr,}
#define REG_EMPTY_GROUP() REG_GROUP(TRACER_GENERAL_GROUP,NULL,Event_Disable)

/********************************************/
/* Events list for general purposes 			*/
/********************************************/
event_entry_t general_group_events[]=
{
/*------------------------------------------------------------------------**
** 	 event_state           event_id			event_description as it will  **
** 	 														appear_in the trace print  **
**                                                                        **
**------------------------------------------------------------------------*/
	{Event_Enable,		GNR_EXAMPLE_EVENT,		"just for example"},

	REG_LAST_EVENT() /* don't remove this line and don't add any events after it */
};
/********************************************/
/* Events list for atm driver		 			*/
/********************************************/
event_entry_t atm_group_events[]=
{
	REG_EVENT_FUNCTION_TRACE(ATM_DRV_SAR_ISR,Event_Enable),
	REG_EVENT_FUNCTION_TRACE(ATM_DRV_TASKLET,Event_Enable),
	REG_EVENT_FUNCTION_TRACE(ATM_DRV_PKT_TX,Event_Enable),
	REG_EVENT_FUNCTION_TRACE(ATM_DRV_PKT_TX_COMPLETE,Event_Enable),
	REG_EVENT_FUNCTION_TRACE(ATM_DRV_TX_COMPLETE,Event_Enable),
	REG_EVENT_FUNCTION_TRACE(ATM_DRV_RX,Event_Enable),
	REG_EVENT_FUNCTION_TRACE(ATM_DRV_PKT_RX,Event_Enable),
    REG_EVENT_FUNCTION_TRACE(ATM_DRV_DSL_ISR, Event_Enable),

	REG_LAST_EVENT() /* don't remove this line and don't add any events after it */
};

/********************************************/
/* Events list for ethernet driver		 		*/
/********************************************/
/* Should be extended to support two instances !!! */
event_entry_t eth_group_events[]=
{
#ifdef CONFIG_MACH_PUMA5
	REG_EVENT_FUNCTION_TRACE(ETH_DRV_TX_ISR,Event_Enable),
	REG_EVENT_FUNCTION_TRACE(ETH_DRV_RX_ISR,Event_Enable),
	REG_EVENT_FUNCTION_TRACE(ETH_DRV_TX_TASKLET,Event_Enable),
	REG_EVENT_FUNCTION_TRACE(ETH_DRV_RX_TASKLET,Event_Enable),
#else
	REG_EVENT_FUNCTION_TRACE(ETH_DRV_ISR,Event_Enable),
	REG_EVENT_FUNCTION_TRACE(ETH_DRV_TASKLET,Event_Enable),
#endif	
	REG_EVENT_FUNCTION_TRACE(ETH_DRV_TMR,Event_Disable), /**< This periodic event overflows the trace buffer*/
	REG_EVENT_FUNCTION_TRACE(ETH_DRV_PKT_TX,Event_Enable),
	REG_EVENT_FUNCTION_TRACE(ETH_DRV_PKT_TX_COMPLETE,Event_Enable),
	REG_EVENT_FUNCTION_TRACE(ETH_DRV_TX_COMPLETE,Event_Enable),
	REG_EVENT_FUNCTION_TRACE(ETH_DRV_RX,Event_Enable),
	REG_EVENT_FUNCTION_TRACE(ETH_DRV_PKT_RX,Event_Enable),

	REG_LAST_EVENT() /* don't remove this line and don't add any events after it */
};

/********************************************/
/* Events list for USB driver			 		*/
/********************************************/
event_entry_t usb_group_events[]=
{
	REG_EVENT_FUNCTION_TRACE(USB_DRV_ISR,Event_Enable),
	REG_EVENT_FUNCTION_TRACE(USB_DRV_TASKLET,Event_Enable),
	REG_EVENT_FUNCTION_TRACE(USB_DRV_TMR,Event_Enable),
	REG_EVENT_FUNCTION_TRACE(USB_DRV_PKT_TX,Event_Enable),
	REG_EVENT_FUNCTION_TRACE(USB_DRV_PKT_TX_COMPLETE,Event_Enable),
	REG_EVENT_FUNCTION_TRACE(USB_DRV_TX_COMPLETE,Event_Enable),
	REG_EVENT_FUNCTION_TRACE(USB_DRV_RX,Event_Enable),
	REG_EVENT_FUNCTION_TRACE(USB_DRV_PKT_RX,Event_Enable),
	REG_EVENT_FUNCTION_TRACE(USB_DRV_TX,Event_Enable),

	REG_LAST_EVENT() /* don't remove this line and don't add any events after it */
};

/********************************************/
/* Events list for ip stack				 		*/
/********************************************/
event_entry_t ip_group_events[]=
{
/*------------------------------------------------------------------------**
** 	 event_state           event_id			event_description as it will  **
** 	 								        appear_in the trace print     **
**                                                                        **
**------------------------------------------------------------------------*/
	REG_EVENT_FUNCTION_TRACE(IP_NET_PROTOCOL,           Event_Enable),
	REG_EVENT_FUNCTION_TRACE(IP_PRE_CONNTRACK,          Event_Enable),
	REG_EVENT_FUNCTION_TRACE(IP_POST_CONNTRACK,         Event_Enable),
	REG_EVENT_FUNCTION_TRACE(IP_PRE_NAT,                Event_Enable),
	REG_EVENT_FUNCTION_TRACE(IP_POST_NAT,               Event_Enable),
	REG_EVENT_FUNCTION_TRACE(IP_MANGLE,                 Event_Enable),
	REG_EVENT_FUNCTION_TRACE(IP_FILTER,                 Event_Enable),

	REG_LAST_EVENT() /* don't remove this line and don't add any events after it */
};

/********************************************/
/* Events list for the bridge			 		*/
/********************************************/
event_entry_t bridge_group_events[]=
{
	REG_EVENT_FUNCTION_TRACE(BR_HANDLE_FRAME,           Event_Enable),
	REG_EVENT_FUNCTION_TRACE(BR_DEV_XMIT,               Event_Enable),

	REG_LAST_EVENT() /* don't remove this line and don't add any events after it */
};

/********************************************/
/* Events list for wlan driver 					*/
/********************************************/

event_entry_t wlan_group_events[]=
{
/*------------------------------------------------------------------------**
** 	 event_state           event_id			event_description as it will  **
** 	 														appear_in the trace print  **
**                                                                        **
**------------------------------------------------------------------------*/
	REG_EVENT_FUNCTION_TRACE(AP_DRV_ISR,             Event_Enable),
	REG_EVENT_FUNCTION_TRACE(AP_DRV_TASKLET,         Event_Enable),
	REG_EVENT_FUNCTION_TRACE(AP_DRV_TMR,             Event_Enable),
	REG_EVENT_FUNCTION_TRACE(AP_DRV_PKT_TX,          Event_Enable),
	REG_EVENT_FUNCTION_TRACE(AP_DRV_PKT_TX_COMPLETE, Event_Enable),
	REG_EVENT_FUNCTION_TRACE(AP_DRV_TX_COMPLETE,     Event_Enable),
	REG_EVENT_FUNCTION_TRACE(AP_DRV_RX,              Event_Enable),
	REG_EVENT_FUNCTION_TRACE(AP_DRV_PKT_RX,          Event_Enable),

	REG_LAST_EVENT() /* don't remove this line and don't add any events after it */
};

/********************************************/
/* Events list for voice application 			*/
/********************************************/

event_entry_t voip_group_events[]=
{
/*------------------------------------------------------------------------**
** 	 event_state           event_id			event_description as it will  **
** 	 														appear_in the trace print  **
**                                                                        **
**------------------------------------------------------------------------*/
	/* {Event_Enable,VOIP_EXAMPLE_EVENT,"VOIP_EXAMPLE_EVENT"} */
	REG_EVENT(VOIP_EXAMPLE_EVENT, Event_Enable), 
	/* {Event_Enable,VOIP_1_EXAMPLE_EVENT_ENTER,"VOIP_1_EXAMPLE_EVENT_ENTER <-"} */
	/* {Event_Enable,VOIP_1_EXAMPLE_EVENT_EXIT,"VOIP_1_EXAMPLE_EVENT_ENTER ->"} */
	REG_EVENT_FUNCTION_TRACE(VOIP_1_EXAMPLE_EVENT,Event_Enable),
	/* Implicit declaration */
	{Event_Enable,VOIP_2_EXAMPLE_EVENT,"Any description"},

	REG_LAST_EVENT() /* don't remove this line and don't add any events after it */
};

/********************************************/
/* Events list for the QoS Framework        */
/********************************************/
event_entry_t qos_group_events[]=
{
/*------------------------------------------------------------------------**
**   event_state           event_id         event_description as it will  **
**                                          appear_in the trace print     **
**                                                                        **
**------------------------------------------------------------------------*/
    REG_EVENT_FUNCTION_TRACE(NSP_QOS_ENQUEUE,       Event_Enable),
    REG_EVENT_FUNCTION_TRACE(NSP_QOS_DEQUEUE,       Event_Enable),
    REG_EVENT_FUNCTION_TRACE(NSP_QOS_CONGESTION,    Event_Enable),
    REG_EVENT_FUNCTION_TRACE(NSP_QOS_PIL_ETH,       Event_Enable),
    REG_EVENT_FUNCTION_TRACE(NSP_QOS_PIL_IP,        Event_Enable),
    REG_EVENT_FUNCTION_TRACE(NSP_QOS_PIL_WLAN,      Event_Enable),
    REG_EVENT_FUNCTION_TRACE(NSP_QOS_TU,            Event_Enable),
    REG_EVENT_FUNCTION_TRACE(NSP_QOS_SHAPER,        Event_Enable),

    REG_LAST_EVENT() /* don't remove this line and don't add any events after it */
};

/********************************************/
/* User defined events 							*/
/********************************************/
event_entry_t user_group_events[MAX_USER_DEFINED_EVENTS + 1]; /**< Need one more line for "last" event*/ 
int num_user_events = 0;

group_entry_t groups_container[TRACER_GROUPS_NUMBER] = {
	REG_GROUP(TRACER_GENERAL_GROUP,general_group_events,Event_Enable),
	REG_GROUP(TRACER_ATM_DRIVER_GROUP,atm_group_events,Event_Enable),
	REG_GROUP(TRACER_ETH_DRIVER_GROUP,eth_group_events,Event_Enable),
	REG_GROUP(TRACER_USB_DRIVER_GROUP,usb_group_events,Event_Enable),
	REG_GROUP(TRACER_BRIDGE_GROUP,bridge_group_events,Event_Enable),
	REG_GROUP(TRACER_IP_STACK_GROUP,ip_group_events,Event_Enable),
	REG_GROUP(TRACER_WLAN_DRIVER_GROUP,wlan_group_events,Event_Enable),
	REG_GROUP(TRACER_VOICE_APP_GROUP,voip_group_events,Event_Enable),
    REG_GROUP(TRACER_QOS_GROUP,qos_group_events,Event_Enable),
	REG_GROUP(TRACER_USER_DEFINED_GROUP,user_group_events,Event_Enable),
};
/* ---------------------------------------------------------------------------- */
void trace_exit(void);

int trace_proc_print(char* page, char **start, off_t off, int count,int *eof, void *data);
int trace_ctl_read(char* page, char **start, off_t off, int count,int *eof, void *data);
int trace_ctl_write (struct file *fp, const char * buf, unsigned long count, void * data);
int trace_event_list_print(char* page, char **start, off_t off, int count,int *eof, void *data);

/* Proc entries */
static struct proc_dir_entry *trace_ctl = NULL;

int __init trace_init(void)
{
	if(trace_control.init_done == True) 
	{
			printk("!!!!!!! Trace is already up !!!!!!!!!\n");
			return -1;
	}

	create_proc_read_entry("avalanche/trace",0,NULL,trace_proc_print,NULL);
	create_proc_read_entry("avalanche/trace_events",0,NULL,trace_event_list_print,NULL);

    trace_ctl = create_proc_entry("avalanche/trace_ctl", 0644, NULL);
    if(trace_ctl)
    {
        trace_ctl->read_proc  = trace_ctl_read;
        trace_ctl->write_proc = trace_ctl_write;
    }

	/* Add last event to user group to indicate the end */
	user_group_events[num_user_events].event_id = TRACE_EMPTY_EVENT_ID;

	trace_control.init_done = True;
	return 0;
}

void trace_exit(void) {
  
	trace_control.init_done = False;
 
}
/**
	\brief  	Main trace function.Stores a trace line in the trace buffer.
	\warning	This function disables interrupts for ~5 instructions.

	\param  	event_id   [IN]	Traced event ID as declared above : GNR_EXAMPLE_EVENT
	\param  	par    	  [IN]	General purpose parameter
	\param  	sec_par    [IN]	General purpose parameter
 */
void trace_event(int event_id,int param,int sec_param)
{   
	unsigned int pos,tmp;
	unsigned long flags;

	if(trace_control.enable == False)
		return;

	{
		register group_entry_t group = groups_container[GET_GROUP(event_id)];
		if( group.state == Event_Disable )
			return;

		if( group.event_list[GET_EVENT(event_id)].state == Event_Disable)
			return;
	}

	TRACE_GET_TIMESTAMP(tmp);

	save_and_cli(flags);

/* 	only position allocation need to be done inside the critical section */
/* 	all the rest can be done later                                       */
	pos = g_position % TRACE_NUM_ENTRIES;
	g_position++;
	
	restore_flags(flags);

	{
		trace_entry_t*  entry = &trace_entry[pos];

		entry->ts = tmp;
		entry->event_id= event_id;
		entry->param= param;
		entry->sec_param= sec_param;
	}
}
 
void trace_disable(void) {
	trace_control.enable = False;
}
void trace_enable(void) {
	trace_control.enable = True;
}

const char end_string[] =   "\n------------------ END OF TRACE BUFFER ------------------------\n\n"  ;

int trace_proc_print(char* page, char **start, off_t off, int count,int *eof, void *data)
{
	signed int len = 0,size ;
	static int new_count;
	static int cur_pos,done,state;

	if (off == 0) 
	{
		/* remember tracer's current state */
		state = trace_control.enable;
		trace_control.enable = False;
		len += sprintf(page+len, "\n------------------ TRACE BUFFER, pos = %d  --------------------\n\n", g_position); 
		new_count=0;
		cur_pos = g_position < TRACE_NUM_ENTRIES ? 0 : g_position % TRACE_NUM_ENTRIES;
		done = 0;
	}

	size = g_position < TRACE_NUM_ENTRIES ? g_position : TRACE_NUM_ENTRIES;

 	
 	if ((new_count >= size) && (done == 1)) 
	{
		g_position = 0;

		/* restore tracer's state */
		trace_control.enable = state;

		*eof = 1;
		return 0;
	}

	while( (len < ((signed int)(count - 80 - sizeof(end_string))))  && ( new_count < size) )
	{
		int event_id = trace_entry[cur_pos].event_id;
		group_entry_t group = groups_container[GET_GROUP(event_id)];
		event_entry_t event = group.event_list[GET_EVENT(event_id)];

		if( event.event_id == event_id )
			len += sprintf(page+len, "%s %lu %x %x\n", 
							event.event_name,
							trace_entry[cur_pos].ts,
							trace_entry[cur_pos].param, trace_entry[cur_pos].sec_param);
		else
			len += sprintf(page+len,"Inconsistante event id %x %x \n",event_id,event.event_id);

		cur_pos = ( cur_pos + 1) % TRACE_NUM_ENTRIES;
		new_count++;
	}
	
	if( new_count >= size && (len < (count - 80 - sizeof(end_string))) )
	{
		len += sprintf(page+len, end_string); 
		done=1;
	}

	*eof = 0;
	*start = page;

	return len;
}
/**
	\brief trace_print_2_scr - prints a content of the trace buffer on the screen.
			Migth be used in case that proc file system is unavailable.
			Call to this function might be added to the oops handler.
 */

void trace_proc_print_2_scr(void)
{
int i,state;

	/* remember tracer's current state */
	state = trace_control.enable;
	trace_control.enable = False;

	printk("\n------------------ TRACE BUFFER, pos = %d  --------------------\n\n", g_position); 


	if( g_position >= TRACE_NUM_ENTRIES )
	{
		for(i= g_position % TRACE_NUM_ENTRIES ; i < TRACE_NUM_ENTRIES; i++)
		{
				int event_id = trace_entry[i].event_id;
				group_entry_t group = groups_container[GET_GROUP(event_id)];
				event_entry_t event = group.event_list[GET_EVENT(event_id)];

				printk("%d %s %x %x %lu\n",i, 
							event.event_name,trace_entry[i].param,
							trace_entry[i].sec_param,trace_entry[i].ts);
		}
	}
			
	for(i = 0; i < ( g_position % TRACE_NUM_ENTRIES ); i++)
	{
			int event_id = trace_entry[i].event_id;
			group_entry_t group = groups_container[GET_GROUP(event_id)];
			event_entry_t event = group.event_list[GET_EVENT(event_id)];
			
			printk("%d %s %x %x %lu\n",i, 
							event.event_name,trace_entry[i].param,
							trace_entry[i].sec_param,trace_entry[i].ts);
	}
	
	printk("\n------------------------ END OF TRACE BUFFER -------------------------\n\n"); 

	/* restore tracer's state */
	trace_control.enable = state;

	return;
}

int trace_ctl_read(char* page, char **start, off_t off, int count,int *eof, void *data)
{
    int len = 0;

	if(trace_control.enable == True)
       len+= sprintf(page+len, "\n Trace enable\n");
	else
       len+= sprintf(page+len, "\n Trace disable\n");

	return(len);
		
}
int trace_ctl_write (struct file *fp, const char * buf, unsigned long count, void * data)
{
    char local_buf[31];
    int  ret_val = 0;
	char cmd[30];
	unsigned int id;
	unsigned int state;
	int tkn_number;

    if(count > 30)
    {
        printk("Error : Buffer Overflow\n");
		printk("Use echo -help > trace_ctl\" to get usage options \n");
        return -EFAULT;
    }

    copy_from_user(local_buf,buf,count);
    local_buf[count-1]='\0'; /* Ignoring last \n char */
    ret_val = count;
    
	tkn_number = sscanf(local_buf,"%s %x %d",cmd,&id,&state);

	switch (tkn_number)
	{
		case 1:
			if(strcmp("0",cmd)==0)
			{
				printk("\n Trace disable\n");
				trace_disable();
				return ret_val;
			}
			if(strcmp("1",cmd)==0)
			{
				printk("\n Trace enable\n");
				trace_enable();
				return ret_val;
			}
			break;
		case 3:
			if(strcmp("-g",cmd)==0)
			{
				if((id > TRACER_GROUPS_NUMBER) || (state > 1) || (state < 0))
					break;
				groups_container[id].state = state;
				printk("\n Changing group %s state to %d\n",
						groups_container[id].group_name,groups_container[id].state);
				return ret_val;
			}
			if(strcmp("-ev",cmd)==0)
			{
				int group = GET_GROUP(id);
				int event = GET_EVENT(id);
				
				printk("gr %d ev %d\n",group,event);
				if( (group > TRACER_GROUPS_NUMBER) )
					break;

				if((state > 1) || (state < 0))
					break;
				((groups_container[group]).event_list[event]).state = state;

				printk("\n Changing event %s state to %d\n",
						((groups_container[group]).event_list[event]).event_name,state);
				return ret_val;
			}
		default:
			break;
	}

	printk("Use echo 0\1 > trace_ctl\" to disable/enable the trace\n");
	printk("Use echo \"-g group_id 0\1\" > trace_ctl\" to disable/enable group\n");
	printk("Example echo \"-g 1 1\" > trace_ctl\" to enable group 1\n");
	printk("Changing group state doesn't change events state in this group \n");
	printk("Use echo \"-ev event_id 0\1\" > trace_ctl\" to disable/enable event\n");
	printk("Example echo \"-ev 10000 0\" > trace_ctl\" to disable event 0x10000\n");
	printk("Use cat trace_events to get available events list\n");

	return -EFAULT;
}
int trace_event_list_print(char* page, char **start, off_t off, int count,int *eof, void *data)
{
    int len = 0;
	static int group_id=0;
	static int event_id=0;
	group_entry_t group;
	event_entry_t event; 
	
	if (off == 0) 
	{
		group_id=0;
		event_id=0;
	}

 	if ((group_id >= TRACER_GROUPS_NUMBER) || (groups_container[group_id].event_list == NULL)) 
	{
		*eof = 1;
		return 0;
	}

	group = groups_container[group_id];
	event = group.event_list[event_id];
		
	len += sprintf(page+len,"Group name %s | group id %x | group state %d | group events are: \n",
																			group.group_name,group_id,group.state);

	while( (len < (count - 80)) && (event.event_id != TRACE_EMPTY_EVENT_ID) )
	{
		len += sprintf(page+len,"Event:%s id=%x state %d\n",event.event_name,event.event_id,event.state);
		event_id++;
		event = group.event_list[event_id];
	}

	if(event.event_id == TRACE_EMPTY_EVENT_ID)
	{
		group_id++;
		event_id=0;
	}


	*eof = 0;
	*start = page;
	return len;
}

/**
	\brief trace_register_new_event - Creates and adds a new event to the TRACER_USER_DEFINED_GROUP group.
			Allows the user to add new events without rebuild the kernel.
			Number of dynamic events limited by MAX_USER_DEFINED_EVENTS
	\param event_name [IN] string decsribing event's name 
	\param state      [IN] event's default state (Event_Enable/Event_Disable)

	\return	event id of the new event if succesfull;
				-EEVENTNAME or -EMAXEV if fails 

	\warning	This function has critical section inside.
 */
int trace_register_new_event(char* event_name,event_state state)
{
	unsigned long flags;
	event_entry_t* event;
	char* name;
 
	if(!event_name)
		return -EEVENTNAME;

	if( num_user_events >= MAX_USER_DEFINED_EVENTS )
		return -EMAXEV;
		
	if((name = (char *) kmalloc(TRACE_MAX_EVENT_NAME_LENGTH, GFP_ATOMIC)) == NULL)
		return -ENOMEM;

	/* 	disable interrupts to avoid critical section */
	save_and_cli(flags);

/* 	only position allocation need to be done inside the critical section */
/* 	all the rest can be done later                                       */
	event = &user_group_events[num_user_events];

	event->event_id = BUILD_EVENT_ID(TRACER_USER_DEFINED_GROUP,num_user_events);
	event->event_name = name;
	event->state = state;

	num_user_events++;
	
	/* TRACE_EMPTY_EVENT_ID must be the last event in the group */
	user_group_events[num_user_events].event_id = TRACE_EMPTY_EVENT_ID;

	/* 	enable interrupts  */
	restore_flags(flags);

	strncpy(event->event_name, event_name, TRACE_MAX_EVENT_NAME_LENGTH );
	event->event_name[TRACE_MAX_EVENT_NAME_LENGTH-1] = '\0';
	
	return event->event_id;	

}
//EXPORT_SYMBOL_NOVERS(trace_event);
 
module_init(trace_init);
EXPORT_SYMBOL(trace_event);
EXPORT_SYMBOL(trace_register_new_event);
EXPORT_SYMBOL(trace_proc_print_2_scr);
