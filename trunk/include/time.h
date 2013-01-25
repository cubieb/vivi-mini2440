#ifndef _VIVI_TIME_H_
#define _VIVI_TIME_H_

#include "config.h"

#ifndef __ASSEMBLY__

//added by ayang 2008-10-28
extern unsigned long timestamp;
extern unsigned long lastdec;
extern unsigned long timer_load_val;

inline void init_time(void);
inline unsigned long get_timer(unsigned long base);
inline void mdelay(unsigned long);
void udelay(unsigned long);
#endif

#endif /* _VIVI_TIME_H_ */
