#ifndef __EARLY_PRINTK_H__
#define __EARLY_PRINTK_H__

#include <xen/string.h>

#define early_puts(s) _early_puts((s), strlen((s)))
void _early_puts(const char *s, size_t nr);
void early_printk(const char *fmt, ...);

#endif /* __EARLY_PRINTK_H__ */
