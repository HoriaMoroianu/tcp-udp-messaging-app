// Copyright (c) 2024 Horia-Valentin MOROIANU

#include <bits/stdc++.h>
#include <arpa/inet.h>
#include <poll.h>

#include "utils.h"

using namespace std;

#define MAX_BUFF 256
#define MAX_CONN 32

void manage_server(int serverfd);

int main(int argc, char *argv[])
{
	// Disable buffering
	setvbuf(stdout, NULL, _IONBF, BUFSIZ);

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
	rc = listen(serverfd, MAX_CONN);
	DIE(rc == -1, "Server listen failed!");

	manage_server(serverfd);
	close(serverfd);

	return 0;
}

void manage_server(int serverfd)
{
	struct pollfd pfds[MAX_CONN];
	int pfds_size = 2;

	pfds[0].fd = serverfd;
	pfds[0].events = POLLIN;

	pfds[1].fd = STDIN_FILENO;
	pfds[1].events = POLLIN;

	while (1) {
		int poll_count = poll(pfds, pfds_size, -1);
		DIE(poll_count == -1, "Server poll error");

		for (int i = 0; i < pfds_size; i++) {
			// Check if someone's ready to read
			if (!(pfds[i].revents & POLLIN))
				continue;

			if (pfds[i].fd == serverfd) {
				// Create client address structure
				struct sockaddr_in conn_addr;
				socklen_t caddr_len = sizeof(conn_addr);
				memset(&conn_addr, 0, caddr_len);

				// Accept connection from new client
				int connfd = accept(serverfd, (struct sockaddr *)&conn_addr, &caddr_len);
				DIE(connfd == -1, "Connection failed!");

				// TODO: check for overflow
				// Add to poll fd
				pfds[pfds_size].fd = connfd;
				pfds[pfds_size].events = POLLIN;
				pfds_size++;

				// TODO: id client
				char con_adr_string[INET_ADDRSTRLEN];
				printf("New client <ID_CLIENT> connected from %s:%hu.\n", 
						inet_ntop(AF_INET, &conn_addr.sin_addr.s_addr, con_adr_string, INET_ADDRSTRLEN),
						ntohs(conn_addr.sin_port));

				continue;
			}

			if (pfds[i].fd == STDIN_FILENO) {
				char buff[MAX_BUFF];
				bzero(buff, MAX_BUFF);

				fgets(buff, MAX_BUFF, stdin);
				if (buff[strlen(buff) - 1] == '\n')
					buff[strlen(buff) - 1] = '\0';

				if ((strncmp(buff, "exit", 4)) == 0) {
					printf("Server Exit...\n");
					for (int j = 0; j < pfds_size; j++) {
						if (pfds[j].fd != serverfd && pfds[j].fd != STDIN_FILENO)
							close(pfds[j].fd);
					}
					return;
				}

				for (int j = 2; j < pfds_size; j++) {
					if (pfds[j].fd != serverfd && pfds[j].fd != STDIN_FILENO)
						send(pfds[j].fd, buff, strlen(buff), 0);
				}
				continue;
			}

			// Client:
			char buff[MAX_BUFF];

			bzero(buff, MAX_BUFF);
			int rc = recv(pfds[i].fd, buff, sizeof(buff), 0);
			DIE(rc == -1, "Error when recieving message");

			if (rc == 0) {
				// Connection closed

				// TODO: id client
				printf("Client <ID_CLIENT> disconnected.\n");
				close(pfds[i].fd);

				// TODO: remove from pdfs
				pfds[i].fd = -1;
				continue;
			}

			printf("[CLIENT]: %s\n", buff);

			for (int j = 0; j < pfds_size; j++) {
				if (pfds[j].fd != serverfd && pfds[j].fd != pfds[i].fd)
					send(pfds[j].fd, buff, strlen(buff), 0);
			}
		}
	}
}
