/* =============================================================================
 * Quark-OS drivers/net.c
 * Intel 82540EM (e1000) PCI network interface initialization.
 * ============================================================================= */

#include <quark/kernel.h>

#define E1000_REG_CTRL       0x0000
#define E1000_REG_STATUS     0x0008
#define E1000_REG_IMC        0x00D8
#define E1000_REG_RCTL       0x0100
#define E1000_REG_TCTL       0x0400
#define E1000_REG_TIPG       0x0410
#define E1000_REG_RDT        0x2818
#define E1000_REG_RAL        0x5400
#define E1000_REG_RAH        0x5404
#define E1000_REG_RDBAL      0x2800
#define E1000_REG_RDBAH      0x2804
#define E1000_REG_RDLEN      0x2808
#define E1000_REG_RDH        0x2810
#define E1000_REG_TDBAL      0x3800
#define E1000_REG_TDBAH      0x3804
#define E1000_REG_TDLEN      0x3808
#define E1000_REG_TDH        0x3810
#define E1000_REG_TDT        0x3818

#define PCI_COMMAND          0x04
#define PCI_BAR0             0x10
#define PCI_COMMAND_IO        0x0001
#define PCI_COMMAND_MEMORY   0x0002
#define PCI_COMMAND_BUSMASTER 0x0004
#define VIA_RHINE_VT86C100A   0x3043
#define VIA_RHINE_VT6102      0x3065
#define VIA_RHINE_VT6105M     0x3053
#define VIA_RHINE_VT6105      0x3106

#define ETH_TYPE_IPV4        0x0800
#define ETH_TYPE_ARP         0x0806
#define ARP_HTYPE_ETHERNET    1
#define ARP_PTYPE_IPV4        0x0800
#define ARP_OP_REQUEST        1
#define ARP_OP_REPLY          2
#define IP_PROTOCOL_ICMP      1
#define IP_PROTOCOL_UDP       17
#define ICMP_ECHO_REPLY       0
#define ICMP_ECHO_REQUEST     8
#define NET_RX_BUDGET         32
#define DHCP_CLIENT_PORT      68
#define DHCP_SERVER_PORT      67
#define DHCP_FIXED_SIZE       240
#define DHCP_RETRY_TICKS      200
#define DHCP_MAX_RETRIES      5
#define DHCP_XID              0x514F5345u
#define NET_ARP_CACHE_SIZE    8
#define NET_ARP_RETRY_TICKS    100
#define NET_ARP_MAX_RETRIES   5
#define NET_DNS_LOCAL_PORT    53000
#define NET_DNS_TIMEOUT_TICKS 500
#define NET_DNS_MAX_PACKET    512
#define RHINE_RX_DESC_COUNT   32
#define RHINE_TX_DESC_COUNT   8
#define RHINE_REG_MAC         0x00
#define RHINE_REG_RX_CONFIG   0x06
#define RHINE_REG_TX_CONFIG   0x07
#define RHINE_REG_COMMAND     0x08
#define RHINE_REG_COMMAND1    0x09
#define RHINE_REG_INTERRUPT   0x0C
#define RHINE_REG_INT_ENABLE  0x0E
#define RHINE_REG_RX_RING     0x18
#define RHINE_REG_TX_RING     0x1C
#define RHINE_REG_RX_MISSED   0x7C
#define RHINE_CMD_START       0x02
#define RHINE_CMD_STOP        0x04
#define RHINE_CMD_RX_ON       0x08
#define RHINE_CMD_TX_ON       0x10
#define RHINE_CMD_TX_DEMAND   0x20
#define RHINE_CMD_RX_DEMAND   0x40
#define RHINE_CMD_RESET       0x80
#define RHINE_DESC_OWN        0x80000000u
#define RHINE_RX_OK           0x00008000u
#define RHINE_RX_WHOLE_PACKET 0x00000300u
#define RHINE_RX_ERROR_MASK   0x0000008Fu
#define RHINE_TX_DESC_FLAGS   0x00E08000u
#define RHINE_RX_CONFIG       0x0000000Cu

static uint32_t mmio_base;
static uint16_t rhine_io_base;
static uint8_t mac_address[6];
static uint8_t link_up;
static uint8_t device_ready;
static uint8_t active_driver;
static uint32_t tx_tail;
static uint32_t rx_tail;
static uint32_t rhine_tx_tail;
static uint32_t rhine_rx_tail;
static uint32_t rx_frames;
static uint32_t arp_replies;
static uint32_t icmp_replies;
static uint32_t dhcp_last_tx_tick;
static uint8_t dhcp_state;
static uint8_t dhcp_retries;
static uint8_t dhcp_configured;
static uint8_t net_ipv4_address[4];
static uint8_t net_subnet_mask[4];
static uint8_t net_gateway[4];
static uint8_t net_dns_server[4];
static uint8_t dhcp_offered_address[4];
static uint8_t dhcp_server_address[4];
static uint8_t arp_cache_ip[NET_ARP_CACHE_SIZE][4];
static uint8_t arp_cache_mac[NET_ARP_CACHE_SIZE][6];
static uint8_t arp_cache_used[NET_ARP_CACHE_SIZE];
static uint32_t arp_cache_next;
static uint8_t arp_waiting;
static uint8_t arp_wait_ip[4];
static uint8_t arp_wait_mac[6];
static uint8_t arp_wait_complete;
static uint16_t dns_next_id = 0x5140;
static uint16_t dns_wait_id;
static uint8_t dns_waiting;
static uint8_t dns_response_ready;
static uint16_t dns_response_length;
static uint8_t dns_response[NET_DNS_MAX_PACKET];

typedef struct {
    uint32_t status;
    uint32_t length;
    uint32_t address;
    uint32_t next;
} __attribute__((packed)) rhine_desc_t;

enum {
    NET_DRIVER_NONE,
    NET_DRIVER_E1000,
    NET_DRIVER_VIA_RHINE
};

static e1000_rx_desc_t rx_ring[E1000_RX_DESC_COUNT]
    __attribute__((aligned(16)));
static e1000_tx_desc_t tx_ring[E1000_TX_DESC_COUNT]
    __attribute__((aligned(16)));
static uint8_t rx_buffers[E1000_RX_DESC_COUNT][E1000_PACKET_SIZE]
    __attribute__((aligned(16)));
