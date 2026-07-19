/*
 * arp.c - ARP 地址解析协议
 */
#include "network.h"
#include "../string.h"
#include "../vga.h"
#include "../timer.h"

static uint8_t  arp_ip_table[ARP_TABLE_SIZE][4];
static uint8_t  arp_mac_table[ARP_TABLE_SIZE][6];
static int      arp_count = 0;

void arp_handle_packet(uint8_t *data, int len)
{
    if (len < (int)sizeof(arp_packet_t)) return;

    arp_packet_t *arp = (arp_packet_t *)data;

    /* 只处理对我们的请求 */
    if (ntohs(arp->oper) != ARP_REQUEST) return;

    int is_ours = 1;
    for (int i = 0; i < 4; i++) {
        if (arp->tpa[i] != nic_ip[i]) { is_ours = 0; break; }
    }
    if (!is_ours) return;

    /* 发送 ARP 回复 */
    arp_packet_t reply;
    reply.htype = htons(1);
    reply.ptype = htons(0x0800);
    reply.hlen  = 6;
    reply.plen  = 4;
    reply.oper  = htons(ARP_REPLY);
    memcpy(reply.sha, nic_mac, 6);
    memcpy(reply.spa, nic_ip, 4);
    memcpy(reply.tha, arp->sha, 6);
    memcpy(reply.tpa, arp->spa, 4);

    ether_send(arp->sha, ETHERTYPE_ARP, (uint8_t *)&reply, sizeof(reply));
}

int arp_resolve(uint8_t *target_ip, uint8_t *out_mac)
{
    /* 检查缓存 */
    for (int i = 0; i < arp_count; i++) {
        if (memcmp(arp_ip_table[i], target_ip, 4) == 0) {
            memcpy(out_mac, arp_mac_table[i], 6);
            return 1;
        }
    }

    /* 发送 ARP 请求 */
    arp_packet_t req;
    req.htype = htons(1);
    req.ptype = htons(0x0800);
    req.hlen  = 6;
    req.plen  = 4;
    req.oper  = htons(ARP_REQUEST);
    memcpy(req.sha, nic_mac, 6);
    memcpy(req.spa, nic_ip, 4);
    memset(req.tha, 0, 6);
    memcpy(req.tpa, target_ip, 4);

    uint8_t broadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    ether_send(broadcast, ETHERTYPE_ARP, (uint8_t *)&req, sizeof(req));

    /* 等待回复 (最多 2 秒) */
    uint8_t pkt[1536];
    int     pkt_len;
    for (int tries = 0; tries < 40; tries++) {
        timer_sleep(50);
        ether_recv(pkt, &pkt_len);

        if (pkt_len > 0) {
            ether_frame_t *ef = (ether_frame_t *)pkt;
            (void)ef;

            arp_packet_t *arp = (arp_packet_t *)pkt;
            if (ntohs(arp->oper) == ARP_REPLY &&
                memcmp(arp->spa, target_ip, 4) == 0) {

                memcpy(out_mac, arp->sha, 6);

                /* 缓存 */
                if (arp_count < ARP_TABLE_SIZE) {
                    memcpy(arp_ip_table[arp_count], target_ip, 4);
                    memcpy(arp_mac_table[arp_count], arp->sha, 6);
                    arp_count++;
                }
                return 1;
            }
        }
    }

    return 0;
}