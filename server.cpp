// Copyright (c) 2024 Horia-Valentin MOROIANU

#include <bits/stdc++.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <poll.h>
#include <string>

#include "utils.h"
#include "server.h"

using namespace std;

int main(int argc, char *argv[])
{
	// Disable buffering
	setvbuf(stdout, NULL, _IONBF, BUFSIZ);

	// Check arguments
	DIE(argc != 2, "Usage: ./server <SERVER_PORT>");

	// Parse server port
	uint16_t server_port = check_input_port(argv[1]);

	// Create TCP server socket
	const int tcpfd = socket(AF_INET, SOCK_STREAM, 0);
	DIE(tcpfd == -1, "Error when creating TCP server socket!");
	disable_nagle(tcpfd);

	// Create UDP server socket
	const int udpfd = socket(AF_INET, SOCK_DGRAM, 0);
	DIE(udpfd == -1, "Error when creating UDP server socket!");

	// Make server socket address reusable
	reusable_address(tcpfd);
	reusable_address(udpfd);

	// Create server address structure
	struct sockaddr_in serv_addr = { };
	socklen_t addr_len = sizeof(serv_addr);
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(server_port);

	// Bind server socket to address
	int rc = bind(tcpfd, (const struct sockaddr *)&serv_addr, addr_len);
	DIE(rc == -1, "Server bind failed!");

	rc = bind(udpfd, (const struct sockaddr *)&serv_addr, addr_len);
	DIE(rc == -1, "Server bind failed!");

	// Listen for connections
	rc = listen(tcpfd, MAX_CONN_QUEUE);
	DIE(rc == -1, "Server listen failed!");

	manage_server(tcpfd, udpfd);

	rc = close(tcpfd);
	DIE(rc == -1, "Error when closing file descriptor!");
	rc = close(udpfd);
	DIE(rc == -1, "Error when closing file descriptor!");

	return 0;
}

void manage_server(int tcpfd, int udpfd)
{
	// Key: client_id; Value: client_fd
	map<string, int> active_conn;
	// Key: topic; Value: client_id
	map<vector<string>, vector<string>> database;

	vector<struct pollfd> pfds;
	pfds.push_back({ STDIN_FILENO, POLLIN, 0 });
	pfds.push_back({ tcpfd, POLLIN, 0 });
	pfds.push_back({ udpfd, POLLIN, 0 });

	while (1) {
		int poll_count = poll(pfds.data(), pfds.size(), -1);
		DIE(poll_count == -1, "Server poll error!");

		for (int i = pfds.size() - 1; i >= 0; i--) {
			// Check if someone's ready to read
			if (!(pfds[i].revents & POLLIN))
				continue;

			// Input from stdin
			if (pfds[i].fd == STDIN_FILENO) {
				if (close_server(pfds))
					return;
				continue;
			}

			// New TCP connection pending
			if (pfds[i].fd == tcpfd) {
				connect_tcp_client(tcpfd, pfds, active_conn);
				continue;
			}

			// UDP message
			if (pfds[i].fd == udpfd) {
				struct packet pack = { };
				if (!recv_udp_data(udpfd, pack))
					continue;

				handle_udp(database, active_conn, pack);
				continue;
			}

			// Client:
			struct packet pack = { };
			int rc = recv_packet(pfds[i].fd, &pack);
			DIE(rc == -1, "Error when recieving message!");

			if (rc == 0) {
				// Connection closed
				string client_id = get_id(pfds[i].fd, active_conn);
				cout << "Client " << client_id << " disconnected.\n";
				close(pfds[i].fd);

				pfds.erase(pfds.begin() + i);
				active_conn.erase(client_id);
				continue;
			}

			if (pack.type == FOLLOW)
				// Client wants to subscribe/unsubscribe
				client_status_update(database, pack);
		}
	}
}

void connect_tcp_client(int tcpfd, vector<struct pollfd> &pfds,
						map<string, int> &active_conn)
{
	// Create client address structure
	struct sockaddr_in conn_addr;
	socklen_t caddr_len = sizeof(conn_addr);
	memset(&conn_addr, 0, caddr_len);

	// Accept connection from new client
	int connfd = accept(tcpfd, (struct sockaddr *)&conn_addr, &caddr_len);
	DIE(connfd == -1, "Connection failed!");
	disable_nagle(connfd);		

	// Wait for client id
	struct packet pack = { };
	int rc = recv_packet(connfd, &pack);
	DIE(rc == -1 || pack.type != CONNECT, "Connection failed!");

	// Check if already connected
	if (active_conn.find(pack.sub.client_id) != active_conn.end()) {
		cout << "Client " << pack.sub.client_id << " already connected.\n";
		close(connfd);
		return;
	}

	// Add to poll fd and active connections
	pfds.push_back({ connfd, POLLIN, 0 });
	active_conn.insert({ pack.sub.client_id, connfd});

	char con_adr_string[INET_ADDRSTRLEN];
	const char *ret = inet_ntop(AF_INET, &conn_addr.sin_addr.s_addr,
								con_adr_string, INET_ADDRSTRLEN);
	DIE(ret == NULL, "Invalid client IP!");

	cout << "New client " << pack.sub.client_id 
		 << " connected from " << con_adr_string 
		 << ":" << ntohs(conn_addr.sin_port) << ".\n";
}

