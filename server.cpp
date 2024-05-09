// Copyright (c) 2024 Horia-Valentin MOROIANU

#include <bits/stdc++.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <poll.h>
#include <string>

#include "utils.h"

using namespace std;

#define MAX_BUFF 1600
#define MAX_CONN_Q 16

// TODO: fix this
// UDP sensor...
#define TOPIC_SIZE 50
#define TYPE_INDEX 50
#define DATA_INDEX 51

void manage_server(int tcpfd, int udpfd);
void recv_data(int udpfd, struct packet &pack);
string getid_by_fd(int fd, map<string, int> &active_conn);
bool close_server(vector<struct pollfd> &pfds);
void connect_tcp_client(int tcpfd, vector<struct pollfd> &pfds,
						map<string, int> &active_conn);
void split_topic(char *topic, vector<string> &words);
bool topic_match(const vector<string> &udptopic, const vector<string> &tcptopic);

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
	rc = listen(tcpfd, MAX_CONN_Q);
	DIE(rc == -1, "Server listen failed!");

	manage_server(tcpfd, udpfd);

	rc = close(tcpfd);
	DIE(rc == -1, "Error when closing file descriptor");
	rc = close(udpfd);
	DIE(rc == -1, "Error when closing file descriptor");

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
		DIE(poll_count == -1, "Server poll error");

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
				struct packet pack;
				memset((void *)&pack, 0, sizeof(packet));

				recv_data(udpfd, pack);
				vector<string> udptopic;
				split_topic(pack.data.topic, udptopic);

				for (auto pair : database) {
					if (topic_match(udptopic, pair.first)) {
						for (auto client_id : pair.second) {
							if (active_conn.find(client_id) != active_conn.end()) {
								int rc = send_packet(active_conn[client_id], &pack);
								DIE(rc == -1, "Error when sending message");
							}
						}
					}
				}
				continue;
			}

			// Client:
			struct packet pack = { };
			int rc = recv_packet(pfds[i].fd, &pack);
			DIE(rc == -1, "Error when recieving message");

			if (rc == 0) {
				// Connection closed
				string client_id = getid_by_fd(pfds[i].fd, active_conn);
				cout << "Client " << client_id << " disconnected.\n";
				close(pfds[i].fd);

				pfds.erase(pfds.begin() + i);
				active_conn.erase(client_id);
				continue;
			}

			if (pack.type == FOLLOW) {
				vector<string> topic;
				split_topic(pack.sub.topic, topic);
				vector<string>& clientsfd = database[topic];
				auto pos = find(clientsfd.begin(), clientsfd.end(), pack.sub.client_id);

				// TODO: unsubscribe for wildcards?
				if (pack.sub.status == SUBSCRIBE) {
					if (pos == clientsfd.end())
						clientsfd.push_back(pack.sub.client_id);
				} else {
					if (pos != clientsfd.end())
						clientsfd.erase(pos);
				}
				if (clientsfd.empty())
					database.erase(topic);
			}
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

	// TODO: check already connected
	struct packet pack = { };
	int rc = recv_packet(connfd, &pack);
	DIE(rc == -1 || pack.type != CONNECT, "Connection failed!");

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

void recv_data(int udpfd, struct packet &pack)
{
	char buff[MAX_BUFF] = {0};

	struct sockaddr_in udp_addr;
	socklen_t adr_len = sizeof(udp_addr);
	memset(&udp_addr, 0, adr_len);

	ssize_t recv_bytes = recvfrom(udpfd, (char *)&buff, MAX_BUFF, 0, 
								  (struct sockaddr *)&udp_addr, &adr_len);
	DIE(recv_bytes == -1, "Error when recieving message");

	// TODO: check packet format
	pack.type = DATA;
	pack.data.udp_ip = udp_addr.sin_addr.s_addr;
	pack.data.udp_port = udp_addr.sin_port;
	pack.data.len = recv_bytes - TOPIC_SIZE;
	memcpy(&pack.data.topic, buff, TOPIC_SIZE);
	pack.data.dtype = buff[TYPE_INDEX];
	memcpy(&pack.data.payload, &buff[DATA_INDEX], pack.data.len);
	pack.data.len = htons(pack.data.len);
}

string getid_by_fd(int fd, map<string, int> &active_conn)
{
	for (auto pair : active_conn) {
		if (pair.second == fd)
			return pair.first;
	}
	return NULL;
}

void split_topic(char *topic, vector<string> &words)
{
	stringstream topic_stream(topic);
	string word;

	while (getline(topic_stream, word, '/'))
		words.push_back(word);
}

bool topic_match(const vector<string> &udptopic, const vector<string> &tcptopic)
{
	if (udptopic.size() != tcptopic.size())
		return false;
	
	for (size_t i = 0; i < udptopic.size(); i++) {
		if (udptopic[i] != tcptopic[i])
			return false;
	}
	return true;
}
