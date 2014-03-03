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
 * dbpool_mysql.c - DBPool implementation
 */

#include "dbpool_mysql.h"

#define HAVE_MYSQL

List *gwlist_create_real(void)
{
    List *list;
    
    list = malloc(sizeof(List));
    list->tab = NULL;
    list->tab_size = 0;
    list->start = 0;
    list->len = 0;
    list->single_operation_lock = mutex_create_real();
    list->permanent_lock = mutex_create_real();
    pthread_cond_init(&list->nonempty, NULL);
    list->num_producers = 0;
    list->num_consumers = 0;
    return list;
}

#define INDEX(list, i)	(((list)->start + i) % (list)->tab_size)
#define GET(list, i)	((list)->tab[INDEX(list, i)])

struct threadinfo
{
    pthread_t self;
    const char *name;
    long number;
    int wakefd_recv;
    int wakefd_send;
    /* joiners may be NULL.  It is not allocated until a thread wants
     * to register.  This is safe because the thread table is always
     * locked when a thread accesses this field. */
    List *joiners;
    pid_t pid;
};

long gwthread_self(void)
{
    struct threadinfo *threadinfo = NULL;
    if (threadinfo)
        return threadinfo->number;
    else
        return -1;
}


static void make_bigger(List *list, long items);
static void delete_items_from_list(List *list, long pos, long count);

void gwlist_lock(List *list)
{
    assert(list != NULL);
    mutex_lock(list->permanent_lock);
}

static void lock(List *list){
    gwlist_lock(list);
}

void gwlist_unlock(List *list)
{
    assert(list != NULL);
    mutex_unlock(list->permanent_lock);
}

static void unlock(List *list){
    gwlist_unlock(list);
}

long gwlist_len(List *list)
{
    long len;
    
    if (list == NULL)
        return 0;
    lock(list);
    len = list->len;
    unlock(list);
    return len;
}

void *gwlist_get(List *list, long pos)
{
    void *item;
    
    lock(list);
    assert(pos >= 0);
    assert(pos < list->len);
    item = GET(list, pos);
    unlock(list);
    return item;
}

void gwlist_remove_producer(List *list)
{
    lock(list);
    assert(list->num_producers > 0);
    --list->num_producers;
    pthread_cond_broadcast(&list->nonempty);
    unlock(list);
}

void gwlist_delete(List *list, long pos, long count)
{
    lock(list);
    delete_items_from_list(list, pos, count);
    unlock(list);
}

void *gwlist_extract_first(List *list)
{
    void *item;
    
    assert(list != NULL);
    lock(list);
    if (list->len == 0)
        item = NULL;
    else {
        item = GET(list, 0);
        delete_items_from_list(list, 0, 1);
    }
    unlock(list);
    return item;
}

void gwlist_add_producer(List *list)
{
    lock(list);
    ++list->num_producers;
    unlock(list);
}

void gwlist_append(List *list, void *item)
{
    make_bigger(list, 1);
    list->tab[INDEX(list, list->len)] = item;
    ++list->len;
    pthread_cond_signal(&list->nonempty);
}

void gwlist_produce(List *list, void *item)
{
    gwlist_append(list, item);
}

void *gwlist_consume(List *list)
{
    void *item;
    
    lock(list);
    ++list->num_consumers;
    while (list->len == 0 && list->num_producers > 0) {
        list->single_operation_lock->owner = -1;
        pthread_cleanup_push((void(*)(void*))pthread_mutex_unlock, &list->single_operation_lock->mutex);
        pthread_cond_wait(&list->nonempty,
                          &list->single_operation_lock->mutex);
        pthread_cleanup_pop(0);
    }
    if (list->len > 0) {
        item = GET(list, 0);
        delete_items_from_list(list, 0, 1);
    } else {
        item = NULL;
    }
    --list->num_consumers;
    unlock(list);
    return item;
}

void gwlist_destroy(List *list)
{
    
    if (list == NULL)
        return;
    
    mutex_destroy(list->permanent_lock);
    mutex_destroy(list->single_operation_lock);
    pthread_cond_destroy(&list->nonempty);
    free(list->tab);
    free(list);
}

void gwthread_sleep(double seconds)
{
 
}


/*
 * Make the array bigger. It might be more efficient to make the size
 * bigger than what is explicitly requested.
 *
 * Assume list has been locked for a single operation already.
 */
