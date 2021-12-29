/*!
 * @file uart.c
 * @brief UART driver.
 */

#include "sdk/sdk_types.h"
#include "registers/regsuart.h"
#include "uart.h"
#include "ccm_pll.h"

#define UART_UFCR_RFDIV    BF_UART_UFCR_RFDIV(4)
//#define UART_UFCR_RFDIV     UART_UFCR_RFDIV_4
//#define UART_UFCR_RFDIV     UART_UFCR_RFDIV_7

uint8_t uart_putchar (uint32_t instance, uint8_t *ch)
{
    /* Wait for Tx FIFO not full */
    while (HW_UART_UTS(instance).B.TXFULL);

    HW_UART_UTXD_WR(instance, *ch);

    while (!HW_UART_UTS(instance).B.TXEMPTY);

    return *ch;
}

uint8_t uart_getchar (uint32_t instance)
{
    uint32_t read_data;

    /* If Rx FIFO has no data ready */
    if (!(HW_UART_USR2(instance).B.RDR))
        return NONE_CHAR;

    read_data = HW_UART_URXD_RD(instance);

    /* If error are detected */
    if (read_data & 0x7C00)
        return NONE_CHAR;

    return (uint8_t) read_data;
}

uint32_t uart_get_reffreq (uint32_t instance)
{
    uint32_t div = UART_UFCR_RFDIV;
    uint32_t ret = 0;
    uint32_t freq = get_peri_clock(UART_MODULE_CLK(instance));

    if (div == BF_UART_UFCR_RFDIV(4))
        ret = freq / 2;
    else if (div == BF_UART_UFCR_RFDIV(2))
        ret = freq / 4;
    else if (div == BF_UART_UFCR_RFDIV(6))
        ret = freq / 7;

    return ret;
}

void uart_set_FIFO_mode (uint32_t instance, uint8_t fifo, uint8_t trigger_level, uint8_t service_mode)
{
    if (fifo == TX_FIFO) {
        /* Configure the TX_FIFO trigger level */
        HW_UART_UFCR_CLR(instance,BM_UART_UFCR_TXTL);
        HW_UART_UFCR_SET(instance, BF_UART_UFCR_TXTL(trigger_level));
        /* Configure the TX_FIFO service mode */
        /* Default mode is polling: IRQ and DMA requests are disabled */
        HW_UART_UCR1_CLR(instance,(BM_UART_UCR1_TRDYEN | BM_UART_UCR1_TXDMAEN));
        if (service_mode == DMA_MODE)
           HW_UART_UCR1_SET(instance,BM_UART_UCR1_TXDMAEN);
        else if (service_mode == IRQ_MODE)
            HW_UART_UCR1_SET(instance,BM_UART_UCR1_TRDYEN);
    } else {                    /* fifo = RX_FIFO */
        /* Configure the RX_FIFO trigger level */
         HW_UART_UFCR_CLR(instance,BM_UART_UFCR_RXTL);
        HW_UART_UFCR_SET(instance,BF_UART_UFCR_RXTL(trigger_level));
        /* Configure the RX_FIFO service mode */
        /* Default mode is polling: IRQ and DMA requests are disabled */
        HW_UART_UCR1_CLR(instance,(BM_UART_UCR1_RRDYEN | BM_UART_UCR1_RXDMAEN));
        if (service_mode == DMA_MODE)
            HW_UART_UCR1_SET(instance,BM_UART_UCR1_RXDMAEN);
        else if (service_mode == IRQ_MODE)
            HW_UART_UCR1_SET(instance,BM_UART_UCR1_RRDYEN);
    }
}

void uart_set_loopback_mode (uint32_t instance, uint8_t state)
{
    if (state == TRUE)
	HW_UART_UTS_SET(instance, BM_UART_UTS_LOOP);
    else
        HW_UART_UTS_CLR(instance, BM_UART_UTS_LOOP);
}

