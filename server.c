#include <stdio.h>
#include <stdlib.h>
#include <sys/fcntl.h>
#include <sys/ipc.h>
#include <sys/signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <signal.h>
#include <string.h>
//#include <sys/sem.h>

enum bool{true = 1,false = 0};
typedef enum bool bool;

struct record{
    char username[256];
    char UserPost[1024];
    int LikesCount;
    bool ifFree;
};
typedef struct record record;

int shmid;
record* data;

void handler(int sig) {
    const char *msg = "Server: cleaning up...\n";
    write(STDERR_FILENO, msg, strlen(msg));

    shmdt(data);
    shmctl(shmid, IPC_RMID, NULL);

    _exit(0);
}



int main(int argc, char **argv) {
    struct sigaction act;

    act.sa_handler = &handler;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;

    sigaction(SIGINT, &act, NULL);
    sigaction(SIGQUIT, &act, NULL);


  if (argc < 3) {
    fprintf(stderr,
            "Server: didn't get enough arguments!\nGot:%d arguments\nNeed: 3 "
            "arguments\n",
            argc - 1);
    fprintf(stderr,"Server exited...\n");
    exit(EXIT_FAILURE);
  }


  key_t key;

  if((key = ftok(argv[1],'A')) == -1){
        perror("ftok: Failed to generate key for IPC SysV object");
        fprintf(stdout,"-1");
        return 1;
    }

  int n = atoi(argv[2]);

  size_t SHM_SIZE = n * sizeof(record);


  if((shmid = shmget(key, SHM_SIZE, 0644|IPC_CREAT)) == -1){
    perror("shmget: Failed to allocate shared memory");
    //free(sem);
    exit(EXIT_FAILURE);
  }

  data = shmat(shmid, (void *)0, 0);
  if (data == (void *)(-1)) {
        perror("shmat: Failed to map memory");
        //free(sem);
        exit(EXIT_FAILURE);
    }

    for(unsigned index = 0; index < n; index++){
        data[index].LikesCount = 0;
        data[index].username[0] = '\0';
        data[index].UserPost[0] = '\0';
        data[index].ifFree = true;
    }

    printf("Server running. Press Ctrl+C/Ctrl \\ to exit.\n");

    for (;;) pause();

  return 0;
}
