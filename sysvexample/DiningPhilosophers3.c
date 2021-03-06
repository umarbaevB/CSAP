#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/signal.h>
#include <sys/sem.h>
#include <sys/time.h>

#define FALSE   0
#define TRUE    1
#define NPHIL   10           /* Number of philosophers */
#define NCHOPSTICKS   NPHIL  /* Number of chopsticks */
#define AVG_THINK 2.5        /* Average Think Time (in seconds) */
#define AVG_EAT   1.5        /* Average Time Eating (in seconds) */
#define DURATION  30         /* Duration to run before interrupting in seconds */

static int alarm_flag = FALSE;
int chopstick;   /* the set of chopsticks */

/* error, just prints an error message and terminates the process */
void error(const char *msg)
{
    fprintf(stderr, "Error <%s> errno = %d errstr = <%s>\n", msg, errno,
	    strerror(errno));
    fflush(stderr);
    exit(0);
}

/* my_signal - Signals semaphore s & t */
void my_signal2(int s, int t)
{
    int             status;
    struct sembuf   sb[2];

    sb[0].sem_num = s;      /* indexes the ``array'' of semaphores  */
    sb[1].sem_num = t;      
    sb[0].sem_op = 1;      /* Signal increments by 1 */
    sb[1].sem_op = 1;      
    /* UNDO in case the job crashes with semaphores screwed up */
    sb[0].sem_flg = SEM_UNDO;   /* traditional semaphore blocking and UNDO */
    sb[1].sem_flg = SEM_UNDO;   
    status = semop(chopstick, sb, 2);
    if (status < 0) {
	error("In my_wait, semop failed");
    }
}

/* Wait on the semaphore s & t */
void my_wait2(int s, int t)
{
    int             status;
    struct sembuf   sb[2];

    sb[0].sem_num = s;      /* indexes the ``array'' of semaphores  */
    sb[1].sem_num = t;      
    sb[0].sem_op = -1;      /* Wait decrements by 1 */
    sb[1].sem_op = -1;      

    sb[0].sem_flg = SEM_UNDO;   /* traditional semaphore blocking and UNDO */
    sb[1].sem_flg = SEM_UNDO;  
    status = semop(chopstick, sb, 2);
    if (status < 0) {
	error("In my_wait, semop failed");
    }
}


void think(int phil)
{
    int duration = drand48() * 1000000.0 * AVG_THINK;
    printf("Phil %d (Proc %d) Thinking %d Microseconds\n", phil, getpid(), duration);
    usleep(duration);   
}

void eat(int phil)
{
   int duration = drand48() * 1000000.0 * AVG_EAT;
   printf("Phil %d (Proc %d) Eating %d Microseconds\n", phil, getpid(),
          duration);
   usleep(duration);   
}



/************************ Timer Control Routines ******************************/


/* Handler for SIGUSR1 */
void alarm_handler(int sig)
{
   alarm_flag = TRUE;
}

/*
 * set_sig_handler  prepares a signal catcher
 */
void set_sig_handler()
{
    signal(SIGUSR1, alarm_handler);
}

/******************************* Create Processes******************************/

int create_philosophers(const int n, int child[])
{
    int i, status;

    for (i = 0; i < n; ++i) {
	child[i] = 0;
    }
    printf("Creating %d Philosophers\n", n);
    for (i = 1; i <= n; ++i) {
	status = fork();
	/* Only the Parent can fork, the child should just return */
	if (status < 0) {            /* fork failed, better say why */
	    error("fork failed in create_philosopher\n");
	} 
	else if (status == 0) {   /* Child process */
	    return(i-1);            /* Do NOT let child fork */
	} 
	else {
	    child[i-1] = status;   /* Record Pid of children */
	}
    }
    return(-1);      /* Parent philosopher is -1 */
}

/******************************* Main *****************************************/

int main(int argc, char *argv[])
{
    int child[NPHIL],   /* track philosopher processes */
	i, status, philosopher_id = 0, left, right;
    int val;
    unsigned short all1[NCHOPSTICKS];
    
   chopstick = semget(IPC_PRIVATE,   /* The key */
			 NCHOPSTICKS,   /* How many semaphores we are getting */
			 0600 | IPC_CREAT); /* Permissions & IPC_CREAT */
   printf("chopstick is %d\n", i, chopstick);
   if (chopstick < 0) {
       error("Bad Semaphore Create");
   }
   for (i=0; i<NCHOPSTICKS; i++) {
       all1[i]=1;
   }
   status = semctl(chopstick, i, SETALL, all1);   /* Set semaphore's count to 1 */
   if (status < 0) {
       error("Could not initialize the semaphore's counter to 1");
   }

   /* Create Philosophers */

   philosopher_id = create_philosophers(NPHIL, child);
   if (philosopher_id == -1) {      /* Is this the Parent process ? */
       sleep(DURATION);
       for (i = 0; i < NPHIL; ++i) {
	   kill(child[i], SIGUSR1);   /* Kill the children */
       }
       for (i = 0; i < NPHIL; ++i) {
	   status = wait(NULL);
	   if (status < 0) {
	       error("Wait error detected");
	   }
       }
       status = semctl(chopstick, 0, IPC_RMID, 0);
       if (status < 0) {
	   error("Bad remove of semaphore");
       }
   } 
   else {
       left = philosopher_id;                  /* left chopstick's number */
       right = (philosopher_id + 1) % NPHIL;   /* right chopstick's number */

       set_sig_handler();                       /* Prepare signal handler */
       while (!alarm_flag) {
	   think(philosopher_id);
	   my_wait2(left,right);  /* get chopsticks */
	   printf("Phil %d (proc %d) Picked up Both Chopsticks\n",
		  philosopher_id, getpid());
	   eat(philosopher_id);
	   my_signal2(left,right); /* release chopsticks */ 
	   printf("Phil %d (proc %d) Put down Both Chopstick\n",
		      philosopher_id, getpid());
       }
   }
}
