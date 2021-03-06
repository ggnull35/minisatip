/*
 * Copyright (C) 2014-2020 Catalin Toda <catalinii@yahoo.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 *
 */

#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <sys/ucontext.h>
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
#include <fcntl.h>
#include "minisatip.h"
#include "socketworks.h"
#include "stream.h"
#include "adapter.h"


struct struct_opts opts;
int rtp,
  rtpc;
extern sockets s[MAX_SOCKS];

void
usage ()
{
  printf ("minisatip [-f] [-r remote_rtp_host] [-d discovery_host] [-w http_server[:port]] [-p public_host] [-s rtp_port] [-a no] [-m mac] [-l]\n \
		-f foreground, otherwise run in background\n\
		-r remote_rtp_host: send remote rtp to remote_rtp_host\n \
		-d send multicast annoucement to discovery_host instead\n \
		-w http_server[:port]: specify the host and the port where the xml file can be downloaded from \n\
		-p public_host: specify the host where this device listens for RTSP or HTTP\n \
		-x port: port for listening on http\n\
		-s port: start port for rtp connections\n\
		-a x: simulate x DVB-S2 adapters on this box\n\
		-t x: simulate x DVB-T2 adapters on this box\n\
		-m xx: simulate xx as local mac address, generates UUID based on mac\n\
		-c X: bandwidth capping for the output to the network (default: unlimited)\n\
		-b X: set the DVR buffer to X KB (default: %dKB)\n\
		",
	  DVR_BUFFER / 1024);
  exit (1);
}

void
set_options (int argc, char *argv[])
{
  int opt;
  int sethost = 0;
  int index;
  char *lip;

  opts.log = 1;
  opts.rrtp = NULL;
  opts.disc_host = "239.255.255.250";
  opts.start_rtp = 5500;
  opts.http_port = 8080;
  opts.http_host = NULL;
  opts.pub_host = NULL;
  opts.log = 0;
  opts.timeout_sec = 30000;
  opts.force_sadapter = 0;
  opts.force_tadapter = 0;
  opts.mac[0] = 0;
  opts.daemon = 1;
  opts.bw = 0;
  opts.dvr = DVR_BUFFER;
  while ((opt = getopt (argc, argv, "flr:a:t:d:w:p:s:hc:b:m:")) != -1)
    {
//              printf("options %d %c %s\n",opt,opt,optarg);
      switch (opt)
	{
	    case FOREGROUND_OPT:
	      {
		opts.daemon = 0;
		break;
	      }
	    case MAC_OPT:
	      {
		strncpy (opts.mac, optarg, 12);
		opts.mac[12] = 0;
		break;
	      }
	    case RRTP_OPT:
	      {
		opts.rrtp = optarg;
		break;
	      }

	    case DISCOVERYIP_OPT:
	      {
		opts.daemon = 1;
		break;
	      }

	    case HTTPSERVER_OPT:
	      {
//                              int i=0;
		opts.http_host = optarg;
		sethost = 1;
/*				for(i=0;i<strlen(opts.remote_host);i++)
					if(opts.remote_host[i]==':'){
						opts.remote_host[i]=0;
						opts.remote_port=&optarg[i+1];
					}
				settings.remote_host = 1;
*/ break;
	      }

	    case PUBLICIP_OPT:
	      {
		opts.pub_host = optarg;
		break;
	      }

	    case LOG_OPT:
	      {
		opts.log = 1;
		break;
	      }


	    case HELP_OPT:
	      {
		usage ();
		exit (0);
	      }

	    case HTTPPORT_OPT:
	      {
		opts.http_port = atoi (optarg);
		break;
	      }

	    case BW_OPT:
	      {
		opts.bw = atoi (optarg) * 1024;
		break;
	      }

	    case DVRBUFFER_OPT:
	      {
		opts.dvr = atoi (optarg) * 1024;
		break;
	      }

	    case DVBS2_ADAPTERS_OPT:
	      {
		opts.force_sadapter = atoi (optarg);
		break;
	      }

	    case DVBT2_ADAPTERS_OPT:
	      {
		opts.force_tadapter = atoi (optarg);
		break;
	      }

	    case RTPPORT_OPT:
	      {
		opts.start_rtp = atoi (optarg);
		break;
	      }

	}
    }
  lip = getlocalip ();
  if (!opts.http_host)
    {
      opts.http_host = (char *) malloc (MAX_HOST);
      sprintf (opts.http_host, "%s:%d", lip, opts.http_port);
    }
  if (!opts.pub_host)
    {
      opts.pub_host = (char *) malloc (MAX_HOST);
      strncpy (opts.pub_host, lip, MAX_HOST);
    }
}

