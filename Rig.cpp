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
#include <FL/Fl_Input_Choice.H>
#include <FL/fl_ask.H>
#include <FL/Fl_Output.H>
#include <FL/Fl_Text_Display.H>
#include <FL/Fl_Text_Buffer.H>
#include <FL/Fl_Slider.H>
#include <FL/Fl_File_Chooser.H>

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

extern int wkeymode;

/*
 * This sets some defaults.
 * They are to be overrun by the preferences
 * in $HOME/.rigcontrol/Prefs
 *
 */

char mycall[150]="DL1AAA";

char cwlab1[10]= "call";
char cwlab2[10]= "cq";
char cwlab3[10]= "test";
char cwlab4[10]= "cfm";
char cwlab5[10]= "tu";

char voicelab1[10]="Voice 1";
char voicelab2[10]="Voice 2";
char voicelab3[10]="Voice 3";
char voicelab4[10]="Voice 4";
char voicelab5[10]="Voice 5";

char cwtxt1[80]="#";
char cwtxt2[80]="cq cq cq de # # cq cq de # + k";
char cwtxt3[80]="test de # # test";
char cwtxt4[80]="cfm ur 5nn";
char cwtxt5[80]="tu 73";

static Fl_Button *cw1, *cw2, *cw3, *cw4, *cw5;
static Fl_Button *voice1, *voice2, *voice3, *voice4, *voice5;
static Fl_Window *set_cw_win;
static Fl_Window *set_voice_win;

/*
 * Some default values for rig connection.
 * The actual values chosen are dumped
 * to a Prefs file (in the current directory
 * or within the APP bundle) whenever a
 * rig is opened. So usually this file exists
 * and those values are taken.
 */

char defrig[150]="";
char defptt[150]="CAT-Data";
char defwky[150]="CAT";
char defsnd[150]="";
char defham[150]="Hamlib Dummy";
int  defbaud=9600;

const char *myrigdev;
const char *mypttdev;
const char *mywkydev;
int        baudrates[]={1200,2400,4800, 9600, 19200,38400,115200};
int        numbaud = sizeof(baudrates)/sizeof(int);
int        mybaud=0;
int        mymodel;
const char *sounddevname[20];
int sounddev[20];  int numsounddev=0;  int sounddevice=-1; int mysounddev=-1;

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


void do_text (Fl_Widget *, void *);
void do_voice(Fl_Widget *, void *);
void do_tune (Fl_Widget *, void *);
void do_mldx (Fl_Widget *, void *);
static int mldx_flag=1;
static int can_mldx=0;
static double mldx_last_freq = -1.0;
void do_setcw(Fl_Widget *, void *);
void do_setvoice(Fl_Widget *, void *);
void do_saveprefs(Fl_Widget *, void *);

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
  char  filename[256];
} sample;

sample wav[5];

#define lentab 480
static double sintab[lentab];
static int tonept;

#define FACT1 0.11780972450961724644234912687298  //  9 * (2 Pi) / 480,   900 Hz tone
#define FACT2 0.22252947962927702105777057298230  // 17 * (2 Pi) / 480,  1700 Hz tone

int getwav(sample *);

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

void AddToChoice(Fl_Input_Choice *w, char *s) {
  int size, i, found;
  const char *item;
  size=w->menu()->size() -1;
  found=0;
  for (i=0; i<size; i++) {
    w->value(i);
    item=w->value();
    if (!strcmp(item, s)) {
      found=1;
      w->value(i);
      break;
    }
  }
  if (found == 0) {
    w->add(s);
    w->value(size);
  } 
}

static int event_pass=1;
static long counter=0;

