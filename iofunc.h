/*
 * Definitions taken from HAMLIB repository,
 * file src/iofunc.h
 */

extern HAMLIB_EXPORT(int) read_string(hamlib_port_t *p,
                                      unsigned char *rxbuffer,
                                      size_t rxmax,
                                      const char *stopset,
                                      int stopset_len,
                                      int flush_flag,
                                      int expected_len);

extern HAMLIB_EXPORT(int) write_block(hamlib_port_t *p,
                                      const unsigned char *txbuffer,
                                      size_t count);

