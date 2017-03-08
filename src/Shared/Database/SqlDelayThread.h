#ifndef __SQLDELAYTHREAD_H
#define __SQLDELAYTHREAD_H

#include "../Threading.h"
#include "SqlOperations.h"

#include <mutex>
#include <queue>
#include <memory>

class Database;
class SqlOperation;
class SqlConnection;

class SqlDelayThread : public Origin::Runnable
{
private:
	std::mutex m_queueMutex;
	std::queue<std::unique_ptr<SqlOperation>> m_sqlQueue;   ///< Queue of SQL statements
	Database* m_dbEngine;                                   ///< Pointer to used Database engine
	SqlConnection* m_dbConnection;                          ///< Pointer to DB connection
	volatile bool m_running;

	// process all enqueued requests
	void ProcessRequests();

public:
	SqlDelayThread(Database* db, SqlConnection* conn);
	~SqlDelayThread();

	///< Put sql statement to delay queue
	bool Delay(SqlOperation* sql)
	{
		std::lock_guard<std::mutex> guard(m_queueMutex);
		m_sqlQueue.push(std::unique_ptr<SqlOperation>(sql));
		return true;
	}

	virtual void Stop();                                ///< Stop event
	virtual void run();                                 ///< Main Thread loop
};
#endif                                                      //__SQLDELAYTHREAD_H
