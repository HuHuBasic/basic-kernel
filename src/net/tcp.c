/*
 * tcp.c - 最小 TCP 协议栈 (仅用于 HTTP 客户端)
 */
#include "network.h"
#include "../string.h"
#include "../vga.h"
#include "../timer.h"
#include "../memory.h"

#define MAX_TCP_CONNS 4
#define TCP_STATE_CLOSED      0
#define TCP_STATE_SYN_SENT    1
#define TCP_STATE_ESTABLISHED 2
#define TCP_STATE_CLOSING     3

static tcp_conn_t tcp_conns[MAX_TCP_CONNS];
static uint16_t   next_port = 49152;

/* 伪头部校验和 */
typedef struct __attribute__((packed)) {
    uint8_t  src_ip[4];
    uint8_t  dst_ip[4];
    uint8_t  zero;
    uint8_t  protocol;
    uint16_t tcp_len;
} tcp_pseudo_t;

static uint16_t tcp_checksum(uint8_t *src_ip, uint8_t *dst_ip,
                              tcp_packet_t *tcp, int tcp_len)
{
    uint8_t buf[1536];
    int pos = 0;

    tcp_pseudo_t pseudo;
    memcpy(pseudo.src_ip, src_ip, 4);
    memcpy(pseudo.dst_ip, dst_ip, 4);
    pseudo.zero     = 0;
    pseudo.protocol = IP_PROTO_TCP;
    pseudo.tcp_len  = htons(tcp_len);

    memcpy(buf + pos, &pseudo, sizeof(pseudo));
    pos += sizeof(pseudo);
    memcpy(buf + pos, tcp, tcp_len);
    pos += tcp_len;

    /* 如果奇数长度, 补零 */
    if (pos & 1) buf[pos++] = 0;

    return net_checksum(buf, pos);
}

static void tcp_send_packet(tcp_conn_t *c, uint8_t flags, uint8_t *data, int dlen)
{
    uint8_t pkt[1536];
    tcp_packet_t *tcp = (tcp_packet_t *)pkt;
    memset(tcp, 0, sizeof(tcp_packet_t));

    int tcp_hdr_len = 20; /* 无选项 */
    int total = tcp_hdr_len + dlen;

    tcp->src_port  = htons(c->local_port);
    tcp->dst_port  = htons(c->remote_port);
    tcp->seq_num   = htonl(c->seq_num);
    tcp->ack_num   = htonl(c->ack_num);
    tcp->data_offset = (tcp_hdr_len / 4) << 4;
    tcp->flags     = flags;
    tcp->window    = htons(4096);

    if (dlen > 0 && data) {
        memcpy(tcp->payload, data, dlen);
    }

    tcp->checksum = 0;
    tcp->checksum = tcp_checksum((uint8_t *)&c->local_ip, (uint8_t *)&c->remote_ip, tcp, total);

    uint8_t dst_ip[4];
    dst_ip[0] = (c->remote_ip >> 24) & 0xFF;
    dst_ip[1] = (c->remote_ip >> 16) & 0xFF;
    dst_ip[2] = (c->remote_ip >> 8) & 0xFF;
    dst_ip[3] = c->remote_ip & 0xFF;

    ipv4_send(dst_ip, IP_PROTO_TCP, pkt, total);

    if (dlen > 0) {
        c->seq_num += dlen;
    }
    if (flags & TCP_FLAG_SYN) c->seq_num++;
    if (flags & TCP_FLAG_FIN) c->seq_num++;
}

static uint32_t ip_to_u32(uint8_t *ip)
{
    return ((uint32_t)ip[0] << 24) | ((uint32_t)ip[1] << 16) |
           ((uint32_t)ip[2] << 8)  | (uint32_t)ip[3];
}

