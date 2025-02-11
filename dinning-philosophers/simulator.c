#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/time.h>
#include <time.h>
#include <signal.h>

#define MAX_PHILOSOPHERS 512

typedef int Fork;
typedef enum state { HUNGRY, EATING, THINKING } statePhilosopher;
typedef struct {
    int nPhilosophers;
    int nPhiloWating;
    int nPhiloEating;
    statePhilosopher states[MAX_PHILOSOPHERS];
    Fork buffer[MAX_PHILOSOPHERS];
} shareMem;

void errorSimulator(char *msgError) {
    char msg[512];
	snprintf(msg, sizeof(msg), "ERROR, in the simulatorPriorityReaders: %s\n", msgError);
	perror(msg);
	exit(1);
}

void createResources(int *idShm, int *idSems, shareMem **shMem, int key, int nPhilosophers) {
    *idShm = shmget(key, sizeof(shareMem), IPC_CREAT | IPC_EXCL);
    if (*idShm == -1)
        errorSimulator("The shared memory hasn't created.");

    *shMem = shmat(*idShm, NULL, 0);
    if (*shMem == (void *) -1) {
        shmctl(*idShm, IPC_RMID, NULL);
        errorSimulator("The simulator couldn't attach to the shared memory.");
    }
    (*shMem)->nPhilosophers = nPhilosophers;
    (*shMem)->nPhiloWating = nPhilosophers;
    (*shMem)->nPhiloEating = 0;
    for (int i = 0; i < nPhilosophers; i++) {
        (*shMem)->states[i] = HUNGRY;
    }

    *idSems = semget(key, nPhilosophers + 3, IPC_CREAT | IPC_EXCL | 0600);
    if (*idSems == -1) {
        shmctl(*idShm, IPC_RMID, NULL);
        errorSimulator("The semaphores haven't created.");
    }

    struct sembuf sops[nPhilosophers + 3]; // 0: mutex; 1: mutexNPhiloWating; 2: mutexNPhiloEating; 3: philo1; 4: philo2; ... ; N: philo(nPhilosophers)
    for (int i = 0; i < 3; i++) {
        sops[i].sem_num = i;
        sops[i].sem_op = 1;
        sops[i].sem_flg = 0;
    }
    
    for (int i = 3; i < nPhilosophers + 3; i++) {
        sops[i].sem_num = i;
        sops[i].sem_op = 0;
        sops[i].sem_flg = 0;
    }
    semop(*idSems, sops, nPhilosophers + 3);
}

void freeAllResources(int idSems, int idShm) {
    shmctl(idShm, IPC_RMID, NULL);
    semctl(idSems, 0, IPC_RMID);
}

int idPhiloSems(int p) {
    return p + 3;
}

int leftPhilo(int p, shareMem *shMem) {
    if (p - 1 == -1) {
        return shMem->nPhilosophers - 1;
    }
    return p - 1;
}

int rightPhilo(int p, shareMem *shMem) {
    if (p + 1 == shMem->nPhilosophers) {
        return 0;
    }
    return p + 1;
}

void think(int timeS) {
    usleep(timeS);
}

void takeForks(int idPhilo, int idSems, shareMem *shMem) {
    struct sembuf sopsMutex, sopsTaken;
    sopsMutex.sem_num = 0;
    sopsMutex.sem_op = -1;
    sopsMutex.sem_flg = 0;
    semop(idSems, &sopsMutex, 1);

    if (shMem->states[rightPhilo(idPhilo, shMem)] != EATING && shMem->states[leftPhilo(idPhilo, shMem)] != EATING) {
        shMem->states[idPhilo] = EATING;

        sopsTaken.sem_num = idPhiloSems(idPhilo);
        sopsTaken.sem_op = 1;
        sopsTaken.sem_flg = 0;
        semop(idSems, &sopsTaken, 1);
    }

    sopsMutex.sem_num = 0;
    sopsMutex.sem_op = 1;
    sopsMutex.sem_flg = 0;
    semop(idSems, &sopsMutex, 1);

    sopsTaken.sem_num = idPhiloSems(idPhilo);
    sopsTaken.sem_op = -1;
    sopsTaken.sem_flg = 0;
    semop(idSems, &sopsTaken, 1);
}

void eat(int timeS, int idPhilo, int idSems, shareMem *shMem) {
    struct sembuf sopsMutexNPhiloWating, sopsMutexNPhiloEating;

    sopsMutexNPhiloWating.sem_num = 1;
    sopsMutexNPhiloWating.sem_op = -1;
    sopsMutexNPhiloWating.sem_flg = 0;
    semop(idSems, &sopsMutexNPhiloWating, 1);
    shMem->nPhiloWating -= 1;
    sopsMutexNPhiloWating.sem_num = 1;
    sopsMutexNPhiloWating.sem_op = 1;
    sopsMutexNPhiloWating.sem_flg = 0;
    semop(idSems, &sopsMutexNPhiloWating, 1);

    sopsMutexNPhiloEating.sem_num = 2;
    sopsMutexNPhiloEating.sem_op = -1;
    sopsMutexNPhiloEating.sem_flg = 0;
    semop(idSems, &sopsMutexNPhiloEating, 1);
    shMem->nPhiloEating += 1;
    sopsMutexNPhiloEating.sem_num = 2;
    sopsMutexNPhiloEating.sem_op = 1;
    sopsMutexNPhiloEating.sem_flg = 0;
    semop(idSems, &sopsMutexNPhiloEating, 1);

    usleep(timeS);
    Fork fr = shMem->buffer[rightPhilo(idPhilo, shMem)];
    Fork fl = shMem->buffer[leftPhilo(idPhilo, shMem)];

    sopsMutexNPhiloEating.sem_num = 2;
    sopsMutexNPhiloEating.sem_op = -1;
    sopsMutexNPhiloEating.sem_flg = 0;
    semop(idSems, &sopsMutexNPhiloEating, 1);
    shMem->nPhiloEating -= 1;
    sopsMutexNPhiloEating.sem_num = 2;
    sopsMutexNPhiloEating.sem_op = 1;
    sopsMutexNPhiloEating.sem_flg = 0;
    semop(idSems, &sopsMutexNPhiloEating, 1);
}

