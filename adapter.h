#ifndef ADAPTER_H
#define ADAPTER_H
#include "minisatip.h"
#include <linux/dvb/frontend.h>
#include "dvb.h"

#define MAX_ADAPTERS 8
#define DVR_BUFFER 30*1024*188
#define MAX_STREAMS_PER_PID 8
#define ADAPTER_BUFFER 7*7*DVB_FRAME
  typedef struct struct_pid
{
  
  int fd;			// fd for this demux    
  signed char sid[MAX_STREAMS_PER_PID];	// stream id - one more to set it -1 
  char flags;			// 0 - disabled , 1 enabled, 2 - will be enabled next tune when tune is called, 3 disable when tune is called
  int cnt;
 

{
  
   
    dvr;
   
    fn;				// physical adapter, physical frontend number
  transponder tp;
   
   
  int sid_cnt;			//number of streams       
  int sock;
   
   
  int last_sort;
   
   
   
 

















#endif	/* 