// Copyright (c) 2024 Horia-Valentin MOROIANU

#include <bits/stdc++.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>

#include "utils.h"

using namespace std;

ssize_t recv_packet(int sockfd, void *buffer)
{
	ssize_t recv_bytes = 0;
	ssize_t bytes_remaining = sizeof(packet);
	char *buff = (char *)buffer;

	while (bytes_remaining) {
		ssize_t rc = recv(sockfd, &buff[recv_bytes], bytes_remaining, 0);
		if (rc == -1)
			return -1;
		if (rc == 0)
			return recv_bytes;

		recv_bytes += rc;
		bytes_remaining -= rc;
	}
	return recv_bytes;
}

ssize_t send_packet(int sockfd, void *buffer)
{
	ssize_t sent_bytes = 0;
	ssize_t bytes_remaining = sizeof(packet);
	char *buff = (char *)buffer;

	while (bytes_remaining) {
		ssize_t rc = send(sockfd, &buff[sent_bytes], bytes_remaining, 0);
		if (rc == -1)
			return -1;
		if (rc == 0)
			return sent_bytes;

		sent_bytes += rc;
		bytes_remaining -= rc;
	}
	return sent_bytes;
}

void disable_nagle(int sockfd)
{
	const int enable = 1;
	int rc = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(int));
	DIE(rc == -1, "Error when diableing Nagle!");
}

void reusable_address(int sockfd)
{
	const int enable = 1;
	int rc = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
	DIE(rc == -1, "Error when setting reusable address!");
}

uint16_t check_input_port(const char *raw_input_port)
{
	int temp;
	int rc = sscanf(raw_input_port, "%d", &temp);
	DIE(rc != 1, "The port should be an integer!");

	if (temp >= MIN_PORT_VALUE && temp <= MAX_PORT_VALUE)
		return (uint16_t)temp;

	cerr << "The port should be between " << MIN_PORT_VALUE 
		 << " and " << MAX_PORT_VALUE << "!\n";

	exit(EXIT_FAILURE);
}

bool check_topic(const char *topic)
{
	for (size_t i = 0; i < strlen(topic); i++) {
		if (topic[i] == ' ')
			return false;

		if (i == strlen(topic) - 1)
			continue;

		if (topic[i] == '/' && topic[i + 1] == '/')
			return false;

		if ((topic[i] == '*' || topic[i] == '+') && topic[i + 1] != '/')
			return false;

		if (i == strlen(topic) - 2)
			continue;

		if ((topic[i] == '*' || topic[i] == '+') && 
			(topic[i + 2] == '*' || topic[i + 2] == '+'))
			return false;
	}
	return true;
}
