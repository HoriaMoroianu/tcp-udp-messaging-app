// Copyright (c) 2024 Horia-Valentin MOROIANU

#include <bits/stdc++.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <poll.h>
#include <string>

#include "utils.h"

using namespace std;

#define MAX_BUFF sizeof(packet)
#define MAX_CONN_Q 16

// UDP sensor...
#define TOPIC_SIZE 50
#define TYPE_INDEX 50
#define DATA_INDEX 51

void manage_server(int tcpfd, int udpfd);
struct packet recv_sdata(int udpfd);
string getid_by_fd(int fd, map<string, int> &active_conn);
int read_from_stdin(vector<struct pollfd> &pfds);
void connect_tcp_client(int tcpfd, vector<struct pollfd> &pfds,
						map<string, int> &active_conn);
void split_topic(char *topic, vector<string> &words);

int main(int argc, char *argv[])
{
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
	// Key: client_id; Value: client_fd
	map<string, int> active_conn;

	// Key: topic; Value: client_id
	map<vector<string>, vector<string>> database;

	vector<struct pollfd> pfds;
	pfds.push_back({ STDIN_FILENO, POLLIN, 0 });
	pfds.push_back({ tcpfd, POLLIN, 0 });
	pfds.push_back({ udpfd, POLLIN, 0 });

	while (1) {
		// TODO: remove
		cout << "\nActive conn:" << pfds.size() << "\n";
		cout << "Subs:" << database.size() << "\n";

		// TODO: remove after working
		// cout << "\n";
		// for (auto pair : database) {
		// 	cout << "Topic: ";
		// 	for (auto word : pair.first)
		// 		cout << word << " ";
		// 	cout << "\nFds: ";
		// 	for (auto fd : pair.second)
		// 		cout << fd << " ";
		// 	cout << "\n";
		// }
		// cout << "\n";

		int poll_count = poll(pfds.data(), pfds.size(), -1);
		DIE(poll_count == -1, "Server poll error");

		for (int i = pfds.size() - 1; i >= 0; i--) {
			// Check if someone's ready to read
			if (!(pfds[i].revents & POLLIN))
				continue;

			// Input from stdin
			if (pfds[i].fd == STDIN_FILENO) {
				if (read_from_stdin(pfds))
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
				struct packet sdata = recv_sdata(udpfd);
				(void)sdata;
				// TODO: sent to all clients
				continue;
			}

			// Client:
			struct packet pack;
			memset((void *)&pack, 0, sizeof(packet));

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

	// TODO: check already connected
	struct packet pack;
	memset((void *)&pack, 0, sizeof(packet));
	int rc = recv_packet(connfd, &pack);
	DIE(rc <= 0 || pack.type != CONNECT, "Connection failed!");

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

int read_from_stdin(vector<struct pollfd> &pfds)
{
	char buff[MAX_BUFF] = {0};
	fgets(buff, MAX_BUFF, stdin);

	if (!(strncmp(buff, "exit", 4))) {
		for (size_t i = 3; i < pfds.size(); i++)
			close(pfds[i].fd);
		return 1;
	}
	return 0;
}

struct packet recv_sdata(int udpfd)
{
	struct packet sdata;
	char buff[MAX_BUFF] = {0};

	struct sockaddr_in conn_addr;
	socklen_t caddr_len = sizeof(conn_addr);
	memset(&conn_addr, 0, caddr_len);

	ssize_t recv_bytes = recvfrom(udpfd, (char *)&buff, MAX_BUFF, 0, 
								  (struct sockaddr *)&conn_addr, &caddr_len);
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
