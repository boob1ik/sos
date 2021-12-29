#include "secure.h"
#include "common/log.h"
#include <os_types.h>
#include "ipc/msg.h"
#include "ipc/connection.h"

//static int secure_conid = 0;

bool secure_init ()
{
//    secure_conid = open_connection(kthr, SECURE_CHANNEL, TIMEOUT_INFINITY, reply);
//    if (secure_conid < 0)
//        return false;
//
//    lprintf("Secure init");
//    secure_chid = chid;
    return true;
}

void secure (msg_t *msg)
{
//    if (!secure_chid)
//        return;
//    send(SECURE, &msg, NO_WAIT);
}
