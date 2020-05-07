#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <pthread.h>
#include <errno.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdlib.h>
#include <signal.h>

#include <hamlib/rig.h>

//#define DEBUG(s, ...) fprintf(stderr,s, ##__VA_ARGS__)
//#define TRACE(s, ...) fprintf(stderr,s, ##__VA_ARGS__)
#define ERROR(s, ...) fprintf(stderr,s, ##__VA_ARGS__)

#ifndef DEBUG
#define DEBUG(s, ...)
#endif
#ifndef TRACE
#define TRACE(s, ...)
#endif
#ifndef ERROR
#define ERROR(s, ...)
#endif

//
// This is for the rigctld thread that we are going to split off
//
static pthread_t       rigctld_thread;
static pthread_mutex_t rigctld_mutex = PTHREAD_MUTEX_INITIALIZER;
void *                 rigctld_func (void *);

// Functions in winkey.c
extern int  open_winkey_port(const char *);
extern void close_winkey_port();
extern void send_winkey(const char *);
extern void set_winkey_speed(int);

RIG *rig = NULL;

int wkeymode=0;   // 0: n/a, 1: CAT,  2: Wkey-Device

static ptt_t rigctl_ptt_cmd=RIG_PTT_ON;

static int        numrigs;
static struct rigs {
  rig_model_t model;
  char name[100];
} rigs[1000];

//
//  return 1 if we can use hamlib, that is:
//  - rig is defined (non-NULL)
//  - we have a locked mutex
//
int can_hamlib()
{
    if (!rig) return 0;
    if (pthread_mutex_lock(&rigctld_mutex)) perror("CAN_HAMLIB:");
    return 1;
}

//
// unlock the mutex
//
void free_hamlib()
{
    if(pthread_mutex_unlock(&rigctld_mutex))  perror("FREE_HAMLIB:");
}

int get_rfpower()
{
    value_t val;
    freq_t freq;
    unsigned int mwpower=0;

    if (can_hamlib()) {
	rig_get_level(rig, RIG_VFO_CURR, RIG_LEVEL_RFPOWER, &val);
	freq=kHz(14000);
	rig_power2mW(rig, &mwpower, val.f, freq, RIG_MODE_CW);
	free_hamlib();
    }
   return mwpower / 1000;
}

void set_rfpower(int pow)
{
    value_t val;
    unsigned int mwpower;
    freq_t freq;
    float fpow;

    if (can_hamlib()) {
	fpow=pow;
        mwpower=1000*pow;
        freq=kHz(14000);
        rig_mW2power(rig, &fpow, mwpower, freq, RIG_MODE_CW);
        val.f=fpow;
        rig_set_level(rig, RIG_VFO_CURR, RIG_LEVEL_RFPOWER, val);
	free_hamlib();
    }
}

//
// Produce a carrier for tuning
// To this end, switch to FM and set specified power
// (given in percent of max. power)
//
void rig_tune(int pow)
{
    value_t val;
    freq_t freq;
    float  fpow;
    static rmode_t saved_mode;
    static pbwidth_t saved_width;
    static float saved_power;
    
    if (can_hamlib()) {
        if (pow > 0) {
            rig_get_mode(rig, RIG_VFO_CURR, &saved_mode, &saved_width);
            rig_get_level(rig, RIG_VFO_CURR, RIG_LEVEL_RFPOWER, &val);
            saved_power=val.f;
            fprintf(stderr,"SAVED POWER=%f\n", saved_power);
            rig_set_ptt(rig, RIG_VFO_CURR, RIG_PTT_OFF);
            rig_set_mode(rig, RIG_VFO_CURR, RIG_MODE_FM,rig_passband_normal(rig,RIG_MODE_FM));
            val.f = 0.01*pow;
            rig_set_level(rig, RIG_VFO_CURR, RIG_LEVEL_RFPOWER, val);
            rig_set_ptt(rig, RIG_VFO_CURR, rigctl_ptt_cmd);
        } else {
            rig_set_ptt(rig, RIG_VFO_CURR, RIG_PTT_OFF);
            val.f=saved_power;
            fprintf(stderr,"RESTORED POWER=%f\n", saved_power);
            rig_set_level(rig, RIG_VFO_CURR, RIG_LEVEL_RFPOWER, val);
            rig_set_mode(rig,RIG_VFO_CURR,saved_mode,saved_width);
        }
	free_hamlib();
    }
}

