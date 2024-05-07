// Copyright (c) 2024 Horia-Valentin MOROIANU

#include <bits/stdc++.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <poll.h>

#include "utils.h"

using namespace std;

#define MAX_BUFF sizeof(packet)
#define MAX_CONN_Q 16

// UDP sensor...
#define TOPIC_SIZE 50
#define TYPE_INDEX 50
#define DATA_INDEX 51

#define MIN_PORT_VALUE 1024
#define MAX_PORT_VALUE 65535

struct client {
	int fd;
	char id[11];
};

uint16_t check_input_port(const char *raw_input_port);
struct packet recv_sdata(int udpfd);
void manage_server(int tcpfd, int udpfd);


int main(int argc, char *argv[])
{
	cout << sizeof(packet);
	// TODO: disble Nagle!

	// Disable buffering
	setvbuf(stdout, NULL, _IONBF, BUFSIZ);

	// Check arguments
	DIE(argc != 2, "Usage: ./server <SERVER_PORT>");

	// Parse server port
	uint16_t server_port = check_input_port(argv[1]);

	// Create TCP server socket
	const int tcpfd = socket(AF_INET, SOCK_STREAM, 0);
	DIE(tcpfd == -1, "Error when creating TCP server socket!");

	// Create UDP server socket
	const int udpfd = socket(AF_INET, SOCK_DGRAM, 0);
	DIE(udpfd == -1, "Error when creating UDP server socket!");

	// Make server socket address reusable
	const int enable = 1;
	int rc = setsockopt(tcpfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
	DIE(rc == -1, "Error when setting reusable address!");

	rc = setsockopt(udpfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
	DIE(rc == -1, "Error when setting reusable address!");

	// Create server address structure
	struct sockaddr_in serv_addr;
	memset(&serv_addr, 0, sizeof(serv_addr));

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(server_port);

	// Bind server socket to address
	rc = bind(tcpfd, (const struct sockaddr *)&serv_addr, sizeof(serv_addr));
	DIE(rc == -1, "Server bind failed!");

	rc = bind(udpfd, (const struct sockaddr *)&serv_addr, sizeof(serv_addr));
	DIE(rc == -1, "Server bind failed!");

	// Listen for connections
	rc = listen(tcpfd, MAX_CONN_Q);
	DIE(rc == -1, "Server listen failed!");

	manage_server(tcpfd, udpfd);

	close(tcpfd);
	close(udpfd);

	return 0;
}

void manage_server(int tcpfd, int udpfd)
{
	vector<struct pollfd> pfds;
	pfds.push_back({ STDIN_FILENO, POLLIN, 0 });
	pfds.push_back({ tcpfd, POLLIN, 0 });
	pfds.push_back({ udpfd, POLLIN, 0 });

	while (1) {
		cout << "Active conn:" << pfds.size() << "\n";

		int poll_count = poll(pfds.data(), pfds.size(), -1);
		DIE(poll_count == -1, "Server poll error");

		for (int i = pfds.size() - 1; i >= 0; i--) {
			// Check if someone's ready to read
			if (!(pfds[i].revents & POLLIN))
				continue;

			// New tcp connection pending
			if (pfds[i].fd == tcpfd) {
				// Create client address structure
				struct sockaddr_in conn_addr;
				socklen_t caddr_len = sizeof(conn_addr);
				memset(&conn_addr, 0, caddr_len);

				// Accept connection from new client
				int connfd = accept(tcpfd, (struct sockaddr *)&conn_addr, &caddr_len);
				DIE(connfd == -1, "Connection failed!");				

				// Add to poll fd
				pfds.push_back({ connfd, POLLIN, 0 });

				// TODO: id client

				char con_adr_string[INET_ADDRSTRLEN];
				printf("New client <ID_CLIENT> connected from %s:%hu.\n", 
						inet_ntop(AF_INET, &conn_addr.sin_addr.s_addr, con_adr_string, INET_ADDRSTRLEN),
						ntohs(conn_addr.sin_port));

				continue;
			}

			// Input from stdin
			if (pfds[i].fd == STDIN_FILENO) {
				char buff[MAX_BUFF];
				bzero(buff, MAX_BUFF);

				fgets(buff, MAX_BUFF, stdin);

				if ((strncmp(buff, "exit", 4)) == 0) {
					printf("Server Exit...\n");

					for (struct pollfd pfd : pfds) {
						if (pfd.fd != tcpfd && pfd.fd != STDIN_FILENO)
							close(pfd.fd);
					}
					return;
				}
				continue;
			}

			// UDP message
			if (pfds[i].fd == udpfd) {
				struct packet sdata = recv_sdata(udpfd);

				// printf("Udp port: %hu\ntopic: %s\ntype: %hu\ndata: %s", htons(sdata.udp_port), sdata.topic, sdata.dtype, sdata.data);

				continue;
			}

			// Client:
			struct packet pack;
			memset(&pack, 0, sizeof(packet));

			int rc = recv_packet(pfds[i].fd, &pack);
			DIE(rc == -1, "Error when recieving message");

			if (rc == 0) {
				// Connection closed

				// TODO: id client
				printf("Client <ID_CLIENT> disconnected.\n");
				close(pfds[i].fd);
				pfds.erase(pfds.begin() + i);

				continue;
			}

			if (pack.type == CONNECT)
				cout << "Client ID: " << pack.sub.client_id;
		}
	}
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

struct packet recv_sdata(int udpfd)
{
	struct packet sdata;
	char buff[MAX_BUFF] = {0};

	struct sockaddr_in conn_addr;
	socklen_t caddr_len = sizeof(conn_addr);
	memset(&conn_addr, 0, caddr_len);

	ssize_t recv_bytes = recvfrom(udpfd, (char *)&buff, MAX_BUFF, 0, (struct sockaddr *)&conn_addr, &caddr_len);
	DIE(recv_bytes == -1, "Error when recieving message");

	sdata.type = SDATA;
	sdata.data.udp_ip = conn_addr.sin_addr.s_addr;
	sdata.data.udp_port = conn_addr.sin_port;
	sdata.data.len = recv_bytes - TOPIC_SIZE;
	memcpy(&sdata.data.topic, buff, TOPIC_SIZE);
	sdata.data.dtype = buff[TYPE_INDEX];
	memcpy(&sdata.data, &buff[DATA_INDEX], sdata.data.len);

	return sdata;
}
