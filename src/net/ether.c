/*
 * ether.c - 以太网帧处理
 */
#include "network.h"
#include "rtl8139.h"
#include "../string.h"
#include "../vga.h"

uint8_t nic_mac[6]     = {0, 0, 0, 0, 0, 0};
uint8_t nic_ip[4]      = {10, 0, 2, 15};
uint8_t nic_gateway[4] = {10, 0, 2, 2};
uint8_t nic_mask[4]    = {255, 255, 255, 0};

void ether_send(uint8_t *dst_mac, uint16_t ethertype, uint8_t *data, int len)
{
    uint8_t frame[1536];
    ether_frame_t *ef = (ether_frame_t *)frame;

    memcpy(ef->dst_mac, dst_mac, 6);
    memcpy(ef->src_mac, nic_mac, 6);
    ef->ethertype = htons(ethertype);

    if (len > 1500) len = 1500;
    memcpy(ef->payload, data, len);

    rtl8139_send(frame, sizeof(ether_frame_t) + len);
}

void ether_recv(uint8_t *buffer, int *len)
{
    int total = rtl8139_recv(buffer, 1536);
    if (total <= 0) {
        *len = 0;
        return;
    }

    ether_frame_t *ef = (ether_frame_t *)buffer;
    *len = total - sizeof(ether_frame_t);

    /* 只处理发给我们的或广播的帧 */
    int is_broadcast = 1;
    for (int i = 0; i < 6; i++) {
        if (ef->dst_mac[i] != 0xFF) { is_broadcast = 0; break; }
    }
    int is_ours = 1;
    for (int i = 0; i < 6; i++) {
        if (ef->dst_mac[i] != nic_mac[i]) { is_ours = 0; break; }
    }

    if (!is_broadcast && !is_ours) {
        *len = 0;
        return;
    }

    /* 把 payload 移到 buffer 开头 */
    memmove(buffer, ef->payload, *len);
}

void network_init(void)
{
    rtl8139_get_mac(nic_mac);
}

int network_configure(uint8_t *ip, uint8_t *gateway, uint8_t *mask)
{
    memcpy(nic_ip, ip, 4);
    memcpy(nic_gateway, gateway, 4);
    memcpy(nic_mask, mask, 4);
    return 0;
}

/* ---- 网络工具函数 ---- */
uint16_t htons(uint16_t v) { return (v >> 8) | (v << 8); }
uint16_t ntohs(uint16_t v) { return htons(v); }
uint32_t htonl(uint32_t v) {
    return ((v & 0xFF) << 24) | ((v & 0xFF00) << 8) |
           ((v & 0xFF0000) >> 8) | ((v & 0xFF000000) >> 24);
}
uint32_t ntohl(uint32_t v) { return htonl(v); }

uint16_t net_checksum(void *data, int len)
{
    uint32_t sum = 0;
    uint16_t *p = (uint16_t *)data;

    while (len > 1) {
        sum += *p++;
        len -= 2;
    }
    if (len) sum += *(uint8_t *)p;

    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return (uint16_t)~sum;
}