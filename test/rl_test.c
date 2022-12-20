#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <readline/readline.h>

int main ()
{
  while (1)
  {
    char* line = readline("");
    printf("you wrote : %s\n", line);
  }
  return 0;
}
