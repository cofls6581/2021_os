// 2021/운영체제/hw2/B911151/이채린
// CPU Schedule Simulator Homework
// Student Number : B911151
// Name : 이채린
//
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <limits.h>

#define SEED 10

// process states
#define S_IDLE 0			
#define S_READY 1
#define S_BLOCKED 2
#define S_RUNNING 3
#define S_TERMINATE 4

int NPROC, NIOREQ, QUANTUM;

struct ioDoneEvent {
	int procid;
	int doneTime;
	int len;
	struct ioDoneEvent* prev;
	struct ioDoneEvent* next;
} ioDoneEventQueue, * ioDoneEvent;

struct process {
	int id;
	int len;		// for queue
	int targetServiceTime; //각 process service time
	int serviceTime; //현재 서비스 받은 시간
	int startTime; //생성 시간
	int endTime;
	char state;
	int priority;
	int saveReg0, saveReg1;
	struct process* prev;
	struct process* next;
} *procTable;

struct process idleProc;
struct process readyQueue;
struct process blockedQueue;
struct process* runningProc; //현재 사용하는 cpu 나타냄

int cpuReg0, cpuReg1;
int currentTime = 0;
int* procIntArrTime, * procServTime, * ioReqIntArrTime, * ioServTime;

void compute() {
	// DO NOT CHANGE THIS FUNCTION
	cpuReg0 = cpuReg0 + runningProc->id; //하드웨어 레지스터
	cpuReg1 = cpuReg1 + runningProc->id;
	//printf("In computer proc %d cpuReg0 %d\n",runningProc->id,cpuReg0);
}

void initProcTable() {
	int i;
	for (i = 0; i < NPROC; i++) {
		procTable[i].id = i;
		procTable[i].len = 0;
		procTable[i].targetServiceTime = procServTime[i];
		procTable[i].serviceTime = 0;
		procTable[i].startTime = 0;
		procTable[i].endTime = 0;
		procTable[i].state = S_IDLE;
		procTable[i].priority = 0;
		procTable[i].saveReg0 = 0; //context switch 일어날 때 자기가 쓰던 레지스터 값 save
		procTable[i].saveReg1 = 0;
		procTable[i].prev = NULL;
		procTable[i].next = NULL;
	}
}

