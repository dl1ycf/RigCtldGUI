extern HAMLIB_EXPORT(int) sprintf_freq(char *str, freq_t);

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

#define ENTERFUNC rig_debug(RIG_DEBUG_VERBOSE, "%s(%d):%s entered\n", __FILENAME__, __LINE__, __func__)
#define RETURNFUNC(rc) {rig_debug(RIG_DEBUG_VERBOSE, "%s(%d):%s return\n", __FILENAME__, __LINE__, __func__);return rc;}


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