void
hexDump (char *desc, void *addr, int len)
{
  int i;
  unsigned char buff[17];
  unsigned char *pc = (unsigned char *) addr;

  // Output description if given.
  if (desc != NULL)
    printf ("%s:\n", desc);

  // Process every byte in the data.
  for (i = -len; i < len; i++)
    {
      // Multiple of 16 means new line (with line offset).

      if ((i % 16) == 0)
	{
	  // Just don't print ASCII for the zeroth line.
	  if (i != 0)
	    printf ("  %s\n", buff);

	  // Output the offset.
	  printf ("  %08x ", ((unsigned int) addr) + i);
	}

      // Now the hex code for the specific character.
      printf (" %02x", pc[i]);

      // And store a printable ASCII character for later.
      if ((pc[i] < 0x20) || (pc[i] > 0x7e))
	buff[i % 16] = '.';
      else
	buff[i % 16] = pc[i];
      buff[(i % 16) + 1] = '\0';
    }

  // Pad out last line if not exactly 16 characters.
  while ((i % 16) != 0)
    {
      printf ("   ");
      i++;
    }

  // And print the final ASCII bit.
  printf ("  %s\n", buff);
}

void posix_signal_handler (int sig, siginfo_t * siginfo, ucontext_t * ctx);
void
set_signal_handler ()
{
  struct sigaction sig_action = { };
  sig_action.sa_sigaction =
    (void (*)(int, siginfo_t *, void *)) posix_signal_handler;
  sigemptyset (&sig_action.sa_mask);

  sig_action.sa_flags = SA_SIGINFO | SA_ONSTACK;

//    if (sigaction(SIGBUS, &sig_action, NULL) != 0) { err(1, "sigaction"); }
//    if (sigaction(SIGSEGV, &sig_action, NULL) != 0) { err(1, "sigaction"); }
//    if (sigaction(SIGFPE,  &sig_action, NULL) != 0) { err(1, "sigaction"); }
  if (sigaction (SIGINT, &sig_action, NULL) != 0)
    {
      err (1, "sigaction");
    }
//    if (sigaction(SIGILL,  &sig_action, NULL) != 0) { err(1, "sigaction"); }
//    if (sigaction(SIGTERM, &sig_action, NULL) != 0) { err(1, "sigaction"); }
//    if (sigaction(SIGABRT, &sig_action, NULL) != 0) { err(1, "sigaction"); }
  if (signal (SIGHUP, SIG_IGN) != 0)
    {
      err ("sigaction SIGHUP");
    }
}


int
split (char **rv, char *s, int lrv, char sep)
{
  int i = 0,
    j = 0;

  if (!s)
    return 0;
  for (i = 0; s[i] && s[i] == sep && s[i] < 32; i++);

  rv[j++] = &s[i];
//      LOG("start %d %d\n",i,j);
  while (j < lrv)
    {
      if (s[i] == 0 || s[i + 1] == 0)
	break;
      if (s[i] == sep || s[i] < 33)
	{
	  s[i] = 0;
	  if (s[i + 1] != sep && s[i + 1] > 32)
	    rv[j++] = &s[i + 1];
	}
      else if (s[i] < 14)
	s[i] = 0;
//              LOG("i=%d j=%d %d %c \n",i,j,s[i],s[i]);
      i++;
    }
  if (s[i] == sep)
    s[i] = 0;
  rv[j] = NULL;
  return j;
}

#define LR(s) {LOG("map_int returns %d",s);return s;}
int
map_int (char *s, pchar_int * v)
{
  int i;

  if (v == NULL)
    return atoi (s);
  for (i = 0; v[i].s; i++)
    if (!strncasecmp (s, v[i].s, strlen (v[i].s)))
      return v[i].v;
  return 0;
}

int
map_float (char *s, int mul)
{
  float f;
  int r;

  if (s[0] < '0' || s[0] > '9')
    return 0;
  f = atof (s);
  r = (int) (f * mul);
//      LOG("atof returned %.1f, mul = %d, result=%d",f,mul,r);
  return r;

}


