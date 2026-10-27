#include <stdint.h>

#define USB2_BASE ((volatile usb2_t *)(0x20200000))
#define USB2_DMA_BASE ((volatile usb2_dma_t *)(0x20201000))

static uint8_t high_speed_enabled = 0;

typedef struct {
    volatile uint16_t tx_maxp, ep0_ctrl;
    uint8_t pad1[4];
    volatile uint8_t ep0_rx_count;
    uint8_t pad2[2];
    volatile uint8_t nak_lim;
    uint8_t pad3[3];
    volatile uint8_t ep0_cfg;
} usb2_ep0_t;

typedef struct {
    volatile uint16_t tx_maxp, tx_ctrl, rx_maxp, rx_ctrl;
    volatile uint16_t rx_count;
    volatile uint8_t tx_trnsc_ctrl, tx_poll; // trnsc is for transaction btw
    volatile uint8_t rx_trnsc_ctrl, rx_poll;
    uint8_t pad1;
    volatile uint8_t fifo_size;
} usb2_ep_t;

typedef struct {
    volatile uint8_t f_addr, power;
    volatile uint16_t tx_int, rx_int, tx_int_en, rx_int_en;
    volatile uint8_t usb_int, usb_int_en;
    volatile uint16_t frame;
    volatile uint8_t index, testmode;

    // offsets 0x10 to 0x20 are unspecified
    uint32_t pad1[4];

    volatile uint32_t fifo_eps[16]; // [0] = EP 0

    volatile uint8_t otg_dev_control, misc, tx_fifo_size, rx_fifo_size;
    volatile uint16_t tx_fifo_addr, rx_fifo_addr;
    volatile uint32_t resv1; // Offsets 0x68 - 0x6C are reserved
    volatile uint16_t hw_ver;
    volatile uint16_t resv2[5]; // Offsets 0x6E - 0x78 are reserved
    volatile uint8_t ep_info, ram_info, link_info, vb_plen;
    volatile uint8_t hs_eof, fs_eof, ls_eof, soft_reset;

    // Offsets 0x80 - 0x100 are unspecified
    uint8_t pad2[128];

    volatile usb2_ep0_t ep0_conf;
    volatile usb2_ep_t ep_confs[15]; // `[0]` = EP 1, `[14]` = EP 15
} usb2_t;

// size = 32
typedef struct {
    volatile uint32_t pause, cfg, chan_int, chan_addr;
    volatile uint32_t len, llist_ptr, chan_addr_h, req;
} usb2_dma_chan_conf_t;

typedef struct {
    volatile uint32_t pause, wait_conf, int_raw;
    volatile uint32_t int_stat, req_stat, en_stat, dbg_stat;

    // Addresses 0x01C - 0xC00 are empty
    uint8_t pad1[3044];

    // [0] = channel 1 cfg
    volatile usb2_dma_chan_conf_t chan_confs[15];
} usb2_dma_t;

// lmao I stole this shit straight from the Boot ROM
// Traditionally tx_ep is 5 and rx_ep is 6
int32_t usb_ep_init(uint8_t tx_ep, uint8_t rx_ep) {
    if (tx_ep == 0 || tx_ep > 15) return -1;
    if (rx_ep == 0 || rx_ep > 15 || rx_ep == tx_ep) return -1;

    volatile usb2_t *usb = USB2_BASE;

    usb->index = tx_ep; // Select endpoint tx_ep
    usb->tx_int_en = 1 | 1 << tx_ep; // Enable TX interrupt for EPs 0 and ep
    usb->rx_int_en = 1 << rx_ep; // Enable RX interrupt for EP 6

    usb->tx_fifo_size = 0x17; // Set tx FIFO size to 1024 & enable double pakcte buffering
    usb->tx_fifo_addr = 0x108; // Set tx FIFO address to 0x840

    // Set max payload size to 512b for HS or 64b otherwise
    usb->ep_confs[tx_ep].tx_maxp = high_speed_enabled ? 512 : 64;

    // Reset EP data toggle to 0, and flush the latest packet from the FIFO.
    usb->ep_confs[tx_ep].tx_ctrl = 0x48;

    // Set endpoint direction to TX.
    usb->ep_confs[tx_ep].tx_ctrl |= 0x20 << 8;

    // Enable automatic setting of TxPktReady, enable DMA requests for EP 5, and
    // select DMA request mode 1
    usb->ep_confs[tx_ep].tx_ctrl |= 0x94 << 8;

    usb->index = rx_ep;
    usb->rx_fifo_size = 0x17; // Set the FIFO size to 1024, enable double packet buffering

    usb->rx_fifo_addr = 0x8; // Set FIFO address to 040

    usb->ep_confs[rx_ep].rx_maxp = high_speed_enabled ? 512 : 64;

    // Flush the next packet to be read, reset data toggle to 0
    usb->ep_confs[rx_ep].rx_ctrl = 0x90;

    // Auto set RDDY bit once max size packet has been read from FIFO, enable DMA
    // requests (with mode 0)
    usb->ep_confs[rx_ep].rx_ctrl = 0xa8 << 8;

    return 0;
}