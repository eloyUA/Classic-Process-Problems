#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/time.h>
#include <time.h>

#define SIZE_BUFFER 512

typedef struct {
    int nReadersProcess;
    int nWritersProcess;
    int nReadersReading;
    int nWritersWriting; // Only 0 or 1
    int buffer[SIZE_BUFFER];
} shareMem;

void errorSimulator(char *msgError) {
    char msg[512];
	snprintf(msg, sizeof(msg), "ERROR, in the simulatorPriorityReaders: %s\n", msgError);
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
    (*shMem)->nReadersProcess = 0;
    (*shMem)->nWritersProcess = 0;
    (*shMem)->nReadersReading = 0;
    (*shMem)->nWritersWriting = 0;
    
    *idSems = semget(key, 4, IPC_CREAT | IPC_EXCL | 0600);
    if (*idSems == -1) {
        shmctl(*idShm, IPC_RMID, NULL);
        errorSimulator("The semaphores haven't created.");
    }

    struct sembuf sops[4]; // 0: mutexReaders; 1: mutexWriters; 2: mutexNReaders; 3: mutexNWriters
    for (int i = 0; i < 4; i++) {
        sops[i].sem_num = i;
        sops[i].sem_op = 1;
        sops[i].sem_flg = 0;
    }
    semop(*idSems, sops, 4);
}

void freeAllResources(int idSems, int idShm) {
    shmctl(idShm, IPC_RMID, NULL);
    semctl(idSems, 0, IPC_RMID);
}

void reader(int idSems, shareMem *shMem, int timeS) {
    struct sembuf sopsMutexReader, sopsMutexWriter, sopsMutexNReader;

    sopsMutexReader.sem_num = 0;
    sopsMutexReader.sem_op = -1;
    sopsMutexReader.sem_flg = 0;
    semop(idSems, &sopsMutexReader, 1); // INPUT SECTION

    if (shMem->nReadersReading == 0) { // CRITICAL SECTION
        sopsMutexWriter.sem_num = 1;
        sopsMutexWriter.sem_op = -1;
        sopsMutexWriter.sem_flg = 0;
        semop(idSems, &sopsMutexWriter, 1);
    }
    shMem->nReadersReading++;

    sopsMutexReader.sem_num = 0; // OUTPUT SECTION
    sopsMutexReader.sem_op = 1;
    sopsMutexReader.sem_flg = 0;
    semop(idSems, &sopsMutexReader, 1);
    
    // Reading...
    usleep(timeS);
    int valueRead = shMem->buffer[rand() % SIZE_BUFFER];

    sopsMutexReader.sem_num = 0;
    sopsMutexReader.sem_op = -1;
    sopsMutexReader.sem_flg = 0;
    semop(idSems, &sopsMutexReader, 1); // INPUT SECTION
    
    shMem->nReadersReading--; // CRITICAL SECTION
    if (shMem->nReadersReading == 0) {
        sopsMutexWriter.sem_num = 1;
        sopsMutexWriter.sem_op = 1;
        sopsMutexWriter.sem_flg = 0;
        semop(idSems, &sopsMutexWriter, 1);
    }

    sopsMutexReader.sem_num = 0; // OUTPUT SECTION
    sopsMutexReader.sem_op = 1;
    sopsMutexReader.sem_flg = 0;
    semop(idSems, &sopsMutexReader, 1);

    // Section for nReadersProcess--
    sopsMutexNReader.sem_num = 2;
    sopsMutexNReader.sem_op = -1;
    sopsMutexNReader.sem_flg = 0;
    semop(idSems, &sopsMutexNReader, 1); // INPUT SECTION

    shMem->nReadersProcess--; // CRITICAL SECTION

    sopsMutexNReader.sem_num = 2; // OUTPUT SECTION
    sopsMutexNReader.sem_op = 1;
    sopsMutexNReader.sem_flg = 0;
    semop(idSems, &sopsMutexNReader, 1);

    shmdt(shMem);
    exit(0);
}

void writer(int idSems, shareMem *shMem, int timeS) {
    struct sembuf sopsMutexWriter, sopsMutexNWriter;
    
    sopsMutexWriter.sem_num = 1;
    sopsMutexWriter.sem_op = -1;
    sopsMutexWriter.sem_flg = 0;
    semop(idSems, &sopsMutexWriter, 1); // INPUT SECTION

    shMem->nWritersWriting = 1; // CRITICAL SECTION
    usleep(timeS);
    shMem->buffer[rand() % SIZE_BUFFER] = getpid();
    shMem->nWritersWriting = 0;

    sopsMutexWriter.sem_num = 1; // OUTPUT SECTION
    sopsMutexWriter.sem_op = 1;
    sopsMutexWriter.sem_flg = 0;
    semop(idSems, &sopsMutexWriter, 1);

    // Section for nWritersProcess--
    sopsMutexNWriter.sem_num = 3;
    sopsMutexNWriter.sem_op = -1;
    sopsMutexNWriter.sem_flg = 0;
    semop(idSems, &sopsMutexNWriter, 1); // INPUT SECTION

    shMem->nWritersProcess--;

    sopsMutexNWriter.sem_num = 3; // OUTPUT SECTION
    sopsMutexNWriter.sem_op = 1;
    sopsMutexNWriter.sem_flg = 0;
    semop(idSems, &sopsMutexNWriter, 1);

    shmdt(shMem);
    exit(0);
}