int tcp_connect(uint8_t *dst_ip, uint16_t dst_port)
{
    /* 查找空闲连接 */
    int cid = -1;
    for (int i = 0; i < MAX_TCP_CONNS; i++) {
        if (tcp_conns[i].state == TCP_STATE_CLOSED) {
            cid = i;
            break;
        }
    }
    if (cid < 0) return -1;

    tcp_conn_t *c = &tcp_conns[cid];
    memset(c, 0, sizeof(tcp_conn_t));

    c->local_ip    = ip_to_u32(nic_ip);
    c->remote_ip   = ip_to_u32(dst_ip);
    c->local_port  = next_port++;
    c->remote_port = dst_port;
    c->seq_num     = 0x12345678;
    c->state       = TCP_STATE_SYN_SENT;

    /* 发送 SYN */
    tcp_send_packet(c, TCP_FLAG_SYN, NULL, 0);

    /* 等待 SYN-ACK */
    uint8_t buf[4096];
    int     len;
    for (int tries = 0; tries < 60; tries++) {
        timer_sleep(50);
        uint8_t src_ip[4];
        len = ipv4_recv(buf, sizeof(buf), src_ip);

        if (len > 0) {
            tcp_packet_t *tcp = (tcp_packet_t *)buf;
            uint16_t flags = tcp->flags;

            if (ntohs(tcp->src_port) == dst_port &&
                ntohs(tcp->dst_port) == c->local_port) {

                if (flags & TCP_FLAG_SYN && flags & TCP_FLAG_ACK) {
                    c->remote_seq = ntohl(tcp->seq_num) + 1;
                    c->ack_num    = ntohl(tcp->seq_num) + 1;

                    /* 发送 ACK */
                    tcp_send_packet(c, TCP_FLAG_ACK, NULL, 0);
                    c->state = TCP_STATE_ESTABLISHED;
                    return cid;
                }
            }
        }
    }

    c->state = TCP_STATE_CLOSED;
    return -1;
}

int tcp_send(int conn_id, uint8_t *data, int len)
{
    if (conn_id < 0 || conn_id >= MAX_TCP_CONNS) return -1;
    tcp_conn_t *c = &tcp_conns[conn_id];
    if (c->state != TCP_STATE_ESTABLISHED) return -1;

    tcp_send_packet(c, TCP_FLAG_PSH | TCP_FLAG_ACK, data, len);
    return len;
}

int tcp_recv(int conn_id, uint8_t *buffer, int max_len)
{
    if (conn_id < 0 || conn_id >= MAX_TCP_CONNS) return -1;
    tcp_conn_t *c = &tcp_conns[conn_id];

    /* 非阻塞接收 */
    uint8_t buf[4096];
    int     len;
    uint8_t src_ip[4];

    len = ipv4_recv(buf, sizeof(buf), src_ip);
    if (len <= 0) return 0;

    tcp_packet_t *tcp = (tcp_packet_t *)buf;
    if (ntohs(tcp->src_port) != c->remote_port) return 0;

    uint8_t flags = tcp->flags;

    if (flags & TCP_FLAG_FIN) {
        c->ack_num = ntohl(tcp->seq_num) + 1;
        tcp_send_packet(c, TCP_FLAG_ACK | TCP_FLAG_FIN, NULL, 0);
        c->state = TCP_STATE_CLOSING;
        return -1;
    }

    if (flags & TCP_FLAG_RST) {
        c->state = TCP_STATE_CLOSED;
        return -1;
    }

    int hdr_len = ((tcp->data_offset >> 4) & 0xF) * 4;
    int dlen = len - hdr_len;

    if (dlen > 0) {
        c->remote_seq = ntohl(tcp->seq_num) + dlen;
        c->ack_num    = ntohl(tcp->seq_num) + dlen;

        if (dlen > max_len) dlen = max_len;
        memcpy(buffer, tcp->payload, dlen);

        /* 发送 ACK */
        tcp_send_packet(c, TCP_FLAG_ACK, NULL, 0);

        return dlen;
    }

    return 0;
}

void tcp_close(int conn_id)
{
    if (conn_id < 0 || conn_id >= MAX_TCP_CONNS) return;
    tcp_conn_t *c = &tcp_conns[conn_id];
    if (c->state != TCP_STATE_ESTABLISHED) return;

    tcp_send_packet(c, TCP_FLAG_FIN | TCP_FLAG_ACK, NULL, 0);
    c->state = TCP_STATE_CLOSING;
}