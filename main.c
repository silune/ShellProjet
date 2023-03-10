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
int execute (struct cmd *cmd, int group);

// name of the program, to be printed in several places
#define NAME "myshell"

// Some helpful functions

void errmsg (char *msg)
{
	fprintf(stderr,"error: %s\n",msg);
}


int pid; //pid pour les forks
int fg_group = -1; //pid du group de premier plan
int terminal_fd; //file descriptor du terminal de premier plan
struct termios terminal_attr; //attributs du terminal de premier plan



// apply_redirects() should modify the file descriptors for standard
// input/output/error (0/1/2) of the current process to the files
// whose names are given in cmd->input/output/error.
// append is like output but the file should be extended rather
// than overwritten.

// effectue les redirections, renvoie le file descriptor du fichier ouvert ou -1 si pas de redirections
int apply_redirects (struct cmd *cmd)
{
  if (cmd->append != NULL) {
    int new_dir = open(cmd->append, O_WRONLY | O_APPEND | O_CREAT, 0644);
    dup2(new_dir, 1);
    return new_dir;
  }
  if (cmd->output != NULL) {
    int new_dir = open(cmd->output, O_WRONLY | O_TRUNC | O_CREAT, 0644);
    dup2(new_dir, 1);
    return new_dir;
  }
  if (cmd->input != NULL) {
    int new_dir = open(cmd->input, O_RDONLY);
    dup2(new_dir, 0);
    return new_dir;
  }
  if (cmd->error != NULL) {
    int new_dir = open(cmd->error, O_WRONLY | O_TRUNC | O_CREAT, 0644);
    dup2(new_dir, 2);
    return new_dir;
  }
  return -1;
} 

// a partir de la sauvegarde des fd, dup2 sur les entrées / sorties standard
void restore_redirects (int save_stdin, int save_stdout, int save_stderr)
{
  dup2(save_stdin, 0);
  dup2(save_stdout, 1);
  dup2(save_stderr, 2);
}

// positionne un groupe au premier plan
void push_foreground(int pid, struct termios terminal_state)
{
  fg_group = pid;
  terminal_fd = tcgetpgrp(0);
  tcgetattr(0, &terminal_attr);
  tcsetpgrp(0, pid);
  tcsetattr(0, TCSADRAIN, &terminal_state);
}

#include "job.h"
#include "builtin_command.h"

//signal handlers

//reset terminal to default state
void reset() {
  rl_crlf();
  rl_reset_line_state();
  rl_replace_line("", 0);
  rl_redisplay();
}

int do_pause = 0;

//pause de foreground process
void pause_process() {
  if (do_pause && fg_group > 0) {
    do_pause = 0;
    kill(-fg_group, SIGTSTP);
  }
}

// The function execute() takes a command parsed at the command line.
// The structure of the command is explained in output.c.
// Returns the exit code of the command in question.

//set up a child process group
int set_up_child(int group, int foreground)
{
  signal(SIGTSTP, SIG_DFL);
  if (group == -1) {
    group = getpid();
    if (foreground) {
      push_foreground(group, terminal_attr);
    }
  }
  setpgid(0, group);
  return group;
}

//set up a parent process group and wait for its child
void set_up_parent(int child_pid, int group, char* proc_name, int* status, int foreground)
{
  if (group == -1) {
    group = child_pid;
    setpgid(child_pid, group);
    struct process* proc = add_new_proc_jobstk(main_jobs_stack, group, proc_name);
    if (foreground) {
      push_foreground(child_pid, terminal_attr);
      fg_group = group;
      wait_for_proc(main_jobs_stack, proc, status); // also gives back terminal to current shell
    }
  }
  else {
    setpgid(child_pid, group);
    if (foreground) {
      int waitTillExited;
      do {
        waitpid(child_pid, &waitTillExited, WUNTRACED);
      } while (! WIFEXITED(waitTillExited));
    }
  }
}

int execute (struct cmd *cmd, int group)
{
  int status;
	switch (cmd->type)
	{
	    case C_PLAIN:
        //builtin case
        if (is_builtin_command(cmd)) {
         return builtin_command(cmd);
        }
        //general case
        pid = fork();
        if (pid == 0) {
        //chil process
        group = set_up_child(group, 1);
        apply_redirects(cmd);
        execvp(cmd->args[0], cmd->args);
        fprintf(stderr, "%s : command not found\n", cmd->args[0]);
        exit(-1);
        }
        //parent process
        set_up_parent(pid, group, cmd->args[0], &status, 1);
        return status;

	    case C_SEQ:
        execute(cmd->left, group);
        status = execute(cmd->right, group);
        return status;

      case C_USEQ:
        return execute(cmd->left, group);

	    case C_AND:
        status = execute(cmd->left, group);
        if (WEXITSTATUS(status) == 0) {
          status = execute(cmd->right, group);
        }
        return status;

	    case C_OR:
        status = execute(cmd->left, group);
        if (WEXITSTATUS(status)) {
          status = execute(cmd->right, group);
        }
        return status;

	    case C_PIPE:
        int tube[2];
        pipe(tube);
        pid = fork();
        if (pid == 0) {
          //child sub pipe process
          dup2(tube[1], 1);
          exit(execute(cmd->left, group));
        }
        else {
          close(tube[1]);
          wait(NULL);
          pid = fork();
          if (pid == 0) {
            //child main process
            dup2(tube[0], 0);
            exit(execute(cmd->right, group));
          }
          else {
            //parent process
            close(tube[0]);
            wait(&status);
          }
        }
        return status;

	    case C_VOID:
        pid = fork();
        if (pid == 0) {
          //child process
          group = set_up_child(group, 1);
          status = execute(cmd->left, group);
          exit(status);
        }
        set_up_parent(pid, group, "complex job", &status, 1);
        return status;

      case C_BGPROC:
        pid = fork();
        if (pid == 0) {
          //child process
          group = set_up_child(group, 0);
          status = execute(cmd->left, group);
          exit(status);
        }
        set_up_parent(pid, group, "complex background job", &status, 0);
        return status;

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
  signal(SIGTSTP, pause_process);
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
    do_pause = 0;
		char *line = readline(prompt);
		if (!line) break;	// user pressed Ctrl+D; quit shell
    refresh_jobstk(main_jobs_stack, 0);
		if (!*line) continue;	// empty line

		add_history (line);	// add line to history

		struct cmd *cmd = parser(line);
		if (!cmd) continue;	// some parse error occurred; ignore
		//output(cmd,0);	// activate this for debugging
    
    do_pause = 1;
    fg_group = -1;
    int i = execute(cmd, fg_group);
    printf("hehehehe : %d\n", i);
	}

  free_jobstk(main_jobs_stack);
	printf("goodbye!\n");
	return 0;
}
