// scheduler.cc 
//	Routines to choose the next thread to run, and to dispatch to
//	that thread.
//
// 	These routines assume that interrupts are already disabled.
//	If interrupts are disabled, we can assume mutual exclusion
//	(since we are on a uniprocessor).
//
// 	NOTE: We can't use Locks to provide mutual exclusion here, since
// 	if we needed to wait for a lock, and the lock was busy, we would 
//	end up calling FindNextToRun(), and that would put us in an 
//	infinite loop.
//
// 	Very simple implementation -- no priorities, straight FIFO.
//	Might need to be improved in later assignments.
//
// Copyright (c) 1992-1996 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "debug.h"
#include "scheduler.h"
#include "main.h"

//----------------------------------------------------------------------
// Scheduler::Scheduler
// 	Initialize the list of ready but not running threads.
//	Initially, no ready threads.
//----------------------------------------------------------------------

Scheduler::Scheduler()
{ 
    L1 = new L1Queue;
    L2 = new L2Queue;
    L3 = new L3Queue;
    toBeDestroyed = NULL;
} 

//----------------------------------------------------------------------
// Scheduler::~Scheduler
// 	De-allocate the list of ready threads.
//----------------------------------------------------------------------

Scheduler::~Scheduler()
{ 
    delete L1; 
    delete L2; 
    delete L3; 
} 

//----------------------------------------------------------------------
// Scheduler::ReadyToRun
// 	Mark a thread as ready, but not running.
//	Put it on the ready list, for later scheduling onto the CPU.
//
//	"thread" is the thread to be put on the ready list.
//----------------------------------------------------------------------

void
Scheduler::ReadyToRun (Thread *thread)
{
    ASSERT(kernel->interrupt->getLevel() == IntOff);
	//cout << "Putting thread on ready list: " << thread->getName() << endl ;
    thread->setStatus(READY);
    AssignToQueue(thread);

}

void
Scheduler::AssignToQueue(Thread *thread)
{
    int queueLevel = priorityToLevel(thread->getPriority());
    if (queueLevel == 1) {
        L1->InsertIntoQueue(thread);
    } else if (queueLevel == 2) {
        L2->InsertIntoQueue(thread);
    } else {
        L3->InsertIntoQueue(thread);
    }
}

int
Scheduler::priorityToLevel (int priority)
{
    if (priority >= 100 & priority <= 149) {
        return 1;
    } else if (priority >= 50 & priority <= 99) {
        return 2;
    } else {
        return 3;
    }
}

//----------------------------------------------------------------------
// Scheduler::FindNextToRun
// 	Return the next thread to be scheduled onto the CPU.
//	If there are no ready threads, return NULL.
// Side effect:
//	Thread is removed from the ready list.
//----------------------------------------------------------------------

Thread *
Scheduler::FindNextToRun ()
{
    ASSERT(kernel->interrupt->getLevel() == IntOff);

    if (L1->IsEmpty()) {
		if (L2->IsEmpty()) {
            if (L3->IsEmpty()) {
                return NULL;
            } else {
                return L3->FindNextToRun();
            }
        } else {
            return L2->FindNextToRun();
        }
    } else {
    	return L1->FindNextToRun();
    }
}

//----------------------------------------------------------------------
// Scheduler::Run
// 	Dispatch the CPU to nextThread.  Save the state of the old thread,
//	and load the state of the new thread, by calling the machine
//	dependent context switch routine, SWITCH.
//
//      Note: we assume the state of the previously running thread has
//	already been changed from running to blocked or ready (depending).
// Side effect:
//	The global variable kernel->currentThread becomes nextThread.
//
//	"nextThread" is the thread to be put into the CPU.
//	"finishing" is set if the current thread is to be deleted
//		once we're no longer running on its stack
//		(when the next thread starts running)
//----------------------------------------------------------------------

