#ifndef _VIVI_STRING_H_
#define _VIVI_STRING_H_

void u32todecimal(char *, unsigned long);
void binarytohex(char *, long, int);
unsigned long strtoul(const char *str, char **endptr, int requestedbase, int *ret);

void putstr_hex(const char *str, unsigned long value);
void putstr(const char *str);

/*
char *strcpy(char *dest, const char *src);
char *strncpy(char *dest, const char *src, int count);
char *strcat(char *dest, const char *src);
char *strncat(char *dest, const char *src, int count);
int strcmp(const char *cs, const char *ct);
int strncmp(const char *cs, const char *ct, int count);
char *strchr(const char *s, int c);
char *strrchr(const char *s, int c);
int strlen(const char *s);
int strnlen(const char *s, int count);

char *strdup(const char *s);
int strspn(const char *s, const char *accept);
char *strpbrk(const char *cs, const char *ct);
char *strsep(char **s, const char *ct);
char *strswab(const char *s);

void *memset(void *s, int c, int count);
char *bcopy(const char *src, char *dest, int count);
void *memcpy(void *dest, const void *src, int count);
void *memmove(void *dest, const void *src, int count);
int memcmp(const void *cs, const void *ct, int count);
void *memscan(void *addr, int c ,int size);
char *strstr(const char *s1, const char *s2);
void *memchr(const void *s, int c, int n);
*/

#endif /* _VIVI_STRING_H_ */

