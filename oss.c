#include "config.h"

int shmID = 0;
int semID = 0;
int pFaults = 0;
int procVar = 0;
int pageVar = 0;
int frameVar = 0;
int prevPrint = 0;
int forkCounter = 0;
char strBuffer[200];
FILE* logPtr = NULL;
int processCounter = 0;
int logLineCounter = 0;
int occupiedFrames = 0;
int memoryAccesses = 0;
unsigned long waitNS = 0;
shmStruct* shmPtr = NULL;
unsigned int frameNum = 0; 
unsigned long sToFork = 0;
unsigned long nsToFork = 0;

int main(int argc, char *argv[]) {
    alarm(2);
    srand(time(NULL));
    setSignals();

    /* proactive measures for zombies */
    struct sigaction siga;
    memset(&siga, 0, sizeof(siga));
    siga.sa_handler = preventZombies;
    sigaction(SIGCHLD, &siga, NULL);

    logPtr = fopen("systemlog.txt", "w");

    if(makeSemaphore() == -1) {
        CleanOSS();
    }

    if(sharedMemory() == -1) {
        CleanOSS();
    }

    semctl(semID, 19, SETVAL, 1);
    semctl(semID, MAXPROCESSES, SETVAL, 1);

    for(int index = 0; index < MAXPROCESSES; index++) {
        semctl(semID, index, SETVAL, 1);
    }
    

    /* initializing necessary process info */    
    for(procVar = 0; procVar < MAXPROCESSES; procVar++) {

        shmPtr->currentPIDs[procVar] = 0;
        shmPtr->processes[procVar].waitingFor = 0;
        shmPtr->processes[procVar].isDead = false;
        shmPtr->processes[procVar].pageCount = 0;
        shmPtr->processes[procVar].pageIndex = 0;

        for(pageVar = 0; pageVar < 32; pageVar++) {
            shmPtr->processes[procVar].pageTable[pageVar].currentFrame = -1; 
            shmPtr->processes[procVar].pageTable[pageVar].pageAddr = 0;
        }
        
    }

    shmPtr->nsecs = 0;
    shmPtr->secs = 0;
    shmPtr->nextFlag = 0;
    
    /* frame */
    for(frameVar = 0; frameVar < 256; frameVar++) {
        shmPtr->frameTable[frameVar].currentProcess = 0;
        shmPtr->frameTable[frameVar].dirtyBit = 0;
        shmPtr->frameTable[frameVar].frameAddr = 0;
    }
    
    /* project driver */
    while(1) {
        if (shmPtr->secs > prevPrint) {
            prevPrint = shmPtr->secs;
        }

        /* checking for dead processes and cleaning if so */
        for (int a = 0; a < MAXPROCESSES; a++) {
            if (shmPtr->processes[a].isDead == true) {
                shmPtr->processes[a].memoryType = 0;
                shmPtr->processes[a].pageCount = 0;
                shmPtr->processes[a].waitingFor = 0;
                shmPtr->processes[a].pageIndex = 0;
                
                shmPtr->processes[a].isDead = false;
                semctl(semID, a, SETVAL, 1);
                for (int b = 0; b < 32; b++) {
                    if (shmPtr->processes[a].pageTable[b].currentFrame != -1) {
                        shmPtr->frameTable[shmPtr->processes[a].pageTable[b].currentFrame].currentProcess = 0;
                        shmPtr->frameTable[shmPtr->processes[a].pageTable[b].currentFrame].dirtyBit = 0;
                        shmPtr->frameTable[shmPtr->processes[a].pageTable[b].currentFrame].frameAddr = 0;
                    }
                }
                shmPtr->currentPIDs[a] = 0;
            }
        }

        /* looking for deadlock */
        unsigned int waitingCount = 0;
        for (int a = 0; a < MAXPROCESSES; a++) {
            if (semctl(semID, a, GETVAL, 0) == 0) {
                waitingCount++;
            }
        }
        if (waitingCount >= MAXPROCESSES) {
            printf("MAXPROCESSES waiting processes, pushing clock\n");
            shmPtr->secs += 3;
        }

        /* fork if we waited long enough */
        if (sToFork == 0 && nsToFork == 0) {
            doFork();
        } else {
            if (didWait) {
                int semVarCount = semctl(semID, 19, GETVAL, 0);

                if (semVarCount < MAXPROCESSES) {
                    doFork();
                }
            }
        }

        /* check occupied frameTable */
        occupiedFrames = 0;
        int tempVar = shmPtr->nextFlag;

        for (int i = 0; i < MAXPROCESSES; i++) {
            occupiedFrames += shmPtr->processes[i].pageCount;
        }

        for (int i = 0; i < MAXPROCESSES; i++) {
            int tempVar = semctl(semID, i, GETVAL, 0);
            if (tempVar == 0) {
                if (occupiedFrames < 256) {
                    emptyFrames(i);     
                } else {
                    noEmptyFrames(i);
                }
            }
        }
        waitSem(MAXPROCESSES);
        waitNS = (rand() % 370000);
        shmPtr->nsecs += waitNS;
        nsToSec();
        signalSem(MAXPROCESSES);
    }

    return 0;
}