static void make_bigger(List *list, long items)
{
    long old_size, new_size;
    long len_at_beginning, len_at_end;
    
    if (list->len + items <= list->tab_size)
        return;
    
    old_size = list->tab_size;
    new_size = old_size + items;
    list->tab = realloc(list->tab, new_size * sizeof(void *));
    list->tab_size = new_size;
    
    /*
     * Now, one of the following situations is in effect
     * (* is used, empty is unused element):
     *
     * Case 1: Used area did not wrap. No action is necessary.
     *
     *			   old_size              new_size
     *			   v                     v
     * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     * | |*|*|*|*|*|*| | | | | | | | | | | | | | | | |
     * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     *   ^           ^
     *   start       start+len
     *
     * Case 2: Used area wrapped, but the part at the beginning
     * of the array fits into the new area. Action: move part
     * from beginning to new area.
     *
     *			   old_size              new_size
     *			   v                     v
     * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     * |*|*| | | | | | | |*|*|*| | | | | | | | | | | |
     * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     *     ^             ^
     *     start+len     start
     *
     * Case 3: Used area wrapped, and the part at the beginning
     * of the array does not fit into the new area. Action: move
     * as much as will fit from beginning to new area and move
     * the rest to the beginning.
     *
     *				      old_size   new_size
     *					     v   v
     * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     * |*|*|*|*|*|*|*|*|*| | | | | | | | |*|*|*|*| | |
     * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     *		     ^               ^
     *		     start+len       start
     */
    
    assert(list->start < old_size ||
           (list->start == 0 && old_size == 0));
    if (list->start + list->len > old_size) {
        len_at_end = old_size - list->start;
        len_at_beginning = list->len - len_at_end;
        if (len_at_beginning <= new_size - old_size) {
            /* This is Case 2. */
            memmove(list->tab + old_size,
                    list->tab,
                    len_at_beginning * sizeof(void *));
        } else {
            /* This is Case 3. */
            memmove(list->tab + old_size,
                    list->tab,
                    (new_size - old_size) * sizeof(void *));
            memmove(list->tab,
                    list->tab + (new_size - old_size),
                    (len_at_beginning - (new_size - old_size))
                    * sizeof(void *));
        }
    }
}

/*
 * Remove items `pos' through `pos+count-1' from list. Assume list has
 * been locked by caller already.
 */
static void delete_items_from_list(List *list, long pos, long count)
{
    long i, from, to;
    
    assert(pos >= 0);
    assert(pos < list->len);
    assert(count >= 0);
    assert(pos + count <= list->len);
    
    /*
     * There are four cases:
     *
     * Case 1: Deletion at beginning of list. Just move start
     * marker forwards (wrapping it at end of array). No need
     * to move any items.
     *
     * Case 2: Deletion at end of list. Just shorten the length
     * of the list. No need to move any items.
     *
     * Case 3: Deletion in the middle so that the list does not
     * wrap in the array. Move remaining items at end of list
     * to the place of the deletion.
     *
     * Case 4: Deletion in the middle so that the list does indeed
     * wrap in the array. Move as many remaining items at the end
     * of the list as will fit to the end of the array, then move
     * the rest to the beginning of the array.
     */
    if (pos == 0) {
        list->start = (list->start + count) % list->tab_size;
        list->len -= count;
    } else if (pos + count == list->len) {
        list->len -= count;
    } else if (list->start + list->len < list->tab_size) {
        memmove(list->tab + list->start + pos,
                list->tab + list->start + pos + count,
                (list->len - pos - count) * sizeof(void *));
        list->len -= count;
    } else {
        /*
         * This is not specially efficient, but it's simple and
         * works. Faster methods would have to take more special
         * cases into account.
         */
        for (i = 0; i < list->len - count - pos; ++i) {
            from = INDEX(list, pos + i + count);
            to = INDEX(list, pos + i);
            list->tab[to] = list->tab[from];
        }
        list->len -= count;
    }
}

