#include "log.h"
#include "uart.h"
#include "registers/regsuart.h"
#include "common/printf.h"
#include <os_types.h>
#include "ipc/msg.h"
#include "ipc/connection.h"
#include "config.h"

static uint8_t logbuf[LOG_BUF_MAX] = {0};
static size_t fill = 0;
//static int log_conid = 0;
static int loglvl = LOGLEVEL_ALL;
static log_target_t logtarget = LOG_TARGET_BUFFER;

int get_loglevel ()
{
    return loglvl;
}

void set_loglevel (int val)
{
    if ((val < 0) || (val > LOGLEVEL_OFF))
        return;
    loglvl = val;
}

log_target_t get_log_target ()
{
    return logtarget;
}

void set_log_target (log_target_t target, ...)
{
    //int chid;
    //va_list va;

    switch (target) {
    case LOG_TARGET_BUFFER:
    case LOG_TARGET_UART1:
    case LOG_TARGET_UART2:
        logtarget = target;
        break;
    case LOG_TARGET_MSG_CHANNEL:
        // log_conid = open_connection(kthr, LOGGER_CHANNEL, TIMEOUT_INFINITY, reply);
        break;
    }
}

size_t logmsg (int lvl, const char *fmt, ...)
{
    if (lvl < loglvl) {
        return 0;
    }
    va_list args;
    va_start(args, fmt);
    size_t len = (size_t) vsnprintf((char*)&logbuf[fill], LOG_BUF_MAX - fill, fmt, args);
    va_end(args);
    fill += len;

    switch (logtarget) {
    case LOG_TARGET_BUFFER:
        break;
    case LOG_TARGET_UART1:
    case LOG_TARGET_UART2:
        for (int i = 0; i < fill; i++)
            uart_putchar(logtarget, &logbuf[i]);
        fill = 0;
        break;
    case LOG_TARGET_MSG_CHANNEL:
        if (fill > 0) {
//            struct msg msg = {0};
//            msg.data = logbuf;
//            msg.size = fill;
            //send(log_conid, logger, &msg, NO_WAIT, NO_FLAGS);
            fill = 0;
        }
        break;
    }
    return len;
}
