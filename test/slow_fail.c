#include<unistd.h>
#include<stdio.h>
#include<stdlib.h>

int main() {
  for (int i = 0; i < 5; i++) {
    printf("Beep Boop ...\n");
    sleep(1);
  }
  exit(EXIT_FAILURE);
  return 0;
}
