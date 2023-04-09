/*
 ************************************************
 *                                              *
 * Group   : 42                                 *
 * Member 1: Utsav Basu (20CS30057)             *
 * Member 2: Anamitra Mukhopadhyay (20CS30064)  *
 *                                              *
 ************************************************
 */

#define _GNU_SOURCE
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define MAX_PKT_SIZE (1 << 7)
#define PKT_NUM (1 << 3)

typedef struct _mypkt
{
	struct icmphdr hdr;
	char msg[MAX_PKT_SIZE - sizeof(struct icmphdr)];
} mypkt;

// Function to set the ip address from the link
void set_dest(struct sockaddr_in *dest_addr, char *link)
{
	// Get the hostent structure
	struct hostent *host = gethostbyname(link);
	if (host == NULL)
	{
		perror("gethostbyname");
		exit(1);
	}

	dest_addr->sin_family = host->h_addrtype;
	dest_addr->sin_port = htons(0);
	dest_addr->sin_addr.s_addr = *(long *)host->h_addr;
}

// Calculating the Check Sum
unsigned short checksum(void *b, int len)
{
	unsigned short *buf = b;
	unsigned int sum = 0;
	unsigned short result;

	for (sum = 0; len > 1; len -= 2)
		sum += *buf++;
	if (len == 1)
		sum += *(unsigned char *)buf;
	sum = (sum >> 16) + (sum & 0xFFFF);
	sum += (sum >> 16);
	result = ~sum;
	return result;
}

void print_icmph(struct icmphdr *icmph)
{
	printf("ICMP Header: ");
	printf("Type: %2d, Code: %d, Checksum: %d, ID: %d, Sequence: %d",
		   icmph->type, icmph->code, icmph->checksum, icmph->un.echo.id, icmph->un.echo.sequence);
	printf("\n");
}

void print_iph(struct iphdr *iph)
{
	printf("IP Header: ");
	printf("Version: %d, Header Length: %d, Type of Service: %d, Total Length: %d, ID: %d, Fragment Offset: %d, Time to Live: %d, Protocol: %d, Checksum: %d, Source IP: %s, Destination IP: %s",
		   iph->version, iph->ihl, iph->tos, iph->tot_len, iph->id, iph->frag_off, iph->ttl, iph->protocol, iph->check, inet_ntoa(*(struct in_addr *)&iph->saddr), inet_ntoa(*(struct in_addr *)&iph->daddr));
	printf("\n");
}

// generate random msg to send
void generate_msg(char *msg, int size)
{
	for (int i = 0; i < size; i++)
	{
		msg[i] = 'a' + (rand() % 26);
	}
}

/*
 * Function to calculate the bandwidth and latency
 * @param delays: array of delays
 * @param pkt_size: array of packet sizes
 * @param num_pkts: number of packets
 * @return: struct _bl containing bandwidth and latency
 * 
 * D = P/B + L, delay = packet_size/bandwidth + latency
 * For two packet sizes P1 and P2, and two delays D1 and D2, we have
 * P1/B + L = D1
 * P2/B + L = D2
 * Solving, B = (P2 - P1) / (D2 - D1) and L = (P2 * D1 - P1 * D2) / (P2 - P1)
 * We take the average of all the values of B and L
*/
struct _bl
{
	double bandwidth, latency;
} calculate_bl(double delays[], int pkt_size[], int num_pkts)
{
	double bandwidth = 0, latency = 0;

	for (int i = 0, j = 1; j < num_pkts; i++, j++)
	{	
		double temp_b = (pkt_size[j] - pkt_size[i]) / (delays[j] - delays[i]);
		bandwidth += temp_b > 0 ? temp_b : -temp_b;
		double temp_l = (pkt_size[j] * delays[i] -
					pkt_size[i] * delays[j]) /
				   (pkt_size[j] - pkt_size[i]);
		latency += temp_l > 0 ? temp_l : -temp_l;
	}

	bandwidth /= num_pkts - 1;
	latency /= num_pkts - 1;

	return (struct _bl){.bandwidth = bandwidth, .latency = latency};
}

