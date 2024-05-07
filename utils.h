// Copyright (c) 2024 Horia-Valentin MOROIANU

#ifndef _UTILS_H
#define _UTILS_H

// #include <stdlib.h>
#include <errno.h>

#define DEB_DIE(assertion, call_description)	\
	do {										\
		if (assertion) {						\
			fprintf(stderr, "(%s, %d): ",		\
					__FILE__, __LINE__);		\
			perror(call_description);			\
			exit(errno);						\
		}										\
	} while (0)

#define DIE(assertion, call_description)	\
	do {									\
		if (assertion) {					\
			fprintf(stderr, "%s\n",			\
					call_description);		\
			exit(errno);					\
		}									\
	} while (0)

#define CONNECT 0
#define SUB_STAT 1
#define	SDATA 2

struct packet {
	uint8_t type;					// type of packet
	union {
		struct {
			char client_id[11];		// id of the client
			uint8_t status;			// subscription status
			char topic[50];			// subscription topic
		} sub;
		struct {
			uint32_t udp_ip;		// UDP sensor ip
			uint16_t udp_port;		// UDP sensor port
			uint16_t len;			// payload length
			char topic[50];			// sensor topic
			uint8_t dtype;			// sensor data type
			char payload[1500];		// sensor data contents
		} data;
	};
};

ssize_t recv_packet(int sockfd, void *buffer);
ssize_t send_packet(int sockfd, void *buffer);

#endif
