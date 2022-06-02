#include <time.h>
#include <math.h>
#include <stdio.h>
#include <errno.h> 
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <stdbool.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <sys/types.h>

#define READ 1
#define WRITE 2
#define MAXPROCESSES 18

typedef struct {

   unsigned int frameAddr;
   unsigned int currentProcess;
   int dirtyBit;

} frame;

typedef struct {

   unsigned int pageAddr;
   unsigned int currentFrame;

} page;

typedef struct {

   unsigned int waitingFor;
   unsigned int pageIndex;
   unsigned int pageCount;
   int memoryType;
   bool isDead;
   page pageTable[32];
   
} pStats;

typedef struct {

   int currentPIDs[MAXPROCESSES];
   unsigned int nextFlag;
   unsigned int secs;
   unsigned int nsecs;
   pStats processes[MAXPROCESSES];
   frame frameTable[256];

} shmStruct;

bool isInTable(unsigned int);
int nextFrame(unsigned int);
void preventZombies(int);
void logOutput(char *);
void handleSignals();
void noEmptyFrames();
void printSummary();
int makeSemaphore();
void emptyFrames();
int sharedMemory();
int findPosition(int);
void setSignals();
void signalSem(int);
void waitSem(int);
void printBits();
void alarmCaught();
void CleanOSS();
void nsToSec();
bool didWait();
void doFork();
int swapF();