//
// PTT on/off
//
void set_ptt(int ptt)
{
    if (can_hamlib()) {
	if (ptt) rig_set_ptt(rig, RIG_VFO_CURR, rigctl_ptt_cmd); else  rig_set_ptt(rig, RIG_VFO_CURR, RIG_PTT_OFF);
	free_hamlib();
    }
}

//
// callback function. This one copies the name of the rig to a local array of strings
// Concat the Manufacturer and the Model name
//
int  get_rig_name(const struct rig_caps *caps, void *data)
{
    int i;

    // Skip Hamlib NET rigctl since this makes no sense
    if (!strcmp(caps->mfg_name,"Hamlib") && !strcmp(caps->model_name,"NET rigctl")) return 1;

    strcpy(rigs[numrigs].name, caps->mfg_name);
    strcat(rigs[numrigs].name, " ");
    strcat(rigs[numrigs].name, caps->model_name);
    rigs[numrigs].model=caps->rig_model;
    /*
     * FLTK treats '/' in a menu item in a special way, replace it by a colon
     */
    for (i=0; i<strlen(rigs[numrigs].name); i++) if (rigs[numrigs].name[i] == '/') rigs[numrigs].name[i]=':';
    numrigs++;
    return 1;
}

//
// This inits the list of rig model/names
//
void init_riglist()
{
    int i,j,jmin;
    char str[100]; int k;
    numrigs=0;
    //
    // This is the first time we enter hamlib, so set verbosity here
    //
    rig_set_debug(RIG_DEBUG_ERR);       // errors only
    rig_load_all_backends();
    rig_list_foreach(get_rig_name, NULL);
    /*
     * primitive sorting algorithm
     */
    for (i=0; i<numrigs; i++) {
      jmin=i;
      for (j=i+1; j<numrigs; j++) {
         if (strcmp(rigs[j].name,rigs[jmin].name) < 0) jmin=j;
      }
      if (jmin != i) {
        // exchange slots i and jmin
        k = rigs[i].model; rigs[i].model=rigs[jmin].model; rigs[jmin].model=k;
        strcpy(str,rigs[i].name); strcpy(rigs[i].name, rigs[jmin].name); strcpy(rigs[jmin].name, str);
      }
    }
}

//
// Return the number of Rigs (menu items)
//
int get_numrigs()
{
  return numrigs;
}

//
// Return name of Rig #i ( 0 ... numrigs-1)
//
const char *get_rigname(int i)
{
  if (i >= 0 && i < numrigs) return rigs[i].name;
  return "NULL";
}

//
// Set Mode: CW, LSB, USB, Data = USB Data
//
int get_mode()
{
    rmode_t mode;
    pbwidth_t width;
    int ret=-1;

    if (can_hamlib()) {
	rig_get_mode(rig, RIG_VFO_CURR, &mode, &width);
	switch (mode) {
	    case RIG_MODE_RTTY:
	    case RIG_MODE_RTTYR:
	    case RIG_MODE_CWR:
	    case RIG_MODE_CW    : ret=0; break;
	    case RIG_MODE_PKTLSB:
	    case RIG_MODE_LSB   : ret=1; break;
	    case RIG_MODE_USB   : ret=2; break;
	    case RIG_MODE_PKTUSB: ret=3; break;
	    case RIG_MODE_PKTFM:
	    case RIG_MODE_WFM   : 
	    case RIG_MODE_FM    : ret=4; break;
	    case RIG_MODE_AM    : ret=5; break;
	    case RIG_MODE_NONE  :
	    default             :  ret=-1; break;
	}
	free_hamlib();
    }
    return ret;
}

void set_mode(int mode)
{
    mode_t my_mode;
    pbwidth_t my_width;

    if (can_hamlib()) {
	switch(mode) {
            case 0:
		my_mode=RIG_MODE_CW;
		my_width=rig_passband_normal(rig,RIG_MODE_CW);
        	break;
            case 1:
		my_mode=RIG_MODE_LSB;
		my_width=rig_passband_normal(rig,RIG_MODE_LSB);
        	break;
            case 2:
		my_mode=RIG_MODE_USB;
		my_width=rig_passband_normal(rig,RIG_MODE_USB);
        	break;
            case 3:
		my_mode=RIG_MODE_PKTUSB;
		my_width=rig_passband_wide(rig,RIG_MODE_PKTUSB);
        	break;
            case 4:
		my_mode=RIG_MODE_FM;
		my_width=rig_passband_wide(rig,RIG_MODE_FM);
        	break;
            case 5:
		my_mode=RIG_MODE_AM;
		my_width=rig_passband_wide(rig,RIG_MODE_AM);
        	break;
	}
	rig_set_mode(rig,RIG_VFO_CURR,my_mode,my_width);
	free_hamlib();
    }
}