void
Scheduler::Run (Thread *nextThread, bool finishing)
{
    Thread *oldThread = kernel->currentThread;

    
    ASSERT(kernel->interrupt->getLevel() == IntOff);

    if (finishing) {	// mark that we need to delete current thread
        ASSERT(toBeDestroyed == NULL);
    toBeDestroyed = oldThread;
    }
    
    if (oldThread->space != NULL) {	// if this thread is a user program,
        oldThread->SaveUserState(); 	// save the user's CPU registers
    oldThread->space->SaveState();
    }
    
    oldThread->CheckOverflow();		    // check if the old thread
                        // had an undetected stack overflow
    if (nextThread != oldThread) {
        DEBUG(dbgSchedule,  "Tick [" << kernel->stats->totalTicks << "]: Thread [" << nextThread->getID() << "] is now selected for execution, thread [" << oldThread->getID() << "] is replaced, and it has executed [" << oldThread->getT() << "] ticks");
        kernel->currentThread = nextThread;  // switch to the next thread
        nextThread->setStatus(RUNNING);      // nextThread is now running
        
        DEBUG(dbgThread, "Switching from: " << oldThread->getName() << " to: " << nextThread->getName());
        
        // This is a machine-dependent assembly language routine defined 
        // in switch.s.  You may have to think
        // a bit to figure out what happens after this, both from the point
        // of view of the thread and from the perspective of the "outside world".

        SWITCH(oldThread, nextThread);

        // we're back, running oldThread
        
        // interrupts are off when we return from switch!
    }
    ASSERT(kernel->interrupt->getLevel() == IntOff);

    DEBUG(dbgThread, "Now in thread: " << oldThread->getName());

    CheckToBeDestroyed();		// check if thread we were running
					// before this one has finished
					// and needs to be cleaned up
    
    if (oldThread->space != NULL) {	    // if there is an address space
        oldThread->RestoreUserState();     // to restore, do it.
	oldThread->space->RestoreState();
    }
}

//----------------------------------------------------------------------
// Scheduler::CheckToBeDestroyed
// 	If the old thread gave up the processor because it was finishing,
// 	we need to delete its carcass.  Note we cannot delete the thread
// 	before now (for example, in Thread::Finish()), because up to this
// 	point, we were still running on the old thread's stack!
//----------------------------------------------------------------------

void
Scheduler::CheckToBeDestroyed()
{
    if (toBeDestroyed != NULL) {
        if (toBeDestroyed == L1->currentThread) {
            L1->currentThread = NULL;
        }
        else if (toBeDestroyed == L2->currentThread) {
            L2->currentThread = NULL;
        }
        else if (toBeDestroyed == L3->currentThread) {
            L3->currentThread = NULL;
        }
        delete toBeDestroyed;
	toBeDestroyed = NULL;
    }
}
 
//----------------------------------------------------------------------
// Scheduler::Print
// 	Print the scheduler state -- in other words, the contents of
//	the ready list.  For debugging.
//----------------------------------------------------------------------
void
Scheduler::Print()
{
    cout << "L1 contents:\n";
    L1->Print();
    cout << "L2 contents:\n";
    L2->Print();
    cout << "L3 contents:\n";
    L3->Print();
}

void
Scheduler::AccumWaitTicks(int ticks)
{
    L1->AccumWaitTicks(ticks);
    L2->AccumWaitTicks(ticks);
    L3->AccumWaitTicks(ticks);
}

void
Scheduler::UpdatePriority()
{
    L1->UpdatePriority();
    L2->UpdatePriority();
    L3->UpdatePriority();
}


SchedQueue::SchedQueue()
{
    readyList = new List<Thread *>;
    queueLevel = 0;
    currentThread = NULL;
}

SchedQueue::~SchedQueue()
{
    delete readyList;
}

bool
SchedQueue::IsEmpty()
{
    return (!IsCurrentThreadAvailable() && readyList->IsEmpty());
}

void
SchedQueue::InsertIntoQueue(Thread* thread)
{
    // 避免 L2 在做 context switch 的時候把 currentThread 重複加入 readyList
    if (thread != currentThread) {
        readyList->Append(thread); 
        DEBUG(dbgSchedule, "Tick [" << kernel->stats->totalTicks << "]: Thread [" << thread->getID() << "] is inserted into queue L[" << queueLevel << "]");
    }
}

Thread*
SchedQueue::FindNextToRun()
{
    Thread *thread = GetNextThread();
    //thread->setWaitTicks(0);
    return thread;
}

