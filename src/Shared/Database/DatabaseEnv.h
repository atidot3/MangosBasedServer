#if !defined(DATABASEENV_H)
#define DATABASEENV_H

#include "../Common.h"
//#include "Log.h"
#include "Errors.h"

#include "Field.h"
#include "QueryResult.h"

#ifdef DO_POSTGRESQL
#include "Database/QueryResultPostgre.h"
#include "Database/Database.h"
#include "Database/DatabasePostgre.h"
typedef DatabasePostgre DatabaseType;
#define _LIKE_           "ILIKE"
#define _TABLE_SIM_      "\""
#define _CONCAT3_(A,B,C) "( " A " || " B " || " C " )"
#define _OFFSET_         "LIMIT 1 OFFSET %d"
#else
#include "QueryResultMysql.h"
#include "Database.h"
#include "DatabaseMysql.h"
typedef DatabaseMysql DatabaseType;
#define _LIKE_           "LIKE"
#define _TABLE_SIM_      "`"
#define _CONCAT3_(A,B,C) "CONCAT( " A " , " B " , " C " )"
#define _OFFSET_         "LIMIT %d,1"
#endif

extern DatabaseType WorldDatabase;
extern DatabaseType CharacterDatabase;
extern DatabaseType LoginDatabase;

#endif
