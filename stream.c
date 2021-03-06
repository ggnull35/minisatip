#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <string.h>
#include <linux/dvb/frontend.h>
#include <linux/dvb/dmx.h>

#include "minisatip.h"
#include "socketworks.h"
#include "stream.h"
#include "dvb.h"
#include "adapter.h"

extern struct struct_opts opts;
extern int rtp,
  rtpc;
streams st[MAX_STREAMS];

int init_tick;
uint32_t
getTick ()
{				//ms 
  struct timespec ts;
  unsigned theTick = 0U;

  clock_gettime (CLOCK_REALTIME, &ts);
  theTick = ts.tv_nsec / 1000000;
  theTick += ts.tv_sec * 1000;
  if (init_tick == 0)
    init_tick = theTick;
  return theTick - init_tick;
}

char str_desc[10];
char *
describe_streams (int sid)
{
  char *str;

  str = describe_adapter (st[sid].adapter);
  str_desc[0] = 0;
  if (str == NULL)
    return str_desc;

  return str;
}

int
send_rtcp (int s_id, int ctime)
{
  int tmp_port;
  streams *sid = &st[s_id];
  uint32_t buf[400];
  char *a = describe_streams (s_id);
  int la = strlen (a);

//      LOG("send_rtcp (sid: %d)-> %s",s_id,a);
  buf[0] = htonl (0x82CC << 16 | la + 16);	// rtp header
  buf[1] = htonl (ctime);
  buf[2] = htonl (0x53455331);
  buf[3] = htonl (la << 16);
  strcpy ((char *) &buf[4], a);
  tmp_port = sid->sa.sin_port;
  sid->sa.sin_port = htons (ntohs (sid->sa.sin_port) + 1);
  sendto (rtpc, buf, la + 16, MSG_NOSIGNAL,
	  (const struct sockaddr *) &sid->sa, sizeof (sid->sa));
  sid->sa.sin_port = tmp_port;
  sid->rtcp_wtime = ctime;


}

// we need to keep the pids from SETUP and PLAY into sid->tp  
void
set_stream_parameters (int s_id, transponder * t)
{
  streams *sid;

  sid = &st[s_id];
  if (!sid->enabled)
    return;
  if (t->apids && t->apids[0] >= '0' && t->apids[0] <= '9')
    {
      strcpy (sid->apids, t->apids);
      t->apids = sid->apids;
    }
  if (t->dpids && t->dpids[0] >= '0' && t->dpids[0] <= '9')
    {
      strcpy (sid->dpids, t->dpids);
      t->dpids = sid->dpids;
    }
  if (t->pids && t->pids[0] >= '0' && t->pids[0] <= '9')
    {
      strcpy (sid->pids, t->pids);
      t->pids = sid->pids;
    }
  if (!t->apids)
    t->apids = sid->tp.apids;
  if (!t->dpids)
    t->dpids = sid->tp.dpids;
  if (!t->pids)
    t->pids = sid->tp.pids;
  copy_dvb_parameters (t, &sid->tp);
}


streams *
setup_stream (char **str, sockets * s, rtp_prop * p)
{
  char *arg[20],
    pol;
  int i,
    stype;
  transponder t;

  init_hw ();
  detect_dvb_parameters (str[1], &t);
  LOG ("Setup stream %d parameters, sock_id %d, handle %d", s->sid,
       s->sock_id, s->sock);
  if (s->sid < 0)		// create the stream
    {
      int a_id = 0,
	s_id = 0;
      char pol;

      stype = -1;
      if (s->type == TYPE_HTTP)
	stype = s->sock;
      if ((s_id = streams_add (-1, p, stype)) < 0)
	return NULL;
      init_dvb_parameters (&st[s_id].tp);
      set_stream_parameters (s_id, &t);
      s->sid = s_id;
      s->close_sec = 200;
      s->timeout = (socket_action) stream_timeout;
      LOG ("Setup stream done: sid: %d (e:%d) for sock %d handle %d", s_id,
	   st[s_id].enabled, s->sock_id, s->sock);
      return &st[s_id];
    }
  else
    set_stream_parameters (s->sid, &t);
  if (s->sid < 0 || st[s->sid].enabled == 0)
    LOG_AND_RETURN (NULL, "Stream %d not enabled for sock_id %d handle %d",
		    s->sid, s->sock_id, s->sock);
  return &st[s->sid];
}

