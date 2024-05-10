// Copyright (c) 2024 Horia-Valentin MOROIANU

#include <bits/stdc++.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <poll.h>

#include "utils.h"
#include "tcp_client.h"

using namespace std;

int main(int argc, char *argv[])
{
	// Disable buffering
	setvbuf(stdout, NULL, _IONBF, BUFSIZ);

	DIE(argc != 4, "Usage: ./subscriber <CLIENT_ID> <SERVER_IP> <SERVER_PORT>");
	DIE(strlen(argv[1]) > 10, "Id should be at most 10 characters long!");

	// Parse server ip
	uint32_t server_ip;
	int rc = inet_pton(AF_INET, argv[2], &server_ip);
	DIE(rc != 1, "Invalid server ip!");

	// Parse server port
	uint16_t server_port = check_input_port(argv[3]);

	// Create server socket
	const int serverfd = socket(AF_INET, SOCK_STREAM, 0);
	DIE(serverfd == -1, "Error when creating server socket!");
	disable_nagle(serverfd);

	// Create server address structure
	struct sockaddr_in serv_addr = { };
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(server_port);
	serv_addr.sin_addr.s_addr = server_ip;

	// Connect to server
	rc = connect(serverfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
	DIE(rc == -1, "Failed to connect to server!");

	// Send client id to server
	struct packet pack = { };
	pack.type = CONNECT;
	memcpy(pack.sub.client_id, argv[1], strlen(argv[1]));
	rc = send_packet(serverfd, &pack);
	DIE(rc == -1, "Failed to connect to server!");

	manage_connection(serverfd, argv[1]);

	rc = close(serverfd);
	DIE(rc == -1, "Error when closing file descriptor!");

	return 0;
}

void manage_connection(int serverfd, char *myid)
{
	struct pollfd pfds[2];
	pfds[0] = { STDIN_FILENO, POLLIN, 0 };
	pfds[1] = { serverfd, POLLIN, 0 };

	while (1) {
		int poll_count = poll(pfds, 2, -1);
		DIE(poll_count == -1, "Subscriber poll error!");

		// From server
		if (pfds[1].revents & POLLIN) {
			struct packet pack = { };
			int rc = recv_packet(serverfd, &pack);
			DIE(rc == -1, "Error when receiving message!");

			// Server disconected
			if (rc == 0)
				return;

			if (pack.type == DATA)
				unpack(pack);
		}

		// From stdin
		if (pfds[0].revents & POLLIN) {
			string command;
			cin >> command;

			if (command == "exit")
				return;
			
			if (command == "subscribe") {
				subscribe_command(serverfd, myid);
				continue;
			}

			if (command == "unsubscribe")
				unsubscribe_command(serverfd, myid);
		}
	}
}

void subscribe_command(int serverfd, char *myid)
{
	struct packet pack = { };

	pack.type = FOLLOW;
	memcpy(pack.sub.client_id, myid, strlen(myid));
	pack.sub.status = SUBSCRIBE;

	cin >> pack.sub.topic;
	if (!check_topic(pack.sub.topic)) {
		cerr << "Invalid topic!\n";
		return;
	}

	int rc = send_packet(serverfd, &pack);
	DIE(rc == -1, "Send failed!");
	cout << "Subscribed to topic " << pack.sub.topic << "\n";
}

void unsubscribe_command(int serverfd, char *myid)
{
	struct packet pack = { };

	pack.type = FOLLOW;
	memcpy(pack.sub.client_id, myid, strlen(myid));
	pack.sub.status = UNSUBSCRIBE;

	cin >> pack.sub.topic;
	if (!check_topic(pack.sub.topic)) {
		cerr << "Invalid topic!\n";
		return;
	}

	int rc = send_packet(serverfd, &pack);
	DIE(rc == -1, "Send failed!");
	cout << "Unsubscribed from topic " << pack.sub.topic << "\n";
}

string get_data_type(uint8_t type)
{
	switch (type) {
		case INT:
			return "INT";
		case SHORT_REAL:
			return "SHORT_REAL";
		case FLOAT:
			return "FLOAT";
		case STRING:
			return "STRING";
		default:
			return "";
	}
}

string parse_payload(uint8_t type, char *payload)
{
	string output = "";
	if (type == INT) {
		uint32_t val;
		memcpy(&val, &payload[1], sizeof(uint32_t));

		if (payload[0] == 1 && val != 0)
			output.append("-");

		return output.append(to_string(ntohl(val)));
	}

	if (type == SHORT_REAL) {
		uint16_t val;
		memcpy(&val, &payload[0], sizeof(uint16_t));

		stringstream temp;
		temp << fixed << setprecision(2) << (float)ntohs(val) / 100;
		return output.append(temp.str());
	}

	if (type == FLOAT) {
		uint32_t val;
		memcpy(&val, &payload[1], sizeof(uint32_t));
		int prec = (uint8_t)payload[5];

		stringstream temp;
		temp << fixed << setprecision(prec) 
			 << (double)ntohl(val) / pow(10, prec);

		if (payload[0] == 1 && val != 0)
			output.append("-");

		return output.append(temp.str());
	}
	// type == STRING
	return payload;
}

void unpack(struct packet &pack)
{
	char udp_ip[INET_ADDRSTRLEN];
	const char *ret = inet_ntop(AF_INET, &pack.data.udp_ip, udp_ip,
								INET_ADDRSTRLEN);
	DIE(ret == NULL, "Invalid UDP IP!");

	cout << udp_ip << ":" << ntohs(pack.data.udp_port) << " - " 
		 << pack.data.topic << " - " << get_data_type(pack.data.dtype) << " - " 
		 << parse_payload(pack.data.dtype, pack.data.payload) << "\n";
}
