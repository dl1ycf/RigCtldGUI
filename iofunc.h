extern HAMLIB_EXPORT(int) read_string(hamlib_port_t *p,
                                      char *rxbuffer,
                                      size_t rxmax,
                                      const char *stopset,
                                      int stopset_len);

extern HAMLIB_EXPORT(int) write_block(hamlib_port_t *p,
                                      const char *txbuffer,
                                      size_t count);

