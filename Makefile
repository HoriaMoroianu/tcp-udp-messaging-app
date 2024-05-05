# Copyright (c) 2024 Horia-Valentin MOROIANU

.PHONY: server subscriber clean

server: server.c
	gcc -o server server.c -Wall -Wextra

subscriber: tcp_client.c
	gcc -o subscriber tcp_client.c -Wall -Wextra

clean:
	rm -f server subscriber

make run_server: server
	./server 1331

make run_subscriber: subscriber
	./subscriber 123 127.0.0.1 1331
