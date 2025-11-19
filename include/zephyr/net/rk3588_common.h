#ifndef __CONFIG_RK3588_COMMON_H
#define __CONFIG_RK3588_COMMON_H


/************************** common ***************************/
typedef unsigned long   ulong;
typedef unsigned long   size_t;

typedef signed char s8;
typedef unsigned char u8;

typedef signed short s16;
typedef unsigned short u16;

typedef signed int s32;
typedef unsigned int u32;

typedef signed long long s64;
typedef unsigned long long u64;

#define ISB	__asm__ volatile ("isb sy" : : : "memory")
#define DSB	__asm__ volatile ("dsb sy" : : : "memory")
#define DMB	__asm__ volatile ("dmb sy" : : : "memory")

#define isb()	ISB
#define dsb()	DSB
#define dmb()	DMB

#define __arch_getb(a)			(*(volatile unsigned char *)(a))
#define __arch_getw(a)			(*(volatile unsigned short *)(a))
#define __arch_getl(a)			(*(volatile unsigned int *)(a))
#define __arch_getq(a)			(*(volatile unsigned long long *)(a))

#define __arch_putb(v,a)		(*(volatile unsigned char *)(a) = (v))
#define __arch_putw(v,a)		(*(volatile unsigned short *)(a) = (v))
#define __arch_putl(v,a)		(*(volatile unsigned int *)(a) = (v))
#define __arch_putq(v,a)		(*(volatile unsigned long long *)(a) = (v))

#define mb()		dsb()
#define __iormb()	dmb()
#define __iowmb()	dmb()

#define readb(c)	({ u8  __v = __arch_getb(c); __iormb(); __v; })
#define readw(c)	({ u16 __v = __arch_getw(c); __iormb(); __v; })
#define readl(c)	({ u32 __v = __arch_getl(c); __iormb(); __v; })
#define readq(c)	({ u64 __v = __arch_getq(c); __iormb(); __v; })

#define writeb(v,c)	({ u8  __v = v; __iowmb(); __arch_putb(__v,c); __v; })
#define writew(v,c)	({ u16 __v = v; __iowmb(); __arch_putw(__v,c); __v; })
#define writel(v,c)	({ u32 __v = v; __iowmb(); __arch_putl(__v,c); __v; })
#define writeq(v,c)	({ u64 __v = v; __iowmb(); __arch_putq(__v,c); __v; })

#define rk_clrsetreg(addr, clr, set)	writel(((clr) | (set)) << 16 | (set), addr)
#define rk_clrreg(addr, clr)			writel((clr) << 16, addr)
#define rk_setreg(addr, set)			writel((set) << 16 | (set), addr)

// #define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)

#define check_member(structure, member, offset) _Static_assert( \
	offsetof(struct structure, member) == offset, \
	"`struct " #structure "` offset for `" #member "` is not " #offset)

#if 0
#define check_member(structure, member, offset) { \
	if(offsetof(struct structure, member) != offset) { \
	} \
}
#endif


/************************** uart2 ***************************/

#define CONFIG_SYS_NS16550_REG_SIZE (-1)

#define UART_REG(x)							\
	unsigned char x;
	//unsigned char postpad_##x[-CONFIG_SYS_NS16550_REG_SIZE - 1];

struct NS16550 {
	UART_REG(rbr);		/* 0 */
	UART_REG(ier);		/* 1 */
	UART_REG(fcr);		/* 2 */
	UART_REG(lcr);		/* 3 */
	UART_REG(mcr);		/* 4 */
	UART_REG(lsr);		/* 5 */
	UART_REG(msr);		/* 6 */
	UART_REG(spr);		/* 7 */
	UART_REG(mdr1);		/* 8 */
	UART_REG(reg9);		/* 9 */
	UART_REG(regA);		/* A */
	UART_REG(regB);		/* B */
	UART_REG(regC);		/* C */
	UART_REG(regD);		/* D */
	UART_REG(regE);		/* E */
	UART_REG(uasr);		/* F */
	UART_REG(scr);		/* 10*/
	UART_REG(ssr);		/* 11*/
	// struct ns16550_platdata *plat;
};

#define CONFIG_DEBUG_UART_SHIFT 2

#define serial_dout(reg, value)	\
	writel(value, (char *)com_port + \
		((char *)reg - (char *)com_port) * \
			(1 << CONFIG_DEBUG_UART_SHIFT) )
#define serial_din(reg) \
	readl((char *)com_port + \
		((char *)reg - (char *)com_port) * \
			(1 << CONFIG_DEBUG_UART_SHIFT))

#define thr rbr
#define iir fcr
#define dll rbr
#define dlm ier
/************************** GIC ***************************/

#endif
