Copyright 2022 Matei Hriscu (matei.hriscu@stud.acs.upb.ro),
			   Maria Sfiraiala (maria.sfiraiala@stud.acs.upb.ro)

GitHub Repo: https://github.com/matthriscu/Hackathon-SO

# Lambda Function Loader - SO Hackaton

## Server Implementation

- Implemented the inter-process-communication using UNIX sockets (`ipc.c`).

- The server function (`main.c`) creates multiple sockets: one that handles the
overall connection, and the others that serve the clients' needs.

- Requests are processed as long as there still is an ongoing connection,
meaning that we parse the given command, construct the out file, populate the
library struct members and, finally, run the given command.

- At the end of the request, we send the answer back via a `send_socket` call.

- These functionalities can be applied simultaneously by creating multiple
processes to handle them. Each client connection is handled by a different
process, by forking the main process in a while loop each time a connection is
accepted. We've chosen this approach for parallelizing our code as this way,
the death of any child process won't affect the main process and therefore will
not cause the server to shut down. 

- The child process takes care of the request and the parent will wait for its
children using a custom `SIGCHLD` handler.

- Finally, we close the socket that took care of the connection.

## Library Loader Implementation

- `lib-load` redirects `stdout` to the output file, opens the library (if it's
valid, if it exists) and then runs the handler, if we are given one.

- `lib-execute` will run our task, however if the parent has a child that
stopped working (due to signals) it will print a message that would flag the
issue.

- `lib-close` closes the library and frees the used memory. 

## Extra Tasks

- Extra security:
	- If one of the functions provided does anything that would affect the
	server (Invalid memory access, exit(), etc), this will have absolutely no
	effect on the server or any other child process as each	function is run in
	its own sandboxed process. If the process dies in any unexpected way, an
	error message will be returned.
	Sidenote: This should allow us to pass tests 10/11, but, as there is no ref
	file provided for these tests the checker fails when trying to match the ref
	with the out file.

- Timeout:
	- After a process is forked to run the function, the parent sleeps for a
	certain timeout and then kills the child, effectively imposing a certain
	time limit.

- Controlled exit:
	- When a connection receives a request that has "exit" as its first
	parameter, it will gracefully close that specific connection. We also tried
	to do this with the server as a whole, by processing the SIGINT signal, but
	didn't manage to implement this while still passing all tests. The main
	problem was that `accept()` is a blocking function. Theoretically, `accept4()`
	exists exactly for this reason, but it didn't seem to be available on the SO
	VM. Therefore we tried to just exit(0) from the server in the handler,
	leaving the connection processes to be adopted by init, but didn't manage to
	implement this properly.

When running the checker, the server has to be started in another terminal
before the checker as the checker itself is unable to start the server for some
reason.