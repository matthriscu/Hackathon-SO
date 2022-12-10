#include <dlfcn.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/ip.h>

#include "utils.h"
#include "ipc.h"
#include "server.h"

#ifndef OUTPUTFILE_TEMPLATE
#define OUTPUTFILE_TEMPLATE "../checker/output/out-XXXXXX"
#endif

static int lib_prehooks(struct lib *lib)
{
	lib = lib;
	return 0;
}

void print_error(const char *name, const char *func, const char *params) {
	printf("Error: %s ", name);
	if (strlen(func)) {
		printf("%s ", func);

		if (strlen(params)) {
			printf("%s ", params);
		}
	}

	puts("could not be executed.");
	fflush(stdout);
}

static int lib_load(struct lib *lib)
{
	int ret = dup2(open(lib->outputfile, O_WRONLY, NULL), STDOUT_FILENO);
	DIE(ret == -1, "dup2");

	lib->handle = dlopen(lib->libname, RTLD_LAZY);
	if (lib->handle == 0) {
		print_error(lib->libname, lib->funcname, lib->filename);
		return -1;
	}

	DIE(lib->handle == NULL, "dlopen");

	if (strlen(lib->filename) == 0) {
		lib->run = (lambda_func_t)dlsym(lib->handle, "run");
		if (lib->run == 0) {
			print_error(lib->libname, lib->funcname, lib->filename);
			return -1;
		}
	} else {
		lib->p_run = (lambda_param_func_t)dlsym(lib->handle, lib->funcname);
		if (lib->p_run == 0) {
			print_error(lib->libname, lib->funcname, lib->filename);
			return -1;
		}
	}

	return 0;
}

static int lib_execute(struct lib *lib)
{
	lib->run ? lib->run() : lib->p_run(lib->filename);
	fflush(stdout);
	return 0;
}

static int lib_close(struct lib *lib)
{
	int ret = dlclose(lib->handle);
	DIE(ret == -1, "dlclose");

	free(lib->filename);
	free(lib->funcname);
	free(lib->libname);
	free(lib->outputfile);

	return 0;
}

static int lib_posthooks(struct lib *lib)
{
	lib = lib;
	return 0;
}

static int lib_run(struct lib *lib)
{
	int err;

	err = lib_prehooks(lib);
	if (err)
		return err;

	err = lib_load(lib);
	if (err)
		return err;

	err = lib_execute(lib);
	if (err)
		return err;

	err = lib_close(lib);
	if (err)
		return err;

	return lib_posthooks(lib);
}

static int parse_command(const char *buf, char *name, char *func, char *params)
{
	return sscanf(buf, "%s %s %s", name, func, params);
}

int main(void)
{
	int ret;
	struct lib lib;

	/* TODO - Implement server connection */
	int fd = create_socket(AF_UNIX, SOCK_STREAM, 0);
	DIE(fd == -1, "socket");

	struct sockaddr_un sockaddr;
	memset(&sockaddr, 0, sizeof(sockaddr));

	sockaddr.sun_family = AF_UNIX;
	strcpy(sockaddr.sun_path, SOCKET_NAME);

	unlink(SOCKET_NAME);

	ret = bind(fd, (struct sockaddr *)&sockaddr, sizeof(sockaddr));
	DIE(ret == -1, "bind");

	ret = listen(fd, 10);
	DIE(ret == -1, "listen");

	char buf[BUFSIZE];

	while (1) {
		int client_fd = accept(fd, (struct sockaddr *)&sockaddr, &(socklen_t){sizeof(sockaddr)});
		DIE(client_fd == -1, "accept");

		memset(buf, 0, sizeof(buf));

		ret = recv_socket(client_fd, buf, sizeof(buf));
		DIE(ret == -1, "recv_socket");

		char name[BUFSIZE] = {},
			 func[BUFSIZE] = {},
			 params[BUFSIZE] = {},
			 output_filename[BUFSIZE] = {};

		ret = parse_command(buf, name, func, params);
		DIE(ret <= 0, "parse_command");

		memset(&lib, 0, sizeof(lib));

		strcpy(output_filename, OUTPUTFILE_TEMPLATE);

		int output_fd = mkstemp(output_filename);
		close(output_fd);

		lib.outputfile = strdup(output_filename);
		lib.libname = strdup(name);
		lib.funcname = strdup(func);
		lib.filename = strdup(params);

		lib_run(&lib);

		ret = send_socket(client_fd, output_filename, strlen(output_filename));
		DIE(ret == -1, "send_socket");

		close_socket(client_fd);
	}

	close_socket(fd);
	DIE(ret == -1, "close");

	return 0;
}
