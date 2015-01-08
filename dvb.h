#ifndef DVB_H
#define DVB_H
#include <linux/dvb/frontend.h>

#define SLOF (11700*1000UL)
#define LOF1 (9750*1000UL)
#define LOF2 (10600*1000UL)
#define LP_CODERATE_DEFAULT (0)
  
#define SYSTEM_DVBS 1
#define SYSTEM_DVBS2 2
#define SYSTEM_DVBT 3
  
{
  
  
   
   
   
   
// DVB-T
  int hprate;
   
   
   
   
   
   
// DVB-S2
  int ro;
   
   
   
   
   
   
   
   *pids,
   *dpids;
 


#define MAX_PIDS 64
  
//int tune_it(int fd_frontend, unsigned int freq, unsigned int srate, char pol, int tone, fe_spectral_inversion_t specInv, unsigned char diseqc,fe_modulation_t modulation,fe_code_rate_t HP_CodeRate,fe_transmit_mode_t TransmissionMode,fe_guard_interval_t guardInterval, fe_bandwidth_t bandwidth);
int tune_it_s2 (int fd_frontend, transponder * tp);







		  uint16_t * strength, uint16_t * snr);

#endif	/* 