/** 
 * @file llthread.cpp
 *
 * $LicenseInfo:firstyear=2004&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 * 
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#include "linden_common.h"
#include "llapr.h"

#include "apr_portable.h"

#include "llthread.h"
#include "llmutex.h"

#include "lltimer.h"
#include "lltrace.h"

#if LL_LINUX || LL_SOLARIS
#include <sched.h>
#endif

//----------------------------------------------------------------------------
// Usage:
// void run_func(LLThread* thread)
// {
// }
// LLThread* thread = new LLThread();
// thread->run(run_func);
// ...
// thread->setQuitting();
// while(!timeout)
// {
//   if (thread->isStopped())
//   {
//     delete thread;
//     break;
//   }
// }
// 
//----------------------------------------------------------------------------

#if LL_DARWIN
// statically allocated thread local storage not supported in Darwin executable formats
#elif LL_WINDOWS
U32 __declspec(thread) LLThread::sThreadIndex = 0;
#elif LL_LINUX
U32 __thread LLThread::sThreadID = 0;
#endif

U32 LLThread::sIDIter = 0;
LLThreadLocalPtr<LLTrace::ThreadTrace> LLThread::sTraceData;


LL_COMMON_API void assert_main_thread()
{
	static U32 s_thread_id = LLThread::currentID();
	if (LLThread::currentID() != s_thread_id)
	{
		llerrs << "Illegal execution outside main thread." << llendl;
	}
}

//
// Handed to the APR thread creation function
//
void *APR_THREAD_FUNC LLThread::staticRun(apr_thread_t *apr_threadp, void *datap)
{
	LLThread *threadp = (LLThread *)datap;

	setTraceData(new LLTrace::SlaveThreadTrace());

#if !LL_DARWIN
	sThreadIndex = threadp->mID;
#endif

	// Run the user supplied function
	threadp->run();

	//llinfos << "LLThread::staticRun() Exiting: " << threadp->mName << llendl;
	
	// We're done with the run function, this thread is done executing now.
	threadp->mStatus = STOPPED;

	delete sTraceData.get();
	sTraceData = NULL;

	return NULL;
}

LLThread::LLThread(const std::string& name, apr_pool_t *poolp) :
	mPaused(FALSE),
	mName(name),
	mAPRThreadp(NULL),
	mStatus(STOPPED)
{

	mID = ++sIDIter;

	// Thread creation probably CAN be paranoid about APR being initialized, if necessary
	if (poolp)
	{
		mIsLocalPool = FALSE;
		mAPRPoolp = poolp;
	}
	else
	{
		mIsLocalPool = TRUE;
		apr_pool_create(&mAPRPoolp, NULL); // Create a subpool for this thread
	}
	mRunCondition = new LLCondition(mAPRPoolp);

	mLocalAPRFilePoolp = NULL ;
}


LLThread::~LLThread()
{
	shutdown();

	if(mLocalAPRFilePoolp)
	{
		delete mLocalAPRFilePoolp ;
		mLocalAPRFilePoolp = NULL ;
	}
}

void LLThread::shutdown()
{
	// Warning!  If you somehow call the thread destructor from itself,
	// the thread will die in an unclean fashion!
	if (mAPRThreadp)
	{
		if (!isStopped())
		{
			// The thread isn't already stopped
			// First, set the flag that indicates that we're ready to die
			setQuitting();

			//llinfos << "LLThread::~LLThread() Killing thread " << mName << " Status: " << mStatus << llendl;
			// Now wait a bit for the thread to exit
			// It's unclear whether I should even bother doing this - this destructor
			// should never get called unless we're already stopped, really...
			S32 counter = 0;
			const S32 MAX_WAIT = 600;
			while (counter < MAX_WAIT)
			{
				if (isStopped())
				{
					break;
				}
				// Sleep for a tenth of a second
				ms_sleep(100);
				yield();
				counter++;
			}
		}

		if (!isStopped())
		{
			// This thread just wouldn't stop, even though we gave it time
			//llwarns << "LLThread::~LLThread() exiting thread before clean exit!" << llendl;
			// Put a stake in its heart.
			apr_thread_exit(mAPRThreadp, -1);
			return;
		}
		mAPRThreadp = NULL;
	}

	delete mRunCondition;
	mRunCondition = 0;
	
	if (mIsLocalPool && mAPRPoolp)
	{
		apr_pool_destroy(mAPRPoolp);
		mAPRPoolp = 0;
	}
}


void LLThread::start()
{
	llassert(isStopped());
	
	// Set thread state to running
	mStatus = RUNNING;

	apr_status_t status =
		apr_thread_create(&mAPRThreadp, NULL, staticRun, (void *)this, mAPRPoolp);
	
	if(status == APR_SUCCESS)
	{	
		// We won't bother joining
		apr_thread_detach(mAPRThreadp);
	}
	else
	{
		mStatus = STOPPED;
		llwarns << "failed to start thread " << mName << llendl;
		ll_apr_warn_status(status);
	}
}

//============================================================================
// Called from MAIN THREAD.

// Request that the thread pause/resume.
// The thread will pause when (and if) it calls checkPause()
void LLThread::pause()
{
	if (!mPaused)
	{
		// this will cause the thread to stop execution as soon as checkPause() is called
		mPaused = 1;		// Does not need to be atomic since this is only set/unset from the main thread
	}	
}

void LLThread::unpause()
{
	if (mPaused)
	{
		mPaused = 0;
	}

	wake(); // wake up the thread if necessary
}

// virtual predicate function -- returns true if the thread should wake up, false if it should sleep.
bool LLThread::runCondition(void)
{
	// by default, always run.  Handling of pause/unpause is done regardless of this function's result.
	return true;
}

//============================================================================
// Called from run() (CHILD THREAD).
// Stop thread execution if requested until unpaused.
void LLThread::checkPause()
{
	mRunCondition->lock();

	// This is in a while loop because the pthread API allows for spurious wakeups.
	while(shouldSleep())
	{
		mRunCondition->wait(); // unlocks mRunCondition
		// mRunCondition is locked when the thread wakes up
	}
	
 	mRunCondition->unlock();
}

//============================================================================

void LLThread::setQuitting()
{
	mRunCondition->lock();
	if (mStatus == RUNNING)
	{
		mStatus = QUITTING;
	}
	mRunCondition->unlock();
	wake();
}

// static
U32 LLThread::currentID()
{
	return (U32)apr_os_thread_current();
}

// static
void LLThread::yield()
{
#if LL_LINUX || LL_SOLARIS
	sched_yield(); // annoyingly, apr_thread_yield  is a noop on linux...
#else
	apr_thread_yield();
#endif
}

void LLThread::wake()
{
	mRunCondition->lock();
	if(!shouldSleep())
	{
		mRunCondition->signal();
	}
	mRunCondition->unlock();
}

void LLThread::wakeLocked()
{
	if(!shouldSleep())
	{
		mRunCondition->signal();
	}
}

LLTrace::ThreadTrace* LLThread::getTraceData()
{
	return sTraceData.get();
}

void LLThread::setTraceData( LLTrace::ThreadTrace* data )
{
	sTraceData = data;
}
