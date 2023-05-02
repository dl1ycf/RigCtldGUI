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

//#define MYDEBUG(s, ...) fprintf(stderr,s, ##__VA_ARGS__)
//#define MYTRACE(s, ...) fprintf(stderr,s, ##__VA_ARGS__)
#define MYERROR(s, ...) fprintf(stderr,s, ##__VA_ARGS__)

#ifndef MYDEBUG
#define MYDEBUG(s, ...)
#endif
#ifndef MYTRACE
#define MYTRACE(s, ...)
#endif
#ifndef MYERROR
#define MYERROR(s, ...)
#endif

//
// This is for the threads etc. that we split off
//
static pthread_t       rigctld_thread;
static pthread_mutex_t rigctld_mutex = PTHREAD_MUTEX_INITIALIZER;
void *                 rigctld_func (void *);
void *                 rigctld_serve(void *);

//
// This is for the threads generated for each connetion
// We need to keep track of them since we have to "kill" them
// since they might get stuck into a syscall
//
#define MAX_CLIENT 5

struct _CLIENT {
  int            id;
  pthread_t      thread;
  pthread_attr_t attr;
  int            sock;
  FILE           *fsockin;
  FILE           *fsockout;
}; 

typedef struct _CLIENT CLIENT;

CLIENT clients[MAX_CLIENT];

// Functions in winkey.c
extern int  open_winkey_port(const char *);
extern void close_winkey_port(void);
extern void send_winkey(const char *);
extern void set_winkey_speed(int);

RIG *rig = NULL;
int  running=0;

int wkeymode=0;   // 0: n/a, 1: CAT,  2: Wkey-Device

static ptt_t rigctl_ptt_cmd=RIG_PTT_ON;

static int        numrigs;
static struct rigs {
  rig_model_t model;
  char name[100];
} rigs[1000];

//
// Obtain (flag===1) or release (flag==0) the hamlib mutex
// This is a function to be passed to rigctl_parse
//
void sync_hamlib(int flag) {
  //
  // This is *only* called by the rigctl client server threads.
  // If we arrive here while the rig is being closed, just terminate
  // the thread
  //
  if (!running) pthread_exit(NULL);
  //
  if (flag) {
    // obtain mutex
    if (pthread_mutex_lock(&rigctld_mutex)) perror("SYNC_HAMLIB_LOCK:");
  } else {
    // release mutex
    if(pthread_mutex_unlock(&rigctld_mutex))  perror("SYNC_HAMLIB_RELEASE:");
  }
}

//
//  return 1 if we can use hamlib, that is:
//  - rig is defined (non-NULL)
//  - we have a locked mutex
//
int can_hamlib()
{
    if (!rig || !running) return 0;
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
            rig_set_ptt(rig, RIG_VFO_CURR, RIG_PTT_OFF);
            rig_set_mode(rig, RIG_VFO_CURR, RIG_MODE_FM,rig_passband_normal(rig,RIG_MODE_FM));
            val.f = 0.01*pow;
            rig_set_level(rig, RIG_VFO_CURR, RIG_LEVEL_RFPOWER, val);
            rig_set_ptt(rig, RIG_VFO_CURR, rigctl_ptt_cmd);
        } else {
            rig_set_ptt(rig, RIG_VFO_CURR, RIG_PTT_OFF);
            val.f=saved_power;
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
      MYTRACE("close_hamlib: got mutex\n");
      //
      // This is the only place where the "running" flag is cleared
      //
      running=0;
      //
      // rigctld_func uses select() with a timeout, so it will terminate
      // shortly after setting running to zero
      //
      if(pthread_join(rigctld_thread, NULL)) perror("CLOSE:JOIN main:");
      MYTRACE("close_hamlib: joined main rigctld thread\n");
      //
      // now terminate all client threads. These are most likely stuck in
      // a read() from the socket so we have to send a signal.
      //
      for (int i=0; i<MAX_CLIENT; i++) {
        //
        // if "thread" is NULL, then the client has already performed the clean-up
        //
        if (clients[i].thread) {
          if (pthread_kill(clients[i].thread,SIGALRM)) perror("CLOSE:KILL");
          MYTRACE("About to join id=%d\n", i);
          if (pthread_join(clients[i].thread,NULL)) perror("CLOSE:JOIN client");
          MYTRACE("Join successful\n");
          clients[i].thread=NULL;
          if (clients[i].sock >= 0) close(clients[i].sock);
          if (clients[i].fsockin)   fclose(clients[i].fsockin);
          if (clients[i].fsockout)  fclose(clients[i].fsockout);
        }
      }
      //
      // Close the rig and release all memory associated with "rig"
      //
      rig_close(rig);
      rig_cleanup(rig);
      rig=NULL;
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

    //
    // Note that because close_hamlib uses can_hamlib(), it is a no-op if
    // the rig is not open
    //
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
    running=1;
    //
    // Start a rigctld server
    //
    // Ignore SIGPIPE, since this may be raised when we send the answer of a command
    // while the client has already closed the connection.
    //
    signal(SIGPIPE, SIG_IGN);
    if (pthread_attr_init(&attr)) {
      perror("ThreadAttrInit rigctld_func:");
    }
    if (pthread_create(&rigctld_thread, &attr, rigctld_func, NULL)) {
      perror("THREAD CREATE rigctld_func:");
    }
    return 1;

err_ret:
    rig_close(rig);
    rig_cleanup(rig);
    rig=NULL;
    return 0;
    
}

