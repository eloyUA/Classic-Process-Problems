#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/time.h>
#include <time.h>

#define SIZE_BUFFER 4096

typedef struct {
    int nProc, nCons;
    int start, end;
    int products;
    int buffer[SIZE_BUFFER];
} shareMem;

void errorSimulator(char *msgError) {
    char msg[512];
	snprintf(msg, sizeof(msg), "ERROR, in the simulator: %s\n", msgError);
	perror(msg);
	exit(1);
}

void createResources(int *idShm, int *idSems, shareMem **shMem, int key) {
    *idShm = shmget(key, sizeof(shareMem), IPC_CREAT | IPC_EXCL);
    if (*idShm == -1)
        errorSimulator("The shared memory hasn't created.");

    *shMem = shmat(*idShm, NULL, 0);
    if (*shMem == (void *) -1) {
        shmctl(*idShm, IPC_RMID, NULL);
        errorSimulator("The simulator couldn't attach to the shared memory.");
    }
    (*shMem)->start = 0;
    (*shMem)->end = 0;
    (*shMem)->products = 0;
    (*shMem)->nProc = 0;
    (*shMem)->nCons = 0;
    
    *idSems = semget(key, 6, IPC_CREAT | IPC_EXCL | 0600);
    if (*idSems == -1) {
        shmctl(*idShm, IPC_RMID, NULL);
        errorSimulator("The semaphores haven't created.");
    }

    /**
     * In "struct sembuf sops[6]":
     *  0: mutexProducer
     *  1: mutexConsumer
     *  2: numberEmpty
     *  3: numberFull
     *  4: mutexNProd
     *  5: mutexNCons
     */
    struct sembuf sops[6];
    sops[0].sem_num = 0;
    sops[0].sem_op = 1;
    sops[0].sem_flg = 0;

    sops[1].sem_num = 1;
    sops[1].sem_op = 1;
    sops[1].sem_flg = 0;

    sops[2].sem_num = 2;
    sops[2].sem_op = SIZE_BUFFER;
    sops[2].sem_flg = 0;

    sops[3].sem_num = 3;
    sops[3].sem_op = 0;
    sops[3].sem_flg = 0;

    sops[4].sem_num = 4;
    sops[4].sem_op = 1;
    sops[4].sem_flg = 0;

    sops[5].sem_num = 5;
    sops[5].sem_op = 1;
    sops[5].sem_flg = 0;
    semop(*idSems, sops, 6);
}

void freeAllResources(int idSems, int idShm) {
    shmctl(idShm, IPC_RMID, NULL);
    semctl(idSems, 0, IPC_RMID);
}

void producer(int idSems, shareMem *shMem, int timeS) {
    struct sembuf sopsEmpty, sopsMutexProd, sopsFull, sopsMutexNProd;
    sopsEmpty.sem_num = 2;
    sopsEmpty.sem_op = -1;
    sopsEmpty.sem_flg = 0;

    sopsMutexProd.sem_num = 0;
    sopsMutexProd.sem_op = -1;
    sopsMutexProd.sem_flg = 0;
    semop(idSems, &sopsEmpty, 1);
    semop(idSems, &sopsMutexProd, 1); // INPUT SECTION

    // CRITICAL SECTION
    usleep(timeS);
    shMem->buffer[shMem->end] = getpid();
    shMem->end = (shMem->end + 1) % SIZE_BUFFER;
    shMem->products = shMem->products + 1;

    sopsMutexProd.sem_num = 0; // OUTPUT SECTION
    sopsMutexProd.sem_op = 1;
    sopsMutexProd.sem_flg = 0;

    sopsFull.sem_num = 3;
    sopsFull.sem_op = 1;
    sopsFull.sem_flg = 0;
    semop(idSems, &sopsMutexProd, 1);
    semop(idSems, &sopsFull, 1);

    sopsMutexNProd.sem_num = 4;
    sopsMutexNProd.sem_op = -1;
    sopsMutexNProd.sem_flg = 0;
    semop(idSems, &sopsMutexNProd, 1);
    shMem->nProc = shMem->nProc - 1;
    sopsMutexNProd.sem_num = 4;
    sopsMutexNProd.sem_op = 1;
    sopsMutexNProd.sem_flg = 0;
    semop(idSems, &sopsMutexNProd, 1);

    shmdt(shMem);
    exit(0);
}

void consumer(int idSems, shareMem *shMem, int timeS) {
    struct sembuf sopsEmpty, sopsMutexCons, sopsFull, sopsMutexNCons;
    sopsFull.sem_num = 3;
    sopsFull.sem_op = -1;
    sopsFull.sem_flg = 0;

    sopsMutexCons.sem_num = 1;
    sopsMutexCons.sem_op = -1;
    sopsMutexCons.sem_flg = 0;
    semop(idSems, &sopsFull, 1);
    semop(idSems, &sopsMutexCons, 1); // INPUT SECTION

    // CRITICAL SECTION
    usleep(timeS);
    int pid = shMem->buffer[shMem->start]; // read
    shMem->start = (shMem->start + 1) % SIZE_BUFFER;
    shMem->products = shMem->products - 1;

    sopsMutexCons.sem_num = 1; // OUTPUT SECTION
    sopsMutexCons.sem_op = 1;
    sopsMutexCons.sem_flg = 0;

    sopsEmpty.sem_num = 2;
    sopsEmpty.sem_op = 1;
    sopsEmpty.sem_flg = 0;
    semop(idSems, &sopsMutexCons, 1);
    semop(idSems, &sopsEmpty, 1);

    // nCons--
    sopsMutexNCons.sem_num = 5;
    sopsMutexNCons.sem_op = -1;
    sopsMutexNCons.sem_flg = 0;
    semop(idSems, &sopsMutexNCons, 1);
    shMem->nCons = shMem->nCons - 1;
    sopsMutexNCons.sem_num = 5;
    sopsMutexNCons.sem_op = 1;
    sopsMutexNCons.sem_flg = 0;
    semop(idSems, &sopsMutexNCons, 1);

    shmdt(shMem);
    exit(0);
}

void createProcess(int *pids, int nProd, int minMicroSecProd, int maxMicroSecProd,
                    int nCons, int minMicroSecCons, int maxMicroSecCons,
                    shareMem *shMem, int idSems) {
    printf("Creating process...\n");
    srand(time(NULL));
    pid_t pidChild;
    int processType, i, cont; // 0: Producer; 1: Consumer

    i = 0;
    cont = 0;
    shMem->nProc = nProd;
    shMem->nCons = nCons;
    while (nProd > 0 && nCons > 0) {
        processType = rand() % 2;
        pidChild = fork();
        if (pidChild == 0) {
            kill(getpid(), SIGSTOP);
            if (processType == 0) { // Producer
                producer(
                    idSems,
                    shMem,
                    rand() % (maxMicroSecProd - minMicroSecProd + 1) + minMicroSecProd
                );
            }
            else { // Consumer
                consumer(
                    idSems,
                    shMem,
                    rand() % (maxMicroSecCons - minMicroSecCons + 1) + minMicroSecCons
                );
            }
        }
        else if (pidChild != -1) {
            if (processType == 0) {
                nProd--;
            }
            else {
                nCons--;
            }
            pids[cont] = pidChild;
            cont++;
        }
    }

    while (nProd > 0) {
        pidChild = fork();
        if (pidChild == 0) {
            kill(getpid(), SIGSTOP);
            producer(
                idSems,
                shMem,
                rand() % (maxMicroSecProd - minMicroSecProd + 1) + minMicroSecProd
            );
        }
        else if (pidChild != -1) {
            pids[cont] = pidChild;
            cont++;
            nProd--;
        }
    }

    while (nCons > 0) {
        pidChild = fork();
        if (pidChild == 0) {
            kill(getpid(), SIGSTOP);
            consumer(
                idSems,
                shMem,
                rand() % (maxMicroSecCons - minMicroSecCons + 1) + minMicroSecCons
            );
        } 
        else if (pidChild != -1) {
            pids[cont] = pidChild;
            cont++;
            nCons--;
        }
    }
}

void simulate(int *pids, int nProd, int nCons, shareMem *shMem, int idSems, int idShm) {
    pid_t pid = fork();
    if (pid == -1) {
        errorSimulator("The simulation cannot be run.");
        freeAllResources(idSems, idShm);
        kill(getpid(), SIGINT);
    }
    else if (pid == 0) {
        for (int i = 0; i < nProd + nCons; i++) {
            kill(pids[i], SIGCONT);
        }
        exit(0);
    }

    struct timeval time_start, time_end;
    double diff;

    gettimeofday(&time_start, NULL);
    do {
        usleep(50000);
        system("clear");
        printf("-------------------------------\n");
        printf("   Simulator\n");
        printf("-------------------------------\n");
        
        printf("Pos. start: %d; Pos. end: %d\n", shMem->start, shMem->end);
        printf("Products: %d; Max. products: %d\n", shMem->products, SIZE_BUFFER);
        printf("Producers process: %d; Consumer process: %d\n", shMem->nProc, shMem->nCons);
        
        gettimeofday(&time_end, NULL);
        diff = (time_end.tv_sec - time_start.tv_sec);
        diff += (time_end.tv_usec - time_start.tv_usec) / 1000000.0;
        printf("Time: %.2lfseg\n", diff);
    } while (!(shMem->nCons > 0 && shMem->nProc == 0 && shMem->products == 0) &&
            !(shMem->nProc > 0 && shMem->nCons == 0 && shMem->products == SIZE_BUFFER)
            && (shMem->nProc > 0 || shMem->nCons > 0));

    int result;
    for (int i = 0; i < nProd + nCons; i++) {
        result = waitpid(pids[i], NULL, WNOHANG);
        if (result == 0) { // The child hasn't finished => Finish
            kill(pids[i], SIGINT);
        }
    }
    freeAllResources(idSems, idShm);
}

/**
 * DESCRIPTION:
 *      Arguments: ./simulator key_t nProd minMicroSecProd maxMicroSecProd nCons
 *                             minMicroSecCons maxMicroSecCons
 *          key_t: The key for shared memory and semaphores
 *          nProd: Number of producer process
 *          minMicroSecProd: The minimum micro seconds that the producer process
 *                           waiting when the product is put in the buffer
 *          maxMicroSecProd: The maximum ...
 *          nCons: Number of consumer process
 *          minMicroSecCons: The minimum micro seconds that the consumer process
 *                           waiting when the product is catch in the buffer
 *          maxMicroSecCons: The maximum ...
 * EXAMPLE:
 *      Key: 123
 *      Number of the producer process = 1200
 *      Time waiting producer process = [25ms, 50ms]
 *      Number of the consumer process = 1000
 *      Time waiting consumer process = [35ms, 60ms]
 * 
 *      ./simulator 123 1200 25000 50000 1000 35000 60000
 */
int main(int argc, char **argv) {
    if (argc != 8)
        errorSimulator("The number of arguments must be 7");

    int key;
    int nProd, minMicroSecProd, maxMicroSecProd;
    int nCons, minMicroSecCons, maxMicroSecCons;

    key = atoi(argv[1]);
    if (key == 0)
        errorSimulator("The key isn't valid");

    nProd = atoi(argv[2]);
    minMicroSecProd = atoi(argv[3]);
    maxMicroSecProd = atoi(argv[4]);
    if (nProd == 0 || minMicroSecProd == 0 || maxMicroSecProd == 0)
        errorSimulator("Producer args invalid.");

    nCons = atoi(argv[5]);
    minMicroSecCons = atoi(argv[6]);
    maxMicroSecCons = atoi(argv[7]);
    if (nCons == 0 || minMicroSecCons == 0 || maxMicroSecCons == 0)
        errorSimulator("Consumer args invalid.");

    int idShm;
    int idSems;
    shareMem *shMem;
    int *pids = (int *) (malloc((nProd + nCons) * sizeof(int)));

    createResources(&idShm, &idSems, &shMem, key);
    createProcess(pids, nProd, minMicroSecProd, maxMicroSecProd, nCons,
        minMicroSecCons, maxMicroSecCons, shMem, idSems);
    simulate(pids, nProd, nCons, shMem, idSems, idShm);

    return 0;
}