int main(int argc, char *argv[])
{
	if (argc < 4)
	{
		printf("Usage: %s <link/ip address> <number of probes per link> <time difference between any two probes>", argv[0]);
		exit(1);
	}

	// Create a raw socket
	int sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
	if (sockfd < 0)
	{
		perror("socket");
		exit(1);
	}

	// Get the number of probes
	int num_probes = atoi(argv[2]);

	// Get the time difference between any two probes
	double time_diff = atof(argv[3]);

	// Get the destination address
	struct sockaddr_in dest_addr = {0}, r_addr;
	set_dest(&dest_addr, argv[1]);

	struct timeval tv_out;
	tv_out.tv_sec = 1;
	tv_out.tv_usec = 0; 

	int ttl = 1;
	struct timespec time_start, time_end;

	// setting timeout of recv setting
	setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv_out, sizeof tv_out);

	bool flag = false;

	double old_delays[PKT_NUM] = {0};
	int pkt_size[PKT_NUM] = {0};
	int cnt = 30;

	for (int i = 0; i < PKT_NUM; ++i)
	{
		pkt_size[i] = (i + 1) * (MAX_PKT_SIZE / PKT_NUM);
	}

	while (cnt--)
	{
		// Set the ttl field
		setsockopt(sockfd, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl));
		printf("--------------HOP %2d--------------\n", ttl);

		printf("Sending 5 packets with no data\n");
		for(int a = 0; a < 5; ++a)
		{
			mypkt pckt = {0};
			// Fill in the icmp header
			pckt.hdr.type = ICMP_ECHO;
			pckt.hdr.un.echo.id = getpid();

			// Calculate the checksum
			pckt.hdr.un.echo.sequence = a;
			pckt.hdr.checksum = 0;
			pckt.hdr.checksum = checksum(&pckt, sizeof(struct icmphdr));

			// Send the packet
			clock_gettime(CLOCK_MONOTONIC, &time_start);
			if (sendto(sockfd, &pckt, pkt_size[a], 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) <= 0)
			{
				perror("sendto");
				exit(1);
			}

			// Receive the packet
			int addr_len = sizeof(r_addr);

			char recvbuf[1024] = {0};
			ssize_t ret = 0;
			if (ret = recvfrom(sockfd, &recvbuf, sizeof(recvbuf), 0, (struct sockaddr *)&r_addr, &addr_len) <= 0)
			{
				if (errno == EAGAIN || errno == EWOULDBLOCK)
				{
					printf("*\t");
					continue;
				}
				else
				{
					perror("recvfrom");
					exit(1);
				}
			}
			else
			{
				clock_gettime(CLOCK_MONOTONIC, &time_end);

				double time_nsec = ((double)(time_end.tv_nsec - time_start.tv_nsec)) / 1000000.0;
				double rtt_msec = (time_end.tv_sec - time_start.tv_sec) * 1000.0 + time_nsec;

				printf("%.3f msec\t", rtt_msec);
			}
		}
		printf("\n");

		double delays[PKT_NUM] = {0};

		for (int a = 0; a < PKT_NUM; a++)
		{
			double delay_mean = 0;

			for (int i = 0; i < num_probes; i++)
			{
				printf("Packet Size: %d bytes, Probe: %d\n", pkt_size[a], i + 1);
				mypkt pckt = {0};
				// Fill in the icmp header
				pckt.hdr.type = ICMP_ECHO;
				pckt.hdr.un.echo.id = getpid();

				// Insert the data
				generate_msg(pckt.msg, pkt_size[a] - sizeof(struct icmphdr));

				// Calculate the checksum
				pckt.hdr.un.echo.sequence = i;
				pckt.hdr.checksum = 0;
				pckt.hdr.checksum = checksum(&pckt, pkt_size[a]);

				// Send the packet
				clock_gettime(CLOCK_MONOTONIC, &time_start);
				if (sendto(sockfd, &pckt, pkt_size[a], 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) <= 0)
				{
					perror("sendto");
					exit(1);
				}
				printf("Packet sent!\n");
				print_icmph(&pckt.hdr);

				// Receive the packet
				int addr_len = sizeof(r_addr);

				char recvbuf[1024] = {0};
				ssize_t ret = 0;
				if (ret = recvfrom(sockfd, &recvbuf, sizeof(recvbuf), 0, (struct sockaddr *)&r_addr, &addr_len) <= 0)
				{
					if (errno == EAGAIN || errno == EWOULDBLOCK)
					{
						printf("Request timed out.\n");
						continue;
					}
					else
					{
						perror("recvfrom");
						exit(1);
					}
				}
				else
				{
					clock_gettime(CLOCK_MONOTONIC, &time_end);

					char *rcv = strdup(inet_ntoa(r_addr.sin_addr));
					char *dest = strdup(inet_ntoa(dest_addr.sin_addr));
					printf("Packet received from %s\n", inet_ntoa(r_addr.sin_addr));
					struct icmphdr *r_icmph = (struct icmphdr *)recvbuf;
					print_icmph(r_icmph);

					// Check if the code is time exceeded or echo reply
					if (r_icmph->type != ICMP_ECHOREPLY && r_icmph->type != ICMP_TIME_EXCEEDED)
					{
						// Check if there is any data and print the ip header
						if (ret > sizeof(struct icmphdr))
						{
							struct iphdr *r_iph = (struct iphdr *)(recvbuf + sizeof(struct icmphdr));
							print_iph(r_iph);
						}
					}

					double time_nsec = ((double)(time_end.tv_nsec - time_start.tv_nsec)) / 1000000.0;
					double rtt_msec = (time_end.tv_sec - time_start.tv_sec) * 1000.0 + time_nsec;

					delay_mean += (rtt_msec / 2);

					printf("RTT: %.3f msec\n", rtt_msec);

					if (strcmp(rcv, dest) == 0)
					{
						flag = true;
					}

					free(rcv);
					free(dest);
				}

				usleep(time_diff * 1e6);
				fflush(stdout);
			}

			delays[a] = delay_mean / num_probes;
		}

		double diff[PKT_NUM] = {0};
		for (int i = 0; i < PKT_NUM; i++)
		{
			diff[i] = delays[i] - old_delays[i];
			old_delays[i] = delays[i];

			// fprintf(stderr, "Hop: %3d, Packet Size: %4i, diff[%d] = %lf\n",
			// 		ttl, pkt_size[i], i, diff[i]);
		}

		struct _bl res = calculate_bl(diff, pkt_size, PKT_NUM);

		printf("Bandwidth: %.3f Kbps\n", 8 * res.bandwidth);
		printf("Latency: %.3f msec\n", res.latency);

		printf("\n");

		if (flag)
		{
			break;
		}

		ttl++;
	}

	puts("\n\n------------------Done------------------------");

	return 0;
}