//
// This is to enforce exiting a thread when a SIGALRM is received
// Only use this for threads which might get stuck in a system call
// Do not use SIGKILL or SIGTERM for this purpose, because it kills
// the whole process (including all threads).
//
void alarm_handler(int signum) {
  MYTRACE("ALARM HANDLER sig=%d\n", signum);
  pthread_exit(NULL);
}

#include "rigctl_parse.h"

void *rigctld_serve(void* arg)
{
    CLIENT *client = (CLIENT *)arg;

    int ret;
    int  ext_resp=0;
    int  vfo_mode=0;
    char resp_sep='\n';
    char send_cmd_term = '\r';

    signal(SIGALRM, alarm_handler);

    client->fsockin=fdopen(client->sock,"rb");
    client->fsockout=fdopen(client->sock,"wb");

    if (!client->fsockin || !client->fsockout) {
        MYTRACE("RIGCTLD: fdopen failed, giving up\n");
        if (client->fsockin)  fclose(client->fsockin);
        if (client->fsockout) fclose(client->fsockout);
        close(client->sock);
        client->fsockin=NULL;
        client->fsockout=NULL;
        client->sock=-1;
        return NULL;
    }
  
    while (running) {
      ret=rigctl_parse(rig,                // The rig
                       client->fsockin,    // The input stream
                       client->fsockout,   // The output stream
                       NULL, 0,            // argv and argc dummy
                       sync_hamlib,        // function to get/release mutex
                       1,                  // interactive
                       0,                  // prompt
                       &vfo_mode,          // vfo_mode
                       send_cmd_term,      // send_cmd_term
                       &ext_resp,          // constant
                       &resp_sep,          // constant
                       0);                 // use password
      MYDEBUG("RIGCTLD: Hamlib command executed, ret=%d\n",ret);

      //
      // simply continue upon "deprecated", "inval", and "not implemented" error conditions
      //
      if (ret < 0 && ret != -RIG_EDEPRECATED && ret != -RIG_EINVAL && ret != -RIG_ENIMPL) break;
    }
    //
    // If the socket has been closed by the client, clean up under a lock.
    // If we come here because hamlib is being closed (runing == 0), then
    // simply return.
    //
    if (can_hamlib()) {
      MYTRACE("RIGCTLD: stopping  server, socket fd=%d\n",client->sock);
      fclose(client->fsockin);
      fclose(client->fsockout);
      close(client->sock);
      client->thread=NULL;
      client->fsockin=NULL;
      client->fsockout=NULL;
      client->sock=-1;
      free_hamlib();
    }
    return NULL;
}

void *rigctld_func(void * w)
{
    int ret;
    struct addrinfo hints;
    struct addrinfo *result;
    const char *portno = "4532";
    const char *src_addr = NULL; /* INADDR_ANY */
    int sock_listen;
    int sockopt;
    int id;
    int reuseaddr = 1;
    struct sockaddr_storage cli_addr;
    socklen_t clilen;
    fd_set fds;
    struct timeval tv;

    MYTRACE("RIGCTLD: starting daemon\n");
    for (id=0; id < MAX_CLIENT; id++) {
      clients[id].id=id;
      clients[id].thread=NULL;
      clients[id].sock=-1;
      clients[id].fsockin=NULL;
      clients[id].fsockout=NULL;
    }

    /*
     * The following code is essentially from rigctld.c in the hamlib distro
     */

    /*
     * Prepare listening socket
     */
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;                 // Allow IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM;             // TCP socket
    hints.ai_flags = AI_PASSIVE;                 // For wildcard IP address
    hints.ai_protocol = 0;                       // Any protocol

    ret = getaddrinfo(src_addr, portno, &hints, &result);
    if (ret != 0) {
        MYERROR("RIGCTLD: GetAddrInfo: %s\n",gai_strerror(ret));
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
        MYERROR("Bind error, no suitable interface found\n");
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
    while(running) {
        // wait for an "unused" client
        id=0;
        while (1) {
          if (!clients[id].thread) break;
          if (id++ >= MAX_CLIENT) break;
        }
        if (id >= MAX_CLIENT) {
          MYTRACE("No free client found!\n");
          usleep(500000);
          continue;
        }
        
        // wait for connection via select() such that we can check for thread termination
        FD_ZERO(&fds);
        FD_SET(sock_listen, &fds);
        tv.tv_sec=0;
        tv.tv_usec=500000;
        if (select(sock_listen+1, &fds, NULL, NULL, &tv) < 1) continue;
        clients[id].sock=accept(sock_listen, (struct sockaddr *)&cli_addr, &clilen);
        if (clients[id].sock < 0) continue;
        MYTRACE("RIGCTLD: start server id=%d socket fd=%d\n",id,clients[id].sock);
        //
        // Spawn a thread which listens to this socket and serves the client.
        //
        if(pthread_attr_init(&clients[id].attr)) {
            perror("RIGCTLD:serve:init\n");
        }
        if(pthread_create(&clients[id].thread, &clients[id].attr, rigctld_serve, &clients[id])) {
            perror("RIGCTLD:serve:create\n");
        }

        // continue: accept new connections until rig is closed
    }
    MYTRACE("RIGCTLD: stopping daemon (hamlib closed)\n");
    close(sock_listen);
    return NULL;
}