/*
 * get_ptt returns 0 if ptt is OFF; 1 is ptt is ON, and -1 if
 * the PTT state cannot be determined
 */
int get_ptt()
{
    ptt_t ptt;
    int ret=-1;
    if (can_hamlib()) {
        if (rig_get_ptt(rig, RIG_VFO_CURR, &ptt) == RIG_OK) {
	  if (ptt) ret=1; else ret=0;
	}
	free_hamlib();
    }
    return ret;
}

double get_freq()
{
    double ret=0.0;
    freq_t freq;
    if (can_hamlib()) {
	rig_get_freq(rig, RIG_VFO_CURR, &freq);
	ret = freq;
	free_hamlib();
    }
    return ret;
}

//
// Set speed of Keyer
//
int get_cwspeed()
{
    value_t val;
    val.i=0;
    if (can_hamlib()) {
	rig_get_level(rig, RIG_VFO_CURR, RIG_LEVEL_KEYSPD, &val);
	free_hamlib();
    }
    return val.i;
}

void set_cwspeed(int i)
{
    value_t val;
    // set keyer speed on rig even if we use WinKey
    if (wkeymode == 2) set_winkey_speed(i);
    if (can_hamlib()) {
	if (i < 12) i=12;
	val.i=i;
	rig_set_level(rig, RIG_VFO_CURR, RIG_LEVEL_KEYSPD, val);
	free_hamlib();
    }
}

void send_cw(const char *msg)
{
    switch (wkeymode) {
        case 1:
           if (can_hamlib()) {
		rig_send_morse(rig, RIG_VFO_CURR, msg);
		free_hamlib();
	   }
           break;
	case 2:
	    send_winkey(msg);
           break;
    }
}

void close_hamlib()
{
    int ret;

    if (can_hamlib()) {
	rig_close(rig);
	rig_cleanup(rig);
	rig=NULL;
	// wait for rigctl thread to terminate
	if(pthread_join(rigctld_thread, NULL)) perror("CLOSE:JOIN:");
	free_hamlib();
    }
    // close wkey device
    if (wkeymode == 2 ) close_winkey_port();
    wkeymode=0;
}


//
// Open rig via hamlib
//
int open_hamlib(const char* rigdev, const char* pttdev, const char *wkeydev, int baud, int model)
{

    char val[50];
    int err;
    pthread_attr_t attr;

    close_hamlib();

    if (!strcmp(wkeydev,"CAT")) {
        wkeymode=1;
    } else  {
	if (open_winkey_port(wkeydev)) wkeymode=2;
    }

    // init Hamlib
    rig_set_debug(RIG_DEBUG_ERR);
    rig = rig_init(rigs[model].model); if (!rig) goto err_ret;
    err = rig_set_conf(rig, rig_token_lookup(rig,"itu_region"), "1");
    err = rig_set_conf(rig, rig_token_lookup(rig,"data_bits"), "8");
    if (baud == 4800) {
      err = rig_set_conf(rig, rig_token_lookup(rig,"stop_bits"), "2");
    } else {
      err = rig_set_conf(rig, rig_token_lookup(rig,"stop_bits"), "1");
    }
    err = rig_set_conf(rig, rig_token_lookup(rig,"serial_parity"), "None");
    err = rig_set_conf(rig, rig_token_lookup(rig,"serial_handshake"), "Hardware");
    sprintf(val, "%d", baud); err=rig_set_conf(rig, rig_token_lookup(rig,"serial_speed"), val);

    err = rig_set_conf(rig, rig_token_lookup(rig,"rig_pathname"), rigdev);
    rigctl_ptt_cmd = RIG_PTT_ON; //default value
    if (!strcmp(pttdev,"CAT-Data") || !strcmp(pttdev,"CAT-Mic")) {
	if (rig->caps->ptt_type == RIG_PTT_RIG_MICDATA) {
	  // rig does support two separate CAT PTT commands, so
	  // remember which one to use.
	  if (!strcmp(pttdev,"CAT-Data")) rigctl_ptt_cmd=RIG_PTT_ON_DATA;
	  if (!strcmp(pttdev,"CAT-Mic"))  rigctl_ptt_cmd=RIG_PTT_ON_MIC;
          err = rig_set_conf(rig, rig_token_lookup(rig,"ptt_type"), "RIGMICDATA");
	} else {
	  // if rig does not have two separate PTT commands, use RIG_PTT_ON
	  // in either case and set ptt_type to RIG_PTT_RIG.
	  err = rig_set_conf(rig, rig_token_lookup(rig,"ptt_type"), "RIG");
	}
    } else { 
        err = rig_set_conf(rig, rig_token_lookup(rig,"ptt_pathname"), pttdev);
        err = rig_set_conf(rig, rig_token_lookup(rig,"ptt_type"), "DTR");
    }
    err = rig_open(rig);
    if (err != RIG_OK) goto err_ret;
    //
    // Start a rigctld server
    //
    // Ignore SIGPIPE, since this may happen if a client is not interested in an answer
    signal(SIGPIPE, SIG_IGN);
    if (pthread_attr_init(&attr))                                   perror("ThreadAttrInit:");
    // Start the ball rolling
    if (pthread_create(&rigctld_thread, &attr, rigctld_func, NULL)) perror("THREAD CREATE:");
    return 1;

err_ret:
    rig_close(rig);
    rig_cleanup(rig);
    rig=NULL;
    return 0;
    
}

