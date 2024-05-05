// Copyright (c) 2024 Horia-Valentin MOROIANU

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "utils.h"

#define MAX_BUFF 256

void chat(int serverfd);

int main(int argc, char *argv[])
{
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

	chat(serverfd);

	close(serverfd);

	return 0;
}

void chat(int serverfd)
{
	char buff[MAX_BUFF];

	while (1) {
		printf("[CLIENT]: ");

		bzero(buff, MAX_BUFF);
		fgets(buff, MAX_BUFF, stdin);
		if (buff[strlen(buff) - 1] == '\n')
			buff[strlen(buff) - 1] = '\0';

		send(serverfd, buff, strlen(buff), 0);

		if ((strncmp(buff, "exit", 4)) == 0) {
			printf("Client Exit...\n");
			break;
		}

		bzero(buff, sizeof(buff));
		recv(serverfd, buff, sizeof(buff), 0);
		printf("[SERVER]: %s\n", buff);
	}
}
