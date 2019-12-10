//
// Note this is C++, since FLTK has C++ bindings
//
// The other files that actually talk to rig/winkey are pure C, though.
//
#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Choice.H>
#include <FL/fl_ask.H>
#include <FL/Fl_Output.H>
#include <FL/Fl_Text_Display.H>
#include <FL/Fl_Text_Buffer.H>
#include <FL/Fl_Slider.H>

#include "portaudio.h"
#include "math.h"
#include <unistd.h>
#include <glob.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <ctype.h>
#include <sys/stat.h>

#ifdef __APPLE__
#define SER_PORT_BASIS "/dev/tty."
#endif
#ifdef __linux__
#define SER_PORT_BASIS "/dev/serial/by-id/"
#endif
#ifndef SER_PORT_BASIS
#define SER_PORT_BASIS "/dev/ttyS"
#endif

/*
 * This sets some defaults.
 * They are to be overrun by the preferences
 * in $HOME/.rigcontrol/Prefs
 *
 */

char maincall[20]="DL1AAA";
char altcall[20]="DL0BBB";

char cwlab1[10]= "call";
char cwlab2[10]= "cq";
char cwlab3[10]= "test";
char cwlab4[10]= "cfm";
char cwlab5[10]= "tu";

char cwtxt1[80]="#";
char cwtxt2[80]="cq cq cq de # # cq cq de # + k";
char cwtxt3[80]="test de # # test";
char cwtxt4[80]="cfm ur 5nn";
char cwtxt5[80]="tu 73";


/*
 * Some default values for rig connection.
 * The actual values chosen are dumped
 * to a Prefs file (in the current directory
 * or within the APP bundle) whenever a
 * rig is opened. So usually this file exists
 * and those values are taken.
 */

char defrig[150]="uh-rig";
char defptt[150]="CAT-Data";
char defwky[150]="CAT";
char defsnd[150]="Built-in output";
char defham[150]="Hamlib Dummy";
int  defbaud=9600;

const char *rigdev[150], *pttdev[150], *wkeydev[150];
int        numrigdev, numpttdev, numwkeydev;
int        myrigdev, mypttdev, mywkeydev;
int        baudrates[]={1200,2400,4800, 9600, 19200,38400,115200};
int        numbaud = sizeof(baudrates)/sizeof(int);
int        mybaud=0;
int        mymodel;
const char *sounddevname[20];
int sounddev[20];  int numsounddev=0;  int sounddevice=-1; int mysounddev=-1;

const char *mycall;

int        numrigs;
struct rigs {
  int model;
  char *name;
} rigs[500];

extern "C" int  open_hamlib(const char *, const char*, const char*, int, int);
extern "C" void close_hamlib();
extern "C" void send_cw(const char *);

extern "C" void set_cwspeed(int);
extern "C" int  get_cwspeed();

extern "C" void set_mode(int);
extern "C" int  get_mode();

extern "C" void set_rfpower(int);
extern "C" int  get_rfpower();

extern "C" double get_freq();
extern "C" int    get_ptt();

extern "C" void set_ptt(int);
extern "C" void init_riglist();
extern "C" int  get_numrigs();
extern "C" const char *get_rigname(int i);
extern "C" void rig_tune(int);

void do_pow(Fl_Widget *, void *);
void do_mode(Fl_Widget *, void *);
void do_speed(Fl_Widget *, void *);

void do_cw(Fl_Widget *, void *);

void do_cwtxt(Fl_Widget *, void *);
void do_cwinp(Fl_Widget *, void *);
void do_modeinp(Fl_Widget *, void *);
void cw_txtout(char *, int);


void do_call (Fl_Widget *, void *);
void do_voice(Fl_Widget *, void *);
void do_tune (Fl_Widget *, void *);
#ifdef __APPLE__
void do_mldx (Fl_Widget *, void *);
static int mldx_flag=1;
static double mldx_last_freq = -1.0;
#endif

static double master_volume = 0.0316;   // -30 dB
void do_volume(Fl_Widget *, void *);


void choose_model(Fl_Widget *, void *);
void choose_serial(Fl_Widget *, void *);
void choose_wkey(Fl_Widget *, void *);
void choose_ptt(Fl_Widget *, void *);
void choose_baud(Fl_Widget *, void *);
void choose_sound(Fl_Widget *, void *);
void do_tone(int,int);
void update_freq(void *);

void open_rig(Fl_Widget *, void*);

typedef struct {
  float *samples;
  int   numsamples;
} sample;

sample wav[5];

#define lentab 480
static double sintab[lentab];
static int tonept;

#define FACT1 0.11780972450961724644234912687298  //  9 * (2 Pi) / 480,   900 Hz tone
#define FACT2 0.22252947962927702105777057298230  // 17 * (2 Pi) / 480,  1700 Hz tone

int getwav(char *, sample *);

char workdir[PATH_MAX];

static Fl_Button *pow1, *pow2, *pow3, *pow4, *pow5;
static Fl_Button *speed1, *speed2, *speed3, *speed4, *speed5;
static Fl_Button *mode1, *mode2, *mode3, *mode4, *mode5;
static Fl_Button *tune1, *tune2, *tune3, *tune4, *tune5;
static Fl_Output *freq;
static Fl_Text_Display *cwsnt;
static Fl_Text_Buffer *cwsntbuf;
static Fl_Input *cwinp,*mode;

void get_token(FILE *fp, char *token, int ltoken, char *label, int llabel)
{
    int i,j;
    char cp[200];
    char *p;
    size_t linecap= 200;
    p=cp;
    i=getline(&p, &linecap, fp);
    if (cp[i-1]=='\n') {
      cp[--i]=0;
    }
    if ( i<=0 ) return;
    if (llabel == 0) {
      if (i > 0 && i < ltoken) {
        sprintf(token, "%s", cp);
      }
      return;
    }
    // the line must be of the form string1:string2
    p=strstr(cp, ":");
    if (!p) return;
    j=p-cp+1;
    if (j <= 0 || j > llabel || i - j > ltoken || i-j <= 0) return;
    strncpy(label, cp, j-1);
    label[j-1]=0;
    strcpy(token, p+1);
}

