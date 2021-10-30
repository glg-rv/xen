/*
 * RISC-V early printk using SBI
 *
 * Copyright (C) 2021 Bobby Eshleman <bobbyeshleman@gmail.com>
 */
#include <asm/sbi.h>
#include <asm/early_printk.h>
#include <xen/stdarg.h>
#include <xen/lib.h>

void _early_puts(const char *s, size_t nr)
{
    while ( nr-- > 0 )
    {
        if (*s == '\n')
            sbi_console_putchar('\r');
        sbi_console_putchar(*s);
        s++;
    }
}

static void vprintk_early(const char *prefix, const char *fmt, va_list args)
{
    char buf[128];
    int sz;

    early_puts(prefix);

    sz = vscnprintf(buf, sizeof(buf), fmt, args);

    if ( sz < 0 ) {
        early_puts("(XEN) vprintk_early error\n");
        return;
    }

    if ( sz == 0 )
        return;

    _early_puts(buf, sz);
}

void early_printk(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vprintk_early("(XEN) ", fmt, args);
    va_end(args);
}