void emptyFrames(int i) {

    if (shmPtr->processes[i].memoryType == READ) {
                        
        sprintf(strBuffer, "P%d requesting read of address %d at time %d : %d\n", i, shmPtr->processes[i].waitingFor, shmPtr->secs, shmPtr->nsecs);
        logOutput(strBuffer);
        memoryAccesses++;
            /* if is in frame */
        if (isInTable(shmPtr->processes[i].waitingFor)) {

           
            sprintf(strBuffer, "Address %d in frame table, giving data to P%d at time %d : %d\n", shmPtr->processes[i].waitingFor, i, shmPtr->secs, shmPtr->nsecs);
            logOutput(strBuffer);
            memoryAccesses++;
            signalSem(i);

            /* if not in frame */
        } else {

            pFaults++;
            sprintf(strBuffer, "Address %d is not in a frame, pagefault\n", shmPtr->processes[i].waitingFor);
            logOutput(strBuffer);

            shmPtr->frameTable[frameNum].frameAddr = shmPtr->processes[i].waitingFor;
            shmPtr->frameTable[frameNum].dirtyBit = 0;
            shmPtr->frameTable[frameNum].currentProcess = i;
            
            sprintf(strBuffer, "Address %d in frame %d, giving data to P%d at time %d : %d\n", shmPtr->processes[i].waitingFor, frameNum, i, shmPtr->secs, shmPtr->nsecs);
            logOutput(strBuffer);

            shmPtr->processes[i].pageTable[shmPtr->processes[i].pageIndex].pageAddr = shmPtr->processes[i].waitingFor;
            shmPtr->processes[i].pageTable[shmPtr->processes[i].pageIndex].currentFrame = frameNum;
            shmPtr->processes[i].pageCount++;
            shmPtr->processes[i].pageIndex += 1;
            occupiedFrames++;
            memoryAccesses++;


            frameNum = nextFrame(frameNum); /* FIFO */
            signalSem(i);
        }

    } else {

        sprintf(strBuffer, "P%d requesting write of address %d at time %d:%d\n", i, shmPtr->processes[i].waitingFor, shmPtr->secs, shmPtr->nsecs);
        logOutput(strBuffer);

            /* if in frame */
        if (isInTable(shmPtr->processes[i].waitingFor)) {
            
            sprintf(strBuffer, "Address %d in frame table, writing data to frame at time %d:%d\n", shmPtr->processes[i].waitingFor, shmPtr->secs, shmPtr->nsecs);
            logOutput(strBuffer);
            memoryAccesses++;
            signalSem(i);


            /* if not in frame */
        } else {


            pFaults++;
            sprintf(strBuffer, "Address %d is not in a frame, pagefault\n", shmPtr->processes[i].waitingFor);
            logOutput(strBuffer);
            

            shmPtr->frameTable[frameNum].currentProcess = i;
            shmPtr->frameTable[frameNum].frameAddr = shmPtr->processes[i].waitingFor;
            shmPtr->nsecs += 50000;
            nsToSec();

            sprintf(strBuffer, "Address %d in frame %d, writing data to frame at time %d:%d\n", shmPtr->processes[i].waitingFor, frameNum, shmPtr->secs, shmPtr->nsecs);
            logOutput(strBuffer);
            shmPtr->frameTable[frameNum].dirtyBit = 1;
            sprintf(strBuffer, "Dirty bit of frame %d set, adding additional time to the clock\n", frameNum);
            logOutput(strBuffer);

            shmPtr->nsecs += 250;
            nsToSec();

            memoryAccesses++;
            shmPtr->processes[i].pageTable[shmPtr->processes[i].pageIndex].pageAddr = shmPtr->processes[i].waitingFor;
            shmPtr->processes[i].pageTable[shmPtr->processes[i].pageIndex].currentFrame = frameNum;
            shmPtr->processes[i].pageCount++;
            shmPtr->processes[i].pageIndex += 1;
            occupiedFrames++;

            frameNum = nextFrame(frameNum);
            signalSem(i);
            printBits();
        }
    }

}