struct db_ops {
    /*
     * Open db connection with given config params.
     * Config params are specificaly for each database type.
     * return NULL if error occurs ; established connection's pointer otherwise
     */
    void* (*open) (const DBConf *conf);
    /*
     * close given connection.
     */
    void (*close) (void *conn);
    /*
     * check if given connection still alive,
     * return -1 if not or error occurs ; 0 if all was fine
     * NOTE: this function is optional
     */
    int (*check) (void *conn);
    /*
     * Destroy specificaly configuration struct.
     */
    void (*conf_destroy) (DBConf *conf);
    /*
     * Database specific select.
     * Note: Result will be stored as follows:
     *           result is the list of rows each row will be stored also as list each column is stored as Octstr.
     * If someone has better idea please tell me ...
     *
     * @params conn - database specific connection; sql - sql statement ;
     *         binds - list of Octstr values for binding holes in sql (NULL if no binds);
     *         result - result will be saved here
     * @return 0 if all was fine ; -1 otherwise
     */
    int (*select) (void *conn, const char *sql, List *binds, List **result);
    /*
     * Database specific update/insert/delete.
     * @params conn - database specific connection ; sql - sql statement;
     *         binds - list of Octstr values for binding holes in sql (NULL if no binds);
     * @return #rows processed ; -1 if a error occurs
     */
    int (*update) (void *conn, const char *sql, List *binds);
};


static void *mysql_open_conn(const DBConf *db_conf)
{
    MYSQL *mysql = NULL;
    MySQLConf *conf = db_conf->mysql; /* make compiler happy */
    
    /* sanity check */
    if (conf == NULL)
        return NULL;
    
    /* pre-allocate */
    mysql = malloc(sizeof(MYSQL));
    assert(mysql != NULL);
    
    /* initialize mysql structures */
    if (!mysql_init(mysql)) {
        printf("MYSQL: init failed!");
        printf("MYSQL: %s", mysql_error(mysql));
        goto failed;
    }
    
    if (!mysql_real_connect(mysql, conf->host,
                            conf->username,
                            conf->password,
                            conf->database,
                            conf->port, NULL, 0)) {
        printf("MYSQL: can not connect to database!");
        printf("MYSQL: %s", mysql_error(mysql));
        goto failed;
    }
    
    printf("MYSQL: Connected to server at %s.", conf->host);
    printf("MYSQL: server version %s, client version %s.", mysql_get_server_info(mysql), mysql_get_client_info());
    
    return mysql;
    
failed:
    if (mysql != NULL)
        free(mysql);
    return NULL;
}

static void mysql_close_conn(void *conn)
{
    if (conn == NULL)
        return;
    
    mysql_close((MYSQL*) conn);
    free(conn);
}


static int mysql_check_conn(void *conn)
{
    if (conn == NULL)
        return -1;
    
    if (mysql_ping((MYSQL*) conn)) {
        printf("MYSQL: database check failed!");
        printf("MYSQL: %s", mysql_error(conn));
        return -1;
    }
    
    return 0;
}