void procExecSim(struct process* (*scheduler)()) {
	int pid, qTime = 0, cpuUseTime = 0, nproc = 0, termProc = 0, nioreq = 0;
	char schedule = 0, nextState = S_IDLE;
	int nextForkTime, nextIOReqTime;

	nextForkTime = procIntArrTime[nproc];
	nextIOReqTime = ioReqIntArrTime[nioreq];

	runningProc = &idleProc;
  runningProc->state=S_IDLE;

	while (1) {
    //초기화
 		nextState = S_IDLE;
	  schedule = 0;
     
		currentTime++;
		qTime++;
		runningProc->serviceTime++;
		if (runningProc != &idleProc) {
			cpuUseTime++;
      cpuReg0 = runningProc->saveReg0;
		  cpuReg1 = runningProc->saveReg1;
			compute();
			runningProc->saveReg0 = cpuReg0;
			runningProc->saveReg1 = cpuReg1;
		}
		//디버깅용
	//	printf("%d runP %d termProc %d readyLen %d servT %d targetServT %d nproc %d cpuUseTime %d qTime %d \n ", currentTime, runningProc->id, termProc, readyQueue.len, runningProc->serviceTime, runningProc->targetServiceTime, nproc, cpuUseTime, qTime);
		// MUST CALL compute() Inside While loop

		if (currentTime == nextForkTime) { /* CASE 2 : a new process created */
				//생성된 프로세스 ready queue에, 현재 running 중인 process는 그 다음 ready queue에, 스케줄러 호출
      struct process *fork=&procTable[nproc];
    
      //fork process info
      fork->startTime = currentTime;
      fork->state=S_READY;
		
    	//readuQueue insert
			readyQueue.len++;
      fork->next = &readyQueue;
      fork->prev = readyQueue.prev;
      readyQueue.prev= (readyQueue.prev)->next =fork;
      
			//next new fork process info
      nproc++;
			nextForkTime += procIntArrTime[nproc];
		  
      //for RunP and scheduler
      schedule = 1;
    	nextState = S_READY;
		}
   
		if (qTime == QUANTUM && runningProc != &idleProc) { /* CASE 1 : The quantum expires */
			//현재 process ready queue로 이동, 스케줄러 호출
      
      //priority 조정
      if(runningProc->state==S_RUNNING)
       runningProc->priority--;
			
      //for RunP and scheduler
      schedule = 1;
			nextState = S_READY;
		}

		while (ioDoneEventQueue.next->doneTime == currentTime) { /* CASE 3 : IO Done Event */
		  //io요청했던 해당 process blocked->ready, 이미 완료는 x, running process는 ready로 전환, 스케줄러 호출, 동시에 여러 io 만료 가능
			//io요청한 프로세스 만료되지않았다면 bloked에서 ready로
			struct process* ioReqP=&procTable[ioDoneEventQueue.next->procid];  //io requested process
      //ioReqP=&procTable[ioDoneEventQueue.next->procid];
      //io요청했던 프로세스 ioDoneEventQueue에서 삭제
			ioDoneEventQueue.len--;
			ioDoneEventQueue.next = (ioDoneEventQueue.next)->next;
			(ioDoneEventQueue.next)->next->prev = &ioDoneEventQueue;

		  //ioReqP manage
     if(ioReqP->state == S_BLOCKED){//io요청했던 프로세스 S_BLOCKED 상태라면
        //blockedQ에서 delete
        blockedQueue.len--;
				ioReqP->next->prev = ioReqP->prev;
				ioReqP->prev->next = ioReqP->next;
        //readyQ에 insert
        readyQueue.len++;
        ioReqP->next = &readyQueue;
        ioReqP->prev = readyQueue.prev;
        readyQueue.prev= (readyQueue.prev)->next =ioReqP;
        ioReqP->state = S_READY;
	   	}
      //for RunP and scheduler    
      schedule = 1;            
			nextState = S_READY;
			//디버깅용
			//printf("IODone pid %d readyLen %d blockLen %d \n ", pid, readyQueue.len, blockedQueue.len);
		}

		if (cpuUseTime == nextIOReqTime) { /* CASE 5: request IO operations (only when the process does not terminate) */
	    //running process가 있는 경우, blocked 상태로 전환, 스케줄러 호출, iodoneevent queue
			//blockedqueue랑 iodoneeventqueue 둘다 고려해야함
			if (runningProc != &idleProc) {//runP idle인지아닌지
				struct ioDoneEvent* request = &ioDoneEvent[nioreq]; //request process info
				struct ioDoneEvent* q = &ioDoneEventQueue;
       
       //priority 조정
        if(qTime != QUANTUM) runningProc->priority++;
       //request process info
				request->procid = runningProc->id;
				request->doneTime = ioServTime[nioreq] + currentTime;
			 //ioDoneEventQueue에 doneT 맞춰서 insert
				ioDoneEventQueue.len++;
				while (request->doneTime >= (q->next)->doneTime) //donetime 맞추기
					q = q->next;
				request->prev = q;
				request->next = q->next;
				q->next = (q->next)->prev = request;
		  //디버깅용
			//printf("IOReqN %d id %d doneT %d \n", nioreq, request->procid, request->doneTime);
        //for RunP and scheduler
        schedule = 1;
        nextState = S_BLOCKED;
			}
      //next ioRequest info
      nioreq++;
			nextIOReqTime += ioReqIntArrTime[nioreq];
		}

		if (runningProc->serviceTime == runningProc->targetServiceTime) { /* CASE 4 : the process job done and terminates */
 	    //struct process* ioReqP; 
      //ioReqP=&procTable[ioDoneEventQueue.next->procid]; 
      //terminate 상태로 전환, 스케줄러 호출
			termProc++;
			runningProc->endTime = currentTime;
			//for RunP and scheduler
      schedule = 1;
      nextState = S_TERMINATE;
      
      /*if(ioReqP->state ==S_BLOCKED){//io요청했던 프로세스 S_TERMINATE 상태라면
        //blockedQ에서 delete
        blockedQueue.len--;
				ioReqP->next->prev = ioReqP->prev;
				ioReqP->prev->next = ioReqP->next;
      }*/
			//디버깅
		//	printf("terminate id %d \n", runningProc->id);
		}
   
	  //finish
  	if (termProc == NPROC) break;

    // call scheduler() if needed
    if (schedule) {
			struct process* p = &readyQueue;
       runningProc->state = nextState;
      //nextState 처리
			if (nextState == S_READY) {//runP to readyQ
				if (runningProc != &idleProc) {
					readyQueue.len++;
          runningProc->next = &readyQueue;
          runningProc->prev = readyQueue.prev;
          readyQueue.prev= (readyQueue.prev)->next  = runningProc;
          runningProc->state = S_READY; 
				}
			}
      else if(nextState==S_BLOCKED){//runP to blockedQ
           blockedQueue.len++;  
			     runningProc->next = &blockedQueue;
			     runningProc->prev = blockedQueue.prev;
		       blockedQueue.prev= (blockedQueue.prev)->next  = runningProc;
           runningProc->state = S_BLOCKED;       
       }
      else if(nextState==S_TERMINATE)
        runningProc->state = S_TERMINATE;
       
      //process scheulder 실행
      readyQueue.len--;
      if(readyQueue.next == &readyQueue){//readyQueue 비어있을 경우
        runningProc=&idleProc;
        runningProc->state = S_IDLE;
        }
      else{
			runningProc = scheduler();
      //readyQueue에서 runP delete
      p=runningProc;
      (p->next)->prev = p->prev;
	    (p->prev)->next = p->next;
			runningProc->state = S_RUNNING;
      }
		 	qTime = 0;
		}
	} // while loop
}

