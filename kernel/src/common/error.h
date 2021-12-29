#ifndef _ERROR_H_
#define _ERROR_H_

static inline err_t error (err_t err)
{
    if (err != OK && err != ERR_DEAD)
        asm volatile ("bkpt");
    return err;
}

#define ERROR(x) error(x)

#endif