int main(int argc, char **argv) {

  int i;
  char str[20];

  char *cp;
  FILE *fp;
  char filename[PATH_MAX];
  struct stat fstat;

  // obtain $HOME/.rigcontrol dir
  strcpy(workdir,".");
  cp=getenv("HOME");
  if (cp) {
      // look if $HOME/.rigcontrol exists, otherwise make it
      sprintf(filename,"%s/.rigcontrol",cp);
      if (stat(filename, &fstat) != 0) {
        if (mkdir(filename,0755) == 0) strcpy(workdir,filename);
      } else {
	strcpy(workdir,filename);
      }
  }
  
  // load values
  sprintf(filename,"%s/Settings",workdir);
  printf("Taking settings from %s\n",filename);
  fp=fopen(filename,"r");
  if (fp) {
    get_token(fp, maincall, 20, NULL, 0);
    get_token(fp, altcall , 20, NULL, 0);
    get_token(fp, cwtxt1  , 80, cwlab1, 10);
    get_token(fp, cwtxt2  , 80, cwlab2, 10);
    get_token(fp, cwtxt3  , 80, cwlab3, 10);
    get_token(fp, cwtxt4  , 80, cwlab4, 10);
    get_token(fp, cwtxt5  , 80, cwlab5, 10);
    fclose(fp);
  }
  // load values from local prefs file
  sprintf(filename,"%s/Prefs",workdir);
  fp=fopen(filename,"r");
  if (fp) {
    get_token(fp, defrig, 150, NULL, 0);
    get_token(fp, defptt, 150, NULL, 0);
    get_token(fp, defwky, 150, NULL, 0);
    fscanf(fp,"%d\n",&defbaud);
    get_token(fp, defham, 150, NULL, 0);
    get_token(fp, defsnd, 150, NULL, 0);
    fclose(fp);
  }
 

  Fl_Window *window = new Fl_Window(512,500);
  window->label("Rig Controller");

  Fl_Box *cwinplab = new Fl_Box(10,70, 30, 20, "Inp");
  cwinplab->box(FL_FLAT_BOX);
  cwinplab->labelfont(FL_BOLD);
  cwinplab->labelsize(14);

  Fl_Box *cwtxtlab = new Fl_Box(10,30, 30, 20, "Snt");
  cwtxtlab->box(FL_FLAT_BOX);
  cwtxtlab->labelfont(FL_BOLD);
  cwtxtlab->labelsize(14);

  cwsntbuf = new Fl_Text_Buffer();

  cwsnt = new Fl_Text_Display(40, 10, 460, 60, (char *) 0); cwsnt->textsize(12); cwsnt->textcolor(1); cwsnt->textfont(FL_COURIER);
  cwsnt->buffer(cwsntbuf);
  cwsnt->wrap_mode(Fl_Text_Display::WRAP_AT_PIXEL, 450);
  cwsnt->scrollbar_width(0);

  cwinp = new Fl_Input (40, 70, 460, 25, "");
  cwinp->textsize(12);
  cwinp->textcolor(0);
  cwinp->textfont(FL_COURIER);
  cwinp->callback(do_cwinp, NULL);
  cwinp->when(FL_WHEN_CHANGED | FL_WHEN_ENTER_KEY_ALWAYS);

  Fl_Box *lab1  = new Fl_Box(440,100,80,20,"Power");
  lab1->box(FL_FLAT_BOX);
  lab1->labelfont(FL_BOLD);
  lab1->labelsize(14);

  Fl_Group *g1 = new Fl_Group(440,100,80,240);
  pow1 = new Fl_Button(460,130,  40, 20, "  5"); pow1->type(FL_RADIO_BUTTON);  pow1->color(7,5); pow1->callback(do_pow, (void*)   5);
  pow2 = new Fl_Button(460,160,  40, 20, " 10"); pow2->type(FL_RADIO_BUTTON);  pow2->color(7,5); pow2->callback(do_pow, (void*)  10);
  pow3 = new Fl_Button(460,190,  40, 20, " 30"); pow3->type(FL_RADIO_BUTTON);  pow3->color(7,5); pow3->callback(do_pow, (void*)  30);
  pow4 = new Fl_Button(460,220,  40, 20, " 50"); pow4->type(FL_RADIO_BUTTON);  pow4->color(7,5); pow4->callback(do_pow, (void*)  50);
  pow5 = new Fl_Button(460,250,  40, 20, "100"); pow5->type(FL_RADIO_BUTTON);  pow5->color(7,5); pow5->callback(do_pow, (void*) 100);
  g1->end();


  Fl_Box *lab2  = new Fl_Box(360,100,80,20,"CW wpm");
  lab2->box(FL_FLAT_BOX);
  lab2->labelfont(FL_BOLD);
  lab2->labelsize(14);

  Fl_Group *g2 = new Fl_Group(360,100,80,240);
  speed1 = new Fl_Button(380,130,  40, 20, "12"); speed1->type(FL_RADIO_BUTTON);  speed1->color(7,5); speed1->callback(do_speed, (void *) 12);
  speed2 = new Fl_Button(380,160,  40, 20, "16"); speed2->type(FL_RADIO_BUTTON);  speed2->color(7,5); speed2->callback(do_speed, (void *) 16);
  speed3 = new Fl_Button(380,190,  40, 20, "20"); speed3->type(FL_RADIO_BUTTON);  speed3->color(7,5); speed3->callback(do_speed, (void *) 20);
  speed4 = new Fl_Button(380,220,  40, 20, "24"); speed4->type(FL_RADIO_BUTTON);  speed4->color(7,5); speed4->callback(do_speed, (void *) 24);
  speed5 = new Fl_Button(380,250,  40, 20, "30"); speed5->type(FL_RADIO_BUTTON);  speed5->color(7,5); speed5->callback(do_speed, (void *) 30);
  g2->end();

  Fl_Box *lab3  = new Fl_Box(280,100,80,20,"CW TXT");
  lab3->box(FL_FLAT_BOX);
  lab3->labelfont(FL_BOLD);
  lab3->labelsize(14);

  Fl_Button *cw1 = new Fl_Button(280,130,  80, 20, cwlab1); cw1->type(FL_TOGGLE_BUTTON); cw1->color(7,2); cw1->callback(do_cwtxt, (void *) cwtxt1);
  Fl_Button *cw2 = new Fl_Button(280,160,  80, 20, cwlab2); cw2->type(FL_TOGGLE_BUTTON); cw2->color(7,2); cw2->callback(do_cwtxt, (void *) cwtxt2);
  Fl_Button *cw3 = new Fl_Button(280,190,  80, 20, cwlab3); cw3->type(FL_TOGGLE_BUTTON); cw3->color(7,2); cw3->callback(do_cwtxt, (void *) cwtxt3);
  Fl_Button *cw4 = new Fl_Button(280,220,  80, 20, cwlab4); cw4->type(FL_TOGGLE_BUTTON); cw4->color(7,2); cw4->callback(do_cwtxt, (void *) cwtxt4);
  Fl_Button *cw5 = new Fl_Button(280,250,  80, 20, cwlab5); cw5->type(FL_TOGGLE_BUTTON); cw5->color(7,2); cw5->callback(do_cwtxt, (void *) cwtxt5);

  Fl_Box *lab4  = new Fl_Box(340,370,80,20,"Active Callsign");
  lab4->box(FL_FLAT_BOX);
  lab4->labelfont(FL_BOLD);
  lab4->labelsize(14);

  Fl_Group *g3 = new Fl_Group(310,370,80,100);
  Fl_Button *call1 = new Fl_Button(320, 400, 120, 20, maincall); call1->type(FL_RADIO_BUTTON);  call1->color(7,2); call1->callback(do_call, (void *) maincall); call1->setonly();
  Fl_Button *call2 = new Fl_Button(320, 430, 120, 20, altcall ); call2->type(FL_RADIO_BUTTON);  call2->color(7,2); call2->callback(do_call, (void *) altcall);
  do_call(NULL, (void *) maincall);
  g3->end();

  Fl_Box *lab5  = new Fl_Box(180,100,80,20,"Mode");
  lab5->box(FL_FLAT_BOX);
  lab5->labelfont(FL_BOLD);
  lab5->labelsize(14);

  Fl_Group *g4 = new Fl_Group(180,100,80,240);
  mode1 = new Fl_Button(180,130, 80, 20, "CW");       mode1->type(FL_RADIO_BUTTON); mode1->color(7,5); mode1->callback(do_mode, (void *) 0);
  mode2 = new Fl_Button(180,160, 80, 20, "LSB");      mode2->type(FL_RADIO_BUTTON); mode2->color(7,5); mode2->callback(do_mode, (void *) 1);
  mode3 = new Fl_Button(180,190, 80, 20, "USB");      mode3->type(FL_RADIO_BUTTON); mode3->color(7,5); mode3->callback(do_mode, (void *) 2);
  mode4 = new Fl_Button(180,220, 80, 20, "USB Data"); mode4->type(FL_RADIO_BUTTON); mode4->color(7,5); mode4->callback(do_mode, (void *) 3);
  mode5 = new Fl_Button(180,250, 80, 20, "FM");       mode5->type(FL_RADIO_BUTTON); mode5->color(7,5); mode5->callback(do_mode, (void *) 4);
  g4->end();

  Fl_Box *lab6 = new Fl_Box(80,100, 80, 20, "Voice");
  lab6->box(FL_FLAT_BOX);
  lab6->labelfont(FL_BOLD);
  lab6->labelsize(14);

  Fl_Button *voice1 = new Fl_Button(80,130,  80, 20, "Smpl 1"); voice1->type(FL_TOGGLE_BUTTON); voice1->color(7,2); voice1->callback(do_voice, (void *) &wav[0]);
  Fl_Button *voice2 = new Fl_Button(80,160,  80, 20, "Smpl 2"); voice2->type(FL_TOGGLE_BUTTON); voice2->color(7,2); voice2->callback(do_voice, (void *) &wav[1]);
  Fl_Button *voice3 = new Fl_Button(80,190,  80, 20, "Smpl 3"); voice3->type(FL_TOGGLE_BUTTON); voice3->color(7,2); voice3->callback(do_voice, (void *) &wav[2]);
  Fl_Button *voice4 = new Fl_Button(80,220,  80, 20, "Smpl 4"); voice4->type(FL_TOGGLE_BUTTON); voice4->color(7,2); voice4->callback(do_voice, (void *) &wav[3]);
  Fl_Button *voice5 = new Fl_Button(80,250,  80, 20, "Smpl 5"); voice5->type(FL_TOGGLE_BUTTON); voice5->color(7,2); voice5->callback(do_voice, (void *) &wav[4]);


  Fl_Box *lab7 = new Fl_Box(10,100, 60, 20, "Tune");
  lab7->box(FL_FLAT_BOX);
  lab7->labelfont(FL_BOLD);
  lab7->labelsize(14);

  tune1 = new Fl_Button(10, 130,  60, 20, "CW  5");   tune1->type(FL_TOGGLE_BUTTON); tune1->color(7,2); tune1->callback(do_tune, (void *) 1);
  tune2 = new Fl_Button(10, 160,  60, 20, "CW 25");   tune2->type(FL_TOGGLE_BUTTON); tune2->color(7,2); tune2->callback(do_tune, (void *) 2);
  tune3 = new Fl_Button(10, 190,  60, 20, "900 Hz");  tune3->type(FL_TOGGLE_BUTTON); tune3->color(7,2); tune3->callback(do_tune, (void *) 3);
  tune4 = new Fl_Button(10, 220,  60, 20, "1700 Hz"); tune4->type(FL_TOGGLE_BUTTON); tune4->color(7,2); tune4->callback(do_tune, (void *) 4);
  tune5 = new Fl_Button(10, 250,  60, 20, "2 Tone");  tune5->type(FL_TOGGLE_BUTTON); tune5->color(7,2); tune5->callback(do_tune, (void *) 5);

  Fl_Button *OpenRig   = new Fl_Button(10,280,290,20,"Open Rig");  OpenRig->type(FL_TOGGLE_BUTTON); OpenRig->color(7,2); OpenRig->callback(open_rig);
  Fl_Choice *RigModel  = new Fl_Choice(90,310,200,20,"Rig model"); RigModel->callback(choose_model);
  Fl_Choice *SerialRig = new Fl_Choice(90,340,200,20,"Rig port");  SerialRig->callback(choose_serial);
  Fl_Choice *SerialPTT = new Fl_Choice(90,370,200,20,"PTT port");  SerialPTT->callback(choose_ptt);
  Fl_Choice *BaudRate  = new Fl_Choice(90,400,200,20,"Baud 8N1");  BaudRate->callback(choose_baud);
  Fl_Choice *WinKey    = new Fl_Choice(90,430,200,20,"WinKey Dev");  WinKey->callback(choose_wkey);
  Fl_Choice *SoundCard = new Fl_Choice(90,460,200,20,"SoundCard"); SoundCard->callback(choose_sound);

  Fl_Slider *Volume    = new Fl_Slider(90,480,200,20,"Volume");
  Volume->type(FL_HOR_NICE_SLIDER);
  Volume->align(FL_ALIGN_LEFT);
  Volume->bounds(0.0,1.0);
  Volume->callback(do_volume, NULL);
  Volume->value(0.5);

  Fl_Box *lab8  = new Fl_Box(340,310,80,20,"Rig Frequency");
  lab8->box(FL_FLAT_BOX);
  lab8->labelfont(FL_BOLD);
  lab8->labelsize(14);
  Fl_Box *lab9  = new Fl_Box(450,310,50,20,"Mode");
  lab9->box(FL_FLAT_BOX);
  lab9->labelfont(FL_BOLD);
  lab9->labelsize(14);

#ifdef __APPLE__
  freq = new Fl_Output(320,330, 120, 30, ""); freq->textsize(24); freq->textcolor(4); freq->textfont(FL_COURIER+FL_BOLD);
  mode = new Fl_Input (445,330,  60, 30, ""); mode->textsize(20); mode->textcolor(4); mode->textfont(FL_COURIER+FL_BOLD);
  mode->callback(do_modeinp, NULL);
  mode->when(FL_WHEN_ENTER_KEY_ALWAYS);

  Fl_Button *mldx = new Fl_Button(320, 280, 180, 20, "Tell MacLoggerDX"); mldx->type(FL_TOGGLE_BUTTON); mldx->color(7,2); mldx->callback(do_mldx, NULL);
  mldx->value(mldx_flag);
#endif

  { // fill in choices for serial ports
    struct stat st;
    char globname[100];

    numrigdev=3; rigdev[0]="uh-rig"; rigdev[1]=":19090"; rigdev[2]="192.168.1.50:19090";
    numpttdev=3; pttdev[0]="CAT-Data"; pttdev[1]="CAT-Mic"; pttdev[2]="uh-ptt";
    numwkeydev=2; wkeydev[0]="CAT"; wkeydev[1]="uh-wkey";

    SerialRig->clear(); for (i=0; i<numrigdev;  i++) SerialRig->add(rigdev[i]);
    SerialPTT->clear(); for (i=0; i<numpttdev;  i++) SerialPTT->add(pttdev[i]);
    WinKey->clear();    for (i=0; i<numwkeydev; i++) WinKey->add(wkeydev[i]);

    strcpy(globname,SER_PORT_BASIS);
    strcat(globname,"*");
    glob_t gbuf;
    int    baslen=strlen(SER_PORT_BASIS);
    glob(globname, 0, NULL, &gbuf);
    for (size_t j = 0; j < gbuf.gl_pathc; j++) {
         int ret1 = !stat(gbuf.gl_pathv[j], &st);
         int ret2 = S_ISCHR(st.st_mode);
         if ( (ret1 && ret2 ) || strstr(gbuf.gl_pathv[j], "modem") ) {
            char *s = (char *) malloc(strlen(gbuf.gl_pathv[j])+1);
            strcpy(s, gbuf.gl_pathv[j]);
            rigdev[numrigdev++] = (const char *) s;
            pttdev[numpttdev++] = (const char *) s;
            wkeydev[numwkeydev++] = (const char *) s;
            SerialRig->add(s+baslen);
            SerialPTT->add(s+baslen);
            WinKey->add(s+baslen);
            printf("Found device: %s\n", gbuf.gl_pathv[j]);
         }  else {
            printf("Not accepted: %s\n", gbuf.gl_pathv[j]);
         }
    }
    globfree(&gbuf);

    for (i=0; i<numbaud; i++) {
      sprintf(str,"%d",baudrates[i]);
      BaudRate->add(str);
    }
  }

  {
    // fill in choices for rig
    int i,n,first,defmod;
    RigModel->clear();
    init_riglist();
    n=get_numrigs();
    for (i=0; i<n; i++) {
      RigModel->add(get_rigname(i));
    }
  }

  { // fill in choices for sound card
    int     i, numDevices, defaultDisplayed;
    const   PaDeviceInfo *deviceInfo;
    PaStreamParameters outputParameters;
    PaError err;
    char name[100]; int j;


       err = Pa_Initialize();
       numDevices = Pa_GetDeviceCount();
       if( err == paNoError && numDevices > 0) {
           for( i=0; i<numDevices; i++ ) {
               deviceInfo = Pa_GetDeviceInfo( i );

               outputParameters.device = i;
               outputParameters.channelCount = 1;
               outputParameters.sampleFormat = paFloat32;
               outputParameters.suggestedLatency = 0; /* ignored by Pa_IsFormatSupported() */
               outputParameters.hostApiSpecificStreamInfo = NULL;
               if (Pa_IsFormatSupported(NULL, &outputParameters, 48000.0) == paFormatIsSupported) {
                 strcpy(name,deviceInfo->name);
		 // for the time being, ignore "dmix" devices
		 if (strncmp(name,"dmix",4)) {
		   sounddevname[numsounddev]=deviceInfo->name;
                   sounddev[numsounddev++] =i;
                   strcpy(name,deviceInfo->name);
		   // unpredictable things happen if the name contains a slash, therefore change it to colon
                   for (j=0; j< strlen(name); j++) {
                     if (name[j] == '/') name[j]=':';
                   }
                   SoundCard->add(name);
                 }
               }
           }
      }
  }
  if (numsounddev == 0) {
	sounddev[0]=-1;
	sounddevname[0]="NULL";
	SoundCard->add(sounddevname[0]);
	numsounddev=1;
  }

#define get16(x) get(wavfile, &x, 2)
#define get32(x) get(wavfile, &x, 4)
#define gets4(x) get(wavfile, x, 4)

  { // load wav files
      char filename[PATH_MAX];
      int i;

      for (i=0; i<=4; i++) {
          wav[i].numsamples=0;
          wav[i].samples=NULL;

          sprintf(filename,"%s/voice%1d.wav",workdir,i+1);
          getwav(filename, &wav[i]);

      }
  }
  
  SoundCard->value(0); mysounddev=0; sounddevice=sounddev[0];
  RigModel->value(0);  mymodel=0;
  BaudRate->value(0);  mybaud=0;
  SerialRig->value(0); myrigdev=0;
  SerialPTT->value(0); mypttdev=0;
  WinKey->value(0);    mywkeydev=0;

  for (i=0; i<numbaud; i++) {
    if (baudrates[i] == defbaud) {
      mybaud=i;
      BaudRate->value(i);
    }
  }
  for (i=0; i<numrigdev; i++) {
    if (!strcmp(defrig, rigdev[i])) {
      myrigdev=i;
      SerialRig->value(i);
    }
  }
  for (i=0; i<numpttdev; i++) {
    if (!strcmp(defptt, pttdev[i])) {
      mypttdev=i;
      SerialPTT->value(i);
    }
  }
  for (i=0; i<numwkeydev; i++) {
    if (!strcmp(defwky, wkeydev[i])) {
      mywkeydev=i;
      WinKey->value(i);
    }
  }
  for (i=0; i<get_numrigs(); i++) {
    if (!strcmp(defham, get_rigname(i))) {
      mymodel=i;
      RigModel->value(i);
    }
  }
  for (i=0; i<numsounddev; i++) {
    if (!strcmp(defsnd, sounddevname[i])) {
      mysounddev=i;
      sounddevice=sounddev[i];
      SoundCard->value(i);
    }
  }


  window->end();
  window->show(argc, argv);

  Fl::add_timeout(2.0, update_freq, NULL);
  Fl::run();
  // Now the window is closed
  close_hamlib();
}

