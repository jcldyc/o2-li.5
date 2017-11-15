#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdbool.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <sys/shm.h>
#include <string.h>
#include <semaphore.h>
#include <fcntl.h>

#define alpha 2
#define bravo 3
#define waitThreshhold 500000000
#define qua 4
#define SEM_NAME "/sem"

void exitfuncCtrlC(int sig);


typedef struct processControlBlock{
  int queue;  
  int totCPUTime;
  int totSysTime;
  int prevBurstTime;
  float waitTime;
  pid_t pid;
  int ready;                  //lets the process know when to run 
  int ranSchedNum;            //between [0,3] to determine certain things
  int quantum;
  int timeEntered[2];
  int timeLeft[2];

}PCB;

typedef struct ShmData{             //struct used to hold the seconds, nanoseconds, and shmMsg and reference in shared memory
  int seconds;
  int nanoseconds;
  int pSeconds;
  int pNanoseconds;
  struct processControlBlock PCB[18];
  int bitVector[18];
  int currentProcessCount;
}shmData;

shmData shm;
shmData *shmPtr;

int main(int argc, char * argv[]){

	int shmKey = 3690;
	int id;

  if (signal(SIGINT, exitfuncCtrlC) == SIG_ERR) {
       printf("SIGINT error\n");
       exit(1);
   }

	 if ((id = shmget(shmKey,sizeof(shm), IPC_CREAT | 0666)) < 0){
       perror("SHMGET:user");
       exit(1);
   }

   if((shmPtr = shmat(id, NULL, 0)) == (shmData *) -1){
       perror("SHMAT");
       exit(1);
   }


     sem_t *semaphore = sem_open(SEM_NAME, O_RDWR);
    if (semaphore == SEM_FAILED) {
        perror("sem_open(3) failed in child");
        exit(EXIT_FAILURE);
    }




   // for(int i = 0;i<18;i++){
   // 	printf("PCB#: %d \t cpuTime: %d\n", shmPtr->PCB[i].queue, shmPtr->PCB[i].totCPUTime);
   // }
  //printf("inside child proces:  PID: %d\n", getpid());

    //need to get the PCB  that matches this child
    pid_t pid = getpid();
    int index;



    for(int x=0;x<18;x++){
      if(shmPtr->PCB[x].pid == pid){
          index=x;
      }
    }

   // shmPtr->PCB[index] += 
    shmPtr->PCB[index].timeEntered[0] = shmPtr->pSeconds; // get when the process entered
    shmPtr->PCB[index].timeEntered[1] = shmPtr->pNanoseconds;
    //printf("\t\t\t%d.%d\n", shmPtr->PCB[index].timeEntered[0], shmPtr->PCB[index].timeEntered[1]);

    int notDone = 1;
    while(notDone){
      if(shmPtr->PCB[index].ready==1){

        if (sem_wait(semaphore) < 0) {
            perror("sem_wait(3) failed on child");
            continue;
        }
        int start = shmPtr->pNanoseconds;


       // printf("heeeyyy yooo.  PID: %d\n", getpid());

        shmPtr->PCB[index].ready = 0;                 //tells oss that this process is done
        int end = shmPtr->pNanoseconds;
        shmPtr->PCB[index].prevBurstTime = end - start;
        shmPtr->PCB[index].totCPUTime += end - start;
        int ranWaitTime = rand() %(300000 + 1 -0) + 200000;

        // if(shmPtr->PCB[index].waitTime > 3000000){     //used to dtermine whether to kill child process
        //   if(shmPtr->PCB[index].queue == 2){


        //   }
        // }

      shmPtr->PCB[index].waitTime += (float) ranWaitTime;


       printf("\t\t\t\t\t\tindex : %d  ||Pid: %d || WaitTime:%lf\n\n",index, shmPtr->PCB[index].pid, shmPtr->PCB[index].waitTime);
       //  printf("\t\t\twait time for pid: %d  -> %d\t\n", shmPtr->PCB[index].pid, (int)shmPtr->PCB[index].waitTime);
               shmPtr->PCB[index].ready = 0;                 //tells oss that this process is done


        if (sem_post(semaphore) < 0) {
              perror("sem_post(3) error on child");
        }

      

      }


    }

    if (sem_close(semaphore) < 0)
            perror("sem_close(3) failed");



   shmdt(shmPtr);
   return 0;

}

void exitfuncCtrlC(int sig){

    fprintf( stderr, "Child %ld is dying from parent\n", (long)getpid());
    shmdt(shmPtr);
    sem_unlink(SEM_NAME);
    exit(1);
}