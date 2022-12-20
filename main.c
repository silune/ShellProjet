#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <termios.h>

#include "global.h"

// This is the file that you should work on.

// declaration
int execute (struct cmd *cmd);

// name of the program, to be printed in several places
#define NAME "myshell"

// Some helpful functions

void errmsg (char *msg)
{
	fprintf(stderr,"error: %s\n",msg);
}



int pid;
int fg_group = -1;
int terminal_fd;
struct termios terminal_attr;



// apply_redirects() should modify the file descriptors for standard
// input/output/error (0/1/2) of the current process to the files
// whose names are given in cmd->input/output/error.
// append is like output but the file should be extended rather
// than overwritten.

void apply_redirects (struct cmd *cmd)
{
  if (cmd->append != NULL) {
    int new_dir = open(cmd->append, O_WRONLY | O_APPEND | O_CREAT, 0644);
    dup2(new_dir, 1);
  }
  if (cmd->output != NULL) {
    int new_dir = open(cmd->output, O_WRONLY | O_TRUNC | O_CREAT, 0644);
    dup2(new_dir, 1);
  }
  if (cmd->input != NULL) {
    int new_dir = open(cmd->input, O_RDONLY);
    dup2(new_dir, 0);
  }
  if (cmd->error != NULL) {
    int new_dir = open(cmd->error, O_WRONLY | O_TRUNC | O_CREAT, 0644);
    dup2(new_dir, 2);
  }
} 

void restore_redirects (int save_stdin, int save_stdout, int save_stderr)
{
  dup2(save_stdin, 0);
  dup2(save_stdout, 1);
  dup2(save_stderr, 2);
}

void push_foreground(int pid, struct termios terminal_state)
{
  terminal_fd = tcgetpgrp(0);
  tcgetattr(0, &terminal_attr);
  tcsetpgrp(0, pid);
  tcsetattr(0, TCSADRAIN, &terminal_state);
}

#include "job.h"
#include "builtin_command.h"


// The function execute() takes a command parsed at the command line.
// The structure of the command is explained in output.c.
// Returns the exit code of the command in question.

//signal handlers

void reset() {
  rl_crlf();
  rl_reset_line_state();
  rl_replace_line("", 0);
  rl_redisplay();
}

int do_pause = 0;

void pause_process() {
  if (do_pause) {
    do_pause = 0;
    kill(-fg_group, SIGTSTP);
  }
}

// This function is the only one who fork and prevent fromdoing useless child of child
void execute_simple (struct cmd *cmd, int *status)
{
  // not appropriate case
  if (cmd->type != C_PLAIN) {
    *status = execute(cmd);
    return;
  }
  // builtin case
  if (is_builtin_command(cmd)) {
    *status = builtin_command(cmd);
    return;
  }

  //general case
  pid = fork();
  if (pid == 0) {
    //chil process
    fg_group = getpid();
    setpgid(0, 0);
    push_foreground(fg_group, terminal_attr);

    apply_redirects(cmd);

    execvp(cmd->args[0], cmd->args);
    fprintf(stderr, "%s : command not found\n", cmd->args[0]);
    exit(-1);
  }
  //parent process
  fg_group = pid;
  setpgid(pid, pid);
  push_foreground(pid, terminal_attr);
          
  struct process* proc = add_new_proc_jobstk(main_jobs_stack, fg_group, cmd->args[0]);
  wait_for_proc(main_jobs_stack, proc, status); // also gives back terminal to current shell
}

int execute (struct cmd *cmd)
{
  int status;
	switch (cmd->type)
	{
	    case C_PLAIN:
        execute_simple(cmd, &status);
        return WEXITSTATUS(status);

	    case C_SEQ:
        execute_simple(cmd->left, &status);
        execute_simple(cmd->right, &status);
        return WEXITSTATUS(status);

	    case C_AND:
        execute_simple(cmd->left, &status);
        if (WEXITSTATUS(status) == 0) {
          execute_simple(cmd->right, &status);
        }
        return WEXITSTATUS(status);

	    case C_OR:
        execute_simple(cmd->left, &status);
        if (WEXITSTATUS(status)) {
          execute_simple(cmd->right, &status);
        }
        return WEXITSTATUS(status);

	    case C_PIPE:
        int tube[2];
        pipe(tube);
        pid = fork();
        if (pid == 0) {
          //child sub pipe process
          dup2(tube[1], 1);
          exit(execute(cmd->left));
        }
        else {
          close(tube[1]);
          wait(NULL);
          pid = fork();
          if (pid == 0) {
            //child main process
            dup2(tube[0], 0);
            exit(execute(cmd->right));
          }
          else {
            //parent process
            close(tube[0]);
            wait(&status);
          }
        }
        return WEXITSTATUS(status);

	    case C_VOID:
        int save_stdin = dup(0);
        int save_stdout = dup(1);
        int save_stderr = dup(2);
        apply_redirects(cmd);
        execute_simple(cmd->left, &status);
        restore_redirects(save_stdin, save_stdout, save_stderr);
        return WEXITSTATUS(status);

		errmsg("I do not know how to do this, please help me!");
		return -1;
	}

	// Just to satisfy the compiler
	errmsg("This cannot happen!");
	return -1;
}

int main (int argc, char **argv)
{
  //signals
  signal(SIGINT, reset);
  signal(SIGTSTP, SIG_IGN);
  signal(SIGTTOU, SIG_IGN);

  terminal_fd = tcgetpgrp(0);
  tcgetattr(0, &terminal_attr);

  main_jobs_stack = new_jobstk();
  using_history();

	char *prompt = malloc(strlen(NAME)+3);
	printf("welcome to %s!\n", NAME);
	sprintf(prompt,"%s> ", NAME);

	while (1)
	{
    signal(SIGTSTP, SIG_IGN);
    do_pause = 0;

		char *line = readline(prompt);
		if (!line) break;	// user pressed Ctrl+D; quit shell
		if (!*line) continue;	// empty line

		add_history (line);	// add line to history

		struct cmd *cmd = parser(line);
		if (!cmd) continue;	// some parse error occurred; ignore
		//output(cmd,0);	// activate this for debugging
    
    signal(SIGTSTP, pause_process);
    do_pause = 1;
    execute(cmd);
    refresh_jobstk(main_jobs_stack, 0);
	}

  free_jobstk(main_jobs_stack);
	printf("goodbye!\n");
	return 0;
}