char *
http_response (int sock, char *proto, int rc, char *ah, char *desc, int cseq)
{
  char *reply =
    "%s/1.0 %d %s\r\nCseq: %d\r\n%s\r\nContent-Length: %d\r\n\r\n%s\r\n\r\n";
  char *reply0 = "%s/1.0 %d %s\r\nCseq: %d\r\n%s\r\n\r\n";
  char *d;
  int lr;

  if (!ah)
    ah = "Public: OPTIONS, DESCRIBE, SETUP, PLAY, TEARDOWN";
  if (!desc)
    desc = "";
  if (rc == 200)
    d = "OK";
  else if (rc == 400)
    d = "Bad Request";
  else if (rc == 403)
    d = "Forbidden";
  else if (rc == 404)
    d = "Not Found";
  else if (rc == 500)
    d = "Internal Server Error";
  else if (rc == 501)
    d = "Not Implemented";
  else
    d = "Service Unavailable";
  static char resp[5000];

  lr = strlen (desc);
  if (lr)
    sprintf (resp, reply, proto, rc, d, cseq, ah, lr, desc);
  else
    sprintf (resp, reply0, proto, rc, d, cseq, ah);
  LOG ("reply -> %d:\n%s", sock, resp);
  send (sock, resp, strlen (resp), MSG_NOSIGNAL);
  return resp;
}


int
read_rtsp (sockets * s)
{
  char *arg[50];
  int cseq,
    la,
    i;
  rtp_prop p;

  LOG ("read RTSP (from handle %d sock_id %d): %s", s->sock, s->sock_id,
       s->buf);
  if (s->rlen < 5
      || (htonl (*(uint32_t *) & s->buf[s->rlen - 4]) != 0x0D0A0D0A))
    {
      printf
	("Not a complete request RTSP_request read_bytes=%d ends with: %08X\n",
	 s->rlen, htonl (*(uint32_t *) & s->buf[s->rlen - 4]));
      return 0;
    }
  la = split (arg, s->buf, 50, ' ');
//      LOG("args: %s -> %s -> %s",arg[0],arg[1],arg[2]);
  for (i = 0; i < la; i++)
    if (strncasecmp ("CSeq:", arg[i], 5) == 0)
      cseq = atoi (arg[i + 1]);
    else if (strncasecmp ("Transport:", arg[i], 5) == 0)
      decode_transport (s, &p, arg[i + 1], opts.rrtp, opts.start_rtp);
//      LOG("read RTSP (CSeq %d): %s from %d",cseq,s->buf,s->sock);

  if ((strncmp (arg[0], "SETUP", 3) == 0)
      || (strncmp (arg[0], "PLAY", 3) == 0))
    {
      char buf[200];
      streams *sid;

      char *ra;

      sid = (streams *) setup_stream (arg, s, &p);

      if (!sid)
	{
	  http_response (s->sock, "RTSP", 503, NULL, NULL, cseq);
	  return 0;
	}

      if (arg[0][0] == 'P')
	if (start_play (sid, s) < 0)
	  {
	    http_response (s->sock, "RTSP", 404, NULL, NULL, cseq);
	    return 0;
	  }
      ra = inet_ntoa (sid->sa.sin_addr);
      if (atoi (ra) < 239)
	sprintf (buf,
		 "Transport: RTP/AVP;unicast;client_port=%d-%d;source=%s;server_port=%d-%d\r\nSession:%08x;timeout=%d\r\ncom.ses.streamID: %d",
		 ntohs (sid->sa.sin_port), ntohs (sid->sa.sin_port) + 1,
		 opts.pub_host, opts.start_rtp, opts.start_rtp + 1, (int) sid,
		 opts.timeout_sec / 1000, sid->sid + 1);
      else
	sprintf (buf,
		 "Transport: RTP/AVP;multicast;destination=%s;port=%d-%d\r\nSession:%08x;timeout=%d\r\ncom.ses.streamID: %d",
		 ra, ntohs (sid->sa.sin_port), ntohs (sid->sa.sin_port) + 1,
		 (int) sid, opts.timeout_sec / 1000, sid->sid + 1);

      http_response (s->sock, "RTSP", 200, buf, NULL, cseq);
    }
  else if (strncmp (arg[0], "TEARDOWN", 8) == 0)
    {
      close_stream (s->sid);
      http_response (s->sock, "RTSP", 200, NULL, NULL, cseq);
    }
  else
    {
      streams *sid;

      sid = get_sid (s->sid);
      if (sid && sid->adapter >= 0)
	start_play (sid, s);	// we have a valid SID, purpose is to set master_sid>=0 (if master_sid == -1)
      if (strncmp (arg[0], "DESCRIBE", 8) == 0)
	http_response (s->sock, "RTSP", 200, NULL, describe_streams (s->sid),
		       cseq);
      else if (strncmp (arg[0], "OPTIONS", 8) == 0)
	http_response (s->sock, "RTSP", 200, NULL, NULL, cseq);
    }
  return 0;
}

