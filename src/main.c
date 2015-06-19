//
//  main.c
//  DBPoolMySQL
//
//  Created by Dimitris Bouzikas on 3/3/14.
//  Copyright (c) 2014 dimimpou. All rights reserved.
//

#include <stdio.h>

#include "gwlib.h"
#include "dbpool.h"

#define LIMIT 1000

static DBPool *pool = NULL;

typedef struct {
	int col_1;
	Octstr *col_2;
} MyTableEntry;

MyTableEntry *mytable_entry_create(void)
{
	MyTableEntry *entry;
	
	entry = gw_malloc(sizeof(*entry));
	gw_assert(entry != NULL);
	
	/* set all values to NULL */
	memset(entry, 0, sizeof(*entry));
	
	return entry;
}

MyTableEntry *mytable_entry_duplicate(MyTableEntry *entry)
{
	MyTableEntry *ret;
	
	if (entry == NULL)
		return NULL;
	
	ret = mytable_entry_create();
	ret->col_1 = entry->col_1;
	ret->col_2 = octstr_duplicate(entry->col_2);
	
	return ret;
}

void mytable_entry_destroy(MyTableEntry *entry)
{
	if (entry == NULL)
		return;
	
	entry->col_1 = 0;
	
	if (entry->col_2)
		octstr_destroy(entry->col_2);
	else
		entry->col_2 = NULL;
	
	gw_free(entry);
}

/**
 * Creating a new dbpool
 *
 * @param char *user		username of user
 * @param char *pass		password to authorize user
 * @param char *host		hostname in which mysql will connect
 * @param char *db			the database to connect
 * @param long int port		the port to connect
 * @return DBPool *pool		struct containing pool information
 */
static DBPool *new_dbpool(char *user, char *pass, char *host, char *db, long int port)
{
	DBConf *db_conf = NULL;
	DBPool *pool;
	
	long pool_size;
	
	pool_size = 1;
	
	db_conf = gw_malloc(sizeof(DBConf));
	gw_assert(db_conf != NULL);
	
	db_conf->mysql = gw_malloc(sizeof(MySQLConf));
	gw_assert(db_conf->mysql != NULL);
	
	db_conf->mysql->host = octstr_create(host);
	db_conf->mysql->port = port;
	db_conf->mysql->username = octstr_create(user);
	db_conf->mysql->password = octstr_create(pass);
	db_conf->mysql->database = octstr_create(db);
	
	pool = dbpool_create(DBPOOL_MYSQL, db_conf, pool_size);
	gw_assert(pool != NULL);
	
	if (dbpool_conn_count(pool) == 0)
		panic(0, "MySQL: database pool has no connections!");
	
	return pool;
}

static int example_create(void)
{
	Octstr *sql;
	DBPoolConn *pconn;
	int ret;
	
	pconn = dbpool_conn_consume(pool);
	
	if (pconn == NULL) {
		return -1;
	}
	
	sql = octstr_create("CREATE TABLE IF NOT EXISTS `myTable` ("
						"`col_1` int(11) NOT NULL, "
						"`col_2` varchar(100) CHARACTER SET latin1 COLLATE latin1_general_ci NOT NULL"
						") ENGINE=MyISAM DEFAULT CHARSET=latin1;");
	
	debug("example", 0, "Create Sql: %s", octstr_get_cstr(sql));
	
	if ((ret = dbpool_conn_update(pconn, sql, NULL)) == -1) {
		error(0, "MYSQL: Error while creating table");
	} else if (!ret) {
		warning(0, "MYSQL: Creation of table failed.");
	}
	
	dbpool_conn_produce(pconn);
	octstr_destroy(sql);
	
	return ret;
}

/**
 * Example function for insert.
 * Inserting integer at `col_1` and a char at `col_2`
 */
static int example_insert(int col_1, Octstr *oct_col_2)
{
	Octstr *sql, *oct_col_1;
	DBPoolConn *pconn;
	List *binds = gwlist_create();
	int ret;
	
	pconn = dbpool_conn_consume(pool);
	
	if (pconn == NULL) {
		return -1;
	}
	
	sql = octstr_create("INSERT INTO `myTable` (`col_1`, `col_2`) VALUES (?, ?)");
	
	oct_col_1 = octstr_format("%d", col_1);
	
	gwlist_append(binds, oct_col_1);
	gwlist_append(binds, oct_col_2);

	debug("example", 0, "SQL: %s", octstr_get_cstr(sql));
	
	if ((ret = dbpool_conn_update(pconn, sql, binds)) == -1) {
		error(0, "MYSQL: Error while adding new entry");
	} else if (!ret) {
		warning(0, "MYSQL: No entry was inserted");
	}
	
	dbpool_conn_produce(pconn);
	octstr_destroy(sql);
	gwlist_destroy(binds, NULL);
	octstr_destroy(oct_col_1);
	
	return ret;
}

static MyTableEntry *example_select_row(int col_num)
{
	int ret, len, i;
	List *result, *row;
	List *res  = NULL;
	MyTableEntry *entry;
	DBPoolConn *pconn;
	Octstr *sql, *oct_col_num;
	List *binds = gwlist_create();
	
	pconn = dbpool_conn_consume(pool);
	if (pool == NULL) {
		return NULL;
	}
	
	sql = octstr_create("SELECT `col_1`, `col_2` FROM `myTable` WHERE `col_1` = ? LIMIT 1");
	
	debug("example", 0, "SQL: %s", octstr_get_cstr(sql));
	
	oct_col_num = octstr_format("%d", col_num);
	
	gwlist_append(binds, oct_col_num);
	
	ret = dbpool_conn_select(pconn, sql, binds, &result);
	dbpool_conn_produce(pconn);
	octstr_destroy(sql);
	
	if (ret != 0) {
		return NULL;
	}
	
	if (gwlist_len(result) > 0) {
		row = gwlist_extract_first(result);
		
		entry = mytable_entry_create();
		entry->col_1 = atoi(octstr_get_cstr(gwlist_get(row, 0)));
		entry->col_2 = octstr_create(octstr_get_cstr(gwlist_get(row, 1)));
		
		gwlist_destroy(row, octstr_destroy_item);
	}
	gwlist_destroy(result, NULL);
	
	return entry;
}


int main(int argc, char* argv[])
{
	
	List *res;
	int ret, i;
	int pool_size = 1;
	int port = 3306;
	
	char *db_user = "user";
	char *db_pass = "password";
	char *db_host = "host";
	char *db  = "database_name";
	
	// Required to be called first before start using the lib
	gwlib_init();
	
	pool = new_dbpool(db_user, db_pass, db_host, db, port);
	
	if (pool != NULL) {
		ret = example_create();
		
		for (i = 1; i < LIMIT; i++) {
			Octstr *str = octstr_format("Text_%d", i);
			ret = example_insert(i, str);
			octstr_destroy(str);
		}
		
		MyTableEntry *entry = example_select_row(10);	// retrieving entry with col_1 = 10
		if (entry != NULL) {
			debug("example", 0, "Entry col_1: %d, col_2: %s", entry->col_1, octstr_get_cstr(entry->col_2));
		}
	}
	
	gwlib_shutdown();
	
	exit(EXIT_SUCCESS);
}