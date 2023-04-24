/*
 * Definitions mostly taken from HAMLIB repository, file
 * src/misc.h
 */

extern HAMLIB_EXPORT(int) sprintf_freq(char *str, int str_len, freq_t);
extern HAMLIB_EXPORT(double) elapsed_ms(struct timespec *start, int start_flag);
extern HAMLIB_EXPORT (const char *) spaces();
extern HAMLIB_EXPORT(char *)date_strget(char *buf, int buflen, int localtime);

void dump_hex(const unsigned char ptr[], size_t size);

#ifdef PRId64
/** \brief printf(3) format to be used for long long (64bits) type */
#  define PRIll PRId64
#  define PRXll PRIx64
#else
#  ifdef FBSD4
#    define PRIll "qd"
#    define PRXll "qx"
#  else
#    define PRIll "lld"
#    define PRXll "lld"
#  endif
#endif

#define ENTERFUNC {     ++rig->state.depth; \
                        rig_debug(RIG_DEBUG_VERBOSE, "%.*s%d:%s(%d):%s entered\n", rig->state.depth, spaces(), rig->state.depth, __FILENAME__, __LINE__, __func__); \
                  }
#define ENTERFUNC2 {    rig_debug(RIG_DEBUG_VERBOSE, "%s(%d):%s entered\n", __FILENAME__, __LINE__, __func__); \
                   }

// we need to refer to rc just once as it 
// could be a function call  

#define RETURNFUNC(rc) {do { \
                                    int rctmp = rc; \
                        rig_debug(RIG_DEBUG_VERBOSE, "%.*s%d:%s(%d):%s returning(%ld) %s\n", rig->state.depth, spaces(), rig->state.depth, __FILENAME__, __LINE__, __func__, (long int) (rctmp), rctmp<0?rigerror2(rctmp):""); \
                        --rig->state.depth; \
                        return (rctmp); \
                       } while(0);}
#define RETURNFUNC2(rc) {do { \
                                    int rctmp = rc; \
                        rig_debug(RIG_DEBUG_VERBOSE, "%s(%d):%s returning2(%ld) %s\n",  __FILENAME__, __LINE__, __func__, (long int) (rctmp), rctmp<0?rigerror2(rctmp):""); \
                        return (rctmp); \
                       } while(0);}



//
// Here we make some definitions that lets this compile both with Hamlib4.0 and Hamlib4.2
//

//
// these compile-time constants changed name since Hamlib4.0
#ifndef FRQRANGESIZ
#define FRQRANGESIZ HAMLIB_FRQRANGESIZ
#endif
#ifndef TSLSTSIZ
#define TSLSTSIZ HAMLIB_TSLSTSIZ
#endif
#ifndef FLTLSTSIZ
#define FLTLSTSIZ HAMLIB_FLTLSTSIZ
#endif
#ifndef MAXDBLSTSIZ
#define MAXDBLSTSIZ HAMLIB_MAXDBLSTSIZ
#endif
#ifndef CHANLSTSIZ
#define CHANLSTSIZ HAMLIB_CHANLSTSIZ
#endif
