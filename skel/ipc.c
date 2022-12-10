#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <netinet/ip.h>

#include "utils.h"
#include "ipc.h"

int create_socket()
{
	int ret = socket(AF_UNIX, SOCK_STREAM, 0);
	if (ret == -1)
		printf("Socket %d\n", ret);
	return ret;
}

int connect_socket(int fd)
{
	struct sockaddr_un sockaddr;
	memset(&sockaddr, 0, sizeof(sockaddr));

	sockaddr.sun_family = AF_UNIX;
	strcpy(sockaddr.sun_path, SOCKET_NAME);

	int ret = connect(fd, (struct sockaddr *)&sockaddr, sizeof(sockaddr));
	if (ret == -1)
		printf("Connect %d\n", ret);
	return ret;
}

ssize_t send_socket(int fd, const char *buf, size_t len)
{
	int ret = send(fd, buf, len, 0);
	if (ret == -1)
		printf("Send %d\n", ret);
	return ret;
}

ssize_t recv_socket(int fd, char *buf, size_t len)
{
	int ret = recv(fd, buf, len, 0);
	if (ret == -1)
		printf("Recv %d\n", ret);
	return ret;
}

void close_socket(int fd)
{
	close(fd);
}