void choose_model(Fl_Widget *w, void*)
{
    mymodel=((Fl_Choice *) w)->value();
}

void choose_wkey(Fl_Widget *w, void*)
{
mywkeydev=((Fl_Choice *) w)->value();
}

void choose_serial(Fl_Widget *w, void*)
{
myrigdev= ((Fl_Choice *) w)->value();
}

void choose_ptt(Fl_Widget *w, void*)
{
mypttdev= ((Fl_Choice *) w)->value();
}

void choose_baud(Fl_Widget *w, void*)
{
   mybaud= ((Fl_Choice *) w)->value();
}

void choose_sound(Fl_Widget *w, void *)
{
int val=((Fl_Choice *) w)->value();
if (val >= 0 && val < numsounddev) {
  sounddevice = sounddev[val];
  mysounddev=val;
}
}

void get(FILE *file, void *data, int len)
{
int i;
char *p = (char *) data;
for (i=0; i<len; i++) p[i]=getc(file);
}

#define get16(x) get(wavfile, &x, 2)
#define get32(x) get(wavfile, &x, 4)
#define gets4(x) get(wavfile, x, 4)
int getwav(char *filename, sample *wav)
{
    FILE *wavfile;
    char label[4];
    int  chunksize;
    int  filesize;
    int i;
    unsigned short compression_code;
    unsigned short num_channels;
    unsigned int   sample_rate;
    unsigned int  bytes_per_second;
    unsigned short align;
    unsigned short bits;
    unsigned short extra_bytes=0;
    short int pcm;
    double max;

    wavfile=fopen(filename,"r");
    if (wavfile == NULL) return -1;
    gets4(label);
    if (strncmp(label,"RIFF",4)) {
       printf("Missing RIFF label\n");
       goto err;
    }
    get32(filesize);
    gets4(label);
    if (strncmp(label,"WAVE",4)) {
       printf("Missing WAVE label\n");
       goto err;
    }
    while (filesize >= 8) {
      gets4(label);
      get32(chunksize);
      if (chunksize & 1) chunksize++;
      filesize -= (chunksize + 8);
      if (!strncmp(label,"fmt ",4)) {
        // process fmt chunk: first 16 bytes
        // printf("Processing fmt chunk, %d bytes\n", chunksize);
        get16(compression_code);
        get16(num_channels);
        get32(sample_rate);
        get32(bytes_per_second);
        get16(align);
        get16(bits);
        // Skip whatever comes next
        for (i=0; i<chunksize-16; i++) (void) getc(wavfile);
        //printf("Num Channels=%d SampleRate%d\n", (int) num_channels, (int) sample_rate);
        continue;
      }
      if (!strncmp(label,"data",4)) {
        // process data chunk
        //printf("Processing data chunk, %d bytes\n",chunksize);
        switch (num_channels) {
          case 1:
            if (chunksize & 1) goto err;
            wav->numsamples=chunksize / 2;
            wav->samples = (float *) malloc(wav->numsamples*sizeof(float));
            max=0.0;
            for (i=0; i<wav->numsamples; i++) {
              get16(pcm);
              wav->samples[i]=pcm/32768.0;
              if (fabs(wav->samples[i]) > max) max=fabs(wav->samples[i]);
            }
            break;
          case 2:
            if (chunksize & 3) goto err;
            wav->numsamples=chunksize / 4;
            wav->samples = (float *) malloc(wav->numsamples*sizeof(float));
            max=0.0;
            for (i=0; i<wav->numsamples; i++) {
              get16(bits); // skip left channel
              get16(bits);
              wav->samples[i]=bits/32768.0;
              if (fabs(wav->samples[i]) > max) max=fabs(wav->samples[i]);
            }
            // normalize
            if (max > 0.1) for (i=0; i<wav->numsamples; i++) wav->samples[i]=wav->samples[i]/max;
            break;
          default:
            goto err;
        }
        continue;
      }
      // unknown chunk, skip it
      //printf("Unknown Chunk with %d bytes\n", chunksize);
      for (i=0; i<chunksize; i++) (void) getc(wavfile);
    }
    fclose(wavfile);
    return 0;

err:
    fclose(wavfile);
    return -1;
}

