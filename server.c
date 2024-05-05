// Copyright (c) 2024 Horia-Valentin MOROIANU

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "utils.h"

#define MAX_BUFF 256

void chat(int connfd);

int main(int argc, char *argv[])
{
	DIE(argc != 2, "Usage: ./server <SERVER_PORT>");

	// TODO: other user input errors
	// Parse server port
	uint16_t server_port;
	int rc = sscanf(argv[1], "%hu", &server_port);
	DIE(rc != 1, "Invalid server port!");

	// Create server socket
	const int serverfd = socket(AF_INET, SOCK_STREAM, 0);
	DIE(serverfd == -1, "Error when creating server socket!");

	// Make server socket address reusable
	const int enable = 1;
	rc = setsockopt(serverfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
	DIE(serverfd == -1, "Error when setting reusable address!");

	// Create server address structure
	struct sockaddr_in serv_addr;
	memset(&serv_addr, 0, sizeof(serv_addr));

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(server_port);

	// Bind server socket to address
	rc = bind(serverfd, (const struct sockaddr *)&serv_addr, sizeof(serv_addr));
	DIE(rc == -1, "Server bind failed!");

	// Listen for connections
	rc = listen(serverfd, 1);
	DIE(rc == -1, "Server listen failed!");

	// Create client address structure
	struct sockaddr_in conn_addr;
	socklen_t caddr_len = sizeof(conn_addr);
	memset(&conn_addr, 0, caddr_len);

	// Accept connection from new client
	int connfd = accept(serverfd, (struct sockaddr *)&conn_addr, &caddr_len);
	DIE(connfd == -1, "Connection failed!");

	printf("Client connected to the server...\n");
	
	chat(connfd);

	close(connfd);
	close(serverfd);

	return 0;
}

void chat(int connfd)
{
	char buff[MAX_BUFF];

	while (1) {
		bzero(buff, MAX_BUFF);
		recv(connfd, buff, sizeof(buff), 0);

		printf("[CLIENT]: %s\n[SERVER]: ", buff);

		bzero(buff, MAX_BUFF);
		fgets(buff, MAX_BUFF, stdin);
		if (buff[strlen(buff) - 1] == '\n')
			buff[strlen(buff) - 1] = '\0';

		send(connfd, buff, strlen(buff), 0);

		if (strncmp("exit", buff, 4) == 0) { 
			printf("Server Exit...\n"); 
			break; 
		}
	}
}
