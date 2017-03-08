#ifndef __SQLOPERATIONS_H
#define __SQLOPERATIONS_H

#include "../Common.h"
#include "../Util/Callback.h"

#include <queue>
#include <vector>
#include <mutex>
#include <memory>

/// ---- BASE ---

class Database;
class SqlConnection;
class SqlDelayThread;
class SqlStmtParameters;

class SqlOperation
{
public:
	virtual void OnRemove() { delete this; }
	virtual bool Execute(SqlConnection* conn) = 0;
	virtual ~SqlOperation() {}
};

/// ---- ASYNC STATEMENTS / TRANSACTIONS ----

class SqlPlainRequest : public SqlOperation
{
private:
	const char* m_sql;
public:
	SqlPlainRequest(const char* sql) : m_sql(origin_strdup(sql)) {}
	~SqlPlainRequest() { char* tofree = const_cast<char*>(m_sql); delete[] tofree; }
	bool Execute(SqlConnection* conn) override;
};

class SqlTransaction : public SqlOperation
{
private:
	std::vector<SqlOperation* > m_queue;

public:
	SqlTransaction() {}
	~SqlTransaction();

	void DelayExecute(SqlOperation* sql) { m_queue.push_back(sql); }

	bool Execute(SqlConnection* conn) override;
};

class SqlPreparedRequest : public SqlOperation
{
public:
	SqlPreparedRequest(int nIndex, SqlStmtParameters* arg);
	~SqlPreparedRequest();

	bool Execute(SqlConnection* conn) override;

private:
	const int m_nIndex;
	SqlStmtParameters* m_param;
};

/// ---- ASYNC QUERIES ----

class SqlQuery;                                             /// contains a single async query
class QueryResult;                                          /// the result of one
class SqlQueryHolder;                                       /// groups several async quries
class SqlQueryHolderEx;                                     /// points to a holder, added to the delay thread

class SqlResultQueue
{
private:
	std::mutex m_mutex;
	std::queue<std::unique_ptr<Origin::IQueryCallback>> m_queue;

public:
	void Update();
	void Add(Origin::IQueryCallback *);
};

class SqlQuery : public SqlOperation
{
private:
	std::vector<char> m_sql;
	Origin::IQueryCallback * const m_callback;
	SqlResultQueue * const m_queue;

public:
	SqlQuery(const char* sql, Origin::IQueryCallback* callback, SqlResultQueue* queue)
		: m_sql(strlen(sql) + 1), m_callback(callback), m_queue(queue)
	{
		memcpy(&m_sql[0], sql, m_sql.size());
	}

	bool Execute(SqlConnection* conn) override;
};

class SqlQueryHolder
{
	friend class SqlQueryHolderEx;
private:
	typedef std::pair<const char*, QueryResult*> SqlResultPair;
	std::vector<SqlResultPair> m_queries;
public:
	SqlQueryHolder() {}
	~SqlQueryHolder();
	bool SetQuery(size_t index, const char* sql);
	bool SetPQuery(size_t index, const char* format, ...) ATTR_PRINTF(3, 4);
	void SetSize(size_t size);
	QueryResult* GetResult(size_t index);
	void SetResult(size_t index, QueryResult* result);
	bool Execute(Origin::IQueryCallback* callback, SqlDelayThread* thread, SqlResultQueue* queue);
};

class SqlQueryHolderEx : public SqlOperation
{
private:
	SqlQueryHolder* m_holder;
	Origin::IQueryCallback* m_callback;
	SqlResultQueue* m_queue;
public:
	SqlQueryHolderEx(SqlQueryHolder* holder, Origin::IQueryCallback* callback, SqlResultQueue* queue)
		: m_holder(holder), m_callback(callback), m_queue(queue) {}
	bool Execute(SqlConnection* conn) override;
};
#endif                                                      //__SQLOPERATIONS_H