static uint8_t tx_packet[E1000_PACKET_SIZE] __attribute__((aligned(16)));
static rhine_desc_t rhine_rx_ring[RHINE_RX_DESC_COUNT]
    __attribute__((aligned(16)));
static rhine_desc_t rhine_tx_ring[RHINE_TX_DESC_COUNT]
    __attribute__((aligned(16)));
static uint8_t rhine_rx_buffers[RHINE_RX_DESC_COUNT][E1000_PACKET_SIZE]
    __attribute__((aligned(16)));
static uint8_t rhine_tx_buffers[RHINE_TX_DESC_COUNT][E1000_PACKET_SIZE]
    __attribute__((aligned(16)));

static inline void write_reg(uint32_t reg, uint32_t value) {
    *(volatile uint32_t *)(uintptr_t)(mmio_base + reg) = value;
}

static inline uint32_t read_reg(uint32_t reg) {
    return *(volatile uint32_t *)(uintptr_t)(mmio_base + reg);
}

static inline uint8_t rhine_read8(uint8_t reg) {
    return inb((uint16_t)(rhine_io_base + reg));
}

static inline uint16_t rhine_read16(uint8_t reg) {
    return inw((uint16_t)(rhine_io_base + reg));
}

static inline uint32_t rhine_read32(uint8_t reg) {
    return inl((uint16_t)(rhine_io_base + reg));
}

static inline void rhine_write8(uint8_t reg, uint8_t value) {
    outb((uint16_t)(rhine_io_base + reg), value);
}

static inline void rhine_write16(uint8_t reg, uint16_t value) {
    outw((uint16_t)(rhine_io_base + reg), value);
}

static inline void rhine_write32(uint8_t reg, uint32_t value) {
    outl((uint16_t)(rhine_io_base + reg), value);
}

static void handle_ethernet_frame(const uint8_t *frame, uint16_t length);
static int via_rhine_init(uint16_t io_base);

static int net_send_frame(const void *data, uint16_t length) {
    if (!device_ready || !data || length == 0 ||
        length > E1000_PACKET_SIZE) {
        return -1;
    }
    if (active_driver == NET_DRIVER_E1000) {
        return e1000_send_packet(data, length);
    }
    if (active_driver != NET_DRIVER_VIA_RHINE) return -1;

    volatile rhine_desc_t *desc = &rhine_tx_ring[rhine_tx_tail];
    if (desc->status & RHINE_DESC_OWN) {
        for (uint32_t timeout = 0; timeout < 1000000u; timeout++) {
            if (!(desc->status & RHINE_DESC_OWN)) break;
            if (timeout == 999999u) return -1;
            __asm__ volatile("pause");
        }
    }

    kmemcpy(rhine_tx_buffers[rhine_tx_tail], data, length);
    desc->length = RHINE_TX_DESC_FLAGS | length;
    desc->status = RHINE_DESC_OWN;
    __asm__ volatile("" ::: "memory");
    rhine_tx_tail = (rhine_tx_tail + 1) % RHINE_TX_DESC_COUNT;
    rhine_write8(RHINE_REG_COMMAND,
                 rhine_read8(RHINE_REG_COMMAND) | RHINE_CMD_TX_DEMAND);

    for (uint32_t timeout = 0; timeout < 1000000u; timeout++) {
        if (!(desc->status & RHINE_DESC_OWN)) return 0;
        __asm__ volatile("pause");
    }
    return -1;
}

static uint16_t read_be16(const uint8_t *p) {
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

static void write_be16(uint8_t *p, uint16_t value) {
    p[0] = (uint8_t)(value >> 8);
    p[1] = (uint8_t)value;
}

static uint16_t checksum(const uint8_t *data, uint32_t length) {
    uint32_t sum = 0;
    while (length > 1) {
        sum += read_be16(data);
        data += 2;
        length -= 2;
    }
    if (length) sum += (uint16_t)((uint16_t)data[0] << 8);
    while (sum >> 16) sum = (sum & 0xFFFFu) + (sum >> 16);
    return (uint16_t)~sum;
}

static uint32_t read_be32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | p[3];
}

static void write_be32(uint8_t *p, uint32_t value) {
    p[0] = (uint8_t)(value >> 24);
    p[1] = (uint8_t)(value >> 16);
    p[2] = (uint8_t)(value >> 8);
    p[3] = (uint8_t)value;
}

static int bytes_equal(const uint8_t *a, const uint8_t *b, uint32_t n) {
    for (uint32_t i = 0; i < n; i++) {
        if (a[i] != b[i]) return 0;
    }
    return 1;
}

static int ipv4_is_zero(const uint8_t address[4]) {
    return !(address[0] | address[1] | address[2] | address[3]);
}

static int net_same_subnet(const uint8_t a[4], const uint8_t b[4]) {
    for (uint32_t i = 0; i < 4; i++) {
        if ((a[i] & net_subnet_mask[i]) != (b[i] & net_subnet_mask[i]))
            return 0;
    }
    return 1;
}

static void arp_cache_add(const uint8_t ip[4], const uint8_t mac[6]) {
    for (uint32_t i = 0; i < NET_ARP_CACHE_SIZE; i++) {
        if (arp_cache_used[i] && bytes_equal(arp_cache_ip[i], ip, 4)) {
            kmemcpy(arp_cache_mac[i], mac, 6);
            return;
        }
    }
    uint32_t slot = arp_cache_next++ % NET_ARP_CACHE_SIZE;
    kmemcpy(arp_cache_ip[slot], ip, 4);
    kmemcpy(arp_cache_mac[slot], mac, 6);
    arp_cache_used[slot] = 1;
}

static int arp_cache_get(const uint8_t ip[4], uint8_t mac[6]) {
    for (uint32_t i = 0; i < NET_ARP_CACHE_SIZE; i++) {
        if (arp_cache_used[i] && bytes_equal(arp_cache_ip[i], ip, 4)) {
            kmemcpy(mac, arp_cache_mac[i], 6);
            return 0;
        }
    }
    return -1;
}