char uuid[100];
int cnt,
  uuidi;
struct sockaddr_in ssdp_sa;

int
read_http (sockets * s)
{
  char buf[2000];
  char *arg[50];
  int la;
  char *xml =
    "<?xml version=\"1.0\"?><root xmlns=\"urn:schemas-upnp-org:device-1-0\" configId=\"0\">\r\n"
    "<specVersion><major>1</major><minor>1</minor> </specVersion><device>\r\n"
    "<deviceType>urn:ses-com:device:SatIPServer:1</deviceType><friendlyName>minisatip</friendlyName>\r\n"
    "<manufacturer>cata</manufacturer><manufacturerURL>http://github.com/catalinii/minisatip</manufacturerURL>\r\n"
    "<modelDescription>long user-friendly title</modelDescription> <modelName>minisatip</modelName><modelNumber>0100</modelNumber>\r\n"
    "<modelURL>http://github.com/catalinii/minisatip</modelURL> <serialNumber>0100</serialNumber>\r\n"
    "<UDN>uuid:%s</UDN><UPC>Universal Product Code</UPC><iconList><icon> <mimetype>image/format</mimetype>\r\n"
    "<width>horizontal pixels</width> <height>vertical pixels</height><depth>color depth</depth><url>URL to icon</url> </icon> </iconList>\r\n"
    "<presentationURL>http://github.com/catalinii/minisatip</presentationURL>\r\n"
    "<satip:X_SATIPCAP xmlns:satip=\"urn:ses-com:satip\">DVBS2-%d,DVBT-%d</satip:X_SATIPCAP>\r\n"
    "</device></root>\r\n";
  LOG ("read HTTP from %d sid: %d: %s", s->sock, s->sid, s->buf);
  if (s->rlen < 5
      || (htonl (*(uint32_t *) & s->buf[s->rlen - 4]) != 0x0D0A0D0A))
    {
      printf ("Not a complete request RTSP_request read_bytes=%d", s->rlen);
      return 0;
    }
  la = split (arg, s->buf, 50, ' ');
//      LOG("args: %s -> %s -> %s",arg[0],arg[1],arg[2]);
  if (strncmp (arg[0], "GET", 3) != 0)
    {
      http_response (s->sock, "HTTP", 503, NULL, NULL, 0);
      return 0;
    }
  if (uuidi == 0)
    ssdp_discovery (s);

  if (strncmp (arg[1], "/desc.xml", 0) == 0)
    {
      int tuner_s2,
        tuner_t;

      tuner_s2 = getS2Adapters ();
      tuner_t = getTAdapters ();
      sprintf (buf, xml, uuid, tuner_s2, tuner_t);
      http_response (s->sock, "HTTP", 200, "Content-type: text/xml", buf, 0);
      return 0;
    }
  http_response (s->sock, "HTTP", 404, NULL, NULL, 0);
  return 0;
}


int
close_http (sockets * s)
{
  LOG ("Closing stream %d", s->sid);
  close_stream (s->sid);
  return 0;
}


