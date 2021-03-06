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
  typedef struct struct_transponder
{
  fe_delivery_system_t sys;
  int freq;
   int inversion;
   int mod;
   int fe;
   
// DVB-T
  int hprate;
   int tmode;
   int gi;
   int bw;
   int sm;
   int t2id;
   
// DVB-S2
  int ro;
   int mtype;
   int plts;
   int fec;
   int sr;
   int pol;
   int diseqc;
   char *apids,
   *pids,
   *dpids;
 } transponder;


#define MAX_PIDS 64
  
//int tune_it(int fd_frontend, unsigned int freq, unsigned int srate, char pol, int tone, fe_spectral_inversion_t specInv, unsigned char diseqc,fe_modulation_t modulation,fe_code_rate_t HP_CodeRate,fe_transmit_mode_t TransmissionMode,fe_guard_interval_t guardInterval, fe_bandwidth_t bandwidth);
int tune_it_s2 (int fd_frontend, transponder * tp);
 int set_pid (int hw, int ad, uint16_t i_pid);
void del_filters (int fd, int pid);
fe_delivery_system_t dvb_delsys (int fd);
int detect_dvb_parameters (char *s, transponder * tp);
void init_dvb_parameters (transponder * tp);
void copy_dvb_parameters (transponder * s, transponder * d);
void get_signal (int fd, fe_status_t * status, uint32_t * ber,
		  uint16_t * strength, uint16_t * snr);
 
#endif	/*  */