#include "rigctl_parse.h"
extern FILE *fmemopen(void *, size_t, const char *);

void *rigctld_serve(void * w)
{
    int sock = *(int *) w;
#define INPUTSIZE 128		// max one line
#define OUTPUTSIZE 4096		// headroom for dump_state
    char input[INPUTSIZE+2];	// one extra char for terminating zero and eol
    char output[OUTPUTSIZE+1];  // one extra char for terminating zero
    char *cp;
    size_t insize;
    FILE *fpin, *fpout;
    fd_set fds;
    struct timeval tv;
    int ret;

    //
    // we have opened input and output channels, so repeatedly execute hamlib commands
    // This loop terminates if a rigctl "q" command is executed, and if some
    // error condition occurs (rig close or I/O error)
    //
    for (;;) {
        // read exactly on line from the socket in invoke rigctl_parse
        // while waiting for characters to arrive, watch out for closing the rig
        insize=0;
        for (;;) {
	    // infinite loop: wait for characters to arrive, "break" if line is full
            if (!rig) {
		TRACE("RIGCTLD: stopping  server, socket fd=%d\n",sock);
                close(sock);
                return NULL;
            }
            tv.tv_sec=0;
            tv.tv_usec=100000;
            FD_ZERO(&fds);
            FD_SET(sock, &fds);
            ret=select(sock+1, &fds, NULL, NULL, &tv);
            if (ret == 0) continue;
            if (ret < 0) {
                // Socket broken, client terminated ungracefully
                ERROR("RIGCTLD: broken pipe (select) -- Closing connetion\n");
		close(sock);
		return NULL;
            }
            // 1 char arrived
            ret=read(sock, input+insize, 1);
            if (ret <= 0) {
                // Socket broken, client terminated ungracefully
                ERROR("RIGCTLD: broken pipe (read) -- Closing connection\n");
		close(sock);
		return NULL;
            }
            if (insize >= INPUTSIZE) {
                ERROR("RIGCTLD: line buffer overflow -- Closing connection\n");
		close(sock);
		return NULL;
            }
            // allow alternate facts for line ending
            if (input[insize] == 0x0d) input[insize]=0x0a;
            if (input[insize] == 0x00) input[insize]=0x0a;
            if (input[insize++] == 0x0a) break;
        }
        input[insize++]=0;
        DEBUG("RIGCTLD: ToHamlibCmd=%s",input);
        // get exclusive access
        if(pthread_mutex_lock(&rigctld_mutex)) perror("RIGCTLD: mutex lock:");
        if (rig) {
            // since we are exclusive here, we cannot lose the rig
            // via hamlib_close in the main thread
            // I/O to and from rigctl_parse via a "memory STREAM"
	    int  ext_resp=0;
            int  vfo_mode=0;
	    char resp_sep='\n';
	    char send_cmd_term = '\r';
            fpin=fmemopen(input, insize, "r");
            fpout=fmemopen(output, OUTPUTSIZE, "w");
            memset(output, 0, OUTPUTSIZE);
            DEBUG("RIGCTLD: calling Hamlib\n");
            ret=rigctl_parse(rig,		// The rig
                             fpin, 		// The input stream
			     fpout, 		// The output stream
			     NULL, 0, 		// argv and argc dummy
			     NULL,		// sync_cb dummy
			     1,			// interactive
			     0,			// prompt
			     &vfo_mode,		// vfo_modeode
			     send_cmd_term,	// send_cmd_term
			     &ext_resp,		// unused
			     &resp_sep);    	// unused
            DEBUG("RIGCTLD: Hamlib execution finished\n");
            fclose(fpin);
            fclose(fpout);
        }
        if (pthread_mutex_unlock(&rigctld_mutex)) perror("RIGCTLD: mutex unlock:");
        //
        // quit if error, empty line, of "q" command. Then there is no response
        //
        if (ret < 0 || ret == 1) {
	    TRACE("RIGCTLD: stopping  server, socket fd=%d\n",sock);
	    close(sock);
	    return NULL;
	}
        //
        // return result to the client. If pipe is broken, quit
        //
        output[OUTPUTSIZE]=0;   // just in case
        DEBUG("%s",output);
        cp=output;
        while (*cp) {
            if (write(sock, cp++, 1) != 1) {
                ERROR("RIGCTLD: broken pipe (write)\n");
		close(sock);
		return NULL;
            }
        }
        DEBUG("RIGCTLD sent back %d chars, ret=%d\n",(int) (cp-output),ret);
    }
}

