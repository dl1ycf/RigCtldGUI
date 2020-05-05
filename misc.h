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