int
start_play (streams * sid, sockets * s)
{
  int a_id;

  LOG ("Play for stream %d, adapter %d, sock_id %d handle %d", s->sid,
       sid->adapter, s->sock_id, s->sock);
  if (sid->adapter == -1)	// associate the adapter only at play (not at setup)
    {
      a_id =
	get_free_adapter (sid->tp.freq, sid->tp.pol, sid->tp.sys,
			  sid->tp.fe - 1);
      LOG ("Got adapter %d on socket %d", a_id, s->sock_id);
      if (a_id < 0)
	return -1;
      sid->adapter = a_id;
      set_adapter_for_stream (sid->sid, a_id);

    }
  if (set_adapter_parameters (sid->adapter, s->sid, &sid->tp) < 0)
    return -1;
  sid->do_play = 1;
  sid->start_streaming = 0;
  sid->tp.apids = sid->tp.dpids = sid->tp.pids = NULL;
  return tune (sid->adapter, s->sid);
}

void
decode_transport (sockets * s, rtp_prop * p, char *arg, char *default_rtp,
		  int start_rtp)
{
  char *arg2[10];
  int l;

  l = split (arg2, arg, 10, ';');
//      LOG("arg2 %s %s %s",arg2[0],arg2[1],arg2[2]);
  memset (p->dest, 0, sizeof (p->dest));
  p->type = 0;
  p->port = p->ttl = 0;
  while (l > 0)
    {
      l--;
      if (strcmp ("multicast", arg2[l]) == 0)
	p->type = TYPE_MCAST;
      if (strcmp ("unicast", arg2[l]) == 0)
	p->type = TYPE_UNICAST;
      if (strncmp ("ttl=", arg2[l], 4) == 0)
	p->ttl = atoi (arg2[l] + 4);
      if (strncmp ("client_port=", arg2[l], 12) == 0)
	p->port = atoi (arg2[l] + 12);
      if (strncmp ("port=", arg2[l], 5) == 0)
	p->port = atoi (arg2[l] + 5);
      if (strncmp ("destination=", arg2[l], 12) == 0)
	strncpy (p->dest, arg2[l] + 12, sizeof (p->dest));
    }
  if (default_rtp)
    strncpy (p->dest, default_rtp, sizeof (p->dest));
  if (p->dest[0] == 0 && p->type == TYPE_UNICAST)
    strncpy (p->dest, inet_ntoa (s->sa.sin_addr), sizeof (p->dest));
  if (p->dest[0] == 0)
    strcpy (p->dest, "239.255.255.250");
  if (p->port == 0)
    p->port = start_rtp;
  LOG ("p->%d %d %d %s", p->type, p->ttl, p->port, p->dest);
}


int
streams_add (int a_id, rtp_prop * p, int https)
{
  int i;
  char *ra;

  for (i = 0; i < MAX_STREAMS; i++)
    if (!st[i].enabled)
      break;
  if (i == MAX_STREAMS)
    {
      LOG ("The stream could not be added - most likely no free stream");
      return -1;
    }
  st[i].rtp = udp_connect (p->dest, p->port, &st[i].sa);
  if (st[i].rtp == -1)
    LOG_AND_RETURN (-1,
		    "Streams_add failed for stream %d: UDP connection to %s:%d failed",
		    i, p->dest, p->port);
  st[i].enabled = 1;
  st[i].adapter = a_id;
  st[i].sid = i;
  st[i].https = https;
  st[i].do_play = 0;
  st[i].iiov = 0;
  st[i].wtime = st[i].rtcp_wtime = getTick ();
  st[i].total_len = 7 * DVB_FRAME;	// max 7 packets
  if (!st[i].pids)
    st[i].pids = malloc1 (MAX_PIDS * 5 + 1);
  if (!st[i].apids)
    st[i].apids = malloc1 (MAX_PIDS * 5 + 1);
  if (!st[i].dpids)
    st[i].dpids = malloc1 (MAX_PIDS * 5 + 1);
  if (!st[i].buf)
    st[i].buf = malloc1 (STREAMS_BUFFER + 10);
  if (!st[i].pids || !st[i].apids || !st[i].dpids || !st[i].buf)
    {
      LOG ("memory allocation failed for stream %d\n", i);
      if (st[i].pids)
	free (st[i].pids);
      if (st[i].apids)
	free (st[i].apids);
      if (st[i].dpids)
	free (st[i].dpids);
      if (st[i].buf)
	free (st[i].buf);
      st[i].pids = st[i].apids = st[i].dpids = st[i].buf = NULL;
      return -1;
    }
  return i;
}


