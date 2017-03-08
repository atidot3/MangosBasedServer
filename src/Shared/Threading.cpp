#include "Threading.h"
#include "Util\Errors.h"

#include <chrono>
#include <system_error>

using namespace Origin;

Thread::Thread() : m_task(nullptr), m_iThreadId(), m_ThreadImp()
{
}

Thread::Thread(Runnable* instance) : m_task(instance), m_ThreadImp(&Thread::ThreadTask, (void*)m_task)
{
	m_iThreadId = m_ThreadImp.get_id();

	// register reference to m_task to prevent it deeltion until destructor
	if (m_task)
		m_task->incReference();
}

Thread::~Thread()
{
	// Wait();

	// deleted runnable object (if no other references)
	if (m_task)
		m_task->decReference();
}

bool Thread::wait()
{
	if (m_iThreadId == std::thread::id() || !m_task)
		return false;

	bool res = true;

	try
	{
		m_ThreadImp.join();
	}
	catch (std::system_error&)
	{
		res = false;
	}

	m_iThreadId = std::thread::id();

	return res;
}

void Thread::destroy()
{
	if (m_iThreadId == std::thread::id() || !m_task)
		return;

	// FIXME: We need to make sure that all threads can be trusted to
	// halt execution on their own as this is not an interrupt
	m_ThreadImp.join();
	m_iThreadId = std::thread::id();
}

void Thread::ThreadTask(void* param)
{
	Runnable* _task = (Runnable*)param;
	_task->run();
}

std::thread::id Thread::currentId()
{
	return std::this_thread::get_id();
}

void Thread::setPriority(Priority priority)
{
	std::thread::native_handle_type handle = m_ThreadImp.native_handle();
	bool _ok = true;
#ifdef WIN32

	switch (priority)
	{
	case Priority_Realtime: _ok = SetThreadPriority(handle, THREAD_PRIORITY_TIME_CRITICAL); break;
	case Priority_Highest: _ok = SetThreadPriority(handle, THREAD_PRIORITY_HIGHEST);       break;
	case Priority_High: _ok = SetThreadPriority(handle, THREAD_PRIORITY_ABOVE_NORMAL);  break;
	case Priority_Normal: _ok = SetThreadPriority(handle, THREAD_PRIORITY_NORMAL);        break;
	case Priority_Low: _ok = SetThreadPriority(handle, THREAD_PRIORITY_BELOW_NORMAL);  break;
	case Priority_Lowest: _ok = SetThreadPriority(handle, THREAD_PRIORITY_LOWEST);        break;
	case Priority_Idle: _ok = SetThreadPriority(handle, THREAD_PRIORITY_IDLE);          break;
	}

	/* MaNGOS use priority for Windows case only
	commented code just for POSIX way reference if will need
	#elif define _POSIX_PRIORITY_SCHEDULING

	int retcode;
	int policy;

	struct sched_param param;

	if (pthread_getschedparam(handle, &policy, &param)) == 0)
	{
	policy = SCHED_FIFO;
	param.sched_priority = ???priority;

	if (pthread_setschedparam(threadID, policy, &param) != 0)
	_ok = false;
	} else
	_ok = false;
	*/
#endif

	// remove this ASSERT in case you don't want to know is thread priority change was successful or not
	ORIGIN_ASSERT(_ok);
}

void Thread::Sleep(unsigned long msecs)
{
	std::this_thread::sleep_for(std::chrono::milliseconds(msecs));
}