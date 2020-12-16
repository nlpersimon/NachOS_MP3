// scheduler.h 
//	Data structures for the thread dispatcher and scheduler.
//	Primarily, the list of threads that are ready to run.
//
// Copyright (c) 1992-1996 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "copyright.h"
#include "list.h"
#include "thread.h"


class SchedQueue {
  public:
    SchedQueue();
    ~SchedQueue();

    Thread* FindNextToRun();
    virtual Thread* RemoveFromQueue() = 0;
    void InsertIntoQueue(Thread* thread);
    bool IsEmpty() { return readyList->IsEmpty(); }
    void Print() { readyList->Apply(ThreadPrint); }

  protected:
    List<Thread *> *readyList;
    int queueLevel;

};

class L1Queue: public SchedQueue {
  public:
    L1Queue() { queueLevel = 1; };
    ~L1Queue() {};
    Thread* RemoveFromQueue();

};

class L2Queue: public SchedQueue {
  public:
    L2Queue() { queueLevel = 2; };
    ~L2Queue() {};
    Thread* RemoveFromQueue();

};

class L3Queue: public SchedQueue {
  public:
    L3Queue() { queueLevel = 3; };
    ~L3Queue() {};
    Thread* RemoveFromQueue();

};

// The following class defines the scheduler/dispatcher abstraction -- 
// the data structures and operations needed to keep track of which 
// thread is running, and which threads are ready but not running.

class Scheduler {
  public:
    Scheduler();		// Initialize list of ready threads 
    ~Scheduler();		// De-allocate ready list

    void ReadyToRun(Thread* thread);	
    				// Thread can be dispatched.
    Thread* FindNextToRun();	// Dequeue first thread on the ready 
				// list, if any, and return thread.
    int priorityToLevel(int priority);
    void Run(Thread* nextThread, bool finishing);
    				// Cause nextThread to start running
    void CheckToBeDestroyed();// Check if thread that had been
    				// running needs to be deleted
    void Print();		// Print contents of ready list
    
    // SelfTest for scheduler is implemented in class Thread
    
  private:
    L1Queue *L1;
    L2Queue *L2;
    L3Queue *L3;
				// but not running
    Thread *toBeDestroyed;	// finishing thread to be destroyed
    				// by the next thread that runs
};
#endif // SCHEDULER_H