void do_tune(Fl_Widget *w, void * data)
{
    int val=((Fl_Button *)w)->value();
    int cmd=(int) (long) data;

    if (val == 1) {
	tune1->value(0);
	tune2->value(0);
	tune3->value(0);
	tune4->value(0);
	tune5->value(0);
    }
    if (val == 1) ((Fl_Button *)w)->value(1);
    switch (cmd) {
      case 1: // "tune" carrier with  5 watts
	do_tone(0,1);
	do_tone(0,2);
	do_tone(0,3);
        if (val == 1) rig_tune(5); else rig_tune(0); break;
      case 2: // "tune" carrier with 25 watt
	do_tone(0,1);
	do_tone(0,2);
	do_tone(0,3);
        if (val == 1) rig_tune(25); else rig_tune(0); break;
      case 3:
	rig_tune(0);
	do_tone(0,1);
	do_tone(0,2);
	do_tone(0,3);
        if (val == 1) do_tone(1,1);
        break;
      case 4:
	rig_tune(0);
	do_tone(0,1);
	do_tone(0,2);
	do_tone(0,3);
        if (val == 1) do_tone(1,2);
        break;
      case 5:
	rig_tune(0);
	do_tone(0,1);
	do_tone(0,2);
	do_tone(0,3);
        if (val == 1) do_tone(1,3);
        break;
    }
}

