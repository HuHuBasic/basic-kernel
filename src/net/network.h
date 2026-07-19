/*
 * network.h - 网络协议栈统一头文件
 */
#ifndef _NETWORK_H
#define _NETWORK_H

#include <stdint.h>
#include <stddef.h>

/* ---- 常量 ---- */
#define MAC_ADDR_LEN 6
#define IP_ADDR_LEN  4

/* ---- 以太网帧 ---- */
#define ETHERTYPE_ARP  0x0806
#define ETHERTYPE_IPV4 0x0800

typedef struct __attribute__((packed)) {
    uint8_t  dst_mac[6];
    uint8_t  src_mac[6];
    uint16_t ethertype;
    uint8_t  payload[];
} ether_frame_t;

/* ---- ARP ---- */
#define ARP_REQUEST 1
#define ARP_REPLY   2
#define ARP_TABLE_SIZE 16

typedef struct __attribute__((packed)) {
    uint16_t htype;
    uint16_t ptype;
    uint8_t  hlen;
    uint8_t  plen;
    uint16_t oper;
    uint8_t  sha[6];
    uint8_t  spa[4];
    uint8_t  tha[6];
    uint8_t  tpa[4];
} arp_packet_t;

/* ---- IPv4 ---- */
#define IP_PROTO_TCP 6
#define IP_PROTO_UDP 17

typedef struct __attribute__((packed)) {
    uint8_t  ver_ihl;
    uint8_t  dscp_ecn;
    uint16_t total_len;
    uint16_t id;
    uint16_t flags_frag;
    uint8_t  ttl;
    uint8_t  protocol;
    uint16_t checksum;
    uint8_t  src_ip[4];
    uint8_t  dst_ip[4];
    uint8_t  payload[];
} ipv4_packet_t;

/* ---- TCP ---- */
#define TCP_FLAG_FIN 0x01
#define TCP_FLAG_SYN 0x02
#define TCP_FLAG_RST 0x04
#define TCP_FLAG_PSH 0x08
#define TCP_FLAG_ACK 0x10

typedef struct __attribute__((packed)) {
    uint16_t src_port;
    uint16_t dst_port;
    uint32_t seq_num;
    uint32_t ack_num;
    uint8_t  data_offset;
    uint8_t  flags;
    uint16_t window;
    uint16_t checksum;
    uint16_t urgent;
    uint8_t  payload[];
} tcp_packet_t;

/* ---- TCP 连接状态 ---- */
typedef struct {
    uint32_t local_ip;
    uint32_t remote_ip;
    uint16_t local_port;
    uint16_t remote_port;
    uint32_t seq_num;
    uint32_t ack_num;
    uint32_t remote_seq;
    int      state;
    uint8_t  rx_buffer[4096];
    int      rx_len;
} tcp_conn_t;

/* ---- 网络初始化 ---- */
void network_init(void);
int  network_configure(uint8_t *ip, uint8_t *gateway, uint8_t *mask);

/* ---- 以太网 ---- */
void ether_send(uint8_t *dst_mac, uint16_t ethertype, uint8_t *data, int len);
void ether_recv(uint8_t *buffer, int *len);

/* ---- ARP ---- */
int  arp_resolve(uint8_t *target_ip, uint8_t *out_mac);
void arp_handle_packet(uint8_t *data, int len);

/* ---- IP ---- */
void ipv4_send(uint8_t *dst_ip, uint8_t protocol, uint8_t *data, int len);
int  ipv4_recv(uint8_t *buffer, int max_len, uint8_t *src_ip);

/* ---- TCP ---- */
int  tcp_connect(uint8_t *dst_ip, uint16_t dst_port);
int  tcp_send(int conn_id, uint8_t *data, int len);
int  tcp_recv(int conn_id, uint8_t *buffer, int max_len);
void tcp_close(int conn_id);

/* ---- HTTP ---- */
int  http_get(const char *url, char *response, int max_len);

/* ---- 工具 ---- */
uint16_t net_checksum(void *data, int len);
uint16_t htons(uint16_t v);
uint16_t ntohs(uint16_t v);
uint32_t htonl(uint32_t v);
uint32_t ntohl(uint32_t v);

/* 网卡 MAC 地址 */
extern uint8_t  nic_mac[6];
extern uint8_t  nic_ip[4];
extern uint8_t  nic_gateway[4];
extern uint8_t  nic_mask[4];

#endif /* _NETWORK_H */