static int mysql_select(void *conn, const char *sql, List *binds, List **res)
{
    MYSQL_STMT *stmt;
    MYSQL_RES *result;
    MYSQL_BIND *bind = NULL;
    long i, binds_len;
    int ret;
    
    *res = NULL;
    
    /* allocate statement handle */
    stmt = mysql_stmt_init((MYSQL*) conn);
    if (stmt == NULL) {
        printf("MYSQL: mysql_stmt_init(), out of memory.");
        return -1;
    }
    if (mysql_stmt_prepare(stmt, sql, strlen(sql))) {
        printf("MYSQL: Unable to prepare statement: %s", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return -1;
    }
    /* bind params if any */
    binds_len = gwlist_len(binds);
    if (binds_len > 0) {
        bind = malloc(sizeof(MYSQL_BIND) * binds_len);
        memset(bind, 0, sizeof(MYSQL_BIND) * binds_len);
        for (i = 0; i < binds_len; i++) {
            char *str = gwlist_get(binds, i);
            
            bind[i].buffer_type = MYSQL_TYPE_STRING;
            bind[i].buffer = str;
            bind[i].buffer_length = strlen(str);
        }
        /* Bind the buffers */
        if (mysql_stmt_bind_param(stmt, bind)) {
            printf("MYSQL: mysql_stmt_bind_param() failed: `%s'", mysql_stmt_error(stmt));
            free(bind);
            mysql_stmt_close(stmt);
            return -1;
        }
    }
    
    /* execute statement */
    if (mysql_stmt_execute(stmt)) {
        printf("MYSQL: mysql_stmt_execute() failed: `%s'", mysql_stmt_error(stmt));
        free(bind);
        mysql_stmt_close(stmt);
        return -1;
    }
    free(bind);
    
#define DESTROY_BIND(bind, binds_len)           \
do {                                        \
long i;                                 \
for (i = 0; i < binds_len; i++) {       \
free(bind[i].buffer);            \
free(bind[i].length);            \
free(bind[i].is_null);           \
}                                       \
free(bind);                          \
} while(0)
    
    /* Fetch result set meta information */
    result = mysql_stmt_result_metadata(stmt);
    if (res == NULL) {
        printf("MYSQL: mysql_stmt_result_metadata() failed: `%s'", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return -1;
    }
    /* Get total columns in the query */
    binds_len = mysql_num_fields(result);
    bind = malloc(sizeof(MYSQL_BIND) * binds_len);
    memset(bind, 0, sizeof(MYSQL_BIND) * binds_len);
    /* bind result bind */
    for (i = 0; i < binds_len; i++) {
        MYSQL_FIELD *field = mysql_fetch_field(result); /* retrieve field metadata */
        
        printf("gwlib.dbpool_mysql | column=%s buffer_type=%d max_length=%ld length=%ld",field->name, field->type, field->max_length, field->length);
        
        switch(field->type) {
            case MYSQL_TYPE_TIME:
            case MYSQL_TYPE_DATE:
            case MYSQL_TYPE_DATETIME:
            case MYSQL_TYPE_TIMESTAMP:
                bind[i].buffer_type = field->type;
                bind[i].buffer = (char*)malloc(sizeof(MYSQL_TIME));
                bind[i].is_null = malloc(sizeof(my_bool));
                bind[i].length = malloc(sizeof(unsigned long));
                break;
            default:
                bind[i].buffer_type = MYSQL_TYPE_STRING;
                bind[i].buffer = malloc(field->length);
                bind[i].buffer_length = field->length;
                bind[i].length = malloc(sizeof(unsigned long));
                bind[i].is_null = malloc(sizeof(my_bool));
                break;
        }
    }
    mysql_free_result(result);
    
    if (mysql_stmt_bind_result(stmt, bind)) {
        printf("MYSQL: mysql_stmt_bind_result() failed: `%s'", mysql_stmt_error(stmt));
        DESTROY_BIND(bind, binds_len);
        mysql_stmt_close(stmt);
        return -1;
    }
    
    *res = gwlist_create_real();
    while(!(ret = mysql_stmt_fetch(stmt))) {
        List *row = gwlist_create_real();
        for (i = 0; i < binds_len; i++) {
            char *str = NULL;
            MYSQL_TIME *ts;
            
            if (*bind[i].is_null) {
                gwlist_produce(row, "");
                continue;
            }
            
            switch(bind[i].buffer_type) {
                case MYSQL_TYPE_DATE:
                    ts = bind[i].buffer;
                    sprintf(str, "%04d-%02d-%02d", ts->year, ts->month, ts->day);
                    break;
                case MYSQL_TYPE_TIME:
                case MYSQL_TYPE_DATETIME:
                case MYSQL_TYPE_TIMESTAMP:
                    ts = bind[i].buffer;
                    sprintf(str, "%04d-%02d-%02d %02d:%02d:%02d", ts->year, ts->month, ts->day, ts->hour, ts->minute, ts->second);
                    break;
                default:
                    if (bind[i].length == 0)
                        str = "";
                    else
                        snprintf(str, *bind[i].length,"%s", bind[i].buffer);
                    break;
            }
            gwlist_produce(row, str);
        }
        gwlist_produce(*res, row);
    }
    DESTROY_BIND(bind, binds_len);
#undef DESTROY_BIND
    
    /* any errors by fetch? */
    if (ret != MYSQL_NO_DATA) {
        List *row;
        printf("MYSQL: mysql_stmt_bind_result() failed: `%s'", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        while((row = gwlist_extract_first(*res)) != NULL)
            gwlist_destroy(row);
        gwlist_destroy(*res);
        *res = NULL;
        return -1;
    }
    
    mysql_stmt_close(stmt);
    
    return 0;
}


static int mysql_update2(void *conn, const char *sql, List *binds)
{
    MYSQL_STMT *stmt;
    MYSQL_BIND *bind = NULL;
    long i, binds_len;
    int ret;
    
    /* allocate statement handle */
    stmt = mysql_stmt_init((MYSQL*) conn);
    if (stmt == NULL) {
        printf("MYSQL: mysql_stmt_init(), out of memory.");
        return -1;
    }
    if (mysql_stmt_prepare(stmt, sql, strlen(sql))) {
        printf("MYSQL: Unable to prepare statement: %s", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return -1;
    }
    /* bind params if any */
    binds_len = gwlist_len(binds);
    if (binds_len > 0) {
        bind = malloc(sizeof(MYSQL_BIND) * binds_len);
        memset(bind, 0, sizeof(MYSQL_BIND) * binds_len);
        for (i = 0; i < binds_len; i++) {
            char *str = gwlist_get(binds, i);
            
            bind[i].buffer_type = MYSQL_TYPE_STRING;
            bind[i].buffer = str;
            bind[i].buffer_length = strlen(str);
        }
        /* Bind the buffers */
        if (mysql_stmt_bind_param(stmt, bind)) {
            printf("MYSQL: mysql_stmt_bind_param() failed: `%s'", mysql_stmt_error(stmt));
            free(bind);
            mysql_stmt_close(stmt);
            return -1;
        }
    }
    
    /* execute statement */
    if (mysql_stmt_execute(stmt)) {
        printf("MYSQL: mysql_stmt_execute() failed: `%s'", mysql_stmt_error(stmt));
        free(bind);
        mysql_stmt_close(stmt);
        return -1;
    }
    free(bind);
    
    ret = mysql_stmt_affected_rows(stmt);
    mysql_stmt_close(stmt);
    
    return ret;
}


static void mysql_conf_destroy(DBConf *db_conf)
{
    MySQLConf *conf = db_conf->mysql;
    
    free(conf->host);
    free(conf->username);
    free(conf->password);
    free(conf->database);
    
    free(conf);
    free(db_conf);
}

static struct db_ops mysql_ops = {
    .open = mysql_open_conn,
    .close = mysql_close_conn,
    .check = mysql_check_conn,
    .select = mysql_select,
    .update = mysql_update2,
    .conf_destroy = mysql_conf_destroy
};

static void dbpool_conn_destroy(DBPoolConn *conn)
{
    assert(conn != NULL);
    
    if (conn->conn != NULL)
        conn->pool->db_ops->close(conn->conn);
    
    free(conn);
}

DBPool *dbpool_create(enum db_type db_type, DBConf *conf, unsigned int connections)
{
    DBPool *p;
    
    if (conf == NULL)
        return NULL;
    
    p = malloc(sizeof(p));
    assert(p != NULL);
    p->pool = gwlist_create_real();
    gwlist_add_producer(p->pool);
    p->max_size = connections;
    p->curr_size = 0;
    p->conf = conf;
    p->db_type = db_type;
    
    p->db_ops = &mysql_ops;
    dbpool_increase(p, connections);
    
    return p;
}

void dbpool_destroy(DBPool *p)
{
    
    if (p == NULL)
        return; /* nothing todo here */
    
    assert(p->pool != NULL && p->db_ops != NULL);
    
    gwlist_remove_producer(p->pool);
    gwlist_destroy(p->pool);
    
    p->db_ops->conf_destroy(p->conf);
    free(p);
}

int dbpool_increase(DBPool *p, unsigned int count)
{
    unsigned int i, opened = 0;
    
    assert(p != NULL && p->conf != NULL && p->db_ops != NULL && p->db_ops->open != NULL);
    
    
    /* lock dbpool for updates */
    gwlist_lock(p->pool);
    
    /* ensure we don't increase more items than the max_size border */
    for (i=0; i < count && p->curr_size < p->max_size; i++) {
        void *conn = p->db_ops->open(p->conf);
        if (conn != NULL) {
            DBPoolConn *pc = malloc(sizeof(DBPoolConn));
            assert(pc != NULL);
            
            pc->conn = conn;
            pc->pool = p;
            
            p->curr_size++;
            opened++;
            gwlist_produce(p->pool, pc);
        }
    }
    
    /* unlock dbpool for updates */
    gwlist_unlock(p->pool);
    
    return opened;
}

unsigned int dbpool_decrease(DBPool *p, unsigned int c)
{
    unsigned int i;
    
    assert(p != NULL && p->pool != NULL && p->db_ops != NULL && p->db_ops->close != NULL);
    
    /* lock dbpool for updates */
    gwlist_lock(p->pool);
    
    /*
     * Ensure we don't try to decrease more then available in pool.
     */
    for (i = 0; i < c; i++) {
        DBPoolConn *pc;
        
        /* gwlist_extract_first doesn't block even if no conn here */
        pc = gwlist_extract_first(p->pool);
        
        /* no conn availible anymore */
        if (pc == NULL)
            break;
        
        /* close connections and destroy pool connection */
        dbpool_conn_destroy(pc);
        p->curr_size--;
    }
    
    /* unlock dbpool for updates */
    gwlist_unlock(p->pool);
    
    return i;
}

long dbpool_conn_count(DBPool *p)
{
    assert(p != NULL && p->pool != NULL);
    
    return gwlist_len(p->pool);
}

DBPoolConn *dbpool_conn_consume(DBPool *p)
{
    DBPoolConn *pc;
    
    assert(p != NULL && p->pool != NULL);
    
    /* check for max connections and if 0 return NULL */
    if (p->max_size < 1)
        return NULL;
    
    /* check if we have any connection */
    while (p->curr_size < 1) {
        printf("DBPool has no connections, reconnecting up to maximum...");
        
        /* dbpool_increase ensure max_size is not exceeded so don't lock */
        dbpool_increase(p, p->max_size - p->curr_size);
    }
    
    /* garantee that you deliver a valid connection to the caller */
    while ((pc = gwlist_consume(p->pool)) != NULL) {
        
        /*
         * XXX check that the connection is still existing.
         * Is this a performance bottle-neck?!
         */
        if (!pc->conn || (p->db_ops->check && p->db_ops->check(pc->conn) != 0)) {
            /* something was wrong, reinitialize the connection */
            /* lock dbpool for update */
            gwlist_lock(p->pool);
            dbpool_conn_destroy(pc);
            p->curr_size--;
            /* unlock dbpool for update */
            gwlist_unlock(p->pool);
            /*
             * maybe not needed, just try to get next connection, but it
             * can be dangeros if all connections where broken, then we will
             * block here for ever.
             */
            while (p->curr_size < 1) {
                printf("DBPool has too few connections, reconnecting up to maximum...");
                
                /* dbpool_increase ensure max_size is not exceeded so don't lock */
                dbpool_increase(p, p->max_size - p->curr_size);
                if (p->curr_size < 1)
                    gwthread_sleep(0.1);
            }
            
        } else {
            break;
        }
    }
    
    return (pc->conn != NULL ? pc : NULL);
}

void dbpool_conn_produce(DBPoolConn *pc)
{
    assert(pc != NULL && pc->conn != NULL && pc->pool != NULL && pc->pool->pool != NULL);
    
    gwlist_produce(pc->pool->pool, pc);
}


unsigned int dbpool_check(DBPool *p)
{
    long i, len, n = 0, reinit = 0;
    
    assert(p != NULL && p->pool != NULL && p->db_ops != NULL);
    
    /*
     * First check if db_ops->check function pointer is here.
     * NOTE: db_ops->check is optional, so if it is not there, then
     * we have nothing todo and we simple return list length.
     */
    if (p->db_ops->check == NULL)
        return gwlist_len(p->pool);
    
    gwlist_lock(p->pool);
    len = gwlist_len(p->pool);
    for (i = 0; i < len; i++) {
        DBPoolConn *pconn;
        
        pconn = gwlist_get(p->pool, i);
        if (p->db_ops->check(pconn->conn) != 0) {
            /* something was wrong, reinitialize the connection */
            gwlist_delete(p->pool, i, 1);
            dbpool_conn_destroy(pconn);
            p->curr_size--;
            reinit++;
            len--;
            i--;
        } else {
            n++;
        }
    }
    gwlist_unlock(p->pool);
    
    /* reinitialize brocken connections */
    if (reinit > 0)
        n += dbpool_increase(p, reinit);
    

    return n;
}


int dbpool_conn_select(DBPoolConn *conn, const char *sql, List *binds, List **result)
{
    if (sql == NULL || conn == NULL)
        return -1;
    
    if (conn->pool->db_ops->select == NULL)
        return -1; /* may be panic here ??? */
    
    return conn->pool->db_ops->select(conn->conn, sql, binds, result);
}


int dbpool_conn_update(DBPoolConn *conn, const char *sql, List *binds)
{
    if (sql == NULL || conn == NULL)
        return -1;
    
    if (conn->pool->db_ops->update == NULL)
        return -1; /* may be panic here ??? */
    
    return conn->pool->db_ops->update(conn->conn, sql, binds);
}



