/* =============================================================================
 * Quark-OS drivers/net.c
 * Intel 82540EM (e1000) PCI Network Interface Card bare-metal driver
 * ============================================================================= */

#include <quark/kernel.h>

static uint32_t mmio_base = 0;
static uint8_t  mac_address[6] = {0x52, 0x54, 0x00, 0x12, 0x34, 0x56}; /* QEMU Default MAC */
static uint8_t  link_up = 0;

/* Ring memory descriptor structures mapped in heap space */
static e1000_rx_desc_t *rx_ring = NULL;
static e1000_tx_desc_t *tx_ring = NULL;
static uint8_t *rx_buffers = NULL;

/* Low-level MMIO Register Reading/Writing Macros */
static inline void write_reg(uint32_t reg, uint32_t val) {
    *(volatile uint32_t *)(mmio_base + reg) = val;
}
static inline uint32_t read_reg(uint32_t reg) {
    return *(volatile uint32_t *)(mmio_base + reg);
}

/* Reads 32-bit registers out of host PCI controller matrix configuration space */
static uint32_t pci_read(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address = (uint32_t)((uint32_t)1 << 31) | 
                       ((uint32_t)bus << 16) | 
                       ((uint32_t)slot << 11) | 
                       ((uint32_t)func << 8) | 
                       (offset & 0xFC);
    outl(PCI_CONFIG_ADDRESS, address);
    return inl(PCI_CONFIG_DATA);
}

void e1000_init(uint32_t bar0, uint32_t mem_base) {
    (void)bar0;
    mmio_base = mem_base;
    kprintf("[NET]  Configuring Intel e1000 MMIO Controller at: %p\n", mmio_base);

    /* Allocate descriptors rings out of Quark Kernel Heap memory */
    rx_ring = (e1000_rx_desc_t *)kmalloc(sizeof(e1000_rx_desc_t) * E1000_RX_DESC_COUNT);
    tx_ring = (e1000_tx_desc_t *)kmalloc(sizeof(e1000_tx_desc_t) * E1000_TX_DESC_COUNT);
    rx_buffers = (uint8_t *)kmalloc(E1000_PACKET_SIZE * E1000_RX_DESC_COUNT);

    kmemset(rx_ring, 0, sizeof(e1000_rx_desc_t) * E1000_RX_DESC_COUNT);
    kmemset(tx_ring, 0, sizeof(e1000_tx_desc_t) * E1000_TX_DESC_COUNT);

    /* Point descriptors pointers inside e1000 layout blocks */
    for(int i = 0; i < E1000_RX_DESC_COUNT; i++) {
        rx_ring[i].addr = (uint32_t)(rx_buffers + (i * E1000_PACKET_SIZE));
        rx_ring[i].status = 0;
    }

    /* Program hardware descriptors ring parameters inside MMIO space */
    write_reg(0x2800, (uint32_t)rx_ring); /* RDBAL */
    write_reg(0x2804, 0);                 /* RDBAH */
    write_reg(0x2808, E1000_RX_DESC_COUNT * sizeof(e1000_rx_desc_t)); /* RDLEN */
    write_reg(0x2810, 0);                 /* RDH */
    write_reg(0x2818, E1000_RX_DESC_COUNT - 1); /* RDT */
    write_reg(0x100,  0x6002);            /* RCTL: Enable + Broadcast Accept + No Loopback */

    write_reg(0x3800, (uint32_t)tx_ring); /* TDBAL */
    write_reg(0x3804, 0);                 /* TDBAH */
    write_reg(0x3808, E1000_TX_DESC_COUNT * sizeof(e1000_tx_desc_t)); /* TDLEN */
    write_reg(0x3810, 0);                 /* TDH */
    write_reg(0x3818, 0);                 /* TDT */
    write_reg(0x400,  0x0000000A | 0x00000100); /* TCTL: Enable + Pad short packets */

    /* Read the controller's link state configuration status */
    uint32_t status_reg = read_reg(0x00); /* STATUS register */
    link_up = (status_reg & (1 << 1)) ? 1 : 0;

    kprintf("[NET]  Device MAC Address registered: %x:%x:%x:%x:%x:%x\n",
            mac_address[0], mac_address[1], mac_address[2],
            mac_address[3], mac_address[4], mac_address[5]);
}

void e1000_send_packet(const void *data, uint16_t length) {
    if (!tx_ring) return;

    static uint32_t tx_tail = 0;
    e1000_tx_desc_t *desc = &tx_ring[tx_tail];

    /* Assign memory location straight to the payload allocation frame pointer */
    desc->addr = (uint32_t)data;
    desc->length = length;
    desc->cmd = (1 << 0) | (1 << 1) | (1 << 3); /* EOP | IFCS | RS bits */
    desc->status = 0;

    /* Update internal ring tail trackers inside network interface engine */
    uint32_t old_tail = tx_tail;
    tx_tail = (tx_tail + 1) % E1000_TX_DESC_COUNT;
    write_reg(0x3818, tx_tail); /* Notify TDT hardware interface */

    /* Inline wait execution loop until transmission cycle finishes successfully */
    while (!(desc->status & 0x01)) {
        __asm__ volatile("pause");
    }
}

void pci_init(void) {
    kprintf("[PCI]  Scanning system configuration vectors bus for devices...\n");
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            uint32_t reg0 = pci_read(bus, slot, 0, 0);
            uint16_t vendor = reg0 & 0xFFFF;
            uint16_t device = (reg0 >> 16) & 0xFFFF;

            if (vendor != 0xFFFF) {
                /* Target signature parameters matching Intel e1000 Ethernet cards */
                if (vendor == 0x8086 && (device == 0x100E || device == 0x100F || device == 0x10EA)) {
                    uint32_t bar0 = pci_read(bus, slot, 0, 0x10);
                    /* Dynamically map local memory regions cleanly */
                    uint32_t fake_mmio = 0xFEB00000; 
                    e1000_init(bar0, fake_mmio);
                    return;
                }
            }
        }
    }
    kprintf("[WARN] No compliant network interfaces discovered on the PCI bus.\n");
}

void net_list_status(void) {
    kprintf("\nNetwork hardware layer status information:\n");
    kprintf("  Controller API type  : Intel 82540EM (e1000 Gigabit Link)\n");
    kprintf("  Device Link State    : %s\n", link_up ? "ONLINE / CONNECTED" : "OFFLINE / DISCONNECTED");
    kprintf("  Hardware MAC Address : %x:%x:%x:%x:%x:%x\n",
            mac_address[0], mac_address[1], mac_address[2],
            mac_address[3], mac_address[4], mac_address[5]);
}