void noEmptyFrames(int i) {

    if (shmPtr->processes[i].memoryType == READ) {
        sprintf(strBuffer, "P%d requesting read of address %d at time %d:%d\n", i, shmPtr->processes[i].waitingFor, shmPtr->secs, shmPtr->nsecs);
        memoryAccesses++;
        logOutput(strBuffer);
            /* if in frame */
        if (isInTable(shmPtr->processes[i].waitingFor)) {

            
            sprintf(strBuffer, "Address %d in frame %d, giving data to P%d at time %d:%d\n", shmPtr->processes[i].waitingFor, frameNum, i, shmPtr->secs, shmPtr->nsecs);
            logOutput(strBuffer);
            memoryAccesses++;

            signalSem(i);

        } else {
            
            /* if not in frame */
            pFaults++;
            shmPtr->nsecs += 100000;
            nsToSec();
            sprintf(strBuffer, "Address %d is not in a frame, pagefault\n", shmPtr->processes[i].waitingFor);
            logOutput(strBuffer);
            memoryAccesses++;
            
            if ((frameNum = swapF()) != -1) {

                shmPtr->frameTable[frameNum].currentProcess = i;
                shmPtr->frameTable[frameNum].frameAddr = shmPtr->processes[i].waitingFor;

                sprintf(strBuffer, "Clearing frame %d and swapping in P%d page \n", frameNum, i);
                logOutput(strBuffer);
                shmPtr->nsecs += 100000;
                nsToSec();

                sprintf(strBuffer, "Address %d in frame %d, giving data to P%d at time %d:%d\n", shmPtr->processes[i].waitingFor, frameNum, i, shmPtr->secs, shmPtr->nsecs);
                logOutput(strBuffer);
                memoryAccesses++;
                shmPtr->frameTable[frameNum].dirtyBit = 0;
                sprintf(strBuffer, "Dirty bit of frame %d set, adding additional time to the clock\n", frameNum);
                logOutput(strBuffer);

                shmPtr->processes[i].pageTable[shmPtr->processes[i].pageIndex].currentFrame = frameNum;
                shmPtr->processes[i].pageTable[shmPtr->processes[i].pageIndex].pageAddr = shmPtr->processes[i].waitingFor;
                shmPtr->processes[i].pageIndex += 1;
                shmPtr->processes[i].pageCount++;
                signalSem(i);
            }
        }

    } else {


        sprintf(strBuffer, "P%d requesting write of address %d at time %d:%d\n", i, shmPtr->processes[i].waitingFor, shmPtr->secs, shmPtr->nsecs);
        logOutput(strBuffer);

        if (isInTable(shmPtr->processes[i].waitingFor)) {
            
            sprintf(strBuffer, "Address %d in frame table, writing data to frame at time %d:%d\n", shmPtr->processes[i].waitingFor, shmPtr->secs, shmPtr->nsecs);
            logOutput(strBuffer);
            memoryAccesses++;
            
            signalSem(i);

        } else {
            
            pFaults++;
            sprintf(strBuffer, "Address %d is not in a frame, pagefault\n", shmPtr->processes[i].waitingFor);
            logOutput(strBuffer);
            
            shmPtr->nsecs += 100000;
            nsToSec();
            memoryAccesses++;

            if ((frameNum = swapF()) != -1) {

                shmPtr->frameTable[frameNum].currentProcess = i;
                shmPtr->frameTable[frameNum].frameAddr = shmPtr->processes[i].waitingFor;
                sprintf(strBuffer, "Clearing frame %d and swapping in P%d page \n", frameNum, i);
                logOutput(strBuffer);
                shmPtr->nsecs += 100000;
                nsToSec();

                sprintf(strBuffer, "Address %d in frame, writing data to P%d at time %d:%d\n", shmPtr->processes[i].waitingFor, i, shmPtr->secs, shmPtr->nsecs);
                logOutput(strBuffer);
                shmPtr->frameTable[frameNum].dirtyBit = 1;
                memoryAccesses++;
                
                sprintf(strBuffer, "Dirty bit of frame %d set, adding additional time to the clock\n", frameNum);
                logOutput(strBuffer);
                
                shmPtr->processes[i].pageTable[shmPtr->processes[i].pageIndex].currentFrame = frameNum;
                shmPtr->processes[i].pageTable[shmPtr->processes[i].pageIndex].pageAddr = shmPtr->processes[i].waitingFor;
                shmPtr->processes[i].pageCount++;
                shmPtr->processes[i].pageIndex += 1;

                signalSem(i);
                
            }
        }
    }
}

 /* seeing if we waited long enough */