void test(int idPhilo, int idSems, shareMem *shMem) {
    struct sembuf sopsTaken;
    if (shMem->states[idPhilo] == HUNGRY && shMem->states[rightPhilo(idPhilo, shMem)] != EATING && shMem->states[leftPhilo(idPhilo, shMem)] != EATING) {
        shMem->states[idPhilo] = EATING;
        
        sopsTaken.sem_num = idPhiloSems(idPhilo);
        sopsTaken.sem_op = 1;
        sopsTaken.sem_flg = 0;
        semop(idSems, &sopsTaken, 1);
    }
}

void putForks(int idPhilo, int idSems, shareMem *shMem) {
    struct sembuf sopsMutex;
    sopsMutex.sem_num = 0;
    sopsMutex.sem_op = -1;
    sopsMutex.sem_flg = 0;
    semop(idSems, &sopsMutex, 1);

    shMem->states[idPhilo] = THINKING;
    test(rightPhilo(idPhilo, shMem), idSems, shMem);
    test(leftPhilo(idPhilo, shMem), idSems, shMem);

    sopsMutex.sem_num = 0;
    sopsMutex.sem_op = 1;
    sopsMutex.sem_flg = 0;
    semop(idSems, &sopsMutex, 1);
}

void philosopher(int idPhilo, int idSems, shareMem *shMem, int timeS) {
    think(timeS);
    takeForks(idPhilo, idSems, shMem);
    eat(timeS, idPhilo, idSems, shMem);
    putForks(idPhilo, idSems, shMem);
}

void createProcess(int *pids, int nPhilosophers, int minMicroSec, int maxMicroSec, shareMem *shMem, int idSems) {
    printf("Creating process...\n");
    shMem->nPhilosophers = nPhilosophers;

    pid_t pidChild;
    for (int i = 0; i < nPhilosophers; i++) {
        pidChild = fork();
        if (pidChild == 0) {
            kill(getpid(), SIGSTOP);
            philosopher(i, idSems, shMem, rand() % (maxMicroSec - minMicroSec + 1) + minMicroSec);
            exit(0);
        }
    }
}

void simulate(int *pids, int nPhilosophers, shareMem *shMem, int idSems, int idShm) {
    pid_t pid = fork();
    if (pid == -1) {
        errorSimulator("The simulation cannot be run.");
        freeAllResources(idSems, idShm);
        kill(getpid(), SIGINT);
    }
    else if (pid == 0) {
        for (int i = 0; i < nPhilosophers; i++) {
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
        
        printf("Philosophers process waiting: %d\n", shMem->nPhiloWating);
        printf("Philosophers process eating: %d\n", shMem->nPhiloEating);
        printf("Size buffer: %d\n", shMem->nPhilosophers);
        
        gettimeofday(&time_end, NULL);
        diff = (time_end.tv_sec - time_start.tv_sec) + (time_end.tv_usec - time_start.tv_usec) / 1000000.0;
        printf("Time: %.2lfseg\n", diff);
    } while (shMem->nPhiloWating > 0 || shMem->nPhiloEating > 0);

    freeAllResources(idSems, idShm);
}

/**
 * DESCRIPTION:
 *      Arguments: ./simulator key_t nPhilosophers minMicroSec maxMicroSec
 *          key_t: The key for shared memory and semaphores
 *          nPhilosophers: Number of philosophers process
 *          minMicroSec: The minimum micro seconds that the philosophers process waiting when eating or thinking
 *          maxMicroSec: The maximum ...
 * EXAMPLE:
 *      Key: 123
 *      Number of the philosophers process = 300
 *      Time waiting readers process = [20ms, 50ms]
 * 
 *      sudo ./sim 123 300 20000 50000
 */
int main(int argc, char **argv) {
    if (argc != 5)
        errorSimulator("The number of arguments must be 4");

    int key;
    int nPhilosophers, minMicroSec, maxMicroSec;

    key = atoi(argv[1]);
    if (key == 0)
        errorSimulator("The key isn't valid");

    nPhilosophers = atoi(argv[2]);
    minMicroSec = atoi(argv[3]);
    maxMicroSec = atoi(argv[4]);
    if (nPhilosophers == 0 || minMicroSec == 0 || maxMicroSec == 0)
        errorSimulator("Args invalid.");
    
    if (nPhilosophers > MAX_PHILOSOPHERS)
        errorSimulator("The number of philosophers process is greather to MAX_PHILOSOPHERS");

    int idShm;
    int idSems;
    shareMem *shMem;
    int *pids = (int *) (malloc(nPhilosophers * sizeof(int)));

    srand(time(NULL));
    createResources(&idShm, &idSems, &shMem, key, nPhilosophers);
    createProcess(pids, nPhilosophers, minMicroSec, maxMicroSec, shMem, idSems);
    simulate(pids, nPhilosophers, shMem, idSems, idShm);

    return 0;
}