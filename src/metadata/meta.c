#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>

#include "meta.h"


int main(void) {
  // printf("%s\n", sqlite3_libversion());

  sqlite3 *db;
  sqlite3_stmt *res;
  char *sql;
  char *remainder;
  UNUSED(remainder);

  // open database - either in memory or from file
  open_db("Metadata-File.db", &db);
  // check if tables exist
  // else create the tables
  int ret = create_tables(db);
  if(ret == -1){
    fprintf(stdout, "Tables probably exist already!\n");
  }
  
  printf("Initializing LRU block...\n");
  init_lru_blk();

  set_block_size(1024);

  // function to init cache_used_size 
  printf("\n");
  init_cache_used_size(db);
  printf("Cache Usage: %lu\n", cache_used_size);

  // insert new file into metadata
  create_file(db, "newdir/hello/Hello.txt", 1234);
  create_file(db, "newdir/hello/Test2.txt", 1234);
  // function to create datablock entry
  printf("Calling insert block...\n");
  insert_block(db, "newdir/hello/Hello.txt", 1234);
  insert_block(db, "newdir/hello/Hello.txt", 1578);

  insert_block(db, "newdir/hello/Test2.txt", 1234);
  insert_block(db, "newdir/hello/Test2.txt", 1578);

  printf("Cache Usage: %lu\n", cache_used_size);


  // function to update remote file size- call on close() &|or open()

  // function to update block timestamp

  // function to delete file (and relevant data-blocks) - using FK cascade

  // check file in cache

  // check block in cache

  // check blocks in cache

  // close database
  sqlite3_close(db);

  
  UNUSED(res);
  UNUSED(sql);

  return EXIT_SUCCESS;
}

void set_block_size(size_t blk_size){
  block_size = blk_size;
  if (VERBOSE)
  {
    printf("Block Size Set to %lu\n", block_size);
  }
}

size_t str_to_num(const char *str) {
  char *endptr;
  // check for -ve nos.
  if (str[0] == '-') {
    printf("Invalid Input:\t%s\n", str);
    exit(EXIT_FAILURE);
  }
  errno = 0;
  size_t val = strtoul(str, &endptr, 10);

  if (errno) {
    perror("Invalid Input: ");
    exit(EXIT_FAILURE);
  }

  if (endptr == str) {
    fprintf(stderr, "Invalid Input:\t%s\nNo digits were found.\n", str);
    exit(EXIT_FAILURE);
  }

  return val;
}


// For pre-exisiting metadata, see how much file size is used already
void init_cache_used_size(sqlite3 *db){
  char *sql, *ErrMsg;
  sql = "SELECT SUM(local_size) FROM Files;";

  /* Execute SQL statement */
  int ret = sqlite3_exec(db, sql, callback, 0, &ErrMsg);
  // check return code for status
   
   if (ret != SQLITE_OK){
      fprintf(stderr, "Create Tables: SQL error: %s\n", ErrMsg);
      sqlite3_free(ErrMsg);
   }
}

// Used for sqlite3
static int callback(void *NotUsed, int argc, char **argv, char **ColName) {
  if (argc == 1)
  {
    // Check if it is the SUM(local_size)
    // if yes, set cache_used_size
    if (strcmp(ColName[0], "SUM(local_size)") == 0)
    {
      cache_used_size = (argv[0] ? str_to_num(argv[0]) : cache_used_size);
      return 0;
    }
  }

  int i;
  for (i = 0; i < argc; i++) {
    printf("%s = %s\n", ColName[i], argv[i] ? argv[i] : "NULL");
  }
  printf("\n");
  return 0;
}

// open database and return the database pointer
void open_db(char * db_name, sqlite3 ** db){
  int ret = sqlite3_open(db_name, db);

  if (ret != SQLITE_OK) {
    fprintf(stderr, "Cannot open database: %s, \n", sqlite3_errmsg(*db));
    sqlite3_close(*db);

    exit(EXIT_FAILURE);
  }

  if (VERBOSE) {
    printf("Opened database successfully!\n");
  }
}

// create the FILES and DATABLOCKS tables
int create_tables(sqlite3 * db){
  char *sql;
  char *ErrMsg = 0;

  /* Create sql statement for table creation */
  sql = "CREATE TABLE Files ("
       "file_id    INTEGER PRIMARY KEY,"
       "relative_path  TEXT  NOT NULL UNIQUE,"
       "remote_size  INTEGER NOT NULL,"
       "local_size INTEGER NOT NULL DEFAULT 0"
  ");"
  "CREATE TABLE Datablocks("
       "block_id   INTEGER   PRIMARY KEY,"
       "blk_start_offset INTEGER   NOT NULL,"
       "valid    BOOLEAN   NOT NULL DEFAULT 1,"
       "timestamp  DATETIME  NOT NULL DEFAULT (DATETIME('now')),"
       "file_id    INTEGER   NOT NULL,"

       "UNIQUE(blk_start_offset, file_id),"

       "CONSTRAINT fk_column"
       " FOREIGN KEY (file_id) REFERENCES Files(file_id) "
       " ON DELETE CASCADE"
       // add foreign key: ON DELETE CASCADE
  ");";

  /* Execute SQL statement */
  int ret = sqlite3_exec(db, sql, callback, 0, &ErrMsg);
  // check return code for status
   
   if (ret != SQLITE_OK){
      fprintf(stderr, "Create Tables: SQL error: %s\n", ErrMsg);
      sqlite3_free(ErrMsg);
      return -1;
   } 
   if (VERBOSE) {
      fprintf(stdout, "Tables created successfully!\n");
   }
   return 0;

}

