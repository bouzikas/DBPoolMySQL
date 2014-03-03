/* ====================================================================
 * The Kannel Software License, Version 1.0
 *
 * Copyright (c) 2001-2013 Kannel Group
 * Copyright (c) 1998-2001 WapIT Ltd.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. The end-user documentation included with the redistribution,
 *    if any, must include the following acknowledgment:
 *       "This product includes software developed by the
 *        Kannel Group (http://www.kannel.org/)."
 *    Alternately, this acknowledgment may appear in the software itself,
 *    if and wherever such third-party acknowledgments normally appear.
 *
 * 4. The names "Kannel" and "Kannel Group" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please
 *    contact org@kannel.org.
 *
 * 5. Products derived from this software may not be called "Kannel",
 *    nor may "Kannel" appear in their name, without prior written
 *    permission of the Kannel Group.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE KANNEL GROUP OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the Kannel Group.  For more information on
 * the Kannel Group, please see <http://www.kannel.org/>.
 *
 * Portions of this software are based upon software originally written at
 * WapIT Ltd., Helsinki, Finland for the Kannel project.
 */

/*
 * dbpool_mysql.h - DBPool implementation
 */

#ifndef dbpool_mysql_h
#define dbpool_mysql_h

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>

#include <pthread.h>
#include <semaphore.h>

#include <assert.h>
#include <stddef.h>
#include <pthread.h>

#include "thread.h"

#include </usr/local/mysql/include/mysql.h>

#define HAVE_MYSQL

enum db_type {
    DBPOOL_MYSQL
};

typedef struct DBPool DBPool;

typedef struct {
    void *conn; /* the pointer holding the database specific connection */
    DBPool *pool; /* pointer of the pool where this connection belongs to */
} DBPoolConn;

typedef struct {
    char *host;
    long port;
    char *username;
    char *password;
    char *database;
} MySQLConf;

typedef union {
    MySQLConf *mysql;
} DBConf;

struct List
{
    void **tab;
    long tab_size;
    long start;
    long len;
    Mutex *single_operation_lock;
    Mutex *permanent_lock;
    pthread_cond_t nonempty;
    long num_producers;
    long num_consumers;
};

typedef struct List List;

struct DBPool
{
    List *pool; /* queue representing the pool */
    unsigned int max_size; /* max #connections */
    unsigned int curr_size; /* current #connections */
    DBConf *conf; /* the database type specific configuration block */
    struct db_ops *db_ops; /* the database operations callbacks */
    enum db_type db_type; /* the type of database */
};

long dbpool_conn_count(DBPool *p);

//int mysql_select(void *conn, const char *sql, List *binds, List **res);
/**
 *
 */
DBPool *dbpool_create(enum db_type db_type, DBConf *conf, unsigned int connections);
DBPoolConn *dbpool_conn_consume(DBPool *p);
void dbpool_conn_produce(DBPoolConn *pc);

/**
 *
 */
//long long mysql_update(const char *sql, DBPool *pool);

/**
 *
 */
int dbpool_increase(DBPool *p, unsigned int count);

#endif
