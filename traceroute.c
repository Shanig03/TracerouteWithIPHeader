#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <poll.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <signal.h>
#include <getopt.h>
#include <stdbool.h>
#include "traceroute.h"

// Function to calculate the rtt, based on starting time and ending time
double get_rtt(struct timeval *start, struct timeval *end) {
    double seconds = end->tv_sec - start->tv_sec;
    double microseconds = end->tv_usec - start->tv_usec;

    return (seconds * 1000.0) + (microseconds / 1000.0);
}

// Function to compute checksum for ICMP packets
unsigned short int checksum(void *data, unsigned int bytes_size) {
    unsigned short int *data_p = (unsigned short int *)data; // Cast the input data to a pointer to unsigned short int
    unsigned int sum = 0;

    // Loop through the data two bytes (16 bits) at a time
    while (bytes_size > 1) { 
        sum += *data_p++;
        bytes_size -= 2;
    }

    // If there is a remaining byte (for odd-sized data), process it
    if (bytes_size > 0) {
        sum += *((unsigned char *)data_p);
    }

    // Handle any overflow from the addition
    while (sum >> 16) { 
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return (~((unsigned short int)sum)); // Return the one's complement of the sum as the checksum
}

// Function to create ICMP header
void create_icmp_header(struct icmphdr *icmp_header, int ttl, int i, char *buffer) {
    // Set the ICMP header fields
    icmp_header->code = 0;
    icmp_header->type = ICMP_ECHO;
    icmp_header->un.echo.sequence = htons(ttl * MAX_RESENDING + i);
    icmp_header->un.echo.id = htons(getpid()); 
    icmp_header->checksum = 0;

    // Copy the ICMP header to the buffer
    memcpy(buffer, icmp_header, sizeof(struct icmphdr));

    // Calculate and set the checksum
    icmp_header->checksum = checksum(buffer, sizeof(struct icmphdr));
    ((struct icmphdr *)buffer)->checksum = icmp_header->checksum;
}


int main(int argc, char *argv[]) {
    int curr;
    char *addr = NULL;
    
    // Parse command-line arguments for the address
    while ((curr = getopt(argc, argv, "a:")) != -1) {
        switch (curr) {
            case 'a':
                addr = optarg; // Store the address provided by the user
                break;
            default:
                fprintf(stderr, "Usage is: %s -a <address>\n", argv[0]);
                return 1;
        }
    }

    // Ensure that the address was provided
    if (!addr) {
        fprintf(stderr, "Error: address is required.\n");
        return 1;
    }

    // Prepare the destination sockaddr_in structure for IPv4
    struct sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    if (inet_pton(AF_INET, addr, &dest.sin_addr) <= 0) {
        fprintf(stderr, "Error: neet to enter a valid IPv4 address.\n");
        return 1;
    }

    // Create a raw socket to send ICMP packets
    int sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sock < 0) {
        perror("socket");
        if (errno == EACCES || errno == EPERM) {
            fprintf(stderr, "Need to run with sudo.\n");
        }
        return 1;
    }

    // Print traceroute info
    fprintf(stdout, "traceroute to %s, %d hops max\n ", addr, HOPS);
    
    int counter = 0; // We wanted that if it gets to the demanded address, it will still sends all 3 packets so that it will show in the terminal like we were asked for.
    int ttl = 1;

    // Loop through the TTL values
    for (ttl = 1; ttl <= HOPS; ++ttl) {
        // Set the TTL for the socket to control the hop limit
        if (setsockopt(sock, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl)) != 0) {
            perror("setsockopt");
            close(sock);
            return 1;
        }

        // Print the hop number
        fprintf(stdout, "%2d  ", ttl); 

        // Send the ICMP packets for the current TTL, 3 times each packet
        for (int i = 0; i < MAX_RESENDING; ++i) {
            char buffer[1024] = {0}; 

            struct icmphdr icmp_header;
            struct ip_hdr ip_header;
            ip_header.ver_ihl = (4 << 4) | 5;
            ip_header.tos = 0;
            ip_header.total_len = htons(sizeof(struct ip_hdr) + sizeof(icmp_header));
            ip_header.identification = htons(12345);
            ip_header.flags_offset = 0;
            ip_header.ttl = ttl;
            ip_header.protocol = IPPROTO_ICMP;
            ip_header.src_ip = inet_addr("109.160.129.27");
            ip_header.dest_ip = inet_addr(*addr);
            ip_header.checksum = checksum(&ip_header,sizeof(ip_header));

            //create_ip_header(&ip_header, ttl,sizeof(icmp_header),dest);

            // Create the ICMP header using the separate function
            create_icmp_header(&icmp_header, ttl, i, buffer);

            struct timeval starting_time, ending_time; 
            gettimeofday(&starting_time, NULL); 

            // Send the ICMP packet to the destination
            if (sendto(sock, buffer, sizeof(icmp_header), 0,
                       (struct sockaddr *)&dest, sizeof(dest)) <= 0 && sendto(sock, &ip_header, sizeof(ip_header),0,(struct sockaddr *)&dest, sizeof(dest)) <= 0) {
                perror("sendto");
                fprintf(stdout, " * "); 
                continue;
            }

            // Poll for a response (with timeout)
            struct pollfd pfd[1];
            pfd[0].fd = sock;
            pfd[0].events = POLLIN;

            int ans = poll(pfd, 1, TIMEOUT);
            if (ans == 0) { 
                // Timeout occurred, no response
                fprintf(stdout, " * ");
                continue;
            } else if (ans < 0) { 
                // Poll error
                perror("poll");
                fprintf(stdout, " * ");
                continue;
            }

            char response[1024];
            struct sockaddr_in res_addr;
            socklen_t len = sizeof(res_addr);
            // Receive the response packet
            if (recvfrom(sock, response, sizeof(response), 0,(struct sockaddr *)&res_addr, &len) <= 0) {
                perror("recvfrom");
                fprintf(stdout, " * ");
                continue;
            }

            gettimeofday(&ending_time, NULL);
            double rtt = get_rtt(&starting_time, &ending_time); 

            // Print the response time and address for the first packet
            if (i == 0) { 
                fprintf(stdout, " %s   ", inet_ntoa(res_addr.sin_addr));
            }
            fprintf(stdout, "%.3fms   ", rtt);

            // Compare the destination address with the response address
            if (memcmp(&dest.sin_addr, &res_addr.sin_addr, sizeof(dest.sin_addr)) == 0) {
                counter++;
            }
            
            if (counter == 3){
                fprintf(stdout, "\n");
                close(sock);
                return 0;
            }
        }

        fprintf(stdout, "\n "); 
    }

    // Close the socket when done
    close(sock); 
    return 0;
}

