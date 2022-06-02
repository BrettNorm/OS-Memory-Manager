#include "config.h"

int shmID;
int semID;
int cIndex;
int randNum;
int currentPID;
int reqeuestedAddr;
char strBuffer[200];
shmStruct* shmPtr = NULL;
unsigned long startNS, startS, waitingNS, waitS;

void setProcessSignals();

void setProcessSignals() {
        
    signal(SIGALRM, handleSignals);
    signal(SIGINT, handleSignals);
    signal(SIGSEGV, handleSignals);
    signal(SIGKILL, handleSignals); 
    signal(SIGTERM, handleSignals);
    
}

int main(int argc, char *argv[]) {

    currentPID = getpid();
    srand(currentPID * 12);

    setProcessSignals();
    if(makeSemaphore() == -1) {
        CleanOSS();
    }

    if(sharedMemory() == -1) {
        CleanOSS();
    }

    startNS = shmPtr->nsecs;
    startS = shmPtr->secs;

    cIndex = findPosition(currentPID);
    waitSem(MAXPROCESSES);
    signalSem(MAXPROCESSES);

    while (1) {

        if (shmPtr->processes[cIndex].pageCount > 31) {
            shmPtr->processes[cIndex].isDead = true;
            break;
        }

        /* offset address */
        reqeuestedAddr = ((rand() % 32) * 1024) + (rand() % 1023);
        randNum = rand() % 100;
        /* favoring reads over writes */
        if (randNum <= 30) {
            shmPtr->processes[cIndex].memoryType = WRITE;
        } else {
            shmPtr->processes[cIndex].memoryType = READ;
        }
        shmPtr->processes[cIndex].waitingFor = reqeuestedAddr;
        waitSem(cIndex);
        while (semctl(semID, cIndex, GETVAL, 0) != 1);
        randNum = rand() % 150;
        if (randNum = 20) {
            shmPtr->processes[cIndex].isDead = true;
            break;
        }
    }

    CleanOSS();
}




void CleanOSS() {
    shmdt(shmPtr);
    waitSem(19);
    exit(0);
}

int findPosition(int pid) {
    for (int i = 0; i < MAXPROCESSES; i++) {
        if (pid == shmPtr->currentPIDs[i]) {
            return i;
        }
    }
    return -1;
}

void handleSignals() {
    CleanOSS();
    exit(0);
}

int sharedMemory() {
    key_t shm_key = ftok("process.c", 'a');

    if((shmID = shmget(shm_key, (sizeof(frame) * 256) + sizeof(shmStruct) + (sizeof(pStats) * MAXPROCESSES) + (sizeof(page) * 256), IPC_EXCL)) == -1) {
        perror("oss.c: shmget");
        return -1;
    }

    if((shmPtr = (shmStruct*)shmat(shmID, 0, 0)) == (shmStruct*)-1) {
        perror("oss.c: shmat");
        return -1;
    }

    return 0;
}

void signalSem(int sem) {
    struct sembuf semV;
    semV.sem_op = 1;
    semV.sem_flg = 0;
    semV.sem_num = sem;
    semop(semID, &semV, 1);
}

void waitSem(int sem) {
    struct sembuf semV;
    semV.sem_op = -1;
    semV.sem_flg = 0;
    semV.sem_num = sem;
    semop(semID, &semV, 1);
}

int makeSemaphore() {
    key_t semKey = ftok("oss.c", 'a');

    if((semID = semget(semKey, 20, 0)) == -1) {
        perror("oss.c: semget");
        return -1;
    }
    return 0;
}

