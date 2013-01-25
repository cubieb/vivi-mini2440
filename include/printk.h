#ifndef _VIVI_PRINTK_H_
#define _VIVI_PRINTK_H_

int printk(const char *fmt, ...);

int sprintf(char *buf, const char *fmt, ...);

unsigned long simple_strtoul(const char *cp, char **endp, unsigned int base);

void print_size(unsigned long size, const char *s);

#endif /* _VIVI_PRINTK_H_ */

