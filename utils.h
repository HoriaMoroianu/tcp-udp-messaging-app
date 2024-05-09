// Copyright (c) 2024 Horia-Valentin MOROIANU

#ifndef _UTILS_H
#define _UTILS_H

#include <errno.h>
#include <string.h>

using namespace std;

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

#define CONNECT 1
#define FOLLOW 2
#define	SDATA 3

#define UNSUBSCRIBE 0
#define SUBSCRIBE 1

#define MIN_PORT_VALUE 1024
#define MAX_PORT_VALUE 65535

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

	// // Default constructor
	// packet() : type(0), data{} {}

	// // Destructor
    // ~packet() {}
};

ssize_t recv_packet(int sockfd, void *buffer);
ssize_t send_packet(int sockfd, void *buffer);
uint16_t check_input_port(const char *raw_input_port);

#endif
