// Copyright (c) 2024 Horia-Valentin MOROIANU

#include <bits/stdc++.h>
#include <sys/socket.h>

#include "utils.h"

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
