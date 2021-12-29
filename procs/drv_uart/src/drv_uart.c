#include <os-libc.h>
PROCNAME("driver_uart");

#include "regsuart.h"
#include "uart.h"
#include "include/drv_uart.h"
//#include <pthread.h>

#include <string.h>
#include <stdlib.h>


static int tid_rx, tid_tx;
static int rx_barrier_id, tx_barrier_id;

#define DEBUG_ASSERT_SC(x) {if (x < 0) {err_t err = (err_t)x; asm volatile ("bkpt");}}

static struct rx {
    unsigned char buf[1024];
    unsigned char *tail, *head;
} rx_buf;

static struct tx {
    size_t size;
    unsigned char *ptr;
} tx_buf = {
    .size = 0,
    .ptr = NULL,
};

static const int uart_interrupt_id[] = { 58, 59, 60, 61, 62 };
static unsigned long instance = 0;

static void interrupt ()
{
    while (tx_buf.size && HW_UART_USR1(instance).B.TRDY) {
        uart_putchar(instance, tx_buf.ptr++);
        if (--tx_buf.size == 0) {
            transmitter_ready_interrupt_disable(instance);
            while (!HW_UART_UTS(instance).B.TXEMPTY)
                ;
            os_syn_wait(tx_barrier_id, TIMEOUT_INFINITY);
        }
    }

    if (HW_UART_USR1(instance).B.RRDY || HW_UART_USR1(instance).B.AGTIM) {
        while (HW_UART_USR2(instance).B.RDR) {
            unsigned char ch = uart_getchar(instance);
            if (rx_buf.tail >= rx_buf.buf + 1024)
                rx_buf.tail = rx_buf.buf;
            *rx_buf.tail++ = ch;
            os_syn_wait(rx_barrier_id, TIMEOUT_INFINITY);
        }
        HW_UART_USR1_SET(instance, BM_UART_USR1_AGTIM);
    }
}

static void tx (int chid)
{
    for (;;) {
        struct msg *m = NULL;
        int res = os_receive(chid, &m, TIMEOUT_INFINITY);
        if (res == ERR_DEAD)
            return;
        DEBUG_ASSERT_SC(res);
        tx_buf.ptr = m->user.data;
        tx_buf.size = m->user.size;
        transmitter_ready_interrupt_enable(instance);
        os_syn_wait(tx_barrier_id, TIMEOUT_INFINITY);
    }
}

static int rx (int conid)
{
    rx_buf.head = rx_buf.tail = rx_buf.buf;
    receiver_ready_interrupt_enable(instance);
    for (;;) {
        int res = os_syn_wait(rx_barrier_id, TIMEOUT_INFINITY);
        if (res == ERR_DEAD)
            return OK;
        DEBUG_ASSERT_SC(res);
        while (rx_buf.head != rx_buf.tail) {
            struct msg m;
            m.sys._ptr = NULL;
            m.user.data = rx_buf.head;
            if (rx_buf.head > rx_buf.tail) {
                m.user.size = rx_buf.buf + 1024 - rx_buf.head;
                rx_buf.head = rx_buf.buf;
            } else {
                m.user.size = rx_buf.tail - rx_buf.head;
                rx_buf.head = rx_buf.tail;
            }
            int res = os_send(conid, &m, TIMEOUT_INFINITY, NO_FLAGS);
            if (res == ERR_DEAD)
                return OK;
            DEBUG_ASSERT_SC(res);
        }
    }
    return ERR;
}

static void confirm_boot (char *path, char *code)
{
    int parent = os_connection_open(path, NO_REPLY, TIMEOUT_INFINITY);
    DEBUG_ASSERT_SC(parent);

    struct msg m = {
        .sys._ptr = NULL,
        .user.size = strlen(code),
        .user.data = code,
    };
    DEBUG_ASSERT_SC(os_send(parent, &m, TIMEOUT_INFINITY, NO_FLAGS));
    DEBUG_ASSERT_SC(os_connection_close(parent));
}

