#ifndef _VIVI_SERIAL_H_
#define _VIVI_SERIAL_H_

#include <config.h>
#include <types.h>

typedef void (*vfuncp)(void);

void putc(char c);
char getc(void);
int tstc(void);
void putnstr(const char *str, size_t n);
char awaitkey(unsigned long delay, int* error_p);
__u8 do_getc(vfuncp, unsigned long, int *);

#ifdef CONFIG_SERIAL_TERM
void serial_term(void);
#endif

#endif /* _VIVI_SERIAL_H_  */
