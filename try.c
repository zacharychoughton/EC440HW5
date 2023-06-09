#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include "ec440threads.h"

void* thread_start(void* arg) {
  printf("Thread started\n");
  printf("A Working Thread ID %d\n",pthread_self());
  double i; 
      while(i<1000){
        i= i+ 0.000001; 
    }
  printf("Thread finished\n");
  return NULL;
}

int main() {
  pthread_t tid[20];
  int j; 

  for (j=0;j<20;j++){

    int ret = pthread_create(&tid[j], NULL, thread_start, NULL);
    if (ret != 0) {
    fprintf(stderr, "Error creating thread\n");
    exit(EXIT_FAILURE);
    }
  printf("Thread IDs: %d %d\n",tid[j],j);
  j++; 
  }
  printf("Main Thread ID %d\n",pthread_self());
  pthread_exit(NULL);
  printf("This line should never be reached\n");

  return 0;
}