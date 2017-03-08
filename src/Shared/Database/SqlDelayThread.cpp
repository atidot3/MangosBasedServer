#include "SqlDelayThread.h"
#include "SqlOperations.h"
#include "DatabaseEnv.h"

SqlDelayThread::SqlDelayThread(Database* db, SqlConnection* conn) : m_dbEngine(db), m_dbConnection(conn), m_running(true)
{
}

SqlDelayThread::~SqlDelayThread()
{
	// process all requests which might have been queued while thread was stopping
	ProcessRequests();
}

void SqlDelayThread::run()
{
#ifndef DO_POSTGRESQL
	mysql_thread_init();
#endif

	const uint32 loopSleepms = 10;

	const uint32 pingEveryLoop = m_dbEngine->GetPingIntervall() / loopSleepms;

	uint32 loopCounter = 0;
	while (m_running)
	{
		// if the running state gets turned off while sleeping
		// empty the queue before exiting
		Origin::Thread::Sleep(loopSleepms);

		ProcessRequests();

		if ((loopCounter++) >= pingEveryLoop)
		{
			loopCounter = 0;
			m_dbEngine->Ping();
		}
	}

#ifndef DO_POSTGRESQL
	mysql_thread_end();
#endif
}

void SqlDelayThread::Stop()
{
	m_running = false;
}

void SqlDelayThread::ProcessRequests()
{
	std::queue<std::unique_ptr<SqlOperation>> sqlQueue;

	// we need to move the contents of the queue to a local copy because executing these statements with the
	// lock in place can result in a deadlock with the world thread which calls Database::ProcessResultQueue()
	{
		std::lock_guard<std::mutex> guard(m_queueMutex);
		sqlQueue = std::move(m_sqlQueue);
	}

	while (!sqlQueue.empty())
	{
		auto const s = std::move(sqlQueue.front());
		sqlQueue.pop();
		s->Execute(m_dbConnection);
	}
}
