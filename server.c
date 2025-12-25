#include <stdio.h>
#include <stdlib.h>
#include <sys/fcntl.h>
#include <sys/ipc.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <signal.h>
#include <string.h>
#include <sys/sem.h>

enum bool{true = 1,false = 0};
typedef enum bool bool;

struct record{
    char username[256];
    char UserPost[1024];
    int LikesCount;
    bool ifFree;
};
typedef struct record record;

int shmid, semid;
record* data;

void handler(int sig) {
    const char *msg = "Server: cleaning up...\n";
    write(STDERR_FILENO, msg, strlen(msg));

    shmdt(data);
    shmctl(shmid, IPC_RMID, NULL);
    semctl(semid, 0, IPC_RMID);

    _exit(0);
}

void client_handler(int sig){
    
    printf("_____Twitter 2.0_____\n");

    struct shmid_ds buf;
    shmctl(shmid, IPC_STAT, &buf);
    int n = buf.shm_segsz / sizeof(record);

    for(unsigned i = 0; i < n; i++){
        if(data[i].ifFree == false){
            printf("[%s]: %s [Likes: %d]\n",data[i].username,data[i].UserPost,data[i].LikesCount);
        }
    }

    printf("\n\n");
}


int main(int argc, char **argv) {
    struct sigaction act;
    struct sigaction clientAct;

    act.sa_handler = &handler;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;

    clientAct.sa_handler = &client_handler;
    sigemptyset(&clientAct.sa_mask);
    clientAct.sa_flags = 0;

    sigaction(SIGINT, &act, NULL);
    sigaction(SIGQUIT, &act, NULL);

    sigaction(SIGUSR1, &clientAct, NULL);


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
    exit(EXIT_FAILURE);
  }

  data = shmat(shmid, (void *)0, 0);
  if (data == (void *)(-1)) {
        perror("shmat: Failed to map memory");
        exit(EXIT_FAILURE);
    }

    for(unsigned index = 0; index < n; index++){
        data[index].LikesCount = 0;
        data[index].username[0] = '\0';
        data[index].UserPost[0] = '\0';
        data[index].ifFree = true;
    }

    semid = semget(key, n, IPC_CREAT | 0644);

    if(semid == -1){
        perror("semget");

        shmdt(data);
        shmctl(shmid, IPC_RMID, NULL);

        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < n; i++) {
        if(semctl(semid, i, SETVAL, 1) == -1){

            fprintf(stderr,"semctl: Couldn't set value of %d semaphore\n",i+1);

            shmdt(data);
            shmctl(shmid, IPC_RMID, NULL);

            
            exit(EXIT_FAILURE);
        }
    }

    int ServersPid_shmid = shmget(key + 1, sizeof(pid_t), IPC_CREAT | 0644);
    if(ServersPid_shmid == -1){
        perror("shmget: Failed to create/associate memory for server's PID");

        shmdt(data);
        shmctl(shmid, IPC_RMID, NULL);
        semctl(semid, 0, IPC_RMID);

        exit(EXIT_FAILURE);
    }

    pid_t *ServersPidPtr = shmat(ServersPid_shmid, NULL, 0);
    if(ServersPidPtr == (void*)(-1)){
        perror("shmat: Failed to create/associate memory for server's PID");

        shmdt(data);
        shmctl(shmid, IPC_RMID, NULL);
        shmctl(ServersPid_shmid, IPC_RMID, NULL);
        semctl(semid, 0, IPC_RMID);

        exit(EXIT_FAILURE);
    }

    *ServersPidPtr = getpid();



    printf("Server running. Press Ctrl+C/Ctrl \\ to exit.\n");

    for (;;) pause();

  return 0;
}