// insert file entry into database
// ips: filename, remote_size
int create_file(sqlite3* db, char * filename, size_t remote_size){
  // INSERT INTO FILES
  char *sql;
  char *ErrMsg = 0;
  UNUSED(ErrMsg);
  sqlite3_stmt *stmt;

  /*-----------Insert into Files------------*/
  sql = "INSERT INTO Files(relative_path, remote_size)"
        " VALUES (?1, ?2);"; // +filename
  /* 
  Prepare
  Bind
  Step
  Finalize
  */
  // Prepare stmt
  sqlite3_prepare_v2(db, sql, -1, &stmt, NULL); 
  // Bind
  sqlite3_bind_text(stmt, 1, filename, -1, SQLITE_STATIC);
  sqlite3_bind_int64(stmt, 2, remote_size);
  sqlite3_bind_int64(stmt, 3, 0); // starting with 0 local size
  // STEP
  int ret = sqlite3_step(stmt); 
  if (ret != SQLITE_DONE) {
    printf("Create File: SQL Error: %s\n", sqlite3_errmsg(db));
    return -1;
  }
  ret = sqlite3_finalize(stmt);
  // check return code for status
   
  if (ret != SQLITE_OK){
    fprintf(stderr, "Create File: Finalize: SQL error: %s\n", ErrMsg);
    sqlite3_free(ErrMsg);
    return -1;
  }
  /*-----------Insert into Files------------*/

  if (VERBOSE) {
    fprintf(stdout, "File inserted\n");
  }
  return 0;

}

int insert_block(sqlite3* db, char * filename, int blk_offset){
  // INSERT INTO DATABLOCKS
  // Then update the local_size of the file
  // and update cache_size_used variable
  char *sql;
  char *ErrMsg = 0;
  sqlite3_stmt *stmt;

  /*-----------Insert into Datablocks------------*/
  sql = "INSERT INTO Datablocks (blk_start_offset, file_id) VALUES"
        "( ?1, (SELECT file_id from Files WHERE relative_path=?2) );";
  /* 
  Prepare
  Bind
  Step
  Finalize
  */
  // Prepare stmt
  sqlite3_prepare_v2(db, sql, -1, &stmt, NULL); 
  // Bind
  sqlite3_bind_int64(stmt, 1, blk_offset);
  sqlite3_bind_text(stmt, 2, filename, -1, SQLITE_STATIC);
  // STEP
  int ret = sqlite3_step(stmt); 
  if (ret != SQLITE_DONE) {
    printf("Insert Block: SQL Error: %s\n", sqlite3_errmsg(db));
    return -1;
  }
    ret = sqlite3_finalize(stmt);
  // check return code for status
   
  if (ret != SQLITE_OK){
    fprintf(stderr, "Insert Block: Finalize: SQL error: %s\n", ErrMsg);
    sqlite3_free(ErrMsg);
    return -1;
  }
  /*-----------Insert into Datablocks------------*/

  /*-----------Update local_size in Files------------*/
  // UPDATE local_size in Files
  sql = "UPDATE Files SET local_size = local_size + ?1"
        " WHERE relative_path = ?2;";
  // Prepare stmt
  sqlite3_prepare_v2(db, sql, -1, &stmt, NULL); 
  // Bind
  sqlite3_bind_int64(stmt, 1, block_size);
  sqlite3_bind_text(stmt, 2, filename, -1, SQLITE_STATIC);
  // STEP
  ret = sqlite3_step(stmt); 
  if (ret != SQLITE_DONE) {
    printf("Create File: SQL Error: %s\n", sqlite3_errmsg(db));
    return -1;
  }
  ret = sqlite3_finalize(stmt);
  // check return code for status
   
  if (ret != SQLITE_OK){
    fprintf(stderr, "Insert Block: Finalize: SQL error: %s\n", ErrMsg);
    sqlite3_free(ErrMsg);
    return -1;
  }
  /*-----------Update local_size in Files------------*/

  /*-------------Update cache_used_size--------------*/
  cache_used_size += block_size;
  /*-------------Update cache_used_size--------------*/

  if (VERBOSE) {
    fprintf(stdout, "Block inserted\n");
  }
  return 0;
}

int delete_block(sqlite3* db, char * filename, int blk_offset){
  return 0;
}


int delete_file(sqlite3* db, char * filename){
  return 0;
}


int update_lru_blk(sqlite3* db, char* filename, int blk_offset){
  return 0;
}

int update_blk_time(sqlite3* db, char * filename, int blk_offset){
  return 0;
}

