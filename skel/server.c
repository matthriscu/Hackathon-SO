#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/ip.h>
#include <sys/wait.h>
#include <setjmp.h>

#include "utils.h"
#include "ipc.h"
#include "server.h"

#define TIMEOUT 60

#ifndef OUTPUTFILE_TEMPLATE
#define OUTPUTFILE_TEMPLATE "../checker/output/out-XXXXXX"
#endif

static void sigchld_handler(int signum) {
	signum = signum;

	int status;
	wait(&status);
}

// This is our attempt to have a controlled exit from the server when
// receiving SIGINT, leaving the children as orphan processes until they
// finish their current query. Unfortunately some tests fail with this
// handler enabled to we decided to leave it off.
// static void sigint_handler(int signum) {
// 	signum = signum;
// 	exit(0);
// }

static void print_error(const char *name, const char *func, const char *params) {
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

static int lib_prehooks(struct lib *lib)
{
	lib = lib;
	return 0;
}

static int lib_load(struct lib *lib) {
	// Open this library's output file
	int output_fd = open(lib->outputfile, O_WRONLY, NULL);
	if (output_fd == -1) {
		print_error(lib->libname, lib->funcname, lib->filename);
		return -1;
	}

	// Redirect stdout to the output file
	int ret = dup2(output_fd, STDOUT_FILENO);
	if (ret == -1) {
		print_error(lib->libname, lib->funcname, lib->filename);
		return -1;
	}
	
	lib->handle = dlopen(lib->libname, RTLD_LAZY);
	if (lib->handle == NULL) {
		print_error(lib->libname, lib->funcname, lib->filename);
		return -1;
	}

	if (strlen(lib->filename) == 0) {
		lib->run = (lambda_func_t)dlsym(lib->handle, strlen(lib->funcname) == 0 ? "run" : lib->funcname);

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
	pid_t pid = fork();

	switch(pid) {
	case 0:
		// Run the function in the child process to avoid killing the parent
		// in case anything goes wrong.
		lib->run ? lib->run() : lib->p_run(lib->filename);
		break;
	case -1:
		print_error(lib->libname, lib->funcname, lib->filename);
		break;
	default: {
		int status;
		
		// Kill the child after a timeout
		sleep(TIMEOUT);
		if (kill(pid, SIGINT) == 0) {
			print_error(lib->libname, lib->funcname, lib->filename);
		}

		wait(&status);
		break;
	}
	}

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

static void handle_client(int client_fd) {
	char buf[BUFSIZE] = {};

	while (recv_socket(client_fd, buf, sizeof(buf)) > 0) {
		if (strncmp(buf, "exit", 4) == 0) {
			break;
		}

		char name[BUFSIZE] = {},
			 func[BUFSIZE] = {},
			 params[BUFSIZE] = {},
			 output_filename[BUFSIZE] = {};

		parse_command(buf, name, func, params);

		struct lib lib;
		memset(&lib, 0, sizeof(lib));

		strcpy(output_filename, OUTPUTFILE_TEMPLATE);

		int output_fd = mkstemp(output_filename);
		close(output_fd);

		lib.outputfile = strdup(output_filename);
		lib.libname = strdup(name);
		lib.funcname = strdup(func);
		lib.filename = strdup(params);

		lib_run(&lib);

		send_socket(client_fd, output_filename, strlen(output_filename));
	}

	close_socket(client_fd);
}

int main(void)
{
	signal(SIGCHLD, sigchld_handler);
	// signal(SIGINT, sigint_handler);

	int ret;

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

	while(1) {
		int client_fd = accept(fd, (struct sockaddr *)&sockaddr, &(socklen_t){sizeof(sockaddr)});

		pid_t pid = fork();
		
		switch(pid) {
			case 0: {
				handle_client(client_fd);
				break;
			}
			case -1:
				DIE(1, "fork");
				break;
			default: {
				break;
			}
		}
	}

	close_socket(fd);

	return 0;
}
