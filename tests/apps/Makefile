BINS = send_server send_client \
       msg_client  msg_server

# server_select client_select client_simple server_simple server_poll\
#	server_poll_multi server_close client_sbr server_sbr fcntl_ex server_read \
#	client_write  test_restore
CC = gcc 

CFLAGS = -Wall -g 
CFLAGS += -fno-common -Wunused -Wmissing-prototypes -Wmissing-declarations -Wpointer-arith -Wchar-subscripts -Wcomment
CFLAGS += -Wformat -Wwrite-strings -Wno-unused-function -Wno-unused-parameter -Wno-strict-aliasing -Wno-format-nonliteral

#CFLAGS += -Werror # This cannot be activated yet

all : $(BINS)

send_server : send_server.c
	gcc $(CFLAGS) -o send_server send_server.c
send_client : send_client.c
	gcc $(CFLAGS) -o send_client send_client.c
msg_server : msg_server.c
	gcc $(CFLAGS) -o msg_server msg_server.c
msg_client : msg_client.c
	gcc $(CFLAGS) -o msg_client msg_client.c





fcntl_ex: fcntl_ex.c
	gcc $(CFLAGS) -o fcntl_ex fcntl_ex.c

server_select: server_select.c
	gcc $(CFLAGS) -o server_select server_select.c

server_read : server_read.c
	gcc $(CFLAGS) -o server_read server_read.c

server_simple: serveur_simple.c
	gcc $(CFLAGS) -o server_simple serveur_simple.c

server_close: server_close.c
	gcc $(CFLAGS) -o server_close server_close.c

server_sbr: server_sbr.c
	gcc $(CFLAGS) -o server_sbr server_sbr.c

server_poll_multi: server_poll_multi.c
	gcc $(CFLAGS) -o server_poll_multi server_poll_multi.c

server_poll: server_poll.c
	gcc $(CFLAGS) -o server_poll server_poll.c

client_select: client_select.c
	gcc $(CFLAGS) -o client_select client_select.c

client_simple: client_simple.c
	gcc $(CFLAGS) -o client_simple client_simple.c

client_write : client_write.c
	gcc $(CFLAGS) -o client_write client_write.c

client_sbr : client_sbr.c
	gcc $(CFLAGS) -o client_sbr client_sbr.c

test_restore: test_restore.c
	gcc $(CFLAGS) -o test_restore test_restore.c

clean:
	rm -f $(BINS) *~