static int arp_send_request(const uint8_t target_ip[4]) {
    uint8_t *frame = tx_packet;
    kmemset(frame, 0, 42);
    kmemset(frame, 0xFF, 6);
    kmemcpy(frame + 6, mac_address, 6);
    write_be16(frame + 12, ETH_TYPE_ARP);
    write_be16(frame + 14, ARP_HTYPE_ETHERNET);
    write_be16(frame + 16, ARP_PTYPE_IPV4);
    frame[18] = 6;
    frame[19] = 4;
    write_be16(frame + 20, ARP_OP_REQUEST);
    kmemcpy(frame + 22, mac_address, 6);
    kmemcpy(frame + 28, net_ipv4_address, 4);
    kmemcpy(frame + 38, target_ip, 4);
    return net_send_frame(frame, 42);
}

static int arp_resolve(const uint8_t ip[4], uint8_t mac[6]);

static int net_udp_send(const uint8_t destination_ip[4],
                        uint16_t source_port, uint16_t destination_port,
                        const uint8_t *payload, uint16_t payload_length) {
    if (!dhcp_configured || !payload || payload_length == 0 ||
        payload_length > E1000_PACKET_SIZE - 42) {
        return -1;
    }

    const uint8_t *next_hop = net_same_subnet(destination_ip, net_ipv4_address)
                                  ? destination_ip : net_gateway;
    uint8_t destination_mac[6];
    if (arp_resolve(next_hop, destination_mac) != 0) return -1;

    uint8_t *frame = tx_packet;
    uint16_t udp_length = (uint16_t)(8 + payload_length);
    uint16_t ip_length = (uint16_t)(20 + udp_length);
    kmemset(frame, 0, 14 + ip_length);
    kmemcpy(frame, destination_mac, 6);
    kmemcpy(frame + 6, mac_address, 6);
    write_be16(frame + 12, ETH_TYPE_IPV4);

    uint8_t *ip = frame + 14;
    ip[0] = 0x45;
    write_be16(ip + 2, ip_length);
    write_be16(ip + 4, timer_get_ticks());
    write_be16(ip + 6, 0x4000);
    ip[8] = 64;
    ip[9] = IP_PROTOCOL_UDP;
    kmemcpy(ip + 12, net_ipv4_address, 4);
    kmemcpy(ip + 16, destination_ip, 4);
    write_be16(ip + 10, checksum(ip, 20));

    uint8_t *udp = ip + 20;
    write_be16(udp, source_port);
    write_be16(udp + 2, destination_port);
    write_be16(udp + 4, udp_length);
    kmemcpy(udp + 8, payload, payload_length);
    return net_send_frame(frame, (uint16_t)(14 + ip_length));
}

static int arp_resolve(const uint8_t ip[4], uint8_t mac[6]) {
    if (arp_cache_get(ip, mac) == 0) return 0;
    if (!dhcp_configured || ipv4_is_zero(ip) || arp_waiting) return -1;

    arp_waiting = 1;
    arp_wait_complete = 0;
    kmemcpy(arp_wait_ip, ip, 4);
    uint32_t last_request = timer_get_ticks() - NET_ARP_RETRY_TICKS;
    uint8_t retries = 0;
    uint32_t deadline = timer_get_ticks() +
                        NET_ARP_RETRY_TICKS * NET_ARP_MAX_RETRIES;

    while (!arp_wait_complete && timer_get_ticks() < deadline) {
        uint32_t now = timer_get_ticks();
        if (now - last_request >= NET_ARP_RETRY_TICKS) {
            if (retries >= NET_ARP_MAX_RETRIES) break;
            if (arp_send_request(ip) != 0) break;
            retries++;
            last_request = now;
        }
        net_poll();
        if (!arp_wait_complete) timer_idle_wait();
    }

    arp_waiting = 0;
    if (!arp_wait_complete) return -1;
    kmemcpy(mac, arp_wait_mac, 6);
    return 0;
}

static uint32_t dhcp_append_option(uint8_t *options, uint32_t offset,
                                   uint8_t code, uint8_t length,
                                   const uint8_t *value) {
    options[offset++] = code;
    options[offset++] = length;
    for (uint8_t i = 0; i < length; i++) options[offset++] = value[i];
    return offset;
}

static int dhcp_send(uint8_t message_type) {
    uint8_t *frame = tx_packet;
    const uint32_t ip_offset = 14;
    const uint32_t udp_offset = 34;
    const uint32_t bootp_offset = 42;
    uint8_t *bootp = frame + bootp_offset;
    uint8_t *options = bootp + DHCP_FIXED_SIZE;
    uint32_t option_length = 0;

    kmemset(frame, 0, sizeof(tx_packet));
    kmemset(frame, 0xFF, 6);
    kmemcpy(frame + 6, mac_address, 6);
    write_be16(frame + 12, ETH_TYPE_IPV4);

    uint8_t *ip = frame + ip_offset;
    ip[0] = 0x45;
    ip[8] = 64;
    ip[9] = IP_PROTOCOL_UDP;
    kmemset(ip + 12, 0, 4);
    kmemset(ip + 16, 0xFF, 4);

    uint8_t *udp = frame + udp_offset;
    write_be16(udp, DHCP_CLIENT_PORT);
    write_be16(udp + 2, DHCP_SERVER_PORT);

    bootp[0] = 1;
    bootp[1] = 1;
    bootp[2] = 6;
    write_be32(bootp + 4, DHCP_XID);
    write_be16(bootp + 10, 0x8000);
    kmemcpy(bootp + 28, mac_address, 6);
    write_be32(bootp + 236, 0x63825363u);

    options[option_length++] = 53;
    options[option_length++] = 1;
    options[option_length++] = message_type;
    if (message_type == 3) {
        option_length = dhcp_append_option(options, option_length, 50, 4,
                                            dhcp_offered_address);
        option_length = dhcp_append_option(options, option_length, 54, 4,
                                            dhcp_server_address);
    } else {
        static const uint8_t requested_options[] = {1, 3, 6, 51, 54};
        option_length = dhcp_append_option(
            options, option_length, 55, sizeof(requested_options),
            requested_options);
    }
    options[option_length++] = 255;

    uint32_t dhcp_length = DHCP_FIXED_SIZE + option_length;
    if (dhcp_length < 300) dhcp_length = 300;
    uint16_t udp_length = (uint16_t)(8 + dhcp_length);
    uint16_t ip_length = (uint16_t)(20 + udp_length);
    write_be16(udp + 4, udp_length);
    write_be16(ip + 2, ip_length);
    write_be16(ip + 6, 0x4000);
    write_be16(ip + 10, checksum(ip, 20));

    dhcp_last_tx_tick = timer_get_ticks();
    return net_send_frame(frame, (uint16_t)(14 + ip_length));
}

