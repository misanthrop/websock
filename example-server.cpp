#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include "websock.hpp"

struct Socket
{
	int fd;
	explicit Socket(int fd) : fd(fd) {}
	Socket(Socket&& o) : fd(o.fd) { o.fd = -1; }
	Socket(const Socket&) = delete;
	~Socket() { if(fd >= 0) close(fd); }
	operator int() const { return fd; }
};

int main()
{
	Socket serverSocket(socket(PF_INET, SOCK_STREAM, 0));
	if(serverSocket < 0)
	{
		perror("socket error");
		return -1;
	}

	sockaddr_in serverAddr;
	memset(&serverAddr, 0, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(8080);
	serverAddr.sin_addr.s_addr = INADDR_ANY;

	if(bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) != 0)
	{
		perror("bind failed");
		return -1;
	}

	if(listen(serverSocket, 32) < 0)
	{
		perror("listen error");
		return -1;
	}

	printf("Server started %s\n", inet_ntoa(serverAddr.sin_addr));
	fflush(stdout);

	sockaddr_in clientAddr;
	socklen_t clientAddrLen = sizeof(clientAddr);
	Socket clientSocket(accept(serverSocket, (struct sockaddr*)&clientAddr, &clientAddrLen));
	if(clientSocket < 0)
	{
		perror("accept failed");
		return -1;
	}

	printf("client %s connected\n", inet_ntoa(clientAddr.sin_addr));
	fflush(stdout);

	websock::Connection ws;
	ws.read = [&](char* data, size_t len) { return recv(clientSocket, data, len, 0); };
	ws.write = [&](const char* data, size_t len) { return send(clientSocket, data, len, 0); };

	ws.onMessage = [&](const websock::Message& msg)
	{
		fwrite(msg.data, 1, msg.len, stdout);
		printf("\n");
		fflush(stdout);
		ws.send(msg.data, msg.len, msg.opcode);
	};

	ws.onClose = [&](const websock::Message&)
	{
		printf("client %s disconnected\n", inet_ntoa(clientAddr.sin_addr));
		fflush(stdout);
	};

	ws.onError = [&](websock::Error error)
	{
		fprintf(stderr, "client %s error: %s", inet_ntoa(clientAddr.sin_addr), websock::ErrorText[error]);
		fflush(stderr);
	};

	while(ws.connected)
	{
		ws.process();
		ws.writePending();
	}

	return 0;
}
