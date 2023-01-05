#include<unistd.h>
#include<stdio.h>
#include<stdlib.h>

int main() {
  for (int i = 0; i < 10; i++) {
    printf("Beep Boop ...\n");
    sleep(1);
  }
  return 0;
}
