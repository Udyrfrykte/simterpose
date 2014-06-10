/* simterpose_msg - simterpose intercepter based on MSG                     */

/* Copyright (c) 2014. The SimGrid Team. All right reserved.                */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#include <sys/ptrace.h>
#include <errno.h>
#include <unistd.h>

#include <msg/msg.h>
#include <xbt.h>

#include "xbt_stuff.h"

XBT_LOG_NEW_DEFAULT_CATEGORY(simterpose, "High-level simterpose category");

static int simterpose_process_runner(int argc, char *argv[]);

int main(int argc, char *argv[]) {
	float msec_per_flop = 0;
	int flop_option = 0;

	MSG_init(&argc, argv);

	if (argc < 3) {
		usage(argv[0],1);
	} else {
		int c;
		while ((c = getopt(argc, argv, "+p:")) != EOF) {
			switch (c) {
			case 'p':
				flop_option = 1;
				msec_per_flop = 1000000 / str_to_double(optarg);
				XBT_INFO("Setting reference power to %s flop/s", optarg);

				break;

			default:
				usage(argv[0],0);
				break;
			}
		}

	}

	const char* platform_file = argv[optind];
	const char* application_file = argv[optind+1];

	MSG_create_environment(platform_file);
	MSG_function_register_default(simterpose_process_runner);
	MSG_launch_application(application_file);

	msg_error_t res = MSG_main();
	XBT_INFO("Simulated time: %g", MSG_get_clock());

	if (res == MSG_OK)
		return 0;
	else
		return 1;
}

static int simterpose_process_runner(int argc, char *argv[]) {
	int status;
	int tracked_pid = fork();
	if (tracked_pid == 0) {
		// in child
		if (ptrace(PTRACE_TRACEME, 0, NULL, NULL) == -1) {
			perror("ptrace traceme");
			exit(1);
		}

		xbt_dynar_t cmdline_dynar = xbt_dynar_new(sizeof(char*), NULL);
		xbt_dynar_push_array(cmdline_dynar, argv, argc);
		char *cmdline_str = xbt_str_join(cmdline_dynar, " ");

		XBT_INFO("Process %d is starting child: %s", getpid(), cmdline_str);

		execv(argv[0], argv); // If successful, the execution flow does not go any further here

		fprintf(stderr, "Error while starting %s: %s (full cmdline: %s)\n",
				argv[0], strerror(errno), cmdline_str);
		exit(1);

	}

	// We are still in simterpose, so we are the thread that is the representative of the external process

	// Wait for the traceme to apply (ie, for the child to start)
	waitpid(tracked_pid, &status, 0);

	// Trace the child and all upcoming granchilds
	if (ptrace(PTRACE_SETOPTIONS, tracked_pid, NULL,
			PTRACE_O_TRACECLONE | PTRACE_O_TRACEFORK | PTRACE_O_TRACEVFORK | PTRACE_O_TRACEVFORKDONE)
			== -1) {
		perror("Error setoptions");
		exit(1);
	}

	// Main loop where we track our external process and do the simcall that represent its syscalls
	while (1) {
		ptrace_resume_process(tracked_pid);
		if (waitpid(pid, &status, 0) < 0)
			xbt_die("[%d] Error while stepping the tracked process: %s (%d)\n", pid, strerror(errno), errno);



	}
	return 0;
}