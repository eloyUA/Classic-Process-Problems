#include <stdio.h>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/sem.h>

#define SIZE_BUFFER 4096

typedef struct {
    int start, end;
    int buffer[SIZE_BUFFER];
} shareMem;

int main(int argc, char **argv) {
    if (argc != 4)
        exit(1);

    int key;
    int idShm;
    int idSems;
    shareMem *shMem;

    key = atoi(argv[1]);
    if (key == 0)
        exit(1);

    idShm = shmget(key, sizeof(shareMem), 0);
    if (idShm == -1)
        exit(1);

    shMem = shmat(idShm, NULL, 0);
    if (shMem == -1)
        exit(1);
    
    idSems = semget(key, 4, 0);
    if (idSems == -1)
        exit(1);

    // Producer
    struct sembuf *sopsEmpty, *sopsMutexProd, *sopsFull;
    sopsEmpty->sem_num = 2;
    sopsEmpty->sem_op = -1;
    sopsEmpty->sem_flg = 0;

    sopsMutexProd->sem_num = 0;
    sopsMutexProd->sem_op = -1;
    sopsMutexProd->sem_flg = 0;
    semop(idSems, sopsEmpty, 1);
    semop(idSems, sopsMutexProd, 1); // INPUT SECTION

    // CRITICAL SECTION
    shMem->buffer[shMem->end] = getpid();
    shMem->end = (shMem->end + 1) % SIZE_BUFFER;

    sopsMutexProd->sem_num = 0; // OUTPUT SECTION
    sopsMutexProd->sem_op = 1;
    sopsMutexProd->sem_flg = 0;

    sopsFull->sem_num = 3;
    sopsFull->sem_op = 1;
    sopsFull->sem_flg = 0;
    semop(idSems, sopsMutexProd, 1);
    semop(idSems, sopsFull, 1);

    return 0;
}