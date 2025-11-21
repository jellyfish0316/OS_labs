#include "sender.h"

// Define keys for IPC mechanisms
#define IPC_KEY_PATH "."
#define IPC_KEY_ID 65
#define SEM_SENDER_IDX 0
#define SEM_RECEIVER_IDX 1
#define EXIT_MSG "exit"

void send(message_t message, mailbox_t* mailbox_ptr){
    /*  TODO: 
        1. Use flag to determine the communication method
        2. According to the communication method, send the message
    */
    
    // (1) 
    switch(mailbox_ptr->flag){
        case MSG_PASSING:
            if (msgsnd(mailbox_ptr->storage.msqid, &message, sizeof(message.msgText), 0) == -1) { //msgflg == 0	阻塞 (Blocking)	如果訊息佇列已滿，msgsnd 會暫停執行，直到佇列有足夠空間為止。
                perror("msgsnd"); 
                exit(1);
            }
            break;
        case SHARED_MEM:
            strcpy(mailbox_ptr->storage.shm_addr, message.msgText);
            break;
        default: 
            fprintf(stderr, "Unknown mechanism flag\n");
            exit(1);
    }
}

void sem_op_wait(int sem_id, int sem_num){
    struct sembuf sop = {sem_num, -1, 0};
    if(semop(sem_id, &sop, 1) == -1){
        perror("semop wait");
        exit(1);
    }
}

void sem_op_signal(int sem_id, int sem_num){
    struct sembuf sop = {sem_num, 1, 0};
    if(semop(sem_id, &sop, 1) == -1){
        perror("semop signal");
        exit(1);
    }
}

int main(int argc, char *argv[]){
    /*  TODO: 
        1) Call send(message, &mailbox) according to the flow in slide 4
        2) Measure the total sending time
        3) Get the mechanism and the input file from command line arguments
            • e.g. ./sender 1 input.txt
                    (1 for Message Passing, 2 for Shared Memory)
        4) Get the messages to be sent from the input file
        5) Print information on the console according to the output format
        6) If the message form the input file is EOF, send an exit message to the receiver.c
        7) Print the total sending time and terminate the sender.c
    */
    // Terminla Inputs
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <1 for Message Passing | 2 for Shared Memory> <input file>\n", argv[0]);
        return 1;
    }

    int mechanism = atoi(argv[1]);
    char* filename = argv[2];

    // Setup key and mailbox
    key_t key = ftok(IPC_KEY_PATH, IPC_KEY_ID);
    if (key == -1) {
        perror("ftok");
        return 1;
    }

    mailbox_t mailbox;
    mailbox.flag = mechanism;

    // Setup Semaphores
    int semid = semget(key, 2, IPC_CREAT | 0666);
    if (semid == -1) {
        perror("semget");
        return 1;
    }

    // Initialize semaphores: Sender_SEM = 0, Receiver_SEM = 1
    // Receiver_SEM starts at 1 so the sender can send the first message without waiting.
    semctl(semid, SEM_SENDER_IDX, SETVAL, 0);
    semctl(semid, SEM_RECEIVER_IDX, SETVAL, 1);

    //setup msqid/shmaddr(where to send)
    if(mechanism == MSG_PASSING){
        printf("Message Passing\n");
        mailbox.storage.msqid = msgget(key, IPC_CREAT | 0666);
        if (mailbox.storage.msqid == -1) {
            perror("msgget");
            return 1;
        }
    }
    else if(mechanism == SHARED_MEM){ 
        printf("Shared Memory\n");
        int shmid = shmget(key, 1024, IPC_CREAT | 0666);
        if(shmid == -1){
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

    //read and send
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        perror("fopen");
        return 1;
    }

    message_t msg;
    msg.mType = 1; // let user set which type to receive, set it to 1 for convenience

    struct timespec start, end;
    double time_taken = 0;


    while(fgets(msg.msgText, sizeof(msg.msgText), fp)){
        
        // Wait for the receiver to be ready
        sem_op_wait(semid, SEM_RECEIVER_IDX);

        clock_gettime(CLOCK_MONOTONIC, &start);
        send(msg, &mailbox);
        clock_gettime(CLOCK_MONOTONIC, &end);
        time_taken += (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) * 1e-9;
        
        printf("Sending message: %s", msg.msgText);

        sem_op_signal(semid, SEM_SENDER_IDX);

    }
    fclose(fp);

    printf("\nEnd of input file! Exit!\n");

    strcpy(msg.msgText, EXIT_MSG);

    // Wait for the receiver to be ready
    sem_op_wait(semid, SEM_RECEIVER_IDX);

    clock_gettime(CLOCK_MONOTONIC, &start);
    send(msg, &mailbox);
    clock_gettime(CLOCK_MONOTONIC, &end);
    time_taken += (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) * 1e-9;

    sem_op_signal(semid, SEM_SENDER_IDX);

    printf("Total time taken in sending msg: %f s\n", time_taken);

    // Give receiver time to read the exit message before cleaning up
    sleep(1);

    //release resources
    if (mechanism == MSG_PASSING) {
        msgctl(mailbox.storage.msqid, IPC_RMID, NULL);
    } else if (mechanism == SHARED_MEM) {
        shmdt(mailbox.storage.shm_addr);
        shmctl(shmget(key, 1024, 0666), IPC_RMID, NULL);
    }

    semctl(semid, 0, IPC_RMID, NULL);

    return 0;

}
