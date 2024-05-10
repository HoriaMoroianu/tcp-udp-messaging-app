# Copyright (c) 2024 Horia-Valentin MOROIANU

.PHONY: server subscriber clean

server: server.cpp utils.cpp
	g++ -std=c++17 -Wall -Wextra server.cpp utils.cpp -o server

subscriber: tcp_client.cpp utils.cpp
	g++ -std=c++17 -Wall -Wextra tcp_client.cpp utils.cpp -o subscriber

clean:
	rm -f server subscriber