bool didWait() {
    if(sToFork == shmPtr->secs) {
        if(nsToFork <= shmPtr->nsecs) {
            return true;
        }
    } else if(sToFork < shmPtr->secs) {
        return true;
    } else {
        return false;
    }
}

/* signal semaphore */
void signalSem(int sem) {
    struct sembuf sema;
    sema.sem_num = sem;
    sema.sem_op = 1;
    sema.sem_flg = 0;
    semop(semID, &sema, 1);
}

/* wait on semaphore */
void waitSem(int sem) {
    struct sembuf sema;
    sema.sem_num = sem;
    sema.sem_op = -1;
    sema.sem_flg = 0;
    semop(semID, &sema, 1);
}

/* handling signals */
void setSignals() {
    signal(SIGALRM, alarmCaught);
    signal(SIGINT, handleSignals);
    signal(SIGSEGV, handleSignals);
    signal(SIGKILL, handleSignals);
}

/* printing to log file */
void logOutput(char * string) {

    if (logLineCounter <= 10000) {
        fputs(string, logPtr);
    }
}

/* cleaning the program */
void CleanOSS() {

    printBits();
    printSummary();
    if (logPtr != NULL) {
        fclose(logPtr);
    }
    system("killall process");
    sleep(10);

    shmdt(shmPtr);
    shmctl(shmID, IPC_RMID, NULL);
    semctl(semID, 0, IPC_RMID, NULL);

    exit(0);
}

/* convert nanoseconds to seconds */
void nsToSec() {
    unsigned long nano_time = shmPtr->nsecs;
    if (nano_time >= 1000000000) {
        shmPtr->secs += 1;
        shmPtr->nsecs -= 1000000000;
    }
}

/* make shared memory */
int sharedMemory() {
    key_t shm_key = ftok("process.c", 'a');

    if((shmID = shmget(shm_key, (sizeof(frame) * 256) + sizeof(shmStruct) + (sizeof(pStats) * MAXPROCESSES) + (sizeof(page) * 256), IPC_CREAT | 0666)) == -1) {
        perror("oss.c: shmget");
        return -1;
    }

    if((shmPtr = (shmStruct*)shmat(shmID, 0, 0)) == (shmStruct*)-1) {
        perror("oss.c: shmat");
        return -1;
    }

    return 0;
}