static PaStream *tstream = NULL;

int OneTone(const void*, void*, unsigned long, const PaStreamCallbackTimeInfo*, PaStreamCallbackFlags, void*);

void do_tone(int flag1, int flag2)
{
    PaError err;
    PaStreamParameters outputParameters;
    long FramesPerBuffer=256;
    int i;
    double ang1,ang2;

    if (sounddevice < 0) return;

    if (tstream) {
      err = Pa_StopStream(tstream);
      err = Pa_CloseStream(tstream);
      tstream = NULL;
    }

    if (flag1 == 0) {
        set_ptt(0);
        usleep(50000);
    } else {
      set_ptt(1);

      ang1=0.0;
      ang2=0.0;
      switch (flag2) {
        case 1:
        for (i=0; i< lentab; i++) {
	    sintab[i] = 0.5*sin(ang1);
	    ang1 += FACT1;
	}
	break;
	case 2:
        for (i=0; i< lentab; i++) {
	    sintab[i] = 0.5*sin(ang2);
	    ang2 += FACT2;
	}
        break;
        case 3:
        for (i=0; i< lentab; i++) {
	    sintab[i] = 0.25*(sin(ang1)+sin(ang2));
	    ang1 += FACT1;
	    ang2 += FACT2;
	}
        break;
      }
      tonept=0;

      usleep(50000);
      /* start  audio */
      bzero(&outputParameters, sizeof(outputParameters));
      outputParameters.device = sounddevice;
      outputParameters.channelCount = 1;
      outputParameters.sampleFormat = paFloat32;
      outputParameters.suggestedLatency = Pa_GetDeviceInfo( outputParameters.device )->defaultHighOutputLatency;
      outputParameters.hostApiSpecificStreamInfo = NULL;
      
      err = Pa_OpenStream(&tstream, NULL, &outputParameters, 48000.0, FramesPerBuffer, paNoFlag, OneTone, (void *) NULL);
      if (err != paNoError) return;
      err=Pa_StartStream(tstream);
      if (err != paNoError) return;
    }
}