void handle_udp(map<vector<string>, vector<string>> &database,
				map<string, int> &active_conn,
				struct packet &pack)
{
	vector<string> udptopic = split_topic(pack.data.topic);
	set<string> registered; // keep track of sent messages

	for (auto pair : database) {
		if (!topic_match(udptopic, pair.first))
			continue;

		for (auto client_id : pair.second) {
			if (active_conn.find(client_id) != active_conn.end() &&
				registered.find(client_id) == registered.end()) {
				// Active unnotified client
				int rc = send_packet(active_conn[client_id], &pack);
				DIE(rc == -1, "Error when sending message!");
				registered.insert(client_id);
			}
		}
	}
}

void client_status_update(map<vector<string>, vector<string>> &database,
						  struct packet &pack)
{
	vector<string> topic = split_topic(pack.sub.topic);

	// Get client fd
	vector<string>& clientsfd = database[topic];
	auto pos = find(clientsfd.begin(), clientsfd.end(), pack.sub.client_id);

	if (pack.sub.status == SUBSCRIBE) {
		if (pos == clientsfd.end())
			// Subscribe only once
			clientsfd.push_back(pack.sub.client_id);
	} else {
		if (pos != clientsfd.end())
			// Unsubscribe if prev subscribed
			clientsfd.erase(pos);
	}

	// Delete empty entries
	if (clientsfd.empty())
		database.erase(topic);
}

bool close_server(vector<struct pollfd> &pfds)
{
	string input;
	getline(cin, input);

	if (input == "exit") {
		for (size_t i = 3; i < pfds.size(); i++)
			close(pfds[i].fd);
		return true;
	}
	return false;
}

bool recv_udp_data(int udpfd, struct packet &pack)
{
	char buff[MAX_UDP_BUFF] = {0};

	// Create socket addr
	struct sockaddr_in udp_addr;
	socklen_t adr_len = sizeof(udp_addr);
	memset(&udp_addr, 0, adr_len);

	// Receive data
	ssize_t recv_bytes = recvfrom(udpfd, (char *)&buff, MAX_UDP_BUFF, 0, 
								  (struct sockaddr *)&udp_addr, &adr_len);
	DIE(recv_bytes == -1, "Error when receiving message!");

	// Parse data into packet
	pack.type = DATA;
	pack.data.udp_ip = udp_addr.sin_addr.s_addr;
	pack.data.udp_port = udp_addr.sin_port;
	memcpy(&pack.data.topic, buff, MAX_TOPIC_SIZE);

	// Check for input errors
	if (!check_payload(buff[TYPE_INDEX], buff[TYPE_INDEX + 1], recv_bytes) ||
		!check_topic(pack.data.topic))
		return false;

	pack.data.dtype = buff[TYPE_INDEX];
	memcpy(&pack.data.payload, &buff[DATA_INDEX], recv_bytes - MAX_TOPIC_SIZE);
	
	// Data received successfully
	return true;
}

string get_id(int fd, map<string, int> &active_conn)
{
	for (auto pair : active_conn) {
		if (pair.second == fd)
			return pair.first;
	}
	return NULL;
}

vector<string> split_topic(char *topic)
{
	stringstream topic_stream(topic);
	vector<string> words;
	string word;

	while (getline(topic_stream, word, '/'))
		words.push_back(word);
	return words;
}

bool topic_match(const vector<string> &udptopic, const vector<string> &tcptopic)
{
	size_t i = 0;
	size_t j = 0;

	while (i < tcptopic.size() && j < udptopic.size()) {
		if (tcptopic[i] == udptopic[j]) {
			i++;
			j++;
			continue;
		}

		if (tcptopic[i] == "+") {
			i++;
			j++;
			continue;
		}

		if (tcptopic[i] == "*") {
			if (i == tcptopic.size() - 1)
				// Ends in '*'
				return true;

			if (j == udptopic.size() - 1)
				// Couldn't match anything after '*'
				return false;

			// Skip one level
			i++;
			j++;

			// Find next match
			auto it = find(udptopic.begin() + j, udptopic.end(), tcptopic[i]);

			if (it == udptopic.end())
				// Couldn't match anything after '*'
				return false;

			// Skip to next level
			j = it - udptopic.begin();
			continue;
		}
		// No match found
		return false;
	}
	return (i == tcptopic.size() && j == udptopic.size());
}

bool check_payload(uint8_t type, uint8_t sign, ssize_t size)
{
	switch (type) {
		case INT:
			if (sign > 1)
				return false;
			if (size < 56)
				return false;
			return true;

		case SHORT_REAL:
			if (size < 53)
				return false;
			return true;

		case FLOAT:
			if (sign > 1)
				return false;
			if (size < 57)
				return false;
			return true;

		case STRING:
			if (size < 52)
				return false;
			return true;

		default:
			return false;
	}
	return false;
}
