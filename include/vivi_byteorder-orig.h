#ifndef _VIVI_BYTEORDER_H_
#define _VIVI_BYTEORDER_H_

#include <types.h>
#include "config.h"

#undef htonl
#undef ntohl
#undef htons
#undef ntohs

//#ifndef CONFIG_CS8900A

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

#if 0
//#else

#define _swab8(x) \
        ((u8)( \
	       (((u8)(x) & (u8)0x0fU) << 4) | \
	       (((u8)(x) & (u8)0xf0U) >> 4) ))

#define _swab16(x) \
	((u16)( \
		(((u16)(x) & (u16)0x000fU) << 12) | \
		(((u16)(x) & (u16)0x00f0U) << 4) | \
		(((u16)(x) & (u16)0x0f00U) >> 4) | \
		(((u16)(x) & (u16)0xf000U) >> 12) ))

#define _swab32(x) \
	((u32)( \
		(((u32)(x) & (u32)0x0000000fUL) << 28) | \
		(((u32)(x) & (u32)0x000000f0UL) << 20) | \
		(((u32)(x) & (u32)0x00000f00UL) << 12) | \
		(((u32)(x) & (u32)0x0000f000UL) << 4) | \
		(((u32)(x) & (u32)0x000f0000UL) >> 4) | \
		(((u32)(x) & (u32)0x00f00000UL) >> 12) | \
		(((u32)(x) & (u32)0x0f000000UL) >> 20) | \
		(((u32)(x) & (u32)0xf0000000UL) >> 28) ))

//#endif /* CONFIG_CS8900A */
#endif /* if 0 */

#define htonl(x) _swab32(x)
//#define ntohl(x) _swab32(x)
#define htons(x) _swab16(x)
//#define ntohs(x) _swab16(x)

#ifndef CONFIG_CS8900A
#define ntohl(x) _swab32(x)
#define ntohs(x) _swab16(x)
#elseÂ
#define ntohs(x) ((u16)(x))
#define ntohl(x) ((u32)(x))
#endif

#if 0

#include <asm/byteorder.h>

#ifdef CONFIG_CS8900A

//#define htonl(x) (x)
#define ntohl(x) (x)
//#define htons(x) (x)
#define ntohs(x) ((u16)(x))

#endif

#endif /* if 0 */


#endif /*_VIVI_BYTEORDER_H_*/

