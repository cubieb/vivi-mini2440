/*
 * vivi/include/memory.h
 *
 * Copyright (C) 2001,2002 MIZI Research, Inc.
 *
 * $Id: memory.h,v 1.1.1.1 2004/02/04 06:22:25 laputa Exp $
 */

#ifndef __VIVI_MEMORY_H__
#define __VIVI_MEMORY_H__

#include <types.h>

unsigned long mem_compare(const char *to, const char *from, size_t len, int echo);
long find_dram_size(long *base, long maxsize);
void clear_mem(unsigned long, unsigned long);

#endif /* __VIVI_MEMORY_H__ */
