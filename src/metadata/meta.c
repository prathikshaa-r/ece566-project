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
  open_db("Metadata-File", &db);
  // check if tables exist
  // else create the tables
  int ret = create_tables(db);
  if(!ret){
    fprintf(stdout, "Tables probably exist already!\n");
  }
  

  printf("Hello World!\n");

  printf("Init LRU block\n");

  init_lru_blk();

  // function to create file entry
  // function to create datablock entry

  // function to update block timestamp

  // function to delete file (and relevant data-blocks) - using FK cascade

  // check file in cache

  // check block in cache

  // check blocks in cache

  
  UNUSED(res);
  UNUSED(sql);

  return EXIT_SUCCESS;
}

static int callback(void *NotUsed, int argc, char **argv, char **azColName) {
  int i;
  for (i = 0; i < argc; i++) {
    printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
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
  char * sql;
  char *ErrMsg = 0;

  /* Create sql statement for table creation */
  sql = "CREATE TABLE files ("
       "file_id    INTEGER PRIMARY KEY,"
       "relative_path  TEXT  NOT NULL,"
       "remote_size  INTEGER NOT NULL,"
       "local_size INTEGER NOT NULL"
");"
"CREATE TABLE datablocks("
       "block_id   INTEGER   PRIMARY KEY,"
       "blk_start_offset INTEGER   NOT NULL,"
       "valid    BOOLEAN   NOT NULL DEFAULT 0,"
       "timestamp  DATETIME  NOT NULL DEFAULT (DATETIME('now'))"
       // add foreign key: ON DELETE CASCADE
");";

  /* Execute SQL statement */
  int ret = sqlite3_exec(db, sql, callback, 0, &ErrMsg);
  // check return code for status
   
   if (ret != SQLITE_OK){
      fprintf(stderr, "SQL error: %s\n", ErrMsg);
      sqlite3_free(ErrMsg);
      return -1;
   } 
   if (VERBOSE) {
      fprintf(stdout, "Table created successfully\n");
   }
   return 0;

}

// insert file entry into database
// ips: filename, local_size (can this be default 0), remote_size
int create_file(sqlite3* db, char * filename, size_t remote_size, size_t local_size){
  // INSERT INTO FILE
  char * sql;
  char *ErrMsg = 0;

  /* Create sql statement for table creation */
  sql = "INSERT INTO files(relative_path, remote_size, local_size);"; // +filename

  /* Execute SQL statement */
  int ret = sqlite3_exec(db, sql, callback, 0, &ErrMsg);
  // check return code for status
   
   if (ret != SQLITE_OK){
      fprintf(stderr, "SQL error: %s\n", ErrMsg);
      sqlite3_free(ErrMsg);
      return -1;
   } 
   if (VERBOSE) {
      fprintf(stdout, "Table created successfully\n");
   }
   return 0;

}

int insert_block(sqlite3* db, char * filename, int blk_offset){
  // 
    // INSERT INTO FILE
  char *sql, *fsql;
  char *ErrMsg = 0;

  /* Create sql statement for table creation */
  fsql = "SELECT file_id FROM files WHERE relative_path="; // + filename

  sql = "INSERT INTO datablocks(relative_path, remote_size, local_size);"; // +filename

  /* Execute SQL statement */
  int ret = sqlite3_exec(db, sql, callback, 0, &ErrMsg);
  // check return code for status
   
   if (ret != SQLITE_OK){
      fprintf(stderr, "SQL error: %s\n", ErrMsg);
      sqlite3_free(ErrMsg);
      return -1;
   } 
   if (VERBOSE) {
      fprintf(stdout, "Table created successfully\n");
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
