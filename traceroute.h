#ifndef TRACEROUTE_H
#define TRACEROUTE_H

#define HOPS 30           // Max hops, defined in the assignment
#define MAX_RESENDING 3     // Resending 3 time the same packet
#define TIMEOUT 1000          // 1 seconed timeout

struct ip_hdr{
    uint8_t ver_ihl;
    uint8_t tos;
    uint16_t total_len;
    uint16_t flags_offset;
    uint16_t identification;
    uint8_t ttl;
    uint8_t protocol;
    uint16_t checksum;
    uint32_t src_ip;
    uint32_t dest_ip;
};

// Functions declerations
unsigned short int checksum(void *data, unsigned int bytes);

double get_rtt(struct timeval *start, struct timeval *end);

void create_icmp_header(struct icmphdr *icmp_header, int ttl, int i, char *buffer);


#endif // TRACEROUTE_H
