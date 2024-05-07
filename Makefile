# Copyright (c) 2024 Horia-Valentin MOROIANU

.PHONY: server subscriber clean

server: server.cpp
	g++ -std=c++17 -Wall -Wextra -g server.cpp utils.cpp -o server

subscriber: tcp_client.cpp
	g++ -std=c++17 -Wall -Wextra -g tcp_client.cpp utils.cpp -o subscriber

clean:
	rm -f server subscriber

run_server: server
	./server 1331

run_subscriber: subscriber
	./subscriber 123 127.0.0.1 1331
