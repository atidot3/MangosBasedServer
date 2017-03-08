#ifndef DO_POSTGRESQL

#if !defined(QUERYRESULTMYSQL_H)
#define QUERYRESULTMYSQL_H

#include "../Common.h"

#ifdef WIN32
#include <WinSock2.h>
#include <mysql/mysql.h>
#else
#include <mysql.h>
#endif

class QueryResultMysql : public QueryResult
{
public:
	QueryResultMysql(MYSQL_RES* result, MYSQL_FIELD* fields, uint64 rowCount, uint32 fieldCount);

	~QueryResultMysql();

	bool NextRow() override;

private:
	enum Field::DataTypes ConvertNativeType(enum_field_types mysqlType) const;
	void EndQuery();

	MYSQL_RES* mResult;
};
#endif
#endif
