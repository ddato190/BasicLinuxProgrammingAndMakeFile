#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <chrono>
#include <mutex>
#include <thread>
#include <getopt.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <pthread.h>
#include <malloc.h>
#include <semaphore.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <fcntl.h>
#include <sys/mman.h>

using namespace std;

std::mutex g_mutex;
extern char** environ;

sig_atomic_t sigusr1_count = 0;
sig_atomic_t child_exit_status;

pthread_mutex_t testPthreadMutex = PTHREAD_MUTEX_INITIALIZER;
sem_t testSemaphore;

int conditionflag = 0;
pthread_mutex_t conditionmutex;
pthread_cond_t conditionvariable;

union semun
{
  int val;
  struct semid_ds *buf;
  unsigned short int *array;
  struct seminfo *__buf;
};

const struct option long_options[]=
  {
    {"help",0,NULL,'h'},
    {"output",1,NULL,'o'},
    {"verbose",0,NULL,'v'},
    {NULL,0,NULL,0},
  };

void testThread(const char* threadName)
{
  g_mutex.lock();
  printf("%s \n", threadName);
  g_mutex.unlock();
}

typedef int (*CustomFP)(int);
int powerofint(int input)
{
  return input * input;
}

int testFP(int input, int (*op)(int))
{
  return (*op)(input);
}

int testFP2(int input, CustomFP op)
{
  return op(input);
}

typedef int temp_file_handle;
temp_file_handle write_temp_file(const char* buffer, size_t length)
{
  char temp_filename[] = "/tmp/ccXXXXXX";
  int fd = mkstemp(temp_filename);
  printf("mkstemp result = %d \n", fd);
  unlink(temp_filename);
  write(fd,&length,sizeof(length));
  write(fd,buffer,length);
  return fd;
}

char* read_temp_file(temp_file_handle temp_file, size_t* length)
{
  char* buffer;
  int fd = temp_file;
  lseek(fd,0,SEEK_SET);
  read(fd,length,sizeof(*length));
  buffer = (char*)malloc(*length);
  read(fd,buffer,*length);
  close(fd);
  return buffer;
}

void spawn(const char* program, char** arg_list)
{
  pid_t child_pid;
  child_pid = fork();
  if(child_pid != 0)
    {
      printf("Spawn Parent Pid = %d \n", getpid());
      child_pid = fork();
      if(child_pid == 0)
      {
        printf("Executing 2nd child process !! \n");
        abort();
      }
      else
      {
	printf("Spawn Parent Pid = %d \n", getpid());
      }
    }
  else
    {
      printf("Executing Child Process ls !! \n");
      execvp(program,arg_list);
      printf("Error Occured in execvp !! \n");
      abort();
    }
}

void sigHandler(int signal_number)
{
  ++sigusr1_count;
}

void clean_up_child_process(int signal_number)
{
  int status;
  wait(&status);
  child_exit_status = status;
  printf("Notified that child process %d is terminated !! \n", getpid());
}

void* testPthread(void* param)
{
  int returnValue;
  returnValue = *((int*)param);
  returnValue ++;
  printf("testPthread has been involked !!! \n");
  return (void*)returnValue;
}

void* thread_function(void* param)
{
  sem_post(&testSemaphore);
  sem_wait(&testSemaphore);
  pthread_mutex_lock(&testPthreadMutex);
  printf("Detached thread is executing !! \n");
  pthread_mutex_unlock(&testPthreadMutex);

  printf("thread_function start to wait condition variable \n");
  pthread_mutex_lock(&conditionmutex);
  while(!conditionflag)
  {
    pthread_cond_wait(&conditionvariable, &conditionmutex);
  }
  pthread_mutex_unlock(&conditionmutex);
  printf("thread_function has reached condition variable \n");
}

void deallocatebuffer(void* buffer)
{
  printf("DeallocateBuffer is executing !!! \n");
  free(buffer);
}