static uint32_t pci_read(uint8_t bus, uint8_t slot, uint8_t func,
                         uint8_t offset) {
    uint32_t address = 0x80000000u |
                       ((uint32_t)bus << 16) |
                       ((uint32_t)slot << 11) |
                       ((uint32_t)func << 8) |
                       (offset & 0xFCu);
    outl(PCI_CONFIG_ADDRESS, address);
    return inl(PCI_CONFIG_DATA);
}

static void pci_write(uint8_t bus, uint8_t slot, uint8_t func,
                      uint8_t offset, uint32_t value) {
    uint32_t address = 0x80000000u |
                       ((uint32_t)bus << 16) |
                       ((uint32_t)slot << 11) |
                       ((uint32_t)func << 8) |
                       (offset & 0xFCu);
    outl(PCI_CONFIG_ADDRESS, address);
    outl(PCI_CONFIG_DATA, value);
}

static int e1000_read_mac(void) {
    uint32_t low = read_reg(E1000_REG_RAL);
    uint32_t high = read_reg(E1000_REG_RAH);

    if (!(high & 0x80000000u)) return -1;
    mac_address[0] = (uint8_t)low;
    mac_address[1] = (uint8_t)(low >> 8);
    mac_address[2] = (uint8_t)(low >> 16);
    mac_address[3] = (uint8_t)(low >> 24);
    mac_address[4] = (uint8_t)high;
    mac_address[5] = (uint8_t)(high >> 8);
    return 0;
}

void e1000_init(uint32_t bar0) {
    uint32_t bar_type = bar0 & 0x7u;
    uint32_t bar_address = bar0 & ~0xFu;

    device_ready = 0;
    active_driver = NET_DRIVER_NONE;
    link_up = 0;
    mmio_base = 0;
    if ((bar0 & 1u) || bar_type == 0x4u || bar_address == 0) {
        kprintf("[NET] Unsupported or invalid e1000 MMIO BAR: 0x%x\n", bar0);
        return;
    }

    mmio_base = bar_address;
    rx_tail = 0;
    kprintf("[NET] Initializing e1000 MMIO at 0x%x\n", mmio_base);

    write_reg(E1000_REG_IMC, 0xFFFFFFFFu);
    write_reg(E1000_REG_CTRL, read_reg(E1000_REG_CTRL) | 0x04000000u);
    for (uint32_t timeout = 0; timeout < 1000000u; timeout++) {
        if (!(read_reg(E1000_REG_CTRL) & 0x04000000u)) break;
        if (timeout == 999999u) {
            kprintf("[NET] e1000 reset timed out\n");
            mmio_base = 0;
            return;
        }
    }

    write_reg(E1000_REG_IMC, 0xFFFFFFFFu);
    if (e1000_read_mac() != 0) {
        kprintf("[NET] e1000 did not report a valid MAC address\n");
        mmio_base = 0;
        return;
    }

    kmemset(rx_ring, 0, sizeof(rx_ring));
    kmemset(tx_ring, 0, sizeof(tx_ring));
    for (uint32_t i = 0; i < E1000_RX_DESC_COUNT; i++) {
        rx_ring[i].addr = (uint32_t)(uintptr_t)rx_buffers[i];
    }
    for (uint32_t i = 0; i < E1000_TX_DESC_COUNT; i++) {
        tx_ring[i].status = 1;
    }

    write_reg(E1000_REG_RCTL, 0);
    write_reg(E1000_REG_TCTL, 0);

    write_reg(E1000_REG_RDBAL, (uint32_t)(uintptr_t)rx_ring);
    write_reg(E1000_REG_RDBAH, 0);
    write_reg(E1000_REG_RDLEN, sizeof(rx_ring));
    write_reg(E1000_REG_RDH, 0);
    write_reg(E1000_REG_RDT, E1000_RX_DESC_COUNT - 1);
    write_reg(E1000_REG_RCTL, 0x04008002u);

    write_reg(E1000_REG_TDBAL, (uint32_t)(uintptr_t)tx_ring);
    write_reg(E1000_REG_TDBAH, 0);
    write_reg(E1000_REG_TDLEN, sizeof(tx_ring));
    write_reg(E1000_REG_TDH, 0);
    write_reg(E1000_REG_TDT, 0);
    write_reg(E1000_REG_TIPG, 0x0060200Au);
    write_reg(E1000_REG_TCTL, 0x000401FAu);

    tx_tail = 0;
    link_up = (read_reg(E1000_REG_STATUS) & (1u << 1)) ? 1 : 0;
    active_driver = NET_DRIVER_E1000;
    device_ready = 1;
    kprintf("[NET] e1000 ready; MAC %x:%x:%x:%x:%x:%x; link %s\n",
            mac_address[0], mac_address[1], mac_address[2],
            mac_address[3], mac_address[4], mac_address[5],
            link_up ? "up" : "down");
}