//RR,SJF(Modified),SRTN,Guaranteed Scheduling(modified),Simple Feed Back Scheduling
struct process* RRschedule() { //들어오는 순서대로
	struct process* p = readyQueue.next;
	
    return p;
}

struct process* SJFschedule() { //quantum 적용, targetservice time이 제일 적은 프로세스 순
	struct process* p = readyQueue.next;
	struct process* temp = readyQueue.next; //비교용
	
  while (temp->next != &readyQueue) {
		if (p->targetServiceTime > temp->next->targetServiceTime)
			p = temp->next;
		temp = temp->next;
	}
 
	return p;
}

struct process* SRTNschedule() { //현재 남아있는 수행시간이 가장 작은 프로세스 순
	struct process* p = readyQueue.next;
  struct process* temp = readyQueue.next; //비교용
	int pTime, tTime;

	pTime = p->targetServiceTime - p->serviceTime;
	while (temp->next != &readyQueue) {
		tTime = temp->next->targetServiceTime - temp->next->serviceTime;
		if (pTime > tTime) {
			p = temp->next;
			pTime =p->targetServiceTime - p->serviceTime;;
		}
		temp = temp->next;
	}
 
	return p;
}

struct process* GSschedule() { //serviceTime/targetServiceTime이 작은 프로세스 순
	struct process* p = readyQueue.next;
	struct process* temp = readyQueue.next; //비교용
	long double pr, t;
 
	pr = (long double)p->serviceTime / p->targetServiceTime;
  while (temp->next != &readyQueue) {
		t = (long double)temp->next->serviceTime / temp->next->targetServiceTime;
		if (pr > t) {
			p = temp->next;
			pr = (long double)p->serviceTime / p->targetServiceTime;
		}
		temp = temp->next;
	}

	return p;
}

struct process* SFSschedule() { //퀀텀 사용 다 안하고 끝나면 우선 순위 증가 반대는 감소, 우선 순위가 큰 프로세스 순 
	struct process* p = readyQueue.next;
	struct process* temp = readyQueue.next; //비교용
	
   while (temp->next != &readyQueue) {
		if ((temp->next)->priority > (p->priority))
			p = temp->next;
		temp = temp->next;
	}
 
	return p;
}