void do_voice(Fl_Widget *w, void * data)
{
    // Play the voice
    sample *s = (sample *) data;
    set_ptt(1);
    if (sounddevice >= 0 && s->numsamples > 0) {
        PaStream *stream;
	PaError err;
	PaStreamParameters outputParameters;
        int i, j, left;
        long FramesPerBuffer=256;
        float audiobuf[256];

        bzero(&outputParameters, sizeof(outputParameters));
	outputParameters.device = sounddevice;
	outputParameters.channelCount = 1;
        outputParameters.sampleFormat = paFloat32;
	outputParameters.suggestedLatency = Pa_GetDeviceInfo( outputParameters.device )->defaultHighOutputLatency;
	outputParameters.hostApiSpecificStreamInfo = NULL;
	/* -- setup stream -- */
	err = Pa_OpenStream(
          &stream,
          NULL,
          &outputParameters,
          48000.0,
          FramesPerBuffer,
          paClipOff,      /* we won't output out of range samples so don't bother clipping them */
          NULL, /* no callback, use blocking API */
          NULL ); /* no callback, so no callback userData */

	if( err != paNoError ) return;
	/* -- start stream -- */
	err = Pa_StartStream( stream );
	if( err != paNoError ) return;
        left=s->numsamples;
        i=0;
        while (left >0) {
          if (left >= FramesPerBuffer) {
            for (j=0; j<FramesPerBuffer; j++) audiobuf[j]=master_volume*s->samples[i+j];
	    err = Pa_WriteStream( stream, audiobuf, FramesPerBuffer);
          } else {
            for (j=0; j<left; j++) audiobuf[j]=master_volume*s->samples[i+j];
	    err = Pa_WriteStream( stream, audiobuf, left);
          }
          left -= FramesPerBuffer;
          i += FramesPerBuffer;
          if (err != paNoError) goto err;
        }
        err:
        Pa_CloseStream(stream);
    }
    set_ptt(0);
    ((Fl_Button *) w)->value(0);
}