void
SchedQueue::AccumWaitTicks(int ticks)
{
    if ((currentThread != NULL) && (currentThread != kernel->currentThread)) {
        //DEBUG(dbgSchedule, "Tick [" << kernel->stats->totalTicks << "] queue L[" << queueLevel << "] is accumulating waiting ticks");
        currentThread->AccumWaitTicks(ticks);
    }
    if (!readyList->IsEmpty()) {
        ListIterator<Thread *> *iterator = new ListIterator<Thread *>(readyList);
        Thread *thread;
        for (; !iterator->IsDone(); iterator->Next()) {
            //DEBUG(dbgSchedule, "Tick [" << kernel->stats->totalTicks << "] queue L[" << queueLevel << "] is accumulating waiting ticks");
            thread = iterator->Item();
            thread->AccumWaitTicks(ticks);
        }
    }
}

void
SchedQueue::UpdatePriority()
{
    if (currentThread != NULL) {
        int oldPriority = currentThread->getPriority();
        currentThread->UpdatePriorityByWaitTicks();
        int newPriority = currentThread->getPriority();
        if ((currentThread != kernel->currentThread) && (kernel->scheduler->priorityToLevel(newPriority) != kernel->scheduler->priorityToLevel(oldPriority))) {
            kernel->scheduler->AssignToQueue(currentThread);
            currentThread = NULL;
        }
    }
    if (!readyList->IsEmpty()) {
        ListIterator<Thread *> *iterator = new ListIterator<Thread *>(readyList);
        Thread *thread;
        for (; !iterator->IsDone(); iterator->Next()) {
            thread = iterator->Item();
            int oldPriority = thread->getPriority();
            thread->UpdatePriorityByWaitTicks();
            int newPriority = thread->getPriority();
            if (kernel->scheduler->priorityToLevel(newPriority) != kernel->scheduler->priorityToLevel(oldPriority)) {
                readyList->Remove(thread);
                kernel->scheduler->AssignToQueue(thread);
            }
        }
    }
}

Thread*
L1Queue::GetNextThread()
{
    if (!readyList->IsEmpty()) {
        int min;
        if (!IsCurrentThreadAvailable()) {
            min = -1;
        } else {
            min = currentThread->getBurstTime();
        }
        ListIterator<Thread *> *iterator = new ListIterator<Thread *>(readyList);
        Thread *thread, *oldThread=currentThread;
        for (; !iterator->IsDone(); iterator->Next()) {
            thread = iterator->Item();
            if (thread->getBurstTime() < min || min < 0) {
                min = thread->getBurstTime();
                currentThread = thread;
            }
        }
        if (oldThread != currentThread) {
            readyList->Remove(currentThread);
            DEBUG(dbgSchedule, "Tick [" << kernel->stats->totalTicks << "]: Thread [" << currentThread->getID() << "] is removed from queue L[" << queueLevel << "]");
        }
    }
    return currentThread;
}

Thread*
L2Queue::GetNextThread()
{
    if (!IsCurrentThreadAvailable() && !readyList->IsEmpty()) {
        int max = -1;
        ListIterator<Thread *> *iterator = new ListIterator<Thread *>(readyList);
        Thread *thread;
        for (; !iterator->IsDone(); iterator->Next()) {
            thread = iterator->Item();
            if (thread->getPriority() > max) {
                max = thread->getPriority();
                currentThread = thread;
            }
        }
        readyList->Remove(currentThread);
        DEBUG(dbgSchedule, "Tick [" << kernel->stats->totalTicks << "]: Thread [" << currentThread->getID() << "] is removed from queue L[" << queueLevel << "]");
    }
    return currentThread;
    // return readyList->RemoveFront();
}

Thread*
L3Queue::GetNextThread()
{
    if (!readyList->IsEmpty()) {
        currentThread = readyList->RemoveFront();
        DEBUG(dbgSchedule, "Tick [" << kernel->stats->totalTicks << "]: Thread [" << currentThread->getID() << "] is removed from queue L[" << queueLevel << "]");
    }
    return currentThread;
    // return readyList->RemoveFront();
}