void printResult() {
	// DO NOT CHANGE THIS FUNCTION
	int i;
	long totalProcIntArrTime = 0, totalProcServTime = 0, totalIOReqIntArrTime = 0, totalIOServTime = 0;
	long totalWallTime = 0, totalRegValue = 0;
	for (i = 0; i < NPROC; i++) {
		totalWallTime += procTable[i].endTime - procTable[i].startTime;
		/*
		printf("proc %d serviceTime %d targetServiceTime %d saveReg0 %d\n",
			i,procTable[i].serviceTime,procTable[i].targetServiceTime, procTable[i].saveReg0);
		*/
		totalRegValue += procTable[i].saveReg0 + procTable[i].saveReg1;
		/* printf("reg0 %d reg1 %d totalRegValue %d\n",procTable[i].saveReg0,procTable[i].saveReg1,totalRegValue);*/
	}
	for (i = 0; i < NPROC; i++) {
		totalProcIntArrTime += procIntArrTime[i];
		totalProcServTime += procServTime[i];
	}
	for (i = 0; i < NIOREQ; i++) {
		totalIOReqIntArrTime += ioReqIntArrTime[i];
		totalIOServTime += ioServTime[i];
	}

	printf("Avg Proc Inter Arrival Time : %g \tAverage Proc Service Time : %g\n", (float)totalProcIntArrTime / NPROC, (float)totalProcServTime / NPROC);
	printf("Avg IOReq Inter Arrival Time : %g \tAverage IOReq Service Time : %g\n", (float)totalIOReqIntArrTime / NIOREQ, (float)totalIOServTime / NIOREQ);
	printf("%d Process processed with %d IO requests\n", NPROC, NIOREQ);
	printf("Average Wall Clock Service Time : %g \tAverage Two Register Sum Value %g\n", (float)totalWallTime / NPROC, (float)totalRegValue / NPROC);

}

