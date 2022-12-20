#include<unistd.h>
#include<stdio.h>
#include<stdlib.h>

int main() {
  for (int i = 0; i < 5; i++) {
    printf("Beep Boop ... (%d, %d)\n", getpid(), getpgid(getpid()));
    sleep(1);
  }
  return 0;
}