int makeSemaphore() {
    key_t semKey = ftok("oss.c", 'a');

    if((semID = semget(semKey, 20, IPC_CREAT | 0666)) == -1) {
        perror("oss.c: semget");
        return -1;
    }
    return 0;
}

/* grab next frame */
int nextFrame(unsigned int currentSpot) {

    int nfLocation = currentSpot;

    while(1) {
        if (shmPtr->frameTable[currentSpot].frameAddr == 0) {
            break;
        } else {
            currentSpot = (currentSpot + 1) % 256;
            return currentSpot;
        }
    }

    if (currentSpot == nfLocation && shmPtr->frameTable[currentSpot].frameAddr != 0) {
        CleanOSS();
    }

    return currentSpot; 
}



void doFork() {
    if (forkCounter >= 100) {
        logOutput("oss.c: 100 children forked, ending program\n");
        printf("oss.c: 100 children forked, ending program.\n");
        CleanOSS();
    }

    int index, pid;
    unsigned long nano_time_fork;
    for (index = 0; index < MAXPROCESSES; index++) {
        if(shmPtr->currentPIDs[index] == 0) {
            signalSem(19);
            forkCounter++;
            pid = fork();

            if(pid != 0) {
                sprintf(strBuffer, "P%d with PID: %d was forked at %d : %d\n", index, pid, shmPtr->secs, shmPtr->nsecs);
                logOutput(strBuffer);
                shmPtr->currentPIDs[index] = pid;
                logOutput("Determining when next process will fork\n");
                nsToFork = shmPtr->nsecs + (rand() % 500000000);
                nano_time_fork = nsToFork;
                if (nano_time_fork >= 1000000000)   {
                    sToFork += 1;
                    nsToFork -= 1000000000;
                }
                return;
            } else {
                execl("./process", "./process", NULL);
            }
        }
    }
}

/* actual call to clean up after handling signals */
void handleSignals() {
    CleanOSS();
}

void printSummary() {
    logOutput("FINAL STATS:\n");
    sprintf(strBuffer, "Memory accesses per second: %.5f per second\n", (double)memoryAccesses / (double)shmPtr->secs);
    logOutput(strBuffer);
    sprintf(strBuffer, "Page faults per memory access: %.5f page faults per mem access\n", (double)pFaults / (double)memoryAccesses);
    logOutput(strBuffer);
    sprintf(strBuffer, "Avg memory access speed: %.5f seconds\n", (double)shmPtr->secs / (double)memoryAccesses);
    logOutput(strBuffer);
}

 /* check if frame is in the table */
bool isInTable(unsigned int address) {

    for (int i = 0; i < 256; i++) {
        if (shmPtr->frameTable[i].frameAddr != address) {
            return false;
        }
    }
    return true;
}

void printBits() {

    sprintf(strBuffer, "Current memory layout at time %d:%d is:\n", shmPtr->secs, shmPtr->nsecs);
    logOutput(strBuffer);
    logOutput("        Occupied   DirtyBit\n");
    for (int i = 0; i < 256; i++) {

        sprintf(strBuffer, "Frame %d: ", i);
        logOutput(strBuffer);
        if (shmPtr->frameTable[i].frameAddr != 0) {
            logOutput("Yes        ");
        } else {
            logOutput("No       ");
        }

        if (shmPtr->frameTable[i].dirtyBit != 1) {
            logOutput("0\n");
        } else {
            logOutput("1\n");
        }     
    }

}


void preventZombies(int sig) {
    pid_t child_pid;
    while ((child_pid = waitpid((pid_t)(-1), 0, WNOHANG)) > 0) {
        
    }
}

/* frame swapping */
int swapF() {
    int tempVar;
    tempVar = shmPtr->nextFlag;

    for (int index = 0; index < 256; index++) {
        if (shmPtr->frameTable[tempVar].dirtyBit == 1) {
            shmPtr->nextFlag = (tempVar + 1) % 256;
            return tempVar;
        }
        tempVar = (tempVar + 1) % 256;
    }
    return -1;
}


void alarmCaught() {
    printf("alarmCaught called, 2 seconds have passed\n");
    CleanOSS();
}