int main(int argc, char* argv[]) {
	// DO NOT CHANGE THIS FUNCTION
	int i;
	int totalProcServTime = 0, ioReqAvgIntArrTime;
	int SCHEDULING_METHOD, MIN_INT_ARRTIME, MAX_INT_ARRTIME, MIN_SERVTIME, MAX_SERVTIME, MIN_IO_SERVTIME, MAX_IO_SERVTIME, MIN_IOREQ_INT_ARRTIME;

	if (argc < 12) {
		printf("%s: SCHEDULING_METHOD NPROC NIOREQ QUANTUM MIN_INT_ARRTIME MAX_INT_ARRTIME MIN_SERVTIME MAX_SERVTIME MIN_IO_SERVTIME MAX_IO_SERVTIME MIN_IOREQ_INT_ARRTIME\n", argv[0]);
		exit(1);
	}

	SCHEDULING_METHOD = atoi(argv[1]); //스케줄링 사용 알고리즘
	NPROC = atoi(argv[2]); //생성될 총 프로세스 수 
	NIOREQ = atoi(argv[3]); //생성될 총 IO 요청 수 
	QUANTUM = atoi(argv[4]); //퀀텀 시간
	MIN_INT_ARRTIME = atoi(argv[5]); //process interval time 최소값
	MAX_INT_ARRTIME = atoi(argv[6]); //process interval time 최대값
	MIN_SERVTIME = atoi(argv[7]); //process 수행 시간 최소값
	MAX_SERVTIME = atoi(argv[8]); //process 수행 시간 최대값
	MIN_IO_SERVTIME = atoi(argv[9]); //IO 요청 완료 필요 최소 시간
	MAX_IO_SERVTIME = atoi(argv[10]); //IO 요청 완료 필요 최대 시간
	MIN_IOREQ_INT_ARRTIME = atoi(argv[11]); //IO요청 생성시간 중 최소 시간

	printf("SIMULATION PARAMETERS : SCHEDULING_METHOD %d NPROC %d NIOREQ %d QUANTUM %d \n", SCHEDULING_METHOD, NPROC, NIOREQ, QUANTUM);
	printf("MIN_INT_ARRTIME %d MAX_INT_ARRTIME %d MIN_SERVTIME %d MAX_SERVTIME %d\n", MIN_INT_ARRTIME, MAX_INT_ARRTIME, MIN_SERVTIME, MAX_SERVTIME);
	printf("MIN_IO_SERVTIME %d MAX_IO_SERVTIME %d MIN_IOREQ_INT_ARRTIME %d\n", MIN_IO_SERVTIME, MAX_IO_SERVTIME, MIN_IOREQ_INT_ARRTIME);

	srandom(SEED);

	// allocate array structures
	procTable = (struct process*) malloc(sizeof(struct process) * NPROC);
	ioDoneEvent = (struct ioDoneEvent*) malloc(sizeof(struct ioDoneEvent) * NIOREQ);
	procIntArrTime = (int*)malloc(sizeof(int) * NPROC);
	procServTime = (int*)malloc(sizeof(int) * NPROC);
	ioReqIntArrTime = (int*)malloc(sizeof(int) * NIOREQ);
	ioServTime = (int*)malloc(sizeof(int) * NIOREQ);

	// initialize queues
	readyQueue.next = readyQueue.prev = &readyQueue;

	blockedQueue.next = blockedQueue.prev = &blockedQueue;
	ioDoneEventQueue.next = ioDoneEventQueue.prev = &ioDoneEventQueue;
	ioDoneEventQueue.doneTime = INT_MAX;
	ioDoneEventQueue.procid = -1;
	ioDoneEventQueue.len = readyQueue.len = blockedQueue.len = 0;

	// generate process interarrival times
	for (i = 0; i < NPROC; i++) {
		procIntArrTime[i] = random() % (MAX_INT_ARRTIME - MIN_INT_ARRTIME + 1) + MIN_INT_ARRTIME;
	}

	// assign service time for each process
	for (i = 0; i < NPROC; i++) {
		procServTime[i] = random() % (MAX_SERVTIME - MIN_SERVTIME + 1) + MIN_SERVTIME;
		totalProcServTime += procServTime[i];
	}

	ioReqAvgIntArrTime = totalProcServTime / (NIOREQ + 1);
	assert(ioReqAvgIntArrTime > MIN_IOREQ_INT_ARRTIME); //assert는 디버깅을 위한 함수

	// generate io request interarrival time
	for (i = 0; i < NIOREQ; i++) {
		ioReqIntArrTime[i] = random() % ((ioReqAvgIntArrTime - MIN_IOREQ_INT_ARRTIME) * 2 + 1) + MIN_IOREQ_INT_ARRTIME;
	}

	// generate io request service time
	for (i = 0; i < NIOREQ; i++) {
		ioServTime[i] = random() % (MAX_IO_SERVTIME - MIN_IO_SERVTIME + 1) + MIN_IO_SERVTIME;
	}

#ifdef DEBUG
	// printing process interarrival time and service time
	printf("Process Interarrival Time :\n");
	for (i = 0; i < NPROC; i++) {
		printf("%d ", procIntArrTime[i]);
	}
	printf("\n");
	printf("Process Target Service Time :\n");
	for (i = 0; i < NPROC; i++) {
		printf("%d ", procTable[i].targetServiceTime);
	}
	printf("\n");
#endif

	// printing io request interarrival time and io request service time
	printf("IO Req Average InterArrival Time %d\n", ioReqAvgIntArrTime);
	printf("IO Req InterArrival Time range : %d ~ %d\n", MIN_IOREQ_INT_ARRTIME,
		(ioReqAvgIntArrTime - MIN_IOREQ_INT_ARRTIME) * 2 + MIN_IOREQ_INT_ARRTIME);

#ifdef DEBUG		
	printf("IO Req Interarrival Time :\n");
	for (i = 0; i < NIOREQ; i++) {
		printf("%d ", ioReqIntArrTime[i]);
	}
	printf("\n");
	printf("IO Req Service Time :\n");
	for (i = 0; i < NIOREQ; i++) {
		printf("%d ", ioServTime[i]);
	}
	printf("\n");
#endif

	struct process* (*schFunc)();
	switch (SCHEDULING_METHOD) {
	case 1: schFunc = RRschedule; break;
	case 2: schFunc = SJFschedule; break;
	case 3: schFunc = SRTNschedule; break;
	case 4: schFunc = GSschedule; break;
	case 5: schFunc = SFSschedule; break;
	default: printf("ERROR : Unknown Scheduling Method\n"); exit(1);
	}
	initProcTable();
	procExecSim(schFunc);
	printResult();

}
