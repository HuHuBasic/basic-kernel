/*
 * ipv4.c - IPv4 协议处理
 */
#include "network.h"
#include "../string.h"
#include "../vga.h"

/* 伪头部 (用于 TCP/UDP 校验和) */
typedef struct __attribute__((packed)) {
    uint8_t  src_ip[4];
    uint8_t  dst_ip[4];
    uint8_t  zero;
    uint8_t  protocol;
    uint16_t length;
} ip_pseudo_header_t;

void ipv4_send(uint8_t *dst_ip, uint8_t protocol, uint8_t *data, int len)
{
    uint8_t pkt[1536];

    ipv4_packet_t *ip = (ipv4_packet_t *)pkt;
    memset(ip, 0, sizeof(ipv4_packet_t));

    int total = sizeof(ipv4_packet_t) + len;

    ip->ver_ihl   = 0x45;
    ip->total_len = htons(total);
    ip->id        = htons(1);
    ip->ttl       = 64;
    ip->protocol  = protocol;
    memcpy(ip->src_ip, nic_ip, 4);
    memcpy(ip->dst_ip, dst_ip, 4);
    memcpy(ip->payload, data, len);

    ip->checksum = 0;
    ip->checksum = net_checksum(ip, sizeof(ipv4_packet_t));

    /* ARP 解析目标 MAC */
    uint8_t dst_mac[6];
    /* 检查是否同一子网 */
    int same_net = 1;
    for (int i = 0; i < 4; i++) {
        if ((nic_ip[i] & nic_mask[i]) != (dst_ip[i] & nic_mask[i])) {
            same_net = 0; break;
        }
    }

    uint8_t *next_hop = same_net ? dst_ip : nic_gateway;

    if (!arp_resolve(next_hop, dst_mac)) {
        return; /* ARP 失败 */
    }

    ether_send(dst_mac, ETHERTYPE_IPV4, pkt, total);
}

int ipv4_recv(uint8_t *buffer, int max_len, uint8_t *src_ip)
{
    uint8_t pkt[1536];
    int pkt_len;

    ether_recv(pkt, &pkt_len);
    if (pkt_len <= 0) return 0;

    ipv4_packet_t *ip = (ipv4_packet_t *)pkt;

    /* 检查是否发给我们的 */
    int is_ours = 1;
    for (int i = 0; i < 4; i++) {
        if (ip->dst_ip[i] != nic_ip[i]) { is_ours = 0; break; }
    }
    if (!is_ours) return 0;

    int payload_len = ntohs(ip->total_len) - sizeof(ipv4_packet_t);
    if (payload_len <= 0 || payload_len > max_len) return 0;

    memcpy(buffer, ip->payload, payload_len);
    if (src_ip) memcpy(src_ip, ip->src_ip, 4);

    return payload_len;
}