void *rigctld_func(void * w)
{
    int ret;
    struct addrinfo hints;
    struct addrinfo *result;
    const char *portno = "4532";
    const char *src_addr = "localhost";
    int sock_listen;
    int sockopt;
    int sock;
    int reuseaddr = 1;
    struct sockaddr_storage cli_addr;
    socklen_t clilen;
    fd_set fds;
    struct timeval tv;
    pthread_t thread;
    pthread_attr_t attr;

    TRACE("RIGCTLD: starting daemon\n");

    /*
     * The following code is essentially from rigctld.c in the hamlib distro
     */

    /*
     * Prepare listening socket
     */
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;     /* Allow IPv4 or IPv6 */
    hints.ai_socktype = SOCK_STREAM;/* TCP socket */
    hints.ai_flags = AI_PASSIVE;    /* For wildcard IP address */
    hints.ai_protocol = 0;          /* Any protocol */

    ret = getaddrinfo(src_addr, portno, &hints, &result);
    if (ret != 0) {
	ERROR("RIGCTLD: GetAddrInfo: %s\n",gai_strerror(ret));
        return NULL;
    }

    // try accepting connections from any network
    do {
	sock_listen = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (sock_listen < 0) {
	    perror("RIGCTLD:SOCKET:");
	    return NULL;
	}
	if (setsockopt(sock_listen, SOL_SOCKET, SO_REUSEADDR, (char *)&reuseaddr, sizeof(reuseaddr)) < 0) {
	    perror("RIGCTLD:SETSOCKOPT:REUSE:");
	    return NULL;
	}
        if (!bind(sock_listen, result->ai_addr, result->ai_addrlen)) break;
	close(sock_listen);
    } while ((result = result->ai_next) != NULL);

    if (result == NULL) {
	ERROR("Bind error, no suitable interface found\n");
	return NULL;
    }
    if (listen(sock_listen, 4) < 0) {
	perror("RIGCTLD:LISTEN:");
        close(sock_listen);
	return NULL;
    }
    /*
     * Now we have a socket, we are accepting connections.
     * This is an "infinite loop", we are exiting only if
     * the rig is closed via "return NULL". Before returning,
     * close open file descriptors!
     */
    while(1) {
	if (!rig) {
	    TRACE("RIGCTLD: stopping daemon (hamlib closed)\n");
	    close(sock_listen);
	    return NULL;
	}
	// wait for connection via select() such that we can check for thread termination
	FD_ZERO(&fds);
	FD_SET(sock_listen, &fds);
	tv.tv_sec=0;
	tv.tv_usec=250000;
	if (select(sock_listen+1, &fds, NULL, NULL, &tv) < 1) continue;
	sock=accept(sock_listen, (struct sockaddr *)&cli_addr, &clilen);
	if (sock < 0) continue;
        TRACE("RIGCTLD: start server socket fd=%d\n",sock);
	//
	// Spawn a thread which listens to this socket and serves the client.
	// Important: since we keep not track of the threads created here, put
	// them in a DETACHED state so we need not JOIN them.

	if(pthread_attr_init(&attr))                                    perror("RIGCTLD:serve:nit\n");
	if(pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED)) perror("RIGCTLD:serve:DETACH\n");
	if(pthread_create(&thread, &attr, rigctld_serve, &sock))        perror("RIGCTLD:serve:create\n");

	// continue: accept new connections until rig is closed
    }
    /* NOTREACHED */
    return NULL;
}