int myHandler(int e, Fl_Window *w) {
//
// The rationale for this handler is that is simply behaves
// is it were not there normally (that is, as long as even_pass == 1).
//
// If event_pass is set to zero, this event handler just calls do_tune with
// two dummy arguments if the triggering event was a mouse click.
//
// This call to do_tune will restore event_pass ==1 so normal operation
// continues.
//
// This mechanism is used if one of the buttons in the "Tune" menu is activated.
// The tuning will continue until a mouse click within the controller window takes
// place, then tuning is stopped.
// In this way, any temporary change of the mode or the output power can properly
// and safely be restored.
//
  if (event_pass == 1) {
    return Fl::handle_(e,w);
  }
  if (Fl::event_buttons()) {
    //
    // Any mouse click will terminate the tune event
    //
    do_tune(NULL, NULL);
  }
  return 0;
}

int main(int argc, char **argv) {

  int i,j;
  char str[20];

  char *cp;
  FILE *fp;
  char filename[PATH_MAX];
  struct stat statbuf;

  //
  // Determine if we run on Mac OS and have MacLoggerDX present
  // In this case, /Applications/MacLoggerDX.app and /usr/bin/open should be
  // present
  //
  can_mldx = (stat("/Applications/MacLoggerDX.app",&statbuf) == 0)  &&  (stat("/usr/bin/open", &statbuf) == 0);
  Fl::event_dispatch(myHandler);
  // obtain $HOME/.rigcontrol dir
  strcpy(workdir,".");
  cp=getenv("HOME");
  if (cp) {
      // look if $HOME/.rigcontrol exists, otherwise make it
      sprintf(filename,"%s/.rigcontrol",cp);
      if (stat(filename, &statbuf) != 0) {
        if (mkdir(filename,0755) == 0) strcpy(workdir,filename);
      } else {
	strcpy(workdir,filename);
      }
  }
  
  //
  // load CW labels/texts from workspace
  //
  sprintf(filename,"%s/CWTXT",workdir);
  fp=fopen(filename,"r");
  if (fp) {
    fprintf(stderr,"Importing CW texts/labels\n");
    get_token(fp, cwtxt1  , 80, cwlab1, 10);
    get_token(fp, cwtxt2  , 80, cwlab2, 10);
    get_token(fp, cwtxt3  , 80, cwlab3, 10);
    get_token(fp, cwtxt4  , 80, cwlab4, 10);
    get_token(fp, cwtxt5  , 80, cwlab5, 10);
    fclose(fp);
  }
  //
  // load default values from local prefs file
  //
  sprintf(filename,"%s/Prefs",workdir);
  fp=fopen(filename,"r");
  if (fp) {
    fprintf(stderr,"Importing default values\n");
    get_token(fp, mycall, 20, NULL, 0);
    get_token(fp, defrig, 150, NULL, 0);
    get_token(fp, defptt, 150, NULL, 0);
    get_token(fp, defwky, 150, NULL, 0);
    fscanf(fp,"%d\n",&defbaud);
    get_token(fp, defham, 150, NULL, 0);
    get_token(fp, defsnd, 150, NULL, 0);
    fclose(fp);
  }
  //
  // load filename for voice keyer from local file
  //
  sprintf(filename,"%s/VOICES",workdir);
  fp=fopen(filename,"r");
  if (fp) {
    fprintf(stderr,"Importing voices\n");
    get_token(fp, wav[0].filename, 256, voicelab1,10);
    get_token(fp, wav[1].filename, 256, voicelab2,10);
    get_token(fp, wav[2].filename, 256, voicelab3,10);
    get_token(fp, wav[3].filename, 256, voicelab4,10);
    get_token(fp, wav[4].filename, 256, voicelab5,10);
    fclose(fp);
  } else {
    strcpy(wav[0].filename,"NothingSelected");
    strcpy(wav[1].filename,"NothingSelected");
    strcpy(wav[2].filename,"NothingSelected");
    strcpy(wav[3].filename,"NothingSelected");
    strcpy(wav[4].filename,"NothingSelected");
  }
 

  Fl_Window *window = new Fl_Window(512,512);
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
  speed1 = new Fl_Button(380,130,  40, 20, "");   speed1->type(FL_RADIO_BUTTON);  speed1->color(7,5); speed1->callback(do_speed, (void *) 12);
  speed2 = new Fl_Button(380,160,  40, 20, "15"); speed2->type(FL_RADIO_BUTTON);  speed2->color(7,5); speed2->callback(do_speed, (void *) 15);
  speed3 = new Fl_Button(380,190,  40, 20, "18"); speed3->type(FL_RADIO_BUTTON);  speed3->color(7,5); speed3->callback(do_speed, (void *) 18);
  speed4 = new Fl_Button(380,220,  40, 20, "21"); speed4->type(FL_RADIO_BUTTON);  speed4->color(7,5); speed4->callback(do_speed, (void *) 21);
  speed5 = new Fl_Button(380,250,  40, 20, "24"); speed5->type(FL_RADIO_BUTTON);  speed5->color(7,5); speed5->callback(do_speed, (void *) 24);
  g2->end();

  Fl_Box *lab3  = new Fl_Box(280,100,80,20,"CW TXT");
  lab3->box(FL_FLAT_BOX);
  lab3->labelfont(FL_BOLD);
  lab3->labelsize(14);

  cw1 = new Fl_Button(280,130,  80, 20, cwlab1); cw1->type(FL_TOGGLE_BUTTON); cw1->color(7,2); cw1->callback(do_cwtxt, (void *) cwtxt1);
  cw2 = new Fl_Button(280,160,  80, 20, cwlab2); cw2->type(FL_TOGGLE_BUTTON); cw2->color(7,2); cw2->callback(do_cwtxt, (void *) cwtxt2);
  cw3 = new Fl_Button(280,190,  80, 20, cwlab3); cw3->type(FL_TOGGLE_BUTTON); cw3->color(7,2); cw3->callback(do_cwtxt, (void *) cwtxt3);
  cw4 = new Fl_Button(280,220,  80, 20, cwlab4); cw4->type(FL_TOGGLE_BUTTON); cw4->color(7,2); cw4->callback(do_cwtxt, (void *) cwtxt4);
  cw5 = new Fl_Button(280,250,  80, 20, cwlab5); cw5->type(FL_TOGGLE_BUTTON); cw5->color(7,2); cw5->callback(do_cwtxt, (void *) cwtxt5);

  Fl_Box *lab4  = new Fl_Box(310,370,70,20,"MyCall");
  lab4->box(FL_FLAT_BOX);
  lab4->labelfont(FL_BOLD);
  lab4->labelsize(14);

  Fl_Input *call1 = new Fl_Input(380, 370, 120, 20, ""); call1->color(7,2); call1->callback(do_text, mycall); call1->value(mycall);

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

  voice1 = new Fl_Button(80,130,  80, 20, voicelab1); voice1->type(FL_TOGGLE_BUTTON); voice1->color(7,2); voice1->callback(do_voice, (void *) &wav[0]);
  voice2 = new Fl_Button(80,160,  80, 20, voicelab2); voice2->type(FL_TOGGLE_BUTTON); voice2->color(7,2); voice2->callback(do_voice, (void *) &wav[1]);
  voice3 = new Fl_Button(80,190,  80, 20, voicelab3); voice3->type(FL_TOGGLE_BUTTON); voice3->color(7,2); voice3->callback(do_voice, (void *) &wav[2]);
  voice4 = new Fl_Button(80,220,  80, 20, voicelab4); voice4->type(FL_TOGGLE_BUTTON); voice4->color(7,2); voice4->callback(do_voice, (void *) &wav[3]);
  voice5 = new Fl_Button(80,250,  80, 20, voicelab5); voice5->type(FL_TOGGLE_BUTTON); voice5->color(7,2); voice5->callback(do_voice, (void *) &wav[4]);

  Fl_Box *lab7 = new Fl_Box(10,100, 60, 20, "Tune");
  lab7->box(FL_FLAT_BOX);
  lab7->labelfont(FL_BOLD);
  lab7->labelsize(14);

  tune1 = new Fl_Button(10, 130,  60, 20, "CW  5");   tune1->type(FL_TOGGLE_BUTTON); tune1->color(7,2); tune1->callback(do_tune, (void *) 1);
  tune2 = new Fl_Button(10, 160,  60, 20, "CW 25");   tune2->type(FL_TOGGLE_BUTTON); tune2->color(7,2); tune2->callback(do_tune, (void *) 2);
  tune3 = new Fl_Button(10, 190,  60, 20, "900 Hz");  tune3->type(FL_TOGGLE_BUTTON); tune3->color(7,2); tune3->callback(do_tune, (void *) 3);
  tune4 = new Fl_Button(10, 220,  60, 20, "1700 Hz"); tune4->type(FL_TOGGLE_BUTTON); tune4->color(7,2); tune4->callback(do_tune, (void *) 4);
  tune5 = new Fl_Button(10, 250,  60, 20, "2 Tone");  tune5->type(FL_TOGGLE_BUTTON); tune5->color(7,2); tune5->callback(do_tune, (void *) 5);

  Fl_Button       *OpenRig   = new Fl_Button(10,280,290,20,"Open Rig");  OpenRig->type(FL_TOGGLE_BUTTON); OpenRig->color(7,2); OpenRig->callback(open_rig);
  Fl_Choice       *RigModel  = new Fl_Choice(90,310,200,20,"Rig model"); RigModel->callback(choose_model);
  Fl_Input_Choice *SerialRig = new Fl_Input_Choice(90,340,200,20,"Rig port");  SerialRig->callback(choose_serial);
  Fl_Input_Choice *SerialPTT = new Fl_Input_Choice(90,370,200,20,"PTT port");  SerialPTT->callback(choose_ptt);
  Fl_Choice       *BaudRate  = new Fl_Choice(90,400,200,20,"Baud 8N1");  BaudRate->callback(choose_baud);
  Fl_Input_Choice *WinKey    = new Fl_Input_Choice(90,430,200,20,"WinKey Dev");  WinKey->callback(choose_wkey);
  Fl_Choice       *SoundCard = new Fl_Choice(90,460,200,20,"SoundCard"); SoundCard->callback(choose_sound);

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

  freq = new Fl_Output(320,330, 120, 30, ""); freq->textsize(24); freq->textcolor(4); freq->textfont(FL_COURIER+FL_BOLD);
  mode = new Fl_Input (445,330,  60, 30, ""); mode->textsize(20); mode->textcolor(4); mode->textfont(FL_COURIER+FL_BOLD);
  mode->callback(do_modeinp, NULL);
  mode->when(FL_WHEN_ENTER_KEY_ALWAYS);

  if (can_mldx) {
    Fl_Button *mldx = new Fl_Button(320, 280, 180, 20, "Tell MacLoggerDX"); mldx->type(FL_TOGGLE_BUTTON); mldx->color(7,2); mldx->callback(do_mldx, NULL);
    mldx->value(mldx_flag);
  }

  Fl_Button *setcw    = new Fl_Button(320, 400, 120, 20, "Set CW texts");    setcw->callback(do_setcw, NULL);
  Fl_Button *setvoice = new Fl_Button(320, 430, 120, 20, "Set voice files"); setvoice->callback(do_setvoice, NULL);
  Fl_Button *saveprefs= new Fl_Button(320, 460, 120, 20, "Save settings");   saveprefs->callback(do_saveprefs, NULL);

    // fill in choices for serial ports
    struct stat st;
    char globname[100];

    SerialRig->clear();
    SerialRig->add(":19090");
    SerialRig->add("uh-rig");

    SerialPTT->clear();
    SerialPTT->add("CAT-Data");
    SerialPTT->add("CAT-Mic");
    SerialPTT->add("uh-ptt");

    WinKey->clear();
    WinKey->add("CAT");
    WinKey->add("uh-wkey");

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
            SerialRig->add(s);
            SerialPTT->add(s);
            WinKey->add(s);
            printf("Found device: %s\n", gbuf.gl_pathv[j]);
         }  else {
            printf("Not accepted: %s\n", gbuf.gl_pathv[j]);
         }
    }
    globfree(&gbuf);

  //
  // For the rig, ptt, and wkey devices, add the defaults
  // if they are not yet present. Set the input fields
  // to that default value
  //
  AddToChoice(SerialRig, defrig); myrigdev=SerialRig->value();
  AddToChoice(SerialPTT, defptt); mypttdev=SerialPTT->value();
  AddToChoice(WinKey,    defwky); mywkydev=WinKey->value();

  for (i=0; i<numbaud; i++) {
    sprintf(str,"%d",baudrates[i]);
    BaudRate->add(str);
  }
  BaudRate->value(0);  mybaud=0;
  for (i=0; i<numbaud; i++) {
    if (baudrates[i] == defbaud) {
      mybaud=i;
      BaudRate->value(i);
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
  RigModel->value(0);  mymodel=0;
  for (i=0; i<get_numrigs(); i++) {
    if (!strcmp(defham, get_rigname(i))) {
      mymodel=i;
      RigModel->value(i);
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
  SoundCard->value(0); mysounddev=0; sounddevice=sounddev[0];
  for (i=0; i<numsounddev; i++) {
    if (!strcmp(defsnd, sounddevname[i])) {
      mysounddev=i;
      sounddevice=sounddev[i];
      SoundCard->value(i);
    }
  }

  //
  // load wav files
  //
  for (i=0; i< 5; i++) {
    wav[i].samples=NULL;
    getwav(&wav[i]);
  }
  
  window->end();
  window->show(argc, argv);

  Fl::add_timeout(2.0, update_freq, NULL);
  Fl::run();

  //
  // We arrive here if the window is closed
  //
  close_hamlib();
}

void choose_model(Fl_Widget *w, void*)
{
    mymodel=((Fl_Choice *) w)->value();
}

void choose_wkey(Fl_Widget *w, void*)
{
mywkydev=((Fl_Input_Choice *) w)->value();
}

void choose_serial(Fl_Widget *w, void*)
{
  myrigdev= ((Fl_Input_Choice *) w)->value();
}

void choose_ptt(Fl_Widget *w, void*)
{
mypttdev= ((Fl_Input_Choice *) w)->value();
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
int getwav(sample *s)
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

    s->numsamples=0;
    if (s->samples != NULL) free(s->samples); 
    s->samples=NULL;

    wavfile=fopen(s->filename,"r");
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
            s->numsamples=chunksize / 2;
            s->samples = (float *) malloc(s->numsamples*sizeof(float));
            max=0.0;
            for (i=0; i<s->numsamples; i++) {
              get16(pcm);
              s->samples[i]=pcm/32768.0;
              if (fabs(s->samples[i]) > max) max=fabs(s->samples[i]);
            }
            break;
          case 2:
            if (chunksize & 3) goto err;
            s->numsamples=chunksize / 4;
            s->samples = (float *) malloc(s->numsamples*sizeof(float));
            max=0.0;
            for (i=0; i<s->numsamples; i++) {
              get16(bits); // skip left channel
              get16(bits);
              s->samples[i]=bits/32768.0;
              if (fabs(s->samples[i]) > max) max=fabs(s->samples[i]);
            }
            // normalize
            if (max > 0.1) for (i=0; i<s->numsamples; i++) s->samples[i]=s->samples[i]/max;
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
    int val,cmd;
    static int last_cmd;
    static Fl_Widget *last_w;

//
//  These buttons are special.
//  Do not return but wait until button is 
//  de-selected
//
    if (event_pass == 1) {
      val=((Fl_Button *)w)->value();
      cmd=(int) (long) data;
      last_cmd=cmd;
      last_w=w;
      event_pass=0;
    } else {
      val=0;
      cmd=last_cmd;
      event_pass=1;
      ((Fl_Button *)last_w)->value(0);
    }
    switch (cmd) {
      case 1: // "tune" carrier with  5 watts
        if (val == 1) rig_tune(5); else rig_tune(0); break;
      case 2: // "tune" carrier with 25 watt
        if (val == 1) rig_tune(25); else rig_tune(0); break;
      case 3:
        if (val == 1) do_tone(1,1); else do_tone(0,1);
        break;
      case 4:
        if (val == 1) do_tone(1,2); else do_tone(0,2);
        break;
      case 5:
        if (val == 1) do_tone(1,3); else do_tone(0,3);
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

void do_text(Fl_Widget *w, void * data)
{
  char *target = (char *) data;
  const char *newcall=((Fl_Input *) w)->value();
  strcpy(target, newcall);
  //
  // convert lower-case letters to upper-case letters
  //
  char *p = target;
  while (*p) {
    *p = toupper(*p);
    p++;
  }
  ((Fl_Input *) w)->value(target);
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
    //
    // in WinKey mode, the first "speed" button mean "use speed pot"
    //
    if (wkeymode == 2 && val == 12) val=0;
    set_cwspeed(val);
}

void open_rig(Fl_Widget *w, void *)
{
    char filename[PATH_MAX];
    FILE *prefs;
    int i;
    int val;
    char *s;

    // clear the CW input and output fields
    cwinp->value("");
    val=cwsnt->insert_position();
    cwsntbuf->remove(0,val);
    
    val=((Fl_Button *)w)->value();
    if (val == 1) {
       val=open_hamlib(myrigdev,mypttdev,mywkydev,baudrates[mybaud],mymodel);
       ((Fl_Button *)w)->value(val);
	if (val > 0) {
	    // ask rig for current values. Adjust accordingly
	    pow1->value(0);
	    pow2->value(0);
	    pow3->value(0);
	    pow4->value(0);
	    pow5->value(0);
	    val=get_rfpower();
	    //
            // Map
            //  0   -   8 Watt  ==>   5 Watt
            //  9   -  19 Watt  ==>  10 Watt
            // 20   -  39 Watt  ==>  30 Watt
            // 40   -  74 Watt  ==>  50 Watt
            // 75   -     Watt  ==> 100 Watt
            //
	    if (val <  8) { set_rfpower(5); pow1->value(1); }
	    else if (val < 20) { set_rfpower(10); pow2->value(1); }
	    else if (val < 40) { set_rfpower(30); pow3->value(1); }
	    else if (val < 75) { set_rfpower(50); pow4->value(1); } else { set_rfpower(100); pow5->value(1); }
	    //
 	    // Make label of first button and set default speed
            //
 	    if (wkeymode == 2) {
		speed1->label("Pot");
	    } else {
		speed1->label("12");
	    }
	    speed1->value(0);
	    speed2->value(0);
	    speed3->value(0);
	    speed4->value(1);
	    speed5->value(0);
	    set_cwspeed(21);
            //
            // Upon each sucessful connection to a rig,
	    // save the parameters
            //
            do_saveprefs(NULL, NULL);
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
    // This triggers sending the changed mode
    mldx_last_freq=-1.0;
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

void do_mldx(Fl_Widget *w, void *)
{
    mldx_flag= ((Fl_Button *) w)->value();
    mldx_last_freq=-1.0;
}

void apply_cw(Fl_Widget *w, void *) {
  char filename[PATH_MAX];
  FILE *fp;
  //
  // update labels of buttons
  //
  cw1->label(cwlab1);
  cw2->label(cwlab2);
  cw3->label(cwlab3);
  cw4->label(cwlab4);
  cw5->label(cwlab5);

  //
  // close the "CW text" window
  //
  Fl::delete_widget(set_cw_win);
}

void apply_voice(Fl_Widget *w, void *) {
  int i;
  char filename[256];
  FILE *fp;
  //
  // Re-label all buttons
  //
  voice1->label(voicelab1);
  voice2->label(voicelab2);
  voice3->label(voicelab3);
  voice4->label(voicelab4);
  voice5->label(voicelab5);
  //
  // re-load all wav files
  //
  for (i=0; i<5; i++) {
    getwav(&wav[i]);
  }
  //
  // CLose (destroy) window
  //
  Fl::delete_widget(set_voice_win);
}

void get_file(Fl_Widget *w, void *data) {
  sample *s = (sample *) data;
  char *fn = fl_file_chooser("WAV file to import", "*.wav", "", 0);
  if (fn) {
    strcpy(s->filename, fn);
  }
}

void do_saveprefs(Fl_Widget *w, void*) {
  FILE *fp;
  char filename[PATH_MAX];
//
// save current status to "Prefs" file
//
  sprintf(filename,"%s/Prefs",workdir);
  fp=fopen(filename,"w");
  if (fp) {
     fprintf(fp,"%s\n",mycall);
     fprintf(fp,"%s\n",myrigdev);
     fprintf(fp,"%s\n",mypttdev);
     fprintf(fp,"%s\n",mywkydev);
     fprintf(fp,"%d\n",baudrates[mybaud]);
     fprintf(fp,"%s\n",get_rigname(mymodel));
     fprintf(fp,"%s\n",sounddevname[mysounddev]);
     fclose(fp);
  }
//
// Save current CW texts to CWTXT file
//
  sprintf(filename,"%s/CWTXT",workdir);
  fp=fopen(filename,"w");
  if (fp) {
    fprintf(fp,"%s:%s\n",cwlab1,cwtxt1);
    fprintf(fp,"%s:%s\n",cwlab2,cwtxt2);
    fprintf(fp,"%s:%s\n",cwlab3,cwtxt3);
    fprintf(fp,"%s:%s\n",cwlab4,cwtxt4);
    fprintf(fp,"%s:%s\n",cwlab5,cwtxt5);
    fclose(fp);
  }
//
// Save current samples to VOICES file
//
  sprintf(filename,"%s/VOICES",workdir);
  fp=fopen(filename,"w");
  if (fp) {
    fprintf(fp,"%s:%s\n",voicelab1,wav[0].filename);
    fprintf(fp,"%s:%s\n",voicelab2,wav[1].filename);
    fprintf(fp,"%s:%s\n",voicelab3,wav[2].filename);
    fprintf(fp,"%s:%s\n",voicelab4,wav[3].filename);
    fprintf(fp,"%s:%s\n",voicelab5,wav[4].filename);
    fclose(fp);
  }
  // w == NULL means a call from rig_open
  if (w != NULL) {
    fl_alert("Preferences saved to %s directory",workdir);
  }
}

void do_setvoice(Fl_Widget *w, void *) {
    set_voice_win = new Fl_Window(10,10,500,200);

    Fl_Input *l1 = new Fl_Input(60,  20, 80, 20, "Label 1"); l1->callback(do_text,voicelab1); l1->value(voicelab1);
    Fl_Input *l2 = new Fl_Input(60,  50, 80, 20, "Label 2"); l2->callback(do_text,voicelab2); l2->value(voicelab2);
    Fl_Input *l3 = new Fl_Input(60,  80, 80, 20, "Label 3"); l3->callback(do_text,voicelab3); l3->value(voicelab3);
    Fl_Input *l4 = new Fl_Input(60, 110, 80, 20, "Label 4"); l4->callback(do_text,voicelab4); l4->value(voicelab4);
    Fl_Input *l5 = new Fl_Input(60, 140, 80, 20, "Label 5"); l5->callback(do_text,voicelab5); l5->value(voicelab5);

    Fl_Button *b1 = new Fl_Button(180,  20, 250, 30, "Choose WAV file for Sample 1"); b1->callback(get_file, &wav[0]);
    Fl_Button *b2 = new Fl_Button(180,  50, 250, 30, "Choose WAV file for Sample 2"); b2->callback(get_file, &wav[1]);
    Fl_Button *b3 = new Fl_Button(180,  80, 250, 30, "Choose WAV file for Sample 3"); b3->callback(get_file, &wav[2]);
    Fl_Button *b4 = new Fl_Button(180, 110, 250, 30, "Choose WAV file for Sample 4"); b4->callback(get_file, &wav[3]);
    Fl_Button *b5 = new Fl_Button(180, 140, 250, 30, "Choose WAV file for Sample 5"); b5->callback(get_file, &wav[4]);

    Fl_Button *apply = new Fl_Button(20, 170, 100, 20, "Apply."); apply->callback(apply_voice, NULL);

    set_voice_win->label("Choose files for voice samples");
    set_voice_win->show();
}

void do_setcw(Fl_Widget *w, void *)
{
    set_cw_win = new Fl_Window(10,10,800,250);
    Fl_Input *l1 = new Fl_Input(60,  20, 60, 20, "Label 1"); l1->callback(do_text,cwlab1); l1->value(cwlab1);
    Fl_Input *l2 = new Fl_Input(60,  50, 60, 20, "Label 2"); l2->callback(do_text,cwlab2); l2->value(cwlab2);
    Fl_Input *l3 = new Fl_Input(60,  80, 60, 20, "Label 3"); l3->callback(do_text,cwlab3); l3->value(cwlab3);
    Fl_Input *l4 = new Fl_Input(60, 110, 60, 20, "Label 4"); l4->callback(do_text,cwlab4); l4->value(cwlab4);
    Fl_Input *l5 = new Fl_Input(60, 140, 60, 20, "Label 5"); l5->callback(do_text,cwlab5); l5->value(cwlab5);

    Fl_Input *t1 = new Fl_Input(180,  20, 600, 20, "Text 1"); t1->callback(do_text,cwtxt1); t1->value(cwtxt1);
    Fl_Input *t2 = new Fl_Input(180,  50, 600, 20, "Text 2"); t2->callback(do_text,cwtxt2); t2->value(cwtxt2);
    Fl_Input *t3 = new Fl_Input(180,  80, 600, 20, "Text 3"); t3->callback(do_text,cwtxt3); t3->value(cwtxt3);
    Fl_Input *t4 = new Fl_Input(180, 110, 600, 20, "Text 4"); t4->callback(do_text,cwtxt4); t4->value(cwtxt4);
    Fl_Input *t5 = new Fl_Input(180, 140, 600, 20, "Text 5"); t5->callback(do_text,cwtxt5); t5->value(cwtxt5);

    Fl_Button *apply = new Fl_Button(20, 180, 100, 20, "Apply."); apply->callback(apply_cw, NULL);

    set_cw_win->label("Enter labels and texts for CW buttons");
    set_cw_win->show();
}

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

//
//  if we are tuning,  just return (do not track mode and power since it is temporarily
//  changed)
//
    if (event_pass == 1) {
      ptt=get_ptt();
      f=get_freq() / 1000000.0;  // in MHz
      if (ptt == 0) ptt=get_ptt(); // ptt==0 means PTT_OFF before and after frequency readout
      sprintf(str,"%8.3f", f);
      freq->value(str);
      if (can_mldx) {
        //
        // Only MacOS:
        // ===========
        // push freq and mode data to MLDX if requested
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
      }
      // track the mode of the rig and set mode button accordingly
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
    }
    // Repeat this every 2 seconds. Note that get_freq(), get_ptt(),  get_mode()
    // may have to wait for the hamlib mutex.
    Fl::repeat_timeout(2.0, update_freq, NULL);
}
