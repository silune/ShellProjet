# Shell Project

Implementation of a Bash like shell for a school project at ENS Paris Saclay
Lexer and Parser are not from me

## Compile the shell

Using the makefile file you can :
 - ```make``` to compile the shell
 - ```make test``` to compile some C program test
 - ```make clean``` to clean files (including the test)

Then :
 - ```./shell``` to enter the shell

## Answers to the questions

1.
I'm using the `execvp` function to execute command.
It is more appropriate than `exec` because the parser does not give the full path of the command.
It is more appropriate than `execl` and `execlp` because of the parser wich put the arguments in the right format.
It is more appropriate than `execv` because we want to have an access to the PATH in the shell.

2.
The sequence operator is used via the `;` symbol. The difference between ```cmd1 && cmd2``` and ```cmd1 ; cmd2``` is that, in the first case, `cmd2` is not executed if `cmd1` fail, but in the second case, the two command are always executed.
For exemple ```false && ls``` will not execute `ls` but ```false ; ls``` will.

4.
Yes the command is possible. The parenthesis are used to write all the errors of the commands in /dev/null (wich forget them). Whithout the parenthesis, only the errors of the last command are redirected.
```(ls ; ls) | wc``` will return the word count of the successive two command while
```ls ; (ls | wc)``` will return the return ls and then the word count of another ls

Note that the parenthesis are executed in a forked processus, for exemple ```(cd ..)``` will not change the current directory because the parent processus has not changed.

5.
`Ctrl-C` will close the current shell. To solve this problem wehave to ignore the `SIGTERM` signal. A better way to do it is to handle the signal with a function that reset the current shell buffer (c.f. `reset` function in main.c).

6.
It print the result of ls in the terminal instead of writing the result in the dump file.
To solve this I use `dup2` function to redirect the flux. This redirection end as the child process exit. For the process that does not fork (like `cd`) I save the old file descriptors of `stdin`, `stdout`, `stderr` and reset them at the end of the process.

7.
We have to use the `pipe` function and not only the `dup` function in order to not rewrite onto the terminal. For example ```ls | wc``` will print the result of `ls` onto the terminal (wich is not what we want) and `wc` may not be able to read it.

## Implementation

Some examples of command to understand my implementation :

 - (simple command) ```ls -a``` :
 This command is parsed in a `C_PLAIN` command type. In this case the program is forked, the parent process wait for its children to exit, the child process use `execvp` to execute the command.

 - (simple operator `&&`, `||`, `;`) ```false || ls -a``` :
 This command is parsed in a `C_OR` command type. In this case the left part is executed as a simple command but I use the `WEXITSTATUS(status)` to check if the command failed. If it has failed then I execute the right part as a simple command, else I simply return the first status.

 - (pipe operator `|`) ```ls -l | wc -l``` :
 This command is parsed in a `C_PIPE` command type. In this case I initalize a pipe using the `pipe` C function. I fork a first time, in the child processI `dup2` stdout and the entry of the pipe then execute the left part. In the parent process I close the entry and wait for the child to exit. Then I fork again, in the child process I `dup2` the exit of the pipe and stdin and execute the right part, in the parent process I close de exit of the pipe and wait for the child to exit.

 - (parenthesis `(cmd)`) ```(ls -l)``` :
 This command is parsed in a `C_VOID` command type. In this type I fork and execute the command inside the parenthesis in the child process.

 - (redirections `>`, `>>`, `2>`) ```ls -l >> test``` :
 Before executing each command, in the child process I open the file with the right flags and apply redirections on `stdin`, `stdout` and `stderr` toward this file.

## Bonus Implementation

### Builtin command

 - (builtin command `cd`) ```cd ..``` :
 Before executing simple command I check if it is not a `cd` command in order to not fork such command and really change the directory. Then I use the `chdir` function, checking special case like : non correct argument, no argument (then moving to HOME)

 - (builtin command `history`) ```history 10``` :
 I use the readline history lib wich let me save all command. Then I just run the history to print the right number of previous command.

### Job implementation

 - (CTRL-Z) ```./slow``` and then CTRL-Z :
 When executing a command I put all children in a same process group and save that process group id on a stack struct I implemented. When a command is executed, the global variable `fg_group` is updated with the current group process ID. When pressing `CTRL-Z` a signal `SIGTSTP` is send to this group, the state of the group is refreshed in the stack, the shell state is saved. Because the child exited, the shell is refreshed to its default state.

 - (command `jobs`) : ```jobs``` :
 This command list all process currently saved in the stack, with an ID, their name and their state. I was not able to handle the name of complex command (parenthesis and `&` command).

 - (command `bg`, `fg`) : ```fg 5``` :
 This command send a signal `SIGCONT` to the process group indicated (if none is given it is the one on the stack with `+` symbol). Depending on the command I wait for the child to exit again or not and give the group process the foreground priority. Each time the shell is refresh I also refresh the stackto see if some process are finished (using `waitpid` and `WNOHANG` flag).

 - (start background process `&`) : ```./slox &``` :
 This command is parsed in a `C_BGPROC` command type and simply execute the command but doesnot wait for the child to exit.

WARNING : I did not check for the conflict in using pipe in background process so I am not sure of the result os such command.
The test : I implemented some tests of program that run slowly in order to test Job implementation.