void uart_init (uint32_t instance, uint32_t baudrate, uint8_t parity, uint8_t stopbits, uint8_t datasize, uint8_t flowcontrol)
{
    uint32_t base = REGS_UART_BASE(instance);

    /* configure the I/O for the port */
    uart_iomux_config(instance);

    /* enable the source clocks to the UART port */
    clock_gating_config(base, CLOCK_ON);

    /* Wait for UART to finish transmitting before changing the configuration */
    while (!(HW_UART_UTS(instance).B.TXEMPTY)) ;

    /* Disable UART */
    HW_UART_UCR1_CLR(instance,BM_UART_UCR1_UARTEN );

    /* Configure FIFOs trigger level to half-full and half-empty */
    HW_UART_UFCR_WR(instance, BF_UART_UFCR_RXTL(16) | UART_UFCR_RFDIV | BF_UART_UFCR_TXTL(16));

    /* Setup One Millisecond timer */
    HW_UART_ONEMS_WR(instance, uart_get_reffreq(instance) / 1000);

    /* Set parity */
    if (parity == PARITY_NONE)
        HW_UART_UCR2_CLR(instance,(BM_UART_UCR2_PREN| BM_UART_UCR2_PROE));
    else if (parity == PARITY_ODD)
        HW_UART_UCR2_SET(instance,(BM_UART_UCR2_PREN| BM_UART_UCR2_PROE));
    else {                      /* parity == PARITY_EVEN */
        HW_UART_UCR2_SET(instance, BM_UART_UCR2_PREN);
        HW_UART_UCR2_CLR(instance, BM_UART_UCR2_PROE);
    }

    /* Set stop bit */
    if (stopbits == STOPBITS_ONE)
        HW_UART_UCR2_CLR(instance, BM_UART_UCR2_STPB);
    else                        /* stopbits == STOPBITS_TWO */
        HW_UART_UCR2_SET(instance, BM_UART_UCR2_STPB);

    /* Set data size */
    if (datasize == EIGHTBITS)
        HW_UART_UCR2_SET(instance, BM_UART_UCR2_WS);
    else                        /* stopbits == STOPBITS_TWO */
        HW_UART_UCR2_CLR(instance, BM_UART_UCR2_WS);

    /* Configure the flow control */
    if (flowcontrol == FLOWCTRL_ON) {
        /* transmit done when RTS asserted */
        HW_UART_UCR2_CLR(instance, BM_UART_UCR2_IRTS );
        /* CTS controlled by the receiver */
        HW_UART_UCR2_SET(instance, BM_UART_UCR2_CTSC );
    } else {                    /* flowcontrol == FLOWCTRL_OFF */
        /* Ignore RTS */
        HW_UART_UCR2_SET(instance,  BM_UART_UCR2_IRTS);
        /* CTS controlled by the CTS bit */
        HW_UART_UCR2_CLR(instance,  BM_UART_UCR2_CTSC);
    }

    /* the reference manual says that this bit must always be set */
    HW_UART_UCR3_SET(instance,  BM_UART_UCR3_RXDMUXSEL);

    /* Enable UART */
    HW_UART_UCR1_SET(instance, BM_UART_UCR1_UARTEN);

    /* Enable FIFOs and does software reset to clear status flags, reset
       the transmit and receive state machine, and reset the FIFOs */
    HW_UART_UCR2_SET(instance, BM_UART_UCR2_TXEN | BM_UART_UCR2_RXEN | BM_UART_UCR2_SRST);

    /* Set the numerator value minus one of the BRM ratio */
    HW_UART_UBIR_WR(instance, (baudrate / 100) - 1);

    /* Set the denominator value minus one of the BRM ratio */
    HW_UART_UBMR_WR(instance,  ((uart_get_reffreq(instance) / 1600) - 1));

    /* Optional: prevent the UART to enter debug state. Useful when debugging
       the code with a JTAG and without active IRQ */
    //HW_UART_UTS_SET(instance, BM_UART_UTS_DBGEN);
}