int
close_stream (int i)
{
  int ad;

  if (i < 0 || i >= MAX_STREAMS || st[i].enabled == 0)
    return;
  LOG ("closing stream %d", i);
  st[i].enabled = 0;
  ad = st[i].adapter;
  st[i].adapter = -1;
  close (st[i].rtp);
  st[i].rtp = -1;
  st[i].https = -1;
  if (ad >= 0)
    close_adapter_for_stream (i, ad);
  sockets_del_for_sid (i);
/*  if(st[i].pids)free(st[i].pids);
  if(st[i].apids)free(st[i].apids);
  if(st[i].dpids)free(st[i].dpids);
  if(st[i].buf)free(st[i].buf);
  st[i].pids = st[i].apids = st[i].dpids = st[i].buf = NULL;
*/ LOG ("closed stream %d", i);
}

int
close_streams_for_adapter (int ad, int except)	// close all streams for adapter, excepting except
{
  int i;

  if (ad < 0)
    return 0;
  for (i = 0; i < MAX_STREAMS; i++)
    if (st[i].enabled && st[i].adapter == ad)
      if (except < 0 || except != i)
	close_stream (i);
}

unsigned char rtp_h[12] = { 0x80, 0x21, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

int
send_rtpb (int sock, unsigned char *b, int len)
{
  struct iovec iov[2];

  iov[0].iov_base = b;
  iov[0].iov_len = len;
//      LOG("called send_rtpb %X %d",b,len);
  return send_rtp (sock, (const struct iovec *) iov, 1);
}

int
send_rtp (int sock, struct iovec *iov, int liov)
{
  struct iovec io[MAX_PACK + 2];

  rtp_h[3]++;
  if (rtp_h[3] == 0)
    rtp_h[2]++;
  rtp_h[11]++;
  if (rtp_h[11] == 0)
    rtp_h[10]++;
  rtp_h[7]++;
  if (rtp_h[7] == 0)
    {
      rtp_h[6]++;
      if (rtp_h[6] == 0)
	{
	  rtp_h[5]++;
	  if (rtp_h[5] == 0)
	    rtp_h[4]++;
	}
    }
  io[0].iov_base = &rtp_h;
  io[0].iov_len = sizeof (rtp_h);
  memcpy (&io[1], iov, liov * sizeof (struct iovec));
//      LOG("write to %d (len:%d)-> %X, %d, %X, %d ",sock,liov*sizeof(iov),io[0].iov_base,io[0].iov_len,io[1].iov_base,io[1].iov_len);
  return writev (sock, (const struct iovec *) io, liov + 1);
}

extern int bw;
void
flush_streamb (streams * sid, char *buf, int rlen, int ctime)
{
  int i;

  if (sid->https > -1)
    send (sid->https, buf, rlen, MSG_NOSIGNAL);
  else
    for (i = 0; i < rlen; i += DVB_FRAME * 7)
      send_rtpb (sid->rtp, &buf[i], DVB_FRAME * 7);
  sid->iiov = 0;
  sid->wtime = ctime;
  sid->len = 0;
  bw += (sid->rtp >= 0) * 12 + rlen;
}

void
flush_streami (streams * sid, int ctime)
{
  if (sid->https > -1)
    writev (sid->https, sid->iov, sid->iiov);
  else
    send_rtp (sid->rtp, sid->iov, sid->iiov);
  sid->iiov = 0;
  sid->wtime = ctime;
  sid->len = 0;
  bw += (sid->rtp >= 0) * 12 + DVB_FRAME * 7;
}


int
read_dmx (sockets * s)
{
  void *min,
   *max;
  int i,
    j,
    dp;
  static int cnt;
  streams *sid;
  adapter *ad;
  unsigned char sids;
  pid *p;
  int pid;

  if (s->rlen % DVB_FRAME != 0)
    s->rlen = ((int) s->rlen / DVB_FRAME) * DVB_FRAME;
  if (s->rlen == s->lbuf)
    cnt++;
  else
    cnt = 0;
//LOG("read_dmx called -> %d bytes read ",s->rlen);
  if (s->rlen < s->lbuf)
    return 0;			// the buffer is not full, read more 
  if (s->rlen > s->lbuf)
    LOG ("Buffer overrun %d %d", s->rlen, s->lbuf);
  ad = get_adapter (s->sid);
  if (!ad)
    return 0;

  int rlen = s->rlen;

  s->rlen = 0;
  if (cnt > 0 && cnt % 100 == 0)
    LOG ("Reading max size for the last %d buffers", cnt);
  if (ad->sid_cnt == 1 && ad->master_sid >= 0)	// we have just 1 stream, do not check the pids, send everything to the destination
    {
      sid = &st[ad->master_sid];
      if (sid->enabled != 1)
	{
	  LOG ("Master SID %d not enabled ", ad->master_sid);
	  return -1;
	}
      if (sid->len > 0)
	flush_streamb (sid, sid->buf, sid->len, s->rtime);
      flush_streamb (sid, s->buf, rlen, s->rtime);

    }
  else
    {
//              LOG("not implemented");
//              char used[DVB_FRAME*7*MAX_PACK]; // if the DVB packet is already sent
//              return 0;
      for (dp = 0; dp < rlen; dp += DVB_FRAME)
	{
	  unsigned char *b = &s->buf[dp];

	  pid = (b[1] & 0x1f) * 256 + b[2];
	  p = ad->pids;
	  for (i = 0; i < MAX_PIDS; i++)
	    if (p[i].pid == pid && p[i].flags == 1)
	      for (j = 0; p[i].sid[j] > -1 && j < MAX_STREAMS_PER_PID; j++)
		{
		  int s_id = p[i].sid[j];

		  if (s_id >= 0 && s_id < MAX_STREAMS && st[s_id].enabled)
		    {
		      sid = &st[s_id];
		      p[i].cnt++;
		      sid->iov[sid->iiov].iov_base = b;
		      sid->iov[sid->iiov++].iov_len = DVB_FRAME;
		      if (sid->iiov == 7)
			flush_streami (sid, s->rtime);
		    }
		  else
		    LOG ("could not find a valid sid %d e:%d for pid:%d",
			 s_id, st[s_id].enabled, pid);
		}
	}
      min = s->buf;
      max = &s->buf[rlen];
      for (i = 0; i < MAX_STREAMS; i++)
	if (st[i].enabled && st[i].adapter == s->sid)	//move all dvb packets that were not sent out of the s->buf 
	  {
	    sid = &st[i];
	    for (j = 0; j < sid->iiov; j++)
	      if (sid->iov[j].iov_base >= min && sid->iov[j].iov_base <= max)
		{
		  memcpy (&sid->buf[sid->len], sid->iov[j].iov_base,
			  DVB_FRAME);
		  sid->iov[j].iov_base = &sid->buf[sid->len];
		  sid->len += DVB_FRAME;
		}
	  }
      if (s->rtime - ad->last_sort > 2000)
	{
	  ad->last_sort = s->rtime + 60000;
	  sort_pids (s->sid);
	}
    }

//      if(!found)LOG("pid not found = %d -> 1:%d 2:%d 1&1f=%d",pid,s->buf[1],s->buf[2],s->buf[1]&0x1f);
//      LOG("done send stream");
  for (i = 0; i < MAX_STREAMS; i++)
    if (st[i].enabled && st[i].https == -1 && st[i].do_play
	&& s->rtime > st[i].rtcp_wtime + 200)
      stream_timeout (s);
}


int
stream_timeout (sockets * s)
{
  if (s->sid < 0 || s->sid > MAX_STREAMS || st[s->sid].enabled == 0
      || st[s->sid].do_play == 0)
    return;
  streams *sid = &st[s->sid];
  int ctime,
    rttime = sid->rtcp_wtime,
    rtime = sid->wtime;

  ctime = getTick ();

  //LOG("stream timeouts called for sid %d c:%d r:%d rt:%d",s->sid,ctime,rtime,rttime);
  if (ctime > rtime + 1000)
    {
      LOG ("no data sent for more than 1s sid: %d for %s:%d", s->sid,
	   inet_ntoa (sid->sa.sin_addr), ntohs (sid->sa.sin_port));
      send_rtpb (sid->rtp, s->buf, 0);
      sid->wtime = ctime;
    }
  if (ctime > rttime + 200)
    send_rtcp (s->sid, ctime);
  //LOG("done stream timeouts called for sid %d c:%d r:%d rt:%d",s->sid,ctime,rtime,rttime);
  if (ctime - s->rtime > opts.timeout_sec)
    return 1;			//most likely called from timeout from sockets, return 1 to close the socket and stream
  return 0;
}

void
dump_streams ()
{
  int i;

  if (!opts.log)
    return;
  LOG ("Dumping streams:");
  for (i = 0; i < MAX_STREAMS; i++)
    if (st[i].enabled)
      LOG ("%d|  a:%d play:%d h:%d remote:%s:%d", i, st[i].adapter,
	   st[i].https, st[i].do_play, inet_ntoa (st[i].sa.sin_addr),
	   ntohs (st[i].sa.sin_port));
}

void
free_all_streams ()
{
  int i;

  for (i = 0; i < MAX_STREAMS; i++)
    {
      if (st[i].pids)
	free1 (st[i].pids);
      if (st[i].apids)
	free1 (st[i].apids);
      if (st[i].dpids)
	free1 (st[i].dpids);
      if (st[i].buf)
	free1 (st[i].buf);
    }
}

streams *
get_sid (int sid)
{
  if (sid < 0 || sid > MAX_STREAMS || st[sid].enabled == 0)
    return NULL;
  return &st[sid];
}
