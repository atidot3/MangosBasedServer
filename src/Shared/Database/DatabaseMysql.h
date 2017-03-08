#ifndef DO_POSTGRESQL

#ifndef _DATABASEMYSQL_H
#define _DATABASEMYSQL_H

//#include "Common.h"
#include "Database.h"
#include "../Config/Singleton.h"

#ifdef WIN32
#include <mysql/mysql.h>
#else
#include <mysql.h>
#endif

// MySQL prepared statement class
class MySqlPreparedStatement : public SqlPreparedStatement
{
public:
	MySqlPreparedStatement(const std::string& fmt, SqlConnection& conn, MYSQL* mysql);
	~MySqlPreparedStatement();

	// prepare statement
	virtual bool prepare() override;

	// bind input parameters
	virtual void bind(const SqlStmtParameters& holder) override;

	// execute DML statement
	virtual bool execute() override;

protected:
	// bind parameters
	void addParam(unsigned int nIndex, const SqlStmtFieldData& data);

	static enum_field_types ToMySQLType(const SqlStmtFieldData& data, my_bool& bUnsigned);

private:
	void RemoveBinds();

	MYSQL* m_pMySQLConn;
	MYSQL_STMT* m_stmt;
	MYSQL_BIND* m_pInputArgs;
	MYSQL_BIND* m_pResult;
	MYSQL_RES* m_pResultMetadata;
};

class MySQLConnection : public SqlConnection
{
public:
	MySQLConnection(Database& db) : SqlConnection(db), mMysql(nullptr) {}
	~MySQLConnection();

	//! Initializes Mysql and connects to a server.
	/*! infoString should be formated like hostname;username;password;database. */
	bool Initialize(const char* infoString) override;

	QueryResult* Query(const char* sql) override;
	QueryNamedResult* QueryNamed(const char* sql) override;
	bool Execute(const char* sql) override;

	unsigned long escape_string(char* to, const char* from, unsigned long length);

	bool BeginTransaction() override;
	bool CommitTransaction() override;
	bool RollbackTransaction() override;

protected:
	SqlPreparedStatement* CreateStatement(const std::string& fmt) override;

private:
	bool _TransactionCmd(const char* sql);
	bool _Query(const char* sql, MYSQL_RES** pResult, MYSQL_FIELD** pFields, uint64* pRowCount, uint32* pFieldCount);

	MYSQL* mMysql;
};

class DatabaseMysql : public Database
{
	friend class Origin::OperatorNew<DatabaseMysql>;

public:
	DatabaseMysql();
	~DatabaseMysql();

	// must be call before first query in thread
	void ThreadStart() override;
	// must be call before finish thread run
	void ThreadEnd() override;

protected:
	virtual SqlConnection* CreateConnection() override;

private:
	static size_t db_count;
};

#endif
#endif