int
ssdp_discovery (sockets * s)
{
  char *reply = "NOTIFY * HTTP/1.1\r\n"
    "HOST: 239.255.255.250:1900\r\n"
    "CACHE-CONTROL: max-age=1800\r\n"
    "LOCATION: http://%s/desc.xml\r\n"
    "NT: upnp:rootdevice\r\n"
    "NTS: ssdp:alive \r\n"
    "SERVER: Linux/1.0 UPnP/1.1 IDL4K/1.2\r\n"
    "USN: uuid:%s::upnp:rootdevice\r\n"
    "BOOTID.UPNP.ORG: %d\r\n"
    "CONFIGID.UPNP.ORG: 0\r\n" "DEVICEID.SES.COM: 1\r\n\r\n\0";
  char buf[500],
    mac[15] = "00000000000000";
  char uuid1[] = "11223344-9999-0000-b7ae";
  socklen_t salen;
  int i;

  if (uuidi == 0)
    {
      uuidi = 1;
      get_mac (mac);
      sprintf (uuid, "%s-%s", uuid1, mac);
      fill_sockaddr (&ssdp_sa, opts.disc_host, 1900);
    }
  sprintf (buf, reply, opts.http_host, uuid, cnt);
  salen = sizeof (ssdp_sa);
  LOG ("ssdp_discovery -> %s", buf);
  for (i = 0; i < 3; i++)
    sendto (s->sock, buf, strlen (buf), MSG_NOSIGNAL,
	    (const struct sockaddr *) &ssdp_sa, salen);
  cnt++;
  s->rtime = getTick ();
  return 0;
}

int ssdp;
int
ssdp_reply (sockets * s)
{
  char *reply = "HTTP/1.1 200 OK\r\n"
    "CACHE-CONTROL: max-age=1800\r\n"
    "DATE: %s\r\n"
    "EXT:\r\n"
    "LOCATION: http://%s/desc.xml\r\n"
    "SERVER: Linux/1.0 UPnP/1.1 IDL4K/1.2\r\n"
    "ST: urn:ses-com:device:SatIPServer:1\r\n"
    "USN: uuid:%s::urn:ses-com:device:SatIPServer:1\r\n"
    "BOOTID.UPNP.ORG: %d\r\n"
    "CONFIGID.UPNP.ORG: 0\r\n" "DEVICEID.SES.COM: 1\r\n\0";
  socklen_t salen;

  if (strncmp (s->buf, "M-SEARCH", 8) != 0)
    {
      //      LOG("not an M-SEARCH, ignoring");
      return 0;
    }
  if (uuidi == 0)
    ssdp_discovery (s);
  char buf[500];

  sprintf (buf, reply, get_current_timestamp (), opts.http_host, uuid, cnt);
  salen = sizeof (s->sa);
  LOG ("ssdp_reply -> %s\n%s", inet_ntoa (s->sa.sin_addr), buf);
  sendto (ssdp, buf, strlen (buf), MSG_NOSIGNAL, (const struct sockaddr *) &s->sa, salen);	//use ssdp (unicast) even if received to multicast address
  cnt++;
  return 0;
}

int
new_rtsp (sockets * s)
{
  s->type = TYPE_RTSP;
  s->action = (socket_action) read_rtsp;
  s->close = (socket_action) close_http;
  return 0;
}

int
new_http (sockets * s)
{
  s->type = TYPE_HTTP;
  s->action = (socket_action) read_http;
  s->close = (socket_action) close_http;
  return 0;
}

int becomeDaemon ();

int
main (int argc, char *argv[])
{
  int rtsp,
    http,
    si,
    si1,
    ssdp1;

  set_signal_handler ();
  set_options (argc, argv);
  if (opts.daemon)
    becomeDaemon ();
  if ((ssdp = udp_bind (NULL, 1900)) < 1)
    FAIL ("SSDP: Could not bind on udp port 1900");
  if ((ssdp1 = udp_bind ("239.255.255.250", 1900)) < 1)
    FAIL ("SSDP: Could not bind on 239.255.255.250 udp port 1900");
  if ((rtsp = tcp_listen (NULL, 554)) < 1)
    FAIL ("RTSP: Could not listen on port 554");
  if ((http = tcp_listen (NULL, opts.http_port)) < 1)
    FAIL ("Could not listen on http port %d", opts.http_port);
  if ((rtp = udp_bind (NULL, opts.start_rtp)) < 1)
    FAIL ("SSDP: Could not bind on rtp port %d", opts.start_rtp);
  if ((rtpc = udp_bind (NULL, opts.start_rtp + 1)) < 1)
    FAIL ("SSDP: Could not listen on rtpc port %d", opts.start_rtp + 1);

  si =
    sockets_add (ssdp, NULL, -1, TYPE_UDP, (socket_action) ssdp_reply, NULL,
		 (socket_action) ssdp_discovery);
  si1 =
    sockets_add (ssdp1, NULL, -1, TYPE_UDP, (socket_action) ssdp_reply, NULL,
		 (socket_action) ssdp_discovery);
  if (si < 0 || si1 < 0)
    FAIL ("sockets_add failed for ssdp");
  s[si].rtime = 0;
  s[si].close_sec = 1800 * 1000;

  if (0 >
      sockets_add (rtsp, NULL, -1, TYPE_SERVER, (socket_action) new_rtsp,
		   NULL, (socket_action) close_http))
    FAIL ("sockets_add failed for rtsp");
  if (0 >
      sockets_add (http, NULL, -1, TYPE_SERVER, (socket_action) new_http,
		   NULL, (socket_action) close_http))
    FAIL ("sockets_add failed for http");
  printf ("Initializing with %d devices\n", init_hw ());
  select_and_execute ();
  free_all ();
  return 0;
}

