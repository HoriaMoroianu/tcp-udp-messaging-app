// Copyright (c) 2024 Horia-Valentin MOROIANU

#ifndef _UTILS_H
#define _UTILS_H

#include <bits/stdc++.h>
using namespace std;

#define DIE(assertion, call_description)	\
	do {									\
		if (assertion) {					\
			cerr << call_description		\
				 << "\n";					\
			exit(errno);					\
		}									\
	} while (0)

// Packet types
#define CONNECT 1
#define FOLLOW 2
#define	DATA 3

// Subscription status
#define UNSUBSCRIBE 0
#define SUBSCRIBE 1

// Payload formats
#define INT 0
#define SHORT_REAL 1
#define FLOAT 2
#define STRING 3

#define MIN_PORT_VALUE 1024
#define MAX_PORT_VALUE 65535

#define MAX_TOPIC_SIZE 50

struct packet {
	uint8_t type;					// type of packet
	union {
		struct {
			char client_id[11];		// id of the client
			uint8_t status;			// subscription status
			char topic[51];			// subscription topic
		} sub;
		struct {
			uint32_t udp_ip;		// UDP ip
			uint16_t udp_port;		// UDP port
			char topic[51];			// topic
			uint8_t dtype;			// data type
			char payload[1501];		// data contents
		} data;
	};
};

ssize_t recv_packet(int sockfd, void *buffer);
ssize_t send_packet(int sockfd, void *buffer);

void disable_nagle(int sockfd);
void reusable_address(int sockfd);

uint16_t check_input_port(const char *raw_input_port);
bool check_topic(const char *topic);

#endif