static int via_rhine_init(uint16_t io_base) {
    rhine_io_base = io_base;
    device_ready = 0;
    active_driver = NET_DRIVER_NONE;
    link_up = 0;

    rhine_write8(RHINE_REG_INT_ENABLE, 0);
    rhine_write8(RHINE_REG_COMMAND1, RHINE_CMD_RESET);
    for (uint32_t timeout = 0; timeout < 1000000u; timeout++) {
        if (!(rhine_read8(RHINE_REG_COMMAND1) & RHINE_CMD_RESET)) break;
        if (timeout == 999999u) {
            kprintf("[NET] VIA Rhine reset timed out\n");
            rhine_io_base = 0;
            return -1;
        }
    }

    for (uint32_t i = 0; i < 6; i++)
        mac_address[i] = inb((uint16_t)(rhine_io_base + RHINE_REG_MAC + i));
    if ((mac_address[0] & 1u) ||
        bytes_equal(mac_address, (const uint8_t[6]){0, 0, 0, 0, 0, 0}, 6) ||
        bytes_equal(mac_address, (const uint8_t[6]){0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, 6)) {
        kprintf("[NET] VIA Rhine reports an invalid MAC address\n");
        rhine_io_base = 0;
        return -1;
    }

    kmemset(rhine_rx_ring, 0, sizeof(rhine_rx_ring));
    kmemset(rhine_tx_ring, 0, sizeof(rhine_tx_ring));
    kmemset(rhine_rx_buffers, 0, sizeof(rhine_rx_buffers));
    kmemset(rhine_tx_buffers, 0, sizeof(rhine_tx_buffers));
    for (uint32_t i = 0; i < RHINE_RX_DESC_COUNT; i++) {
        rhine_rx_ring[i].length = E1000_PACKET_SIZE;
        rhine_rx_ring[i].address =
            (uint32_t)(uintptr_t)rhine_rx_buffers[i];
        rhine_rx_ring[i].next =
            (uint32_t)(uintptr_t)&rhine_rx_ring[(i + 1) %
                                                RHINE_RX_DESC_COUNT];
        rhine_rx_ring[i].status = RHINE_DESC_OWN;
    }
    for (uint32_t i = 0; i < RHINE_TX_DESC_COUNT; i++) {
        rhine_tx_ring[i].length = RHINE_TX_DESC_FLAGS;
        rhine_tx_ring[i].address =
            (uint32_t)(uintptr_t)rhine_tx_buffers[i];
        rhine_tx_ring[i].next =
            (uint32_t)(uintptr_t)&rhine_tx_ring[(i + 1) %
                                                RHINE_TX_DESC_COUNT];
    }

    rhine_write8(RHINE_REG_COMMAND, RHINE_CMD_STOP);
    rhine_write32(RHINE_REG_RX_RING, (uint32_t)(uintptr_t)rhine_rx_ring);
    rhine_write32(RHINE_REG_TX_RING, (uint32_t)(uintptr_t)rhine_tx_ring);
    rhine_write8(RHINE_REG_RX_CONFIG, RHINE_RX_CONFIG);
    rhine_write8(RHINE_REG_TX_CONFIG, 0x60);
    rhine_write16(RHINE_REG_RX_MISSED, 0xFFFF);
    rhine_write16(RHINE_REG_INTERRUPT, 0xFFFF);
    rhine_rx_tail = 0;
    rhine_tx_tail = 0;

    active_driver = NET_DRIVER_VIA_RHINE;
    device_ready = 1;
    rhine_write8(RHINE_REG_COMMAND,
                 RHINE_CMD_START | RHINE_CMD_RX_ON | RHINE_CMD_TX_ON);
    link_up = 0;
    kprintf("[NET] VIA Rhine Fast Ethernet initialized; MAC %x:%x:%x:%x:%x:%x\n",
            mac_address[0], mac_address[1], mac_address[2],
            mac_address[3], mac_address[4], mac_address[5]);
    return 0;
}

int e1000_send_packet(const void *data, uint16_t length) {
    if (!device_ready || !data || length == 0 || length > E1000_PACKET_SIZE)
        return -1;

    volatile e1000_tx_desc_t *desc = &tx_ring[tx_tail];
    if (!(desc->status & 1u)) {
        for (uint32_t timeout = 0; timeout < 1000000u; timeout++) {
            if (desc->status & 1u) break;
            if (timeout == 999999u) return -1;
            __asm__ volatile("pause");
        }
    }

    desc->addr = (uint32_t)(uintptr_t)data;
    desc->length = length;
    desc->cmd = (1u << 0) | (1u << 1) | (1u << 3);
    desc->status = 0;
    tx_tail = (tx_tail + 1) % E1000_TX_DESC_COUNT;
    write_reg(E1000_REG_TDT, tx_tail);

    for (uint32_t timeout = 0; timeout < 1000000u; timeout++) {
        if (desc->status & 1u) return 0;
        if (timeout == 999999u) return -1;
        __asm__ volatile("pause");
    }
    return -1;
}

static void send_arp_reply(const uint8_t *request) {
    uint8_t *reply = tx_packet;
    const uint8_t *sender_mac = request + 22;
    const uint8_t *sender_ip = request + 28;

    kmemset(reply, 0, 42);
    kmemcpy(reply, sender_mac, 6);
    kmemcpy(reply + 6, mac_address, 6);
    write_be16(reply + 12, ETH_TYPE_ARP);
    write_be16(reply + 14, ARP_HTYPE_ETHERNET);
    write_be16(reply + 16, ARP_PTYPE_IPV4);
    reply[18] = 6;
    reply[19] = 4;
    write_be16(reply + 20, ARP_OP_REPLY);
    kmemcpy(reply + 22, mac_address, 6);
    kmemcpy(reply + 28, net_ipv4_address, 4);
    kmemcpy(reply + 32, sender_mac, 6);
    kmemcpy(reply + 38, sender_ip, 4);
    if (net_send_frame(reply, 42) == 0) arp_replies++;
}

static void handle_arp(const uint8_t *frame, uint16_t length) {
    const uint8_t *arp = frame + 14;
    if (length < 42 ||
        read_be16(arp) != ARP_HTYPE_ETHERNET ||
        read_be16(arp + 2) != ARP_PTYPE_IPV4 ||
        arp[4] != 6 || arp[5] != 4) {
        return;
    }

    uint16_t operation = read_be16(arp + 6);
    if (operation == ARP_OP_REPLY) {
        if (!dhcp_configured ||
            !bytes_equal(arp + 24, net_ipv4_address, 4) ||
            !bytes_equal(arp + 18, mac_address, 6)) {
            return;
        }
        arp_cache_add(arp + 14, arp + 8);
        if (arp_waiting && bytes_equal(arp + 14, arp_wait_ip, 4)) {
            kmemcpy(arp_wait_mac, arp + 8, 6);
            arp_wait_complete = 1;
        }
    } else if (operation == ARP_OP_REQUEST && dhcp_configured &&
               bytes_equal(arp + 24, net_ipv4_address, 4)) {
        arp_cache_add(arp + 14, arp + 8);
        send_arp_reply(frame);
    }
}

static void handle_dns_response(const uint8_t *udp, uint16_t udp_length,
                                const uint8_t *source_ip,
                                const uint8_t *destination_ip) {
    if (!dns_waiting || udp_length < 8 + 12 ||
        read_be16(udp) != 53 ||
        read_be16(udp + 2) != NET_DNS_LOCAL_PORT ||
        read_be16(udp + 4) < 8 + 12 ||
        !bytes_equal(source_ip, net_dns_server, 4) ||
        !bytes_equal(destination_ip, net_ipv4_address, 4)) {
        return;
    }

    const uint8_t *dns = udp + 8;
    uint16_t dns_length = read_be16(udp + 4) - 8;
    uint16_t flags = read_be16(dns + 2);
    if (read_be16(dns) != dns_wait_id || !(flags & 0x8000u) ||
        (flags & 0x000Fu) != 0 || dns_length > sizeof(dns_response)) {
        return;
    }

    kmemcpy(dns_response, dns, dns_length);
    dns_response_length = dns_length;
    dns_response_ready = 1;
}

static void handle_dhcp(const uint8_t *udp, uint16_t udp_length,
                        const uint8_t *source_ip) {
    if (udp_length < 8 + DHCP_FIXED_SIZE ||
        read_be16(udp) != DHCP_SERVER_PORT ||
        read_be16(udp + 2) != DHCP_CLIENT_PORT ||
        read_be16(udp + 4) < 8 + DHCP_FIXED_SIZE) {
        return;
    }

    const uint8_t *bootp = udp + 8;
    uint32_t payload_length = read_be16(udp + 4) - 8;
    if (bootp[0] != 2 || bootp[1] != 1 || bootp[2] != 6 ||
        read_be32(bootp + 4) != DHCP_XID ||
        !bytes_equal(bootp + 28, mac_address, 6) ||
        read_be32(bootp + 236) != 0x63825363u) {
        return;
    }

    uint8_t message_type = 0;
    uint8_t subnet[4] = {0};
    uint8_t gateway[4] = {0};
    uint8_t dns[4] = {0};
    uint8_t server[4] = {0};
    uint32_t offset = DHCP_FIXED_SIZE;
    while (offset < payload_length) {
        uint8_t code = bootp[offset++];
        if (code == 255) break;
        if (code == 0) continue;
        if (offset >= payload_length) return;
        uint8_t option_length = bootp[offset++];
        if (option_length > payload_length - offset) return;
        const uint8_t *value = bootp + offset;
        if (code == 53 && option_length == 1) {
            message_type = value[0];
        } else if (code == 1 && option_length >= 4) {
            kmemcpy(subnet, value, 4);
        } else if (code == 3 && option_length >= 4) {
            kmemcpy(gateway, value, 4);
        } else if (code == 6 && option_length >= 4) {
            kmemcpy(dns, value, 4);
        } else if (code == 54 && option_length == 4) {
            kmemcpy(server, value, 4);
        }
        offset += option_length;
    }

    if (message_type == 2 && dhcp_state == 1) {
        kmemcpy(dhcp_offered_address, bootp + 16, 4);
        if (ipv4_is_zero(dhcp_offered_address)) return;
        if (ipv4_is_zero(server)) kmemcpy(server, source_ip, 4);
        kmemcpy(dhcp_server_address, server, 4);
        dhcp_state = 2;
        dhcp_retries = 0;
        if (dhcp_send(3) != 0) {
            dhcp_state = 4;
            kprintf("[NET] DHCP request transmission failed\n");
        }
    } else if (message_type == 5 && dhcp_state == 2) {
        if (ipv4_is_zero(bootp + 16)) return;
        kmemcpy(net_ipv4_address, bootp + 16, 4);
        kmemcpy(net_subnet_mask, subnet, 4);
        kmemcpy(net_gateway, gateway, 4);
        kmemcpy(net_dns_server, dns, 4);
        dhcp_state = 3;
        dhcp_configured = 1;
        dhcp_retries = 0;
        kprintf("[NET] DHCP configured: %u.%u.%u.%u\n",
                net_ipv4_address[0], net_ipv4_address[1],
                net_ipv4_address[2], net_ipv4_address[3]);
    } else if (message_type == 6 && dhcp_state == 2) {
        dhcp_state = 4;
        kprintf("[NET] DHCP server rejected the address request\n");
    }
}

static void handle_ipv4(const uint8_t *frame, uint16_t length) {
    const uint8_t *ip = frame + 14;
    if (length < 34 || (ip[0] >> 4) != 4) return;

    uint32_t ip_header_length = (uint32_t)(ip[0] & 0x0Fu) * 4u;
    if (ip_header_length < 20 || length < 14 + ip_header_length) return;

    uint16_t ip_total_length = read_be16(ip + 2);
    if (ip_total_length < ip_header_length ||
        ip_total_length > length - 14 ||
        checksum(ip, ip_header_length) != 0 ||
        (read_be16(ip + 6) & 0x3FFFu) != 0) {
        return;
    }

    if (ip[9] == IP_PROTOCOL_UDP) {
        if (ip_total_length < ip_header_length + 8) return;
        const uint8_t *udp = ip + ip_header_length;
        uint16_t udp_length = read_be16(udp + 4);
        if (udp_length < 8 ||
            udp_length > ip_total_length - ip_header_length) {
            return;
        }
        if (read_be16(udp + 2) == DHCP_CLIENT_PORT) {
            handle_dhcp(udp, udp_length, ip + 12);
        } else if (read_be16(udp + 2) == NET_DNS_LOCAL_PORT) {
            handle_dns_response(udp, udp_length, ip + 12, ip + 16);
        }
        return;
    }

    if (ip[9] != IP_PROTOCOL_ICMP || ip[8] <= 1 ||
        !dhcp_configured ||
        !bytes_equal(ip + 16, net_ipv4_address, 4)) {
        return;
    }

    uint8_t *icmp = (uint8_t *)(uintptr_t)(ip + ip_header_length);
    uint32_t icmp_length = ip_total_length - ip_header_length;
    if (icmp_length < 8 || icmp[0] != ICMP_ECHO_REQUEST ||
        icmp[1] != 0 || checksum(icmp, icmp_length) != 0) {
        return;
    }

    uint8_t *reply = tx_packet;
    uint32_t frame_length = 14 + ip_total_length;
    if (frame_length > sizeof(tx_packet)) return;
    kmemcpy(reply, frame, frame_length);

    kmemcpy(reply, frame + 6, 6);
    kmemcpy(reply + 6, mac_address, 6);

    uint8_t *reply_ip = reply + 14;
    kmemcpy(reply_ip + 12, ip + 16, 4);
    kmemcpy(reply_ip + 16, ip + 12, 4);
    reply_ip[8]--;
    reply_ip[10] = 0;
    reply_ip[11] = 0;
    write_be16(reply_ip + 10, checksum(reply_ip, ip_header_length));

    uint8_t *reply_icmp = reply_ip + ip_header_length;
    reply_icmp[0] = ICMP_ECHO_REPLY;
    reply_icmp[2] = 0;
    reply_icmp[3] = 0;
    write_be16(reply_icmp + 2, checksum(reply_icmp, icmp_length));
    if (net_send_frame(reply, (uint16_t)frame_length) == 0)
        icmp_replies++;
}

static void handle_ethernet_frame(const uint8_t *frame, uint16_t length) {
    if (length < 14) return;
    uint16_t ether_type = read_be16(frame + 12);
    if (ether_type == ETH_TYPE_ARP) {
        handle_arp(frame, length);
    } else if (ether_type == ETH_TYPE_IPV4) {
        handle_ipv4(frame, length);
    }
}

static void via_rhine_poll(void) {
    for (uint32_t handled = 0; handled < NET_RX_BUDGET; handled++) {
        volatile rhine_desc_t *desc = &rhine_rx_ring[rhine_rx_tail];
        uint32_t status = desc->status;
        if (status & RHINE_DESC_OWN) break;

        uint32_t length = desc->length & 0x7FFu;
        if ((status & RHINE_RX_OK) &&
            (status & RHINE_RX_WHOLE_PACKET) == RHINE_RX_WHOLE_PACKET &&
            !(status & RHINE_RX_ERROR_MASK) &&
            length >= 4 && length <= E1000_PACKET_SIZE) {
            rx_frames++;
            handle_ethernet_frame(rhine_rx_buffers[rhine_rx_tail],
                                  (uint16_t)(length - 4));
        }

        desc->length = E1000_PACKET_SIZE;
        __asm__ volatile("" ::: "memory");
        desc->status = RHINE_DESC_OWN;
        rhine_rx_tail = (rhine_rx_tail + 1) % RHINE_RX_DESC_COUNT;
        rhine_write8(RHINE_REG_COMMAND,
                     rhine_read8(RHINE_REG_COMMAND) | RHINE_CMD_RX_DEMAND);
    }
}

void net_poll(void) {
    if (!device_ready) return;

    if ((dhcp_state == 1 || dhcp_state == 2) &&
        timer_get_ticks() - dhcp_last_tx_tick >= DHCP_RETRY_TICKS) {
        if (dhcp_retries >= DHCP_MAX_RETRIES) {
            dhcp_state = 4;
            kprintf("[NET] DHCP timed out; no IPv4 address assigned\n");
        } else {
            uint8_t message_type = (dhcp_state == 2) ? 3 : 1;
            dhcp_retries++;
            if (dhcp_send(message_type) != 0) {
                dhcp_state = 4;
                kprintf("[NET] DHCP retransmission failed\n");
            }
        }
    }

    if (active_driver == NET_DRIVER_E1000) {
        for (uint32_t handled = 0; handled < NET_RX_BUDGET; handled++) {
            volatile e1000_rx_desc_t *desc = &rx_ring[rx_tail];
            uint8_t status = desc->status;
            if (!(status & 1u)) break;

            uint16_t length = desc->length;
            if ((status & 2u) && desc->errors == 0 &&
                length <= E1000_PACKET_SIZE) {
                rx_frames++;
                handle_ethernet_frame(rx_buffers[rx_tail], length);
            }

            desc->status = 0;
            desc->length = 0;
            desc->errors = 0;
            rx_tail = (rx_tail + 1) % E1000_RX_DESC_COUNT;
            write_reg(E1000_REG_RDT,
                      (rx_tail + E1000_RX_DESC_COUNT - 1) %
                      E1000_RX_DESC_COUNT);
        }
    } else if (active_driver == NET_DRIVER_VIA_RHINE) {
        via_rhine_poll();
    }
}

void net_dhcp_start(void) {
    if (!device_ready || dhcp_state != 0) return;
    dhcp_state = 1;
    dhcp_retries = 0;
    if (dhcp_send(1) != 0) {
        dhcp_state = 4;
        kprintf("[NET] DHCP discovery transmission failed\n");
    } else {
        kprintf("[NET] Requesting IPv4 configuration over Ethernet...\n");
    }
}

int net_get_ipv4_config(uint8_t address[4], uint8_t subnet_mask[4],
                        uint8_t gateway[4], uint8_t dns_server[4]) {
    if (!dhcp_configured) return -1;
    if (address) kmemcpy(address, net_ipv4_address, 4);
    if (subnet_mask) kmemcpy(subnet_mask, net_subnet_mask, 4);
    if (gateway) kmemcpy(gateway, net_gateway, 4);
    if (dns_server) kmemcpy(dns_server, net_dns_server, 4);
    return 0;
}

static int dns_skip_name(const uint8_t *packet, uint16_t packet_length,
                         uint32_t *offset) {
    uint32_t position = *offset;
    uint32_t resume = 0;
    uint32_t jumps = 0;

    for (;;) {
        if (position >= packet_length) return -1;
        uint8_t length = packet[position];
        if ((length & 0xC0u) == 0xC0u) {
            if (position + 1 >= packet_length || ++jumps > 16) return -1;
            uint32_t target = ((uint32_t)(length & 0x3Fu) << 8) |
                              packet[position + 1];
            if (target >= packet_length) return -1;
            if (resume == 0) resume = position + 2;
            position = target;
        } else if (length & 0xC0u) {
            return -1;
        } else if (length == 0) {
            *offset = resume ? resume : position + 1;
            return 0;
        } else {
            if (length > 63 || position + 1 + length > packet_length)
                return -1;
            position += 1 + length;
            if (resume == 0) *offset = position;
        }
    }
}

static int dns_find_ipv4(const uint8_t *packet, uint16_t packet_length,
                         uint8_t address[4]) {
    if (packet_length < 12) return -1;
    uint16_t questions = read_be16(packet + 4);
    uint16_t answers = read_be16(packet + 6);
    uint32_t offset = 12;

    for (uint16_t i = 0; i < questions; i++) {
        if (dns_skip_name(packet, packet_length, &offset) != 0 ||
            offset + 4 > packet_length) {
            return -1;
        }
        offset += 4;
    }

    for (uint16_t i = 0; i < answers; i++) {
        if (dns_skip_name(packet, packet_length, &offset) != 0 ||
            offset + 10 > packet_length) {
            return -1;
        }
        uint16_t type = read_be16(packet + offset);
        uint16_t klass = read_be16(packet + offset + 2);
        uint16_t data_length = read_be16(packet + offset + 8);
        offset += 10;
        if (offset + data_length > packet_length) return -1;
        if (type == 1 && klass == 1 && data_length == 4) {
            kmemcpy(address, packet + offset, 4);
            return 0;
        }
        offset += data_length;
    }
    return -1;
}

int net_dns_lookup(const char *hostname, uint8_t address[4]) {
    if (!device_ready || !dhcp_configured || !hostname || !address ||
        !*hostname || ipv4_is_zero(net_dns_server)) {
        return -1;
    }

    uint8_t query[NET_DNS_MAX_PACKET];
    kmemset(query, 0, sizeof(query));
    uint16_t id = ++dns_next_id;
    write_be16(query, id);
    write_be16(query + 2, 0x0100);
    write_be16(query + 4, 1);

    uint32_t length = 12;
    uint32_t hostname_length = 0;
    const char *label = hostname;
    while (*label) {
        if (length >= sizeof(query) - 8) return -1;
        uint32_t label_offset = length++;
        uint32_t label_length = 0;
        while (*hostname && *hostname != '.') {
            if (label_length >= 63 || hostname_length >= 253 ||
                length >= sizeof(query) - 8) {
                return -1;
            }
            query[length++] = (uint8_t)*hostname++;
            label_length++;
            hostname_length++;
        }
        if (label_length == 0) return -1;
        query[label_offset] = (uint8_t)label_length;
        if (*hostname == '.') {
            hostname++;
            hostname_length++;
            if (!*hostname) break;
        }
        label = hostname;
    }
    if (hostname_length > 253) return -1;
    query[length++] = 0;
    write_be16(query + length, 1);
    write_be16(query + length + 2, 1);
    length += 4;

    dns_wait_id = id;
    dns_response_ready = 0;
    dns_response_length = 0;
    dns_waiting = 1;
    if (net_udp_send(net_dns_server, NET_DNS_LOCAL_PORT, 53,
                     query, (uint16_t)length) != 0) {
        dns_waiting = 0;
        return -1;
    }

    uint32_t deadline = timer_get_ticks() + NET_DNS_TIMEOUT_TICKS;
    while (!dns_response_ready && timer_get_ticks() < deadline) {
        net_poll();
        if (!dns_response_ready) timer_idle_wait();
    }
    dns_waiting = 0;
    if (!dns_response_ready) return -1;
    return dns_find_ipv4(dns_response, dns_response_length, address);
}

static int is_supported_e1000(uint16_t vendor, uint16_t device) {
    return vendor == 0x8086 &&
           (device == 0x100E || device == 0x100F || device == 0x10EA);
}

static int is_supported_via_rhine(uint16_t vendor, uint16_t device) {
    return vendor == 0x1106 &&
           (device == VIA_RHINE_VT86C100A ||
            device == VIA_RHINE_VT6102 ||
            device == VIA_RHINE_VT6105M ||
            device == VIA_RHINE_VT6105);
}

void pci_init(void) {
    kprintf("[PCI] Scanning for Intel e1000 and VIA Rhine Ethernet devices...\n");
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            uint32_t id = pci_read((uint8_t)bus, slot, 0, 0);
            if ((uint16_t)id == 0xFFFFu) continue;

            uint8_t functions =
                (pci_read((uint8_t)bus, slot, 0, 0x0C) & 0x00800000u)
                    ? 8 : 1;
            for (uint8_t func = 0; func < functions; func++) {
                id = pci_read((uint8_t)bus, slot, func, 0);
                uint16_t vendor = (uint16_t)id;
                uint16_t device = (uint16_t)(id >> 16);
                if (vendor == 0xFFFFu) continue;
                int intel = is_supported_e1000(vendor, device);
                int via = is_supported_via_rhine(vendor, device);
                if (!intel && !via) continue;

                uint32_t bar0 = pci_read((uint8_t)bus, slot, func, PCI_BAR0);
                if (via && !(bar0 & 1u)) {
                    kprintf("[NET] VIA Rhine has no supported I/O BAR\n");
                    continue;
                }
                uint32_t command = pci_read((uint8_t)bus, slot, func,
                                            PCI_COMMAND);
                pci_write((uint8_t)bus, slot, func, PCI_COMMAND,
                          (command & 0xFFFFu) |
                          (via ? PCI_COMMAND_IO : PCI_COMMAND_MEMORY) |
                          PCI_COMMAND_BUSMASTER);
                if (intel) {
                    e1000_init(bar0);
                } else {
                    uint16_t io_base = (uint16_t)(bar0 & ~0x3u);
                    if (via_rhine_init(io_base) != 0) continue;
                }
                if (device_ready) return;
            }
        }
    }
    kprintf("[NET] No supported Ethernet device found\n");
}