#ifdef __mips__

#endif
extern int run_loop;
void
posix_signal_handler (int sig, siginfo_t * siginfo, ucontext_t * ctx)
{
  char *sp,
   *ip;

  if (sig == SIGINT)
    {
      run_loop = 0;
      return;
    }
#ifdef REG_RSP
  sp = (char *) ctx->uc_mcontext.gregs[REG_RSP];
  ip = (char *) ctx->uc_mcontext.gregs[REG_RIP];
#elif REG_ESP
  sp = (char *) ctx->uc_mcontext.gregs[REG_ESP];
  ip = (char *) ctx->uc_mcontext.gregs[REG_EIP];
#elif __mips__
  sp = (char *) ctx->uc_mcontext.gregs[29];
  ip = (char *) ctx->uc_mcontext.pc;

#elif __arm__
  sp = (char *) ctx->uc_mcontext.arm_sp;
  ip = (char *) ctx->uc_mcontext.arm_pc;
#else
#warning No arch defined
  printf ("\nRECEIVED SIGNAL %d\n", sig);
  exit (1);
#endif

  printf
    ("RECEIVED SIGNAL %d - SP=%lX IP=%lX main=%lX read_dmx=%lX clock_gettime=%lX\n",
     sig, (long unsigned int) sp, (long unsigned int) ip,
     (long unsigned int) main, (long unsigned int) read_dmx,
     (long unsigned int) clock_gettime);
  hexDump ("Stack dump: ", sp, 128);
  exit (1);
}

int				/* Returns 0 on success, -1 on error */
becomeDaemon ()
{
  int maxfd,
    fd;

  switch (fork ())
    {				/* Become background process */
	case -1:
	  return -1;
	case 0:
	  break;		/* Child falls through... */
	default:
	  _exit (EXIT_SUCCESS);	/* while parent terminates */
    }

  if (setsid () == -1)		/* Become leader of new session */
    return -1;

  switch (fork ())
    {				/* Ensure we are not session leader */
	case -1:
	  return -1;
	case 0:
	  break;
	default:
	  _exit (EXIT_SUCCESS);
    }

  umask (0);			/* Clear file mode creation mask */

  maxfd = sysconf (_SC_OPEN_MAX);
  if (maxfd == -1)		/* Limit is indeterminate... */
    maxfd = 1024;		/* so take a guess */

  for (fd = 0; fd < maxfd; fd++)
    close (fd);

  close (STDIN_FILENO);		/* Reopen standard fd's to /dev/null */
  chdir ("/tmp");		/* Change to root directory */

  fd = open ("/dev/null", O_RDWR);

  if (fd != STDIN_FILENO)	/* 'fd' should be 0 */
    return -1;
  fd = open ("log", O_RDWR);

  if (fd != STDOUT_FILENO)	/* 'fd' should be 1 */
    return -1;
  if (dup2 (STDOUT_FILENO, STDERR_FILENO) != STDERR_FILENO)
    return -1;

  return 0;
}

void *
mymalloc (int a, char *f, int l)
{
  void *x;

  x = malloc (a);
  printf ("%s:%d allocation_wrapper malloc returns %X\n", f, l,
	  (unsigned int) x);
  fflush (stdout);
  return x;
}

void
myfree (void *x, char *f, int l)
{
  printf ("%s:%d allocation_wrapper free called with argument %X", f, l,
	  (unsigned int) x);
  fflush (stdout);
  free (x);
  puts (" - done free");
  fflush (stdout);
}
