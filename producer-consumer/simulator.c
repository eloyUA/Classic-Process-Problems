#include <stdio.h>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <time.h>

#define SIZE_BUFFER 4096

typedef struct {
    int start, end;
    int buffer[SIZE_BUFFER];
} shareMem;

void errorSimulator(char *msgError) {
    char msg[512];
	snprintf(msg, sizeof(msg), "ERROR, in the simulator: %s\n", msgError);
	perror(msg);
	exit(1);
}

int main(int argc, char **argv) { // ./simulator key_t nProd nCons
    if (argc != 4)
        errorSimulator("The number of arguments must be 3");

    int key, nProc, nCons;
    int idShm;
    int idSems;
    shareMem *shMem;

    key = atoi(argv[1]);
    if (key == 0)
        errorSimulator("The key isn't valid");
    
    nProc = atoi(argv[2]);
    nCons = atoi(argv[3]);
    if (nProc <= 0 || nCons <= 0) {
        errorSimulator("The number of producers and consumers must be greather than 0.");
        shmctl(idShm, IPC_RMID, NULL);
        semctl(idSems, NULL, IPC_RMID);
    }

    idShm = shmget(key, sizeof(shareMem), IPC_CREAT | IPC_EXCL);
    if (idShm == -1)
        errorSimulator("The shared memory hasn't created.");

    shMem = shmat(idShm, NULL, 0);
    if (shMem == -1)
        errorSimulator("The simulator couldn't attach to the shared memory.");
    shMem->start = 0;
    shMem->end = 0;
    
    idSems = semget(key, 4, IPC_CREAT | IPC_EXCL | 0600);
    if (idSems == -1) {
        errorSimulator("The semaphores haven't created.");
        shmctl(idShm, IPC_RMID, NULL);
    }

    struct sembuf sops[4];
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
    semop(idSems, sops, 4);

    // Simulation
    srand(time(NULL));
    pid_t pidChild;
    int processType; // 0: Producer; 1: Consumer
    for (int i = 0; i < nProc + nCons; i++) {
        if (nProc > 0 && nCons > 0) {
            processType = rand() % 2;
        }
        else if (nProc > 0) {
            processType = 0;
        }
        else {
            processType = 1;
        }

        pidChild = fork();
        if (pidChild == 0) {
            if (processType == 0) { // Producer
                execl("./producer", "./producer", argv[1]);
            }
            else { // Consumer
                execl("./consumer", "./consumer", argv[1]);
            }
            exit(1);
        }
    }

    return 0;
}