int OneTone(const void *inputBuffer, void *outputBuffer, unsigned long framesPerBuffer,
            const PaStreamCallbackTimeInfo* timeInfo,
            PaStreamCallbackFlags statusFlags,
            void *userdata)
{
    float *out = (float *)outputBuffer;
    int i;

    if (out != NULL) {
      for (i=0; i<framesPerBuffer; i++) {
        out[i] = master_volume*sintab[tonept++];
        if (tonept >= lentab) tonept=0;
      }
    }
    return paContinue;
}

void do_call(Fl_Widget *, void * data)
{
    mycall = (const char *) data;
}

void do_pow(Fl_Widget *, void* data)
{
    int val=(int) (long) data;
    set_rfpower(val);
}


void do_mode(Fl_Widget *, void *data)
{
    int val=(int) (long) data;
    set_mode(val);
}

void do_speed(Fl_Widget *, void* data)
{
    int val = (int) (long) data;
    set_cwspeed(val);
}

void open_rig(Fl_Widget *w, void *)
{
    char filename[PATH_MAX];
    FILE *prefs;
    int i;
    int val;

    // clear the CW input and output fields
    cwinp->value("");
    val=cwsnt->insert_position();
    cwsntbuf->remove(0,val);
    
    val=((Fl_Button *)w)->value();
    if (val == 1) {
       // print current values to prefs file
       sprintf(filename,"%s/Prefs",workdir);
       prefs=fopen(filename,"w");
       if (prefs == NULL) {
         prefs=fopen("Prefs","w");
       }
       if (prefs) {
	  fprintf(prefs,"%s\n",rigdev[myrigdev]);
	  fprintf(prefs,"%s\n",pttdev[mypttdev]);
	  fprintf(prefs,"%s\n",wkeydev[mywkeydev]);
	  fprintf(prefs,"%d\n",baudrates[mybaud]);
	  fprintf(prefs,"%s\n",get_rigname(mymodel));
	  fprintf(prefs,"%s\n",sounddevname[mysounddev]);
	  fclose(prefs);
       }
       val=open_hamlib(rigdev[myrigdev],pttdev[mypttdev],wkeydev[mywkeydev],baudrates[mybaud],mymodel);
       ((Fl_Button *)w)->value(val);
	if (val > 0) {
	    // ask rig for current values. Adjust accordingly
	    pow1->value(0);
	    pow2->value(0);
	    pow3->value(0);
	    pow4->value(0);
	    pow5->value(0);
	    val=get_rfpower();
	    if (val <  8) { set_rfpower(5); pow1->value(1); }
	    else if (val < 20) { set_rfpower(10); pow2->value(1); }
	    else if (val < 40) { set_rfpower(30); pow3->value(1); }
	    else if (val < 75) { set_rfpower(50); pow4->value(1); } else { set_rfpower(100); pow5->value(1); }

	    // Set default CW speed (20 wpm)
	    speed1->value(0);
	    speed2->value(0);
	    speed3->value(1);
	    speed4->value(0);
	    speed5->value(0);
	    set_cwspeed(20);

	    // Mode is tracked in update_freq
	}
    } else {
	close_hamlib();
	mode1->value(0);
	mode2->value(0);
	mode3->value(0);
	mode4->value(0);
	mode5->value(0);
	speed1->value(0);
	speed2->value(0);
	speed3->value(0);
	speed4->value(0);
	speed5->value(0);
	pow1->value(0);
	pow2->value(0);
	pow3->value(0);
	pow4->value(0);
	pow5->value(0);
    }
}

