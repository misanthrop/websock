#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <unistd.h>
#include "websock.hpp"

struct Socket
{
	int fd;
	operator int() const { return fd; }

	Socket() : fd(-1) {}
	Socket(int fd) : fd(fd) {}
	Socket(Socket&& o) : fd(o.fd) { o.fd = -1; }
	Socket(const Socket&) = delete;
	~Socket() { if(fd >= 0) close(fd); }

	Socket& operator=(int f) { if(fd >= 0) close(fd); fd = f; return *this; }
	Socket& operator=(Socket&& o) { if(fd >= 0) close(fd); fd = o.fd; o.fd = -1; return *this; }
	Socket& operator=(const Socket&) = delete;
};

const char* addrToStr(const sockaddr_in& addr)
{
	static char result[32];
	sprintf(result, "%s:%d", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
	return result;
}

int main()
{
	Socket serverSocket = socket(PF_INET, SOCK_STREAM, 0);
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

	printf("Server started %s\n", addrToStr(serverAddr));
	fflush(stdout);

	sockaddr_in clientAddr;
	socklen_t clientAddrLen = sizeof(clientAddr);
	Socket clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientAddrLen);
	if(clientSocket < 0)
	{
		perror("accept failed");
		return -1;
	}

	printf("[%s] connected\n", addrToStr(clientAddr));
	fflush(stdout);

	websock::Connection ws;
	ws.read = [&](char* data, size_t len) { return recv(clientSocket, data, len, 0); };
	ws.write = [&](const char* data, size_t len) { return send(clientSocket, data, len, 0); };

	ws.onMessage = [&](const websock::Message& msg)
	{
		printf("[%s] said: ", addrToStr(clientAddr));
		fwrite(msg.data, 1, msg.len, stdout);
		printf("\n");
		fflush(stdout);
		ws.send(msg.data, msg.len, msg.opcode); // echo
	};

	ws.onClose = [&](const websock::Message& msg)
	{
		printf("[%s] disconnected: ", addrToStr(clientAddr));
		fwrite(msg.data, 1, msg.len, stdout);
		printf("\n");
		fflush(stdout);
	};

	ws.onError = [&](websock::Error error)
	{
		fprintf(stderr, "[%s] error: %s\n", addrToStr(clientAddr), websock::ErrorText[error]);
		fflush(stderr);
	};

	while(ws.connected)
	{
		ws.process();
		ws.writePending();
	}

	return 0;
}
