# Copyright (c) 2024 Horia-Valentin MOROIANU

.PHONY: server subscriber clean

server:
	gcc -o server server.c -Wall -Wextra

subscriber:
	gcc -o subscriber subscriber.c -Wall -Wextra

clean:
	rm -f server subscriber