void createProcess(int *pids, int nReaders, int minMicroSecReaders, int maxMicroSecReaders, int nWriters, int minMicroSecWriters, int maxMicroSecWriters, shareMem *shMem, int idSems) {
    printf("Creating process...\n");
    shMem->nReadersProcess = nReaders;
    shMem->nWritersProcess = nWriters;

    int end_pos = nReaders + nWriters - 1;
    int *randomPositions = (int *) (malloc(sizeof(nReaders + nWriters)));
    if (randomPositions == NULL)
        errorSimulator("There isn't memory in the system for malloc");
    
    for (int i = 0; i < nReaders + nWriters; i++) {
        randomPositions[i] = i;
    }

    int pos, aux;
    pid_t pidChild;
    while (nReaders > 0) {
        pidChild = fork();
        if (pidChild == 0) {
            kill(getpid(), SIGSTOP);
            reader(idSems, shMem, rand() % (maxMicroSecReaders - minMicroSecReaders + 1) + minMicroSecReaders);
        }
        else if (pidChild != -1) {
            pos = rand() % (end_pos + 1);
            pids[randomPositions[pos]] = pidChild;

            aux = randomPositions[pos];
            randomPositions[pos] = randomPositions[end_pos];
            randomPositions[end_pos] = aux;
            end_pos--;
            nReaders--;
        }
    }

    while (nWriters > 0) {
        pidChild = fork();
        if (pidChild == 0) {
            kill(getpid(), SIGSTOP);
            writer(idSems, shMem, rand() % (maxMicroSecWriters - minMicroSecWriters + 1) + minMicroSecWriters);
        }
        else if (pidChild != -1) {
            pos = rand() % (end_pos + 1);
            pids[randomPositions[pos]] = pidChild;

            aux = randomPositions[pos];
            randomPositions[pos] = randomPositions[end_pos];
            randomPositions[end_pos] = aux;
            end_pos--;
            nWriters--;
        }
    }

    free(randomPositions);
}

void simulate(int *pids, int nReaders, int nWriters, shareMem *shMem, int idSems, int idShm) {
    pid_t pid = fork();
    if (pid == -1) {
        errorSimulator("The simulation cannot be run.");
        freeAllResources(idSems, idShm);
        kill(getpid(), SIGINT);
    }
    else if (pid == 0) {
        for (int i = 0; i < nReaders + nWriters; i++) {
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
        
        printf("Readers process reading: %d\n", shMem->nReadersReading);
        printf("Writers process writing: %d\n", shMem->nWritersWriting);
        printf("Size buffer: %d\n", SIZE_BUFFER);
        printf("Readers process: %d; Writers process: %d\n", shMem->nReadersProcess, shMem->nWritersProcess);
        
        gettimeofday(&time_end, NULL);
        diff = (time_end.tv_sec - time_start.tv_sec) + (time_end.tv_usec - time_start.tv_usec) / 1000000.0;
        printf("Time: %.2lfseg\n", diff);
    } while (shMem->nReadersProcess > 0 || shMem->nWritersProcess > 0);

    freeAllResources(idSems, idShm);
}

/**
 * DESCRIPTION:
 *      Arguments: ./simulator key_t nReaders minMicroSecReaders maxMicroSecReaders nWriters minMicroSecWriters maxMicroSecWriters
 *          key_t: The key for shared memory and semaphores
 *          nReaders: Number of readers process
 *          minMicroSecReaders: The minimum micro seconds that the readers process waiting when reading buffer
 *          maxMicroSecReaders: The maximum ...
 *          nWriters: Number of writers process
 *          minMicroSecWriters: The minimum micro seconds that the writers process waiting when writing a value from buffer
 *          maxMicroSecWriters: The maximum ...
 * EXAMPLE:
 *      Key: 123
 *      Number of the readers process = 3000
 *      Time waiting readers process = [8s, 10s]
 *      Number of the writers process = 2000
 *      Time waiting writers process = [1ms, 5ms]
 * 
 *      sudo ./sim 123 3000 8000000 10000000 2000 1000 5000
 */
int main(int argc, char **argv) {
    if (argc != 8)
        errorSimulator("The number of arguments must be 7");

    int key;
    int nReaders, minMicroSecReaders, maxMicroSecReaders;
    int nWriters, minMicroSecWriters, maxMicroSecWriters;

    key = atoi(argv[1]);
    if (key == 0)
        errorSimulator("The key isn't valid");

    nReaders = atoi(argv[2]);
    minMicroSecReaders = atoi(argv[3]);
    maxMicroSecReaders = atoi(argv[4]);
    if (nReaders == 0 || minMicroSecReaders == 0 || maxMicroSecReaders == 0)
        errorSimulator("Readers args invalid.");

    nWriters = atoi(argv[5]);
    minMicroSecWriters = atoi(argv[6]);
    maxMicroSecWriters = atoi(argv[7]);
    if (nWriters == 0 || minMicroSecWriters == 0 || maxMicroSecWriters == 0)
        errorSimulator("Writers args invalid.");

    int idShm;
    int idSems;
    shareMem *shMem;
    int *pids = (int *) (malloc((nReaders + nWriters) * sizeof(int)));

    srand(time(NULL));
    createResources(&idShm, &idSems, &shMem, key);
    createProcess(pids, nReaders, minMicroSecReaders, maxMicroSecReaders, nWriters, minMicroSecWriters, maxMicroSecWriters, shMem, idSems);
    simulate(pids, nReaders, nWriters, shMem, idSems, idShm);

    return 0;
}