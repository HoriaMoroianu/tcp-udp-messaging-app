// Copyright (c) 2024 Horia-Valentin MOROIANU

#ifndef _TCP_CLIENT_H
#define _TCP_CLIENT_H

void manage_connection(int serverfd, char *myid);
void subscribe_command(int serverfd, char *myid);
void unsubscribe_command(int serverfd, char *myid);
void unpack(struct packet &pack);

#endif