int main (int argc, char *argv[])
{
    if (argc < 2)
        return ERR_ILLEGAL_ARGS;

    char *instance_str = argv[1];
    instance = atoi(instance_str);
    if (instance < 0 || instance > HW_UART_INSTANCE_COUNT)
        return ERR_ILLEGAL_ARGS;

    // параметры по умолчанию
    unsigned long baudrate = 115200;
    enum _uart_parity parity = PARITY_NONE;
    enum _uart_stopbits stopbits = STOPBITS_ONE;
    enum _uart_bits bits = EIGHTBITS;
    enum _uart_flowctrl flowctrl = FLOWCTRL_OFF;
    // TODO через аргументы передавать настроечные параметры (корректировать указанные флагами параметры)

    char *parent_path = argv[0];
    char path[100];
    strcpy(path, parent_path);
    strcat(path, "/uart/");
    strcat(path, instance_str);
    int connect_chid = os_channel_open(CHANNEL_PROTECTED, path, 4096, CHANNEL_SINGLE_CONNECTION);
    DEBUG_ASSERT_SC(connect_chid);

    if (argc >= 4) {
        char *confirm_path = argv[2];
        char *confirm_code = argv[3];
        confirm_boot(confirm_path, confirm_code);
    }

    struct connection_info info;
    while (os_channel_wait_connection(connect_chid, &info, TIMEOUT_INFINITY) != OK)
        ;

    // TODO сначала захватить ноги у драйвера IOMUX  (концепт - пока есть соединение - тебе принадлежат ноги)

    const mem_attributes_t attr = {
        .shared = MEM_SHARED_OFF,
        .exec = MEM_EXEC_NEVER,
        .type = MEM_TYPE_DEVICE,
        .inner_cached = MEM_CACHED_OFF,
        .outer_cached = MEM_CACHED_OFF,
        .process_access = MEM_ACCESS_RW,
        .os_access = MEM_ACCESS_RW,
        .security = MEM_SECURITY_OFF,
        .multu_alloc = MEM_MULTU_ALLOC_OFF,
    };
    DEBUG_ASSERT_SC(os_mmap(uart_base_adr(instance), uart_pages(instance), attr));

    uart_init(instance, baudrate, parity, stopbits, bits, flowctrl);

    // TODO включить тактирование узла в драйвера CCM  (концепт - пока есть соединение - включено тактирование узла)

    int private = os_channel_open(CHANNEL_PRIVATE, NULL, 4096, CHANNEL_SINGLE_CONNECTION | CHANNEL_AUTO_KILL);
    DEBUG_ASSERT_SC(private);

    syn_t synobj = {
        .type = BARRIER_TYPE_PLOCAL,
        .limit = 2,
    };
    rx_barrier_id = os_syn_create(&synobj);
    DEBUG_ASSERT_SC(rx_barrier_id);
    tx_barrier_id = os_syn_create(&synobj);
    DEBUG_ASSERT_SC(tx_barrier_id);

    struct thread_attr irq_attributes = {
        .entry = interrupt,
        .arg = NULL,
        .tid = 0,
        .stack_size = DEFAULT_PAGE_SIZE,
        .ts = 1,
        .flags = NO_FLAGS,
        .type = THREAD_TYPE_IRQ_HANDLER,
    };
    int tid_interrupt = os_thread_create(&irq_attributes);
    DEBUG_ASSERT_SC(tid_interrupt);

    DEBUG_ASSERT_SC(os_irq_hook(uart_interrupt_id[instance - 1], tid_interrupt));
    irq_ctrl_t ctrl = {
        .enable = 1,
        .assert_type = IRQ_ASSERT_DEFAULT, };
    DEBUG_ASSERT_SC(os_irq_ctrl(uart_interrupt_id[instance - 1], ctrl));

    struct thread_attr tx_attributes = {
        .entry = tx,
        .arg = (void*)private,
        .tid = 0,
        .stack_size = DEFAULT_PAGE_SIZE,
        .ts = 1,
        .flags = NO_FLAGS,
        .type = THREAD_TYPE_DETACHED,
    };
    tid_tx = os_thread_create(&tx_attributes);
    DEBUG_ASSERT_SC(tid_tx);
    DEBUG_ASSERT_SC(os_thread_run(tid_tx));

    int reply = os_channel_complete_connection(connect_chid, CONNECTION_TO_PRIVATE, private);
    DEBUG_ASSERT_SC(reply);

    DEBUG_ASSERT_SC(os_channel_close(connect_chid));

    struct thread_attr rx_attributes = {
        .entry = rx,
        .arg = (void*)reply,
        .tid = 0,
        .stack_size = DEFAULT_PAGE_SIZE,
        .ts = 1,
        .flags = NO_FLAGS,
        .type = THREAD_TYPE_DETACHED,
    };
    tid_rx = os_thread_create(&rx_attributes);
    DEBUG_ASSERT_SC(tid_rx);
    DEBUG_ASSERT_SC(os_thread_run(tid_rx));

    DEBUG_ASSERT_SC(os_thread_join(tid_tx));

    DEBUG_ASSERT_SC(os_syn_delete(rx_barrier_id, 0));
    DEBUG_ASSERT_SC(os_syn_delete(tx_barrier_id, 0));

    DEBUG_ASSERT_SC(os_connection_close(reply));
    DEBUG_ASSERT_SC(os_channel_close(private));

    DEBUG_ASSERT_SC(os_irq_release(uart_interrupt_id[instance - 1]));
    return OK;
}