void net_list_status(void) {
    kprintf("\nNetwork hardware status:\n");
    const char *controller = "not initialized";
    if (active_driver == NET_DRIVER_E1000) controller = "Intel e1000";
    else if (active_driver == NET_DRIVER_VIA_RHINE)
        controller = "VIA Rhine (10/100)";
    kprintf("  Controller : %s\n", controller);
    kprintf("  Link       : %s\n", link_up ? "up" : "down");
    if (device_ready) {
        kprintf("  MAC        : %x:%x:%x:%x:%x:%x\n",
                mac_address[0], mac_address[1], mac_address[2],
                mac_address[3], mac_address[4], mac_address[5]);
        kprintf("  DHCP       : %s\n", dhcp_configured ? "configured" :
                (dhcp_state == 4 ? "failed" : "waiting"));
        kprintf("  IPv4       : %u.%u.%u.%u\n",
                net_ipv4_address[0], net_ipv4_address[1],
                net_ipv4_address[2], net_ipv4_address[3]);
        kprintf("  Netmask    : %u.%u.%u.%u\n",
                net_subnet_mask[0], net_subnet_mask[1],
                net_subnet_mask[2], net_subnet_mask[3]);
        kprintf("  Gateway    : %u.%u.%u.%u\n",
                net_gateway[0], net_gateway[1],
                net_gateway[2], net_gateway[3]);
        kprintf("  DNS        : %u.%u.%u.%u\n",
                net_dns_server[0], net_dns_server[1],
                net_dns_server[2], net_dns_server[3]);
        kprintf("  RX frames  : %u; ARP replies: %u; ICMP replies: %u\n",
                rx_frames, arp_replies, icmp_replies);
    }
}