int main(int argc, char**argv)
{
  printf("The name of this program is %s \n", argv[0]);
  printf("This program was involked with %d arguments \n", argc);

  pthread_mutex_init(&conditionmutex,NULL);
  pthread_cond_init(&conditionvariable,NULL);

  std::thread thread1(testThread, "Thread 1");
  std::thread thread2(testThread, "Thread 2");


  thread1.join();
  thread2.join();

  int fpResult = testFP(2,&powerofint);
  printf("Function Pointer Result = %d \n", fpResult);

  CustomFP cfp = &powerofint;
  fpResult = testFP2(5,cfp);
  printf("FP Result2 = %d \n",fpResult);

  fprintf(stderr,"Error \n");

  temp_file_handle writeHandle = write_temp_file("Hello",5);
  size_t readSize;
  char* readResult = read_temp_file(writeHandle, &readSize);
  printf("Read Result = %s \n", readResult);
  free(readResult);

  printf("Current Process ID is %d \n", getpid());

  //system("ls -l");

  //Combine fork and exec
  char* arg_list[] = 
    {
      "ls",
      "-l",
      "/",
      NULL
    };

  struct sigaction sigchildaction;
  memset(&sigchildaction,0,sizeof(sigchildaction));
  sigchildaction.sa_handler = &clean_up_child_process;
  sigaction(SIGCHLD,&sigchildaction,NULL);

  int status = 0;
  spawn("ls",arg_list);
  do
    {
      wait(&status);
      if(WIFEXITED(status))
	{
	  printf("Child Process exited normally !! \n");
	}
      else
	{
	  printf("Child Process exited abnormally \n");
	}
      printf("Wait Status = %d \n", status);
    }
    while(status > 0);

  printf("Done with main process !! \n");

  struct sigaction sa;
  memset(&sa,0,sizeof(sa));
  sa.sa_handler = &sigHandler;
  sigaction(SIGUSR1,&sa,NULL);

  printf("SIGUSR1 has been raised for %d times \n", sigusr1_count);

  int testThreadParam = 3;
  int returnThreadValue;
  pthread_t threadID;
  pthread_create(&threadID,NULL, &testPthread,&testThreadParam);
  pthread_join(threadID,(void**)&returnThreadValue);
  printf("Thread Return Value = %d \n", returnThreadValue);

  pthread_attr_t attr;
  pthread_t thread;
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);
  pthread_create(&thread,&attr,&thread_function,NULL);
  pthread_attr_destroy(&attr);

  int oldcancelstate;
  pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,&oldcancelstate);
  pthread_setcancelstate(oldcancelstate,NULL);

  void* tempBuffer = (void*)malloc(1024);
  pthread_cleanup_push(deallocatebuffer,tempBuffer);
  pthread_cleanup_pop(1);  

  int* tempBufferInt = new int[1024];
  delete[] tempBufferInt;

  sem_init(&testSemaphore,0,5);

  fgetc(stdin);

  printf("Condition Variable Set Start !! \n");
  pthread_mutex_lock(&conditionmutex);
  conditionflag = 1;
  pthread_cond_signal(&conditionvariable);
  pthread_mutex_unlock(&conditionmutex);
  printf("Condition Variable Set Passed !! \n");

  //Shared Memory
  int segment_id = 0;
  char* shared_memory;
  struct shmid_ds shmbuffer;
  int segment_size;
  const int shared_segment_size = 0x6400;
  segment_id = shmget(IPC_PRIVATE,shared_segment_size,
		      IPC_CREAT|IPC_EXCL|S_IRUSR|S_IWUSR);
  shared_memory = (char*)shmat(segment_id,0,0);
  printf("shared memory attached at %p \n", shared_memory);
  shmctl(segment_id,IPC_STAT,&shmbuffer);
  segment_size = shmbuffer.shm_segsz;
  printf("segment size is %d \n",segment_size);
  sprintf(shared_memory,"Hello Shared Memory !!");
  shmdt(shared_memory);

  shared_memory = (char*)shmat(segment_id,(void*)0x5000000,0);
  printf("shared memory attached at %p \n", shared_memory);
  printf("Data Inside Shared Memory is %s \n", shared_memory);
  sprintf(shared_memory,"Hello Modified Shared Memory !!");
  shmdt(shared_memory);

  shared_memory = (char*)shmat(segment_id,(void*)0x5000000,0);
  printf("shared memory attached at %p \n", shared_memory);
  printf("Data Inside Shared Memory is %s \n", shared_memory);
  shmdt(shared_memory);

  shmctl(segment_id,IPC_RMID,0);

  //IPC Semaphore
  key_t key, key2;
  int semid, semid2;
  union semun arg;
  key = ftok("testCPP.cpp",'J');
  semid = semget(key,1,IPC_CREAT|IPC_EXCL);
  printf("semget semid = %d \n", semid);
  unsigned short values[1];
  values[0] = 1;
  arg.array = values;
  semctl(semid,0,SETALL,arg);

  struct sembuf sb = {0, -1, 0};
  key2 = ftok("testCPP.cpp",'J');
  semid2 = semget(key2,1,0);
  printf("Press to lock IPC semaphore !! semid = %d \n", semid);
  fgetc(stdin);
  semop(semid2,&sb,1);
  printf("Locked !! Press to Unlock !! semid2 = %d\n", semid2);
  fgetc(stdin);
  sb.sem_op = 1;
  semop(semid2,&sb,1);

  union semun ignorearg;
  semctl(semid2,1,IPC_RMID,ignorearg);
  printf("Unlocked IPC Semophore !! \n");

  //Mapped Memory
  int fd;
  void* file_memory;
  srand(time(NULL));
  fd = open("mmap.txt",O_RDWR|O_CREAT,S_IRUSR|S_IWUSR);
  lseek(fd,0,SEEK_SET);
  write(fd,"TEST",4);
  lseek(fd,0,SEEK_SET);
  file_memory = mmap(0,4,PROT_WRITE|PROT_READ,MAP_SHARED,fd,0);
  close(fd);
  sprintf((char*)file_memory,"MMAP");
  munmap(file_memory,4);


  return 0;
}
