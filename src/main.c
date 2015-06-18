//
//  main.c
//  DBPoolMySQL
//
//  Created by Dimitris Bouzikas on 3/3/14.
//  Copyright (c) 2014 dimimpou. All rights reserved.
//

#include <stdio.h>

#include "thread.h"
#include "dbpool_mysql.h"


typedef struct confdata
{
    DBConf *db_conf;        /* Database connection variables. */
    DBPool *db_pool;
    char *db_user, *db_pass, *db,  *db_table, *db_host;
    unsigned int db_pool_size, db_port;
    
} ConfData;

/**
 * Preparing using
 *
 * @param char *user, username of user
 * @param char *pass, password to authorize user
 * @param char *host, hostname in which mysql will connect
 * @param char *db, the database to connect
 * @param long int port, the port to connect
 * @return DBConf *conf, struct of entire configuration
 */
static DBConf *createConfigFile(char *user, char *pass, char *host, char *db, long int port)
{
    DBConf *conf;
    conf = malloc(sizeof(DBConf));
    conf->mysql = malloc(sizeof(MySQLConf));
    
    conf->mysql->username = user;
    conf->mysql->password = pass;
    conf->mysql->database = db;
    conf->mysql->host = host;
    conf->mysql->port = port;
    
    return conf;
}

static long mysql_update(const char *sql, DBPool *pool)
{
    DBPoolConn *pc;
    long res = 0;
	
    pc = dbpool_conn_consume(pool);
    if (pc == NULL) {
        printf("MYSQL: Database pool got no connection! DB update failed!");
        return 0;
    }
    
    if (mysql_query(pc->conn, sql) != 0) {
        printf("MYSQL: %s", mysql_error(pc->conn));
    } else {
        res = mysql_affected_rows(pc->conn);
    }
	
    dbpool_conn_produce(pc);
    
    return res;
}

static MYSQL_RES *mysql_select(const char *sql, DBPool *pool)
{
    DBPoolConn *pc;
	
    pc = dbpool_conn_consume(pool);
    if (pc == NULL) {
        printf("MYSQL: Database pool got no connection! DB update failed!");
        return 0;
    }
    
    if (mysql_query(pc->conn, sql) != 0) {
        printf("MYSQL: %s", mysql_error(pc->conn));
    }
	
    dbpool_conn_produce(pc);
    
    return mysql_store_result(pc->conn);
}


int main(int argc, char* argv[])
{
    static ConfData conf_data;
    
    conf_data.db_pool_size = 1;
    conf_data.db_user = "user";
    conf_data.db_pass = "password";
    conf_data.db_host = "hostname";
    conf_data.db  = "db_name";
    conf_data.db_port = 3306;
    
    conf_data.db_conf = createConfigFile(conf_data.db_user, conf_data.db_pass, conf_data.db_host, conf_data.db, conf_data.db_port);
    conf_data.db_pool = dbpool_create(DBPOOL_MYSQL, conf_data.db_conf, conf_data.db_pool_size);
    
    if (dbpool_conn_count(conf_data.db_pool) == 0) {
        printf("Unable to start without DBConns...");
        exit(1);
    }
    
    char *sql = "UPDATE mtTable SET column='This is the %d update' WHERE id='1'";
    char *sqlQuery = (char *)calloc(strlen(sql) + 10, sizeof(char));
    
    int i = 1;
    int lim_loop = 10000;
    // A test big loop
    do {
        sprintf(sqlQuery, sql, i);
        mysql_update(sqlQuery, conf_data.db_pool);
        i++;
    } while (i < lim_loop);
    
    char *selectQuery = "SELECT column FROM mtTable WHERE id = '1'";
    MYSQL_RES *myRes = mysql_select(selectQuery, conf_data.db_pool);
    MYSQL_ROW record = mysql_fetch_row(myRes);
    printf("Column value is: %s", record[0]);
    
    exit(EXIT_SUCCESS);
}

