
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

// Entry point for Goliath unified loader
int wsl_main(int argc, char *argv[]) {
	if (argc < 2) {
		fprintf(stderr, "[Goliath/WSL] Usage: %s <linux-elf-binary> [args...]\n", argv[0]);
		return 1;
	}
	// For now, just exec the Linux ELF binary directly (placeholder for real WSL integration)
	pid_t pid = fork();
	if (pid == 0) {
		execv(argv[1], &argv[1]);
		perror("[Goliath/WSL] execv failed");
		exit(127);
	} else if (pid > 0) {
		int status = 0;
		waitpid(pid, &status, 0);
		return WIFEXITED(status) ? WEXITSTATUS(status) : 127;
	} else {
		perror("[Goliath/WSL] fork failed");
		return 127;
	}
}