//
// Ship out CW text word by word
//
// translate # into mycall
// translate ä ö ü ß UNICODE to ae oe ue ss etc.
// convert everything to upper case
// translate !n to fixed CW text number n (1 <= n <= 5)
// do this recursively
//
void splitCW(const char *text, int flag)
{
char word[100];
char *p, *q, *r;
int line,pos,p1,p2,i;

    fprintf(stderr,"SPLITCW TXT >>%s<<\n", text);
    p=(char *)text;
    q=word;
    for(;;) {
        switch ((signed char) *p) {
	  case '!':
	    // get number and replace by active CW text
 	    switch (*(++p)) {
	      case '1': splitCW(cwtxt1,0); break;
	      case '2': splitCW(cwtxt2,0); break;
	      case '3': splitCW(cwtxt3,0); break;
	      case '4': splitCW(cwtxt4,0); break;
	      case '5': splitCW(cwtxt5,0); break;
              default: if (q-word < 99) *q++ = toupper(*p); break;
	    }
	    break;
          case '#':
	    // replace '#' by the active call sign
            splitCW(mycall,0);
	    break;
	  case -61:  // convert some unicode two-byte characters
	    p++;
	    if (q-word < 98) switch((signed char) *p) {
		case -74:
		case -106:
		    *q++='O'; *q++='E';
		    break;
		case -92:
		case -124:
		    *q++='A'; *q++='E';
		    break;
		case -68:
		case -100:
		    *q++='U'; *q++='E';
		    break;
		case -97:
		    *q++='S'; *q++='S';
		    break;
	    }
	    break;
          default:
	    if (q-word < 99) *q++ = toupper(*p);
        }
	if (*p == 0) {
	  *q=0;
	  // send CW string obtained so far and that's it
          if (strlen(word) > 0) send_cw(word);
	  cwsnt->insert(word);
          break;
        }
        if (*p == ' ') {
	  // send the word and continue analyzing the input text
	  *q = 0;
	  send_cw(word);
          cwsnt->insert(word);
	  q=word;
        }
	p++;
    }
    // if flag, or if line position is large, insert line break
    pos=cwsnt->insert_position();
    i=pos-cwsnt->line_start(pos);
    if (i>50) flag=1;
    if (flag) cwsnt->insert("\n"); else cwsnt->insert(" ");
    // Display no more than three lines. Delete the first one if there are too many.
    line=cwsntbuf->count_lines(0,pos);
    if (line > 3) {
      p1=cwsntbuf->line_start(0);
      p2=cwsntbuf->line_end(0);
      cwsntbuf->remove(p1,p2+1);
    }
}

void do_modeinp(Fl_Widget *w, void *data)
{
   // Triggered when ENTER is hit in the mode field
   // Convert input field to upper case
    char *val = (char *) (((Fl_Input *) w)->value());
    while (*val) { *val=toupper(*val); val++; }
#ifdef __APPLE__
    // This triggers sending the changed mode
    mldx_last_freq=-1.0;
#endif
}

void do_cwinp(Fl_Widget *w, void* data )
{
    char buffer[128];
    char *p;
    char *val = (char *) (((Fl_Input *) w)->value());

    // If a "return" has been pressed, ship out everything
    // and produce a new line on the cwsnt output area
    if (Fl::event_key(FL_Enter)) {
      splitCW(val,1);
      ((Fl_Input *) w)->value("");
      return;
    }
    // Check if string contains a blank. Ship out CW until that blank, no new-line
    // Normally, the blank will be at the end of the string, so the input field gets cleared.
    p=strchr(val, ' ');
    if (p) {
	strcpy(buffer, val);
	p=strchr(buffer, ' ');
	*p=0;
        splitCW(buffer, 0);
	((Fl_Input *) w)->value(p+1);
    }
}

void do_cwtxt(Fl_Widget *w, void* data )
{
   char *text = (char *) data;
   if (((Fl_Button *) w)->value() == 0) return;
   splitCW(text,1);
   ((Fl_Button *) w)->value(0);
}

void do_volume(Fl_Widget *w, void *)
{
  double val;
  val = ((Fl_Slider *) w)->value();
  // convert using a logarithmic scale
  // 0.0 --> -40dB = 0.01
  // 1.0 -->   0dB = 1.0
  master_volume=(double) pow(10.0,2.0*(val-1));
}

#ifdef __APPLE__
void do_mldx(Fl_Widget *w, void *)
{
    mldx_flag= ((Fl_Button *) w)->value();
    mldx_last_freq=-1.0;
}
#endif

void update_freq(void *)
{
    double f;
    char str[256];
    pid_t pid;
    int   stat;
    int   ptt;
    int   doit;
    int   m;
    char  *p;

#ifdef __APPLE__
    ptt=get_ptt();
    f=get_freq() / 1000000.0;  // in MHz
    if (ptt == 0) ptt=get_ptt(); // ptt==0 means PTT_OFF before and after frequency readout
    sprintf(str,"%8.3f", f);
    freq->value(str);
    //
    // push data to MLDX if requested
    // but do not push data do MLDX when transmitting (so the frequency may be
    // different from the RX frequency).
    // 
    // If we CANNOT GET THE PTT STATUS and if the frequency change is less than 3 kHz,
    // this is possibly a split-tx freq and no frequency is reported to MLDX.
    // REASON: do not overwrite the MODE field when you are filling out the logbook
    // entry while TXing.
    //
    // If the PTT state can be determined, we send frequency information only if
    // PTT was OFF before and after the frequency readout.
    //
    doit = mldx_flag;
    if (ptt < 0) doit = doit && (fabs(f-mldx_last_freq) > 0.003);    // Cannot get PTT
    if (ptt == 0) doit = doit && (fabs(f-mldx_last_freq) > 0.001);   // we are in RX mode
    if (ptt == 1) doit = 0;                                          // possibly in TX mode
    if (doit) {
	mldx_last_freq=f;
	sprintf(str,"mldx://tune?freq=%0.3f",f);
        p=(char *)mode->value();
        if (strlen(p) > 0) {
          strcat(str,"&mode=");
          strcat(str,p);
        }
	pid=fork();
        if (pid == 0) {
	  // Child process.
	  execl("/usr/bin/open","open","-g",str,NULL);
	  // NOTREACHED, but PARANOIA
	  (void) exit(0);
	}
	// Forking process. Wait for child to terminate
	waitpid(pid, &stat, 0);
    }
#endif
    // On non-MacOS systems, we simply track the mode of the rig
    // update mode
    m=get_mode();
    mode1->value(0);
    mode2->value(0);
    mode3->value(0);
    mode4->value(0);
    mode5->value(0);
    switch (m) {
        case 0: mode1->value(1); break;
        case 1: mode2->value(1); break;
        case 2: mode3->value(1); break;
        case 3: mode4->value(1); break;
        case 4: mode5->value(1); break;
    }
    // Repeat this every 2 seconds. Note that get_freq(), get_ptt(),  get_mode()
    // may have to wait for the hamlib mutex.
    Fl::repeat_timeout(2.0, update_freq, NULL);
}
