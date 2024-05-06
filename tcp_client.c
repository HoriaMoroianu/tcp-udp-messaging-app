// Copyright (c) 2024 Horia-Valentin MOROIANU

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "utils.h"

#define MAX_BUFF 256

void manage_connection(int serverfd);

int main(int argc, char *argv[])
{
	// Disable buffering
	setvbuf(stdout, NULL, _IONBF, BUFSIZ);

	DIE(argc != 4, "Usage: ./subscriber <CLIENT_ID> <SERVER_IP> <SERVER_PORT>");

	// TODO: other user input errors

	// Parse server ip
	uint32_t server_ip;
	int rc = inet_pton(AF_INET, argv[2], &server_ip);
	DIE(rc != 1, "Invalid server ip!");

	// Parse server port
	uint16_t server_port;
	rc = sscanf(argv[3], "%hu", &server_port);
	DIE(rc != 1, "Invalid server port!");

	// Create server socket
	const int serverfd = socket(AF_INET, SOCK_STREAM, 0);
	DIE(serverfd == -1, "Error when creating server socket!");

	// Create server address structure
	struct sockaddr_in serv_addr;
	memset(&serv_addr, 0, sizeof(serv_addr));

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(server_port);
	serv_addr.sin_addr.s_addr = server_ip;

	// Connect to server
	rc = connect(serverfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
	DIE(rc == -1, "Failed to connect to server!");

	manage_connection(serverfd);

	close(serverfd);

	return 0;
}

void manage_connection(int serverfd)
{
	struct pollfd pfds[2];

	pfds[0].fd = serverfd;
	pfds[0].events = POLLIN;

	pfds[1].fd = STDIN_FILENO;
	pfds[1].events = POLLIN;

	while (1) {
		int poll_count = poll(pfds, 2, -1);
		DIE(poll_count == -1, "Subscriber poll error");

		if (pfds[0].revents & POLLIN) {
			char buff[MAX_BUFF];
			bzero(buff, MAX_BUFF);

			int rc = recv(serverfd, buff, sizeof(buff), 0);
			DIE(rc == -1, "Error when recieving message");

			if (rc == 0)
				return;
			
			printf("[SERVER]: %s\n", buff);
		}

		if (pfds[1].revents & POLLIN) {
			char buff[MAX_BUFF];
			bzero(buff, MAX_BUFF);

			fgets(buff, MAX_BUFF, stdin);
			if (buff[strlen(buff) - 1] == '\n')
				buff[strlen(buff) - 1] = '\0';

			if ((strncmp(buff, "exit", 4)) == 0) {
				printf("Client Exit...\n");
				return;
			}

			send(serverfd, buff, strlen(buff), 0);
		}
	}
}
