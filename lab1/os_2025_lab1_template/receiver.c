#include "receiver.h"

// Define keys for IPC mechanisms (must match sender)
#define IPC_KEY_PATH "."
#define IPC_KEY_ID 65
#define SEM_SENDER_IDX 0
#define SEM_RECEIVER_IDX 1
#define EXIT_MSG "exit"

void receive(message_t* message_ptr, mailbox_t* mailbox_ptr){
    /*  TODO: 
        1. Use flag to determine the communication method
        2. According to the communication method, receive the message
    */
    
    // (1) 判斷模式
    switch(mailbox_ptr->flag){
        case MSG_PASSING:
            if (msgrcv(mailbox_ptr->storage.msqid, message_ptr, sizeof(message_ptr->msgText), 0, 0) == -1) { //msgflg == 0	阻塞 (Blocking)	如果訊息佇列已滿，msgsnd 會暫停執行，直到佇列有足夠空間為止。
                perror("msgrcv"); 
                exit(1);
            }
            break;
        case SHARED_MEM:
            strcpy(message_ptr->msgText, mailbox_ptr->storage.shm_addr);
            break;
        default: 
            fprintf(stderr, "Unknown mechanism flag\n");
            exit(1);
    }
    
}

// Semaphore operations
void sem_op_wait(int semid, int sem_num) {
    struct sembuf sop = {sem_num, -1, 0};
    if (semop(semid, &sop, 1) == -1) {
        perror("semop wait");
        exit(1);
    }
}

void sem_op_signal(int semid, int sem_num) {
    struct sembuf sop = {sem_num, 1, 0};
    if (semop(semid, &sop, 1) == -1) {
        perror("semop signal");
        exit(1);
    }
}

int main(int argc, char *argv[]){
    /*  TODO: 
        1) Call receive(&message, &mailbox) according to the flow in slide 4
        2) Measure the total receiving time
        3) Get the mechanism from command line arguments
            • e.g. ./receiver 1
        4) Print information on the console according to the output format
        5) If the exit message is received, print the total receiving time and terminate the receiver.c
    */

    // terminal inputs
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <1 for Message Passing | 2 for Shared Memory>\n", argv[0]);
        return 1;
    }

    int mechanism = atoi(argv[1]);

    // setup key and mailbox
    key_t key = ftok(IPC_KEY_PATH, IPC_KEY_ID);
    if (key == -1) {
        perror("ftok");
        return 1;
    }

    mailbox_t mailbox;
    mailbox.flag = mechanism;

    //get semaphores
    int semid = semget(key, 2, 0666);
    if (semid == -1) {
        perror("semget");
        return 1;
    }

    //get msgid/shm_addr(where to receive)
    if(mechanism == MSG_PASSING){
        printf("Message Passing\n");
        mailbox.storage.msqid = msgget(key, 0666);
        if (mailbox.storage.msqid == -1) {
            perror("msgget");
            return 1;
        }
    }
    else if(mechanism == SHARED_MEM){
        printf("Shared Memory\n");
        int shmid = shmget(key, 1024, 0666);
        if (shmid == -1) {
            perror("shmget");
            return 1;
        }
        mailbox.storage.shm_addr = (char*)shmat(shmid, NULL, 0);
        if (mailbox.storage.shm_addr == (char*)-1) {
            perror("shmat");
            return 1;
        }
    }
    else{
        fprintf(stderr, "Invalid mechanism choice.\n");
        return 1;
    }

    //receice message

    message_t msg;
    struct timespec start, end;
    double time_taken = 0;

    while(1){
        sem_op_wait(semid, SEM_SENDER_IDX);

        clock_gettime(CLOCK_MONOTONIC, &start);
        receive(&msg, &mailbox);
        clock_gettime(CLOCK_MONOTONIC, &end);
        time_taken += (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) * 1e-9;
        

        //check if sender exited
        if(strcmp(msg.msgText, EXIT_MSG) == 0){
            printf("\nSender exit!\n");
            sem_op_signal(semid, SEM_RECEIVER_IDX);
            break;
        }

        printf("Receiveing Message: %s", msg.msgText);

        sem_op_signal(semid, SEM_RECEIVER_IDX);

    }

    printf("Total time taken in receiving msg: %f s\n", time_taken);

    if (mechanism == SHARED_MEM) {
        shmdt(mailbox.storage.shm_addr);
    }
    
    return 0;


}
