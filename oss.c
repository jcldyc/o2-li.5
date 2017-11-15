#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdbool.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <sys/shm.h>
#include <string.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <fcntl.h>           /* For O_* constants */
#include <sys/stat.h>        /* For mode constants */
#include <semaphore.h>
#include <errno.h>
#include <time.h>

#define billion 1000000000
#define maxTotalProc 100
#define alpha 2
#define bravo 2
#define waitThreshhold 200000
#define waitThreshhold2 600000
#define qua 4

// semaphore globals
#define SEM_NAME "/sem"
#define SEM_PERMS (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)
#define INITIAL_VALUE 1
#define CHILD_PROGRAM "./user"

//function declarations

void ChildProcess(void);
void ctrlPlusC(int sig);
int rand02(void);
int pcbEmpty(int *bitVector);

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

void qOne(int index, struct processControlBlock *array);
void qTwo(int index, struct processControlBlock *array);
void insertShift(int start, int end, struct processControlBlock *array, int queueNum);
void qZero(int index, struct processControlBlock *PCB);

int main(int argc, char *argv[]){
  int option;
  char logFile[] = "logFile";         //set the name of the default log file
  int custLF = 0;
  int runIt = 1;                      //flag to determine if program should run.
  FILE* file_ptr;
  pid_t pid;
  int shmKey = 3690;			                //this  is the key used to identify the shared memory

  shmPtr = &shm;			                 //points the shared memory pointer to teh address in shared memory
  int id;
  clock_t start_t = clock();
  


  //----------------------getOpt-------------------------------------------------------------------
    while((option=getopt(argc, argv, "hl:")) != -1){
      switch(option){
        case 'h':               //this option shows all the arguments available
          runIt = 0;            //how to only asked.  Set to 0 *report to log file*
          printf(" \t./oss: \n\t\t[-l (Log File Name)]\n");
          break;

        case 'l':               //option to name the logfile used
          custLF = 1;

          if(custLF){
            file_ptr = fopen(optarg, "w");
            fclose(file_ptr);
            file_ptr = fopen(optarg, "a");
            printf("\tLog File used: %s\n", optarg);
          }
          break;
        default:
          printf("\tno option used \n");
          break;
      }
    }

    //create the log file specified by user
  //otherwise, use the default log file name : logFile

  if(!custLF && runIt){
    file_ptr = fopen(logFile, "w");
    fclose(file_ptr);
    file_ptr = fopen(logFile, "a");
    printf("\tLog File name: %s\n", logFile);
  }
  printf("-----------------------------------------------------------------------\n\n");

  if (signal(SIGINT, ctrlPlusC) == SIG_ERR) {
        printf("SIGINT error\n");
        exit(1);
    }

  if(runIt){

    //printf("size of shm: %d\n", (int)sizeof(shm));


    ///only creates the shared memory if the program runs through for(runIt)
      id = shmget(shmKey,sizeof(shm), IPC_CREAT | 0666);
      if (id < 0){
        perror("SHMGET");
        exit(1);
      }

      shmPtr = shmat(id, NULL, 0);
      if(shmPtr == (shmData *) -1){
        perror("SHMAT");
        exit(1);
      }

      shmPtr->pSeconds = 0;
      shmPtr->pNanoseconds = 0;


     /* We initialize the semaphore counter to 1 (INITIAL_VALUE) */
       sem_t *semaphore = sem_open(SEM_NAME, O_CREAT | O_EXCL, SEM_PERMS, INITIAL_VALUE);

       if (semaphore == SEM_FAILED) {
           perror("sem_open(3) error");
           exit(EXIT_FAILURE);
       }




          /* Close the semaphore as we won't be using it in the parent process */
       if (sem_close(semaphore) < 0) {
           perror("sem_close(3) failed");
           /* We ignore possible sem_unlink(3) errors here */
           sem_unlink(SEM_NAME);
           exit(EXIT_FAILURE);
       }
      






       int needMoreProcesses = 1;
       int rand02 = rand() % (2 + 1 -0) + 0;  //int between 0 & 2;
       //rand02 = rand02();
       int newProcTime[2] ={rand02, shmPtr->nanoseconds};   //[0] holds seconds [1] holds nano
       int currentProcessCount = 0;
       //int bitVector[18];
       for(int z=0;z<18;z++){
        shmPtr->bitVector[z] = 1;
       }


       while(needMoreProcesses){

         int shmSecond = shmPtr->seconds;           //gets seconds & nanoseconds in shm
         int shmNano = shmPtr->nanoseconds;
         printf("TIME: Sec: %d Nan: %d\n", shmSecond, shmNano);
         printf("P-TIME: Sec: %d Nan: %d\n\n", shmPtr->pSeconds, shmPtr->pNanoseconds);


         //create a new process if
         //shmSecond = current seconds so if it's greater than the stored newProcTime seconds, new  process
         //or if seconds are the same and shmNano is bigger than newProcTimeNano, newprocess

         // for(int z = 0;z<18;z++){
         //    printf("bv[%d] = %d || pid = %d\n", z, shmPtr->bitVector[z], shmPtr->PCB[z].pid);
         //  }
         //  printf("-----------------------------after----------------------------------------\n");

         if(newProcTime[0]<shmSecond || (newProcTime[0]==shmSecond && newProcTime[1] < shmNano)){
        

          int bvFull = 1;
          int emptyFlag = 1;
          int index=0;
          int i = 0;
          
          while(emptyFlag && i<18){
            if(shmPtr->bitVector[i] == 1){
             // bvFull = 0;
              index = i;
              emptyFlag=0;
              if((shmPtr->PCB[index].pid = fork()) == 0){
                fprintf(file_ptr,"Child created at PCB[%d]\n", index);
                //sleep(1);
                shmPtr->PCB[index].pid = getpid();
                shmPtr->bitVector[index]=0;
                shmPtr->currentProcessCount++;
                shmPtr->PCB[index].ranSchedNum = rand() % (3 + 1 - 0) + 0;
                shmPtr->PCB[index].quantum = qua;
                ChildProcess();
              }else if(shmPtr->PCB[index].pid < 0){
                fprintf(file_ptr, "Child failed to fork. \n");
              }
            }i++;
          }

      
        // for(int z = 0;z<18;z++){
        //     printf("bv[%d] = %d || pid = %d  || queue =  %d  || waitTime = %lf\n"
        //     	, z, shmPtr->bitVector[z], shmPtr->PCB[z].pid, shmPtr->PCB[z].queue, shmPtr->PCB[z].waitTime);
        //  }




          

         // printf("---------------------------before------------------------------------------\n");
          fprintf(file_ptr, "currentProcessCount: %d\n\n", shmPtr->currentProcessCount);
         }

         
         //First I'm going to cycle through bitVector to see which PCB has been initiliazed
         //then run through only those initialized and set it's queue if it needs to be changed.
         //Upon scheduling, I need to create a random number between [0,3]




         //getting the avg wait time in  queue 1 and 2

         float sumHolder1=0.0, sumHolder2=0.0, sumHolder3=0.0, amtOfQ0=0.0, amtOfQ1=0.0, amtOfQ2=0.0;
         float qZeroAvg = 0.0, qOneAvg = 0.0, qTwoAvg = 0.0;

         for(int x = 0; x<18;x++){
         	if(!shmPtr->bitVector[x] && shmPtr->PCB[x].queue == 0){
                sumHolder3 = sumHolder3 + shmPtr->PCB[x].waitTime;
                amtOfQ0++;
                fprintf(file_ptr, "\t\t\t sumholder for q0 = %.3f\n", sumHolder3);
            }
            if(!shmPtr->bitVector[x] && shmPtr->PCB[x].queue == 1){
                sumHolder1 = sumHolder1 + shmPtr->PCB[x].waitTime;
                amtOfQ1++;
                fprintf(file_ptr, "\t\t\t sumholder for q1 = %.3f\n", sumHolder1);
             }
            if(!shmPtr->bitVector[x] && shmPtr->PCB[x].queue == 2){
                sumHolder2 = sumHolder2 + shmPtr->PCB[x].waitTime;
                amtOfQ2++;
                fprintf(file_ptr, "\t\t\t sumholder for q2 = %.3f\n\n", sumHolder2);
            }
         }
         if(amtOfQ1 == 0){
         	qOneAvg = 0.000;
         }else {qOneAvg = sumHolder1/amtOfQ1;}

         if(amtOfQ2 == 0){
         	qTwoAvg = 0.000;
         }else {qTwoAvg = sumHolder2/amtOfQ2;}

         if(amtOfQ0 == 0){
         	qZeroAvg = 0.000;
         }else { qZeroAvg = sumHolder3/amtOfQ0;}



        
      

         //moves process from q0 to q1 if it's wait time is higher than threshhold and q1 avg wait time * alpha
         //                   q1 to q2                                                 q2 avg wait time * bravo

         for(int q=0;q<18;q++){
          if(!shmPtr->bitVector[q] && shmPtr->PCB[q].queue == 0){               //It would be better to change it to 0 for empty and 1 for initialized
            if(shmPtr->PCB[q].waitTime > waitThreshhold && (float)shmPtr->PCB[q].waitTime > (qOneAvg*alpha)){
                shmPtr->PCB[q].queue = 1;
                shmPtr->PCB[q].waitTime = 0;
                shmPtr->PCB[q].ranSchedNum = rand() % (3 + 1 - 0) + 0;
                shmPtr->PCB[q].quantum = qua/2;
            }
          }
          if(!shmPtr->bitVector[q] && shmPtr->PCB[q].queue == 1){               //It would be better to change it to 0 for empty and 1 for initialized
            if(shmPtr->PCB[q].waitTime > waitThreshhold2 && (float)shmPtr->PCB[q].waitTime > (qTwoAvg*bravo)){
                shmPtr->PCB[q].queue = 2;
                shmPtr->PCB[q].waitTime = 0;
                shmPtr->PCB[q].ranSchedNum = rand() % (3 + 1 - 0) + 0;
                shmPtr->PCB[q].quantum = qua/4;
            }
          }
         }





         //tell the processes to run

         //qZero processes

         int addToNano;

         for(int c=0;c<18;c++){
          if(!shmPtr->bitVector[c] && shmPtr->PCB[c].queue == 0){
            shmPtr->PCB[c].ready = 1;
            int processStillRunning = 1;

            addToNano = rand() %(300000 + 1 -0) + 200000;
               if((addToNano % billion) != addToNano){        //determines whether to increase seconds or nano
                 shmPtr->pSeconds++;
                 shmPtr->pNanoseconds = addToNano % billion;
                 //fprintf(file_ptr, "\t pSeconds = %d || pNano = %d", shmPtr->pSeconds, shmPtr->pNanoseconds);
               }else{
                 shmPtr->pNanoseconds += addToNano;
               }


            while(processStillRunning){
            //printf("\t\t\t\t\t\t\t\tdooooooobbb %d\n", c);

              if(shmPtr->PCB[c].ready==0){
                processStillRunning = 0;
              }

            }


          }

         }

           //qOne processes

         for(int c=0;c<18;c++){
          if(!shmPtr->bitVector[c] && shmPtr->PCB[c].queue == 1){
            shmPtr->PCB[c].ready = 1;
            int processStillRunning = 1;

			addToNano = rand() %(300000 + 1 -0) + 200000;
               if((addToNano % billion) != addToNano){        //determines whether to increase seconds or nano
                 shmPtr->pSeconds++;
                 shmPtr->pNanoseconds = addToNano % billion;
                 //fprintf(file_ptr, "\t pSeconds = %d || pNano = %d", shmPtr->pSeconds, shmPtr->pNanoseconds);
               }else{
                 shmPtr->pNanoseconds += addToNano;
               }

            while(processStillRunning){

              
              if(shmPtr->PCB[c].ready==0){
                processStillRunning = 0;
              }
            }



          }

         }

           //qTwo processes

         for(int c=0;c<18;c++){
          if(!shmPtr->bitVector[c] && shmPtr->PCB[c].queue == 2){
            shmPtr->PCB[c].ready = 1;
            int processStillRunning = 1;

            addToNano = rand() %(300000 + 1 -0) + 200000;
               if((addToNano % billion) != addToNano){        //determines whether to increase seconds or nano
                 shmPtr->pSeconds++;
                 shmPtr->pNanoseconds = addToNano % billion;
                 //fprintf(file_ptr, "\t pSeconds = %d || pNano = %d", shmPtr->pSeconds, shmPtr->pNanoseconds);
               }else{
                 shmPtr->pNanoseconds += addToNano;
               }

            while(processStillRunning){
              

              if(!shmPtr->PCB[c].ready){
                processStillRunning = 0;
              }
            }


          }

         }




         //-------------LOGICAL CLOCK ++ ------------------------------------------
         //add time to clock at end of loop by adding rand(0-1000) nanoseconds to clock
         int addToNano1 = rand() % (1000 + 1 -0) + 0;    //what's added to nanoseconds
         shmPtr->seconds++;
         if((addToNano1 % billion) != addToNano1){        //determines whether to increase seconds or nano
           shmPtr->seconds++;
           shmPtr->nanoseconds = addToNano1 % billion;
         }else{
           shmPtr->nanoseconds += addToNano1;
         }



         //quits creating processes when 100 is created
         if(shmPtr->currentProcessCount >= 100){
         	needMoreProcesses = 0;
         }

         for(int z = 0;z<18;z++){
            fprintf(file_ptr, "bv[%d] = %d || pid = %d  || queue =  %d  || waitTime = %lf\n"
            	, z, shmPtr->bitVector[z], shmPtr->PCB[z].pid, shmPtr->PCB[z].queue, shmPtr->PCB[z].waitTime);
         }

           fprintf(file_ptr, "\n\nq0 avg wait = %.3f || q1 avg wait = %.3f || q2 avg = %.3f\n", qZeroAvg, qOneAvg, qTwoAvg);
        fprintf(file_ptr, "q0 pcb count = %d || q1 pcb count = %d || q2 pcb count = %d\n\n", (int)amtOfQ0, (int)amtOfQ1, (int)amtOfQ2);

        fprintf(file_ptr, "---------------------------End of One Cycle------------------------------------------\n");




         //sleep(1);
        
       }
       //sleep(1);




  }//runIt end




  //----------------------------------------------------------------------------------------------------
  if (sem_unlink(SEM_NAME) < 0)
      perror("sem_unlink(3) failed");

  shmdt(shmPtr);

  return 0;

}

void ChildProcess(void){
    char *args[]={"./user",NULL};
    execvp(args[0],args);
}

void ctrlPlusC(int sig){

    fprintf( stderr, "Child %ld is dying from parent\n", (long)getpid());
    shmdt(shmPtr);
    sem_unlink(SEM_NAME);
    exit(1);
}

int rand02(void){
  int x = rand() % (2 + 1 -0) + 0;  //int between 0 & 2
  return x;
}


