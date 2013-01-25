#ifndef _VIVI_BYTEORDER_H_
#define _VIVI_BYTEORDER_H_

#include <types.h>
#include "config.h"

#undef htonl
#undef ntohl
#undef htons
#undef ntohs

#define _swab16(x) \
	((u16)( \
		(((u16)(x) & (u16)0x00ffU) << 8) | \
		(((u16)(x) & (u16)0xff00U) >> 8) ))

#define _swab32(x) \
	((u32)( \
		(((u32)(x) & (u32)0x000000ffUL) << 24) | \
		(((u32)(x) & (u32)0x0000ff00UL) <<  8) | \
		(((u32)(x) & (u32)0x00ff0000UL) >>  8) | \
		(((u32)(x) & (u32)0xff000000UL) >> 24) ))

#define htonl(x) _swab32(x)
#define htons(x) _swab16(x)

#ifndef CONFIG_CS8900A
#define ntohl(x) _swab32(x)
#define ntohs(x) _swab16(x)
#elseÂ
#define ntohs(x) ((u16)(x))
#define ntohl(x) ((u32)(x))
#endif

#endif /*_VIVI_BYTEORDER_H_*/

