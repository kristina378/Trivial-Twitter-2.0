#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>

enum bool { true = 1, false = 0 };
typedef enum bool bool;

struct record {
    char username[256];
    char UserPost[1024];
    int LikesCount;
    bool ifFree;
};
typedef struct record record;

int main(int argc, char **argv) {

    if (argc < 3) {
        fprintf(stderr,
            "Client: not enough arguments!\nUsage: ./client <file> <N/P> [username]\n");
        exit(EXIT_FAILURE);
    }

    if (strcmp(argv[2], "N") != 0 && strcmp(argv[2], "P") != 0) {
        fprintf(stderr,
            "Client: wrong option! Use N (new post) or P (print/like)\n");
        exit(EXIT_FAILURE);
    }

    if (strcmp(argv[2], "N") == 0 && argc < 4) {
        fprintf(stderr,
            "Client: N requires username as 3rd argument!\n");
        exit(EXIT_FAILURE);
    }


    key_t key = ftok(argv[1], 'A');
    if (key == -1) {
        perror("ftok");
        exit(EXIT_FAILURE);
    }


    int shmid = shmget(key, 0, 0644);
    if (shmid == -1) {
        perror("shmget");
        exit(EXIT_FAILURE);
    }

    record *data = shmat(shmid, NULL, 0);
    if (data == (void *)-1) {
        perror("shmat");
        exit(EXIT_FAILURE);
    }


    struct shmid_ds buf;
    shmctl(shmid, IPC_STAT, &buf);
    int n = buf.shm_segsz / sizeof(record);


    int semid = semget(key, 0, 0644);

    if(semid == -1){
        perror("semget");
        if (shmdt(data) == -1) {
            perror("shmdt");
            exit(EXIT_FAILURE);
        }

        exit(EXIT_FAILURE);
    }

    int ServersPid_shmid = shmget(key + 1,0,0644);
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


    if (strcmp(argv[2], "N") == 0) {

        int index = -1;
        
        for (int i = 0; i < n; i++) {
            if (data[i].ifFree == true) {
                index = i;
                break;
            }
        }

        if (index == -1) {
            fprintf(stderr, "Twitter 2.0: No free space left!\n");
            shmdt(data);
            exit(EXIT_SUCCESS);
        }

        char new_entry[1024];
        printf("Greetings from Twitter 2.0 :) (version A)\n");
        printf("Enter new entry:\n> ");

        fgets(new_entry, sizeof(new_entry), stdin);
        new_entry[strcspn(new_entry, "\n")] = '\0';

        struct sembuf SemOp;

        SemOp.sem_num = index;
        SemOp.sem_op = -1;
        SemOp.sem_flg = 0;

        if(semop(semid,&SemOp,1) == -1){
            perror("semop");
            shmdt(data);
            exit(EXIT_FAILURE);
        }

        snprintf(data[index].username, sizeof(data[index].username), "%s", argv[3]);
        snprintf(data[index].UserPost, sizeof(data[index].UserPost), "%s", new_entry);
        data[index].LikesCount = 0;
        data[index].ifFree = false;
        
        SemOp.sem_op = 1;

        if(semop(semid,&SemOp,1) == -1){
            perror("semop");
            shmdt(data);
            exit(EXIT_FAILURE);
        }

        printf("Post added successfully!\n");

        kill(*ServersPidPtr,SIGUSR1);
    }

    else if (strcmp(argv[2], "P") == 0) {

        printf("Greetings from Twitter 2.0 :) (version A)\n");
        printf("Entries in service:\n");

        int printed = 0;

        for (int i = 0; i < n; i++) {
            if (data[i].ifFree == false) {
                printf("%d. %s [Author: %s, Likes: %d]\n",
                       i + 1,
                       data[i].UserPost,
                       data[i].username,
                       data[i].LikesCount);
                printed++;
            }
        }

        if (printed == 0) {
            printf("No posts yet.\n");
            shmdt(data);
            exit(EXIT_SUCCESS);
        }

        printf("Enter post number to like:\n> ");
        int post_num;
        scanf("%d", &post_num);

        if (post_num < 1 || post_num > n || data[post_num - 1].ifFree == true) {
            fprintf(stderr, "Invalid post number!\n");
        } else {

            struct sembuf SemOp;

            SemOp.sem_num = post_num - 1;
            SemOp.sem_flg = 0;
            SemOp.sem_op = -1;

            if(semop(semid, &SemOp, 1) == -1){
                perror("semop");
                shmdt(data);
                exit(EXIT_FAILURE);
            }

            data[post_num - 1].LikesCount++;
            printf("Post liked!\n");

            kill(*ServersPidPtr,SIGUSR1);


            SemOp.sem_op = 1;

            if(semop(semid, &SemOp, 1) == -1){
                perror("semop");
                shmdt(data);
                exit(EXIT_FAILURE);
            }
        }
    }

    printf("Thank you for using Twitter 2.0 ;)\n");

    if (shmdt(data) == -1) {
        perror("shmdt");
        exit(EXIT_FAILURE);
    }

    return 0;
}
