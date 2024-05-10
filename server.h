// Copyright (c) 2024 Horia-Valentin MOROIANU

#ifndef _SERVER_H
#define _SERVER_H

#include <bits/stdc++.h>
using namespace std;

#define MAX_UDP_BUFF 1600
#define MAX_CONN_QUEUE 16

#define TYPE_INDEX 50
#define DATA_INDEX 51

void manage_server(int tcpfd, int udpfd);
bool close_server(vector<struct pollfd> &pfds);

bool recv_udp_data(int udpfd, struct packet &pack);
void handle_udp(map<vector<string>, vector<string>> &database,
				map<string, int> &active_conn,
				struct packet &pack);

string get_id(int fd, map<string, int> &active_conn);
vector<string> split_topic(char *topic);
bool check_payload(uint8_t type, uint8_t sign, ssize_t size);
bool topic_match(const vector<string> &udptopic,
                 const vector<string> &tcptopic);

void connect_tcp_client(int tcpfd, vector<struct pollfd> &pfds,
						map<string, int> &active_conn);

void client_status_update(map<vector<string>, vector<string>> &database,
						  struct packet &pack);

#endif
