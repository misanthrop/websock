#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include "websock.h"
#include "buffer.h"

typedef enum websocket_state
{
	websocket_expect_handshake,
	websocket_ok,
	websocket_closed
} websocket_state;

//void handle_message(websocket_protocol* wsp, const char* data, size_t len, uint8_t opcode)
//{
//	fwrite(data, 1, len, stdout);
//	putc('\n', stdout);
//	fflush(stdout);
//	if(opcode != websocket_ping && opcode != websocket_pong && opcode != websocket_close)
//		wsp->out.last = websocket_write_message(wsp->out.last, data, len, websocket_text|websocket_final);
//}

ssize_t buffer_recv(int sock, buffer* in)
{
	ssize_t available = recv(sock, in->last, buffer_space_left(in), 0);
	if(available > 0)
		buffer_write(in, 0, available);
	return available;
}

ssize_t buffer_send(int sock, buffer* out)
{
	ssize_t sent = send(sock, out->cur, buffer_pending_len(out), 0);
	if(sent > 0)
	{
		buffer_skip(out, sent);
		buffer_shrink(out);
	}
	return sent;
}

int main()
{
	int server_socket = socket(PF_INET, SOCK_STREAM, 0);
	if(server_socket < 0)
	{
		perror("socket error");
		return -1;
	}

	struct sockaddr_in server_addr;
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(8080);
	server_addr.sin_addr.s_addr = INADDR_ANY;

	if(bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) != 0)
	{
		perror("bind failed");
		goto close_server;
	}

	if(listen(server_socket, 32) < 0)
	{
		perror("listen error");
		goto close_server;
	}

	printf("Server started %s\n", inet_ntoa(server_addr.sin_addr));
	fflush(stdout);

	struct sockaddr_in client_addr;
	socklen_t client_addr_len = sizeof(client_addr);
	int client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_addr_len);
	if(client_socket < 0)
	{
		perror("accept failed");
		goto close_server;
	}

	printf("client %s connected\n", inet_ntoa(client_addr.sin_addr));
	fflush(stdout);

	buffer in, out;
	buffer_create(&in, 2048);
	buffer_create(&out, 2048);

	websocket_state state;
	while(state < websocket_closed)
	{
		buffer_shrink(&in);
		if(!buffer_space_left(&in))
			break;
		if(buffer_recv(client_socket, &in) <= 0)
		{
			perror("connection closed");
			goto destroy_wsp;
		}
		switch(state)
		{
			case websocket_expect_handshake:
			{
				websock_handshake hs;
				size_t n = websocket_parse_handshake(buffer_pending(&in), buffer_pending_len(&in), &hs);
				if(n)
				{
					fwrite(buffer_pending(&in), 1, n, stdout);
					fflush(stdout);
					buffer_skip(&in, n);
					out.last = websock_accept_handshake(out.last, hs.sec_key, hs.sec_key_len);
					fwrite(buffer_data(&out), 1, buffer_data_len(&out), stdout);
					fflush(stdout);
					state = websocket_ok;
				}
				break;
			}
			case websocket_ok:
			{
				websock_message msg;
				size_t n = websocket_decode_message(buffer_pending(&in), buffer_pending_len(&in), &msg);
				if(n)
				{
					fwrite(msg.data, 1, msg.len, stdout);
					printf(" [%zu]\n", msg.len);
					fflush(stdout);
					buffer_skip(&in, n);
					out.last = websocket_write_message(out.last, msg.data, msg.len, websock_opcode_text);
				}
				break;
			}
			default:
				goto destroy_wsp;
		}

		if(buffer_pending_len(&out))
		{
			if(buffer_send(client_socket, &out) <= 0)
			{
				perror("send failed");
				goto destroy_wsp;
			}
		}
	}

	if(state != websocket_ok)
	{
		perror("handshake failed");
		goto destroy_wsp;
	}

destroy_wsp:
	buffer_destroy(&in);
	buffer_destroy(&out);
close_client:
	close(client_socket);
close_server:
	close(server_socket);
	return 0;
}
