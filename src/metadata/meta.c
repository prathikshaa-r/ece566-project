#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "meta.h"


int main(void) {
  // printf("%s\n", sqlite3_libversion());

  sqlite3 *db;

  // open database - either in memory or from file
  open_db("Metadata-File.db", &db);
  // check if tables exist
  // else create the tables
  int ret = create_tables(db);
  if(ret == -1){
    fprintf(stdout, "Tables probably exist already!\n");
  }
  
  // printf("Initializing LRU block...\n");
  // init_lru_blk();

  set_block_size(1024);

  // function to init cache_used_size 
  printf("\n");
  init_cache_used_size(db);

  print_cache_used_size();

  // insert new file into metadata
  create_file(db, "newdir/hello/Hello.txt", 1234);
  create_file(db, "newdir/hello/Test2.txt", 1234);
  // function to create datablock entry
  printf("Calling insert block...\n");
  insert_block(db, "newdir/hello/Hello.txt", 1234);
  insert_block(db, "newdir/hello/Hello.txt", 1578);

  insert_block(db, "newdir/hello/Test2.txt", 1234);
  insert_block(db, "newdir/hello/Test2.txt", 1578);

  // insert blocks
  size_t *blk_arr = (size_t*)malloc(sizeof(*blk_arr)*4);
  memcpy(blk_arr, (size_t[4]) {1024, 1948, 0000, 2134}, sizeof(blk_arr[0])*4);
  insert_blocks(db, "newdir/hello/Hello.txt", 4, blk_arr);
  free(blk_arr);

  // function to delete file (and relevant data-blocks) - using FK cascade
  delete_file(db, "newdir/hello/Test2.txt");
  delete_block(db, "newdir/hello/Hello.txt", 2134);

  print_cache_used_size();

  // function to update remote file size- call on close() &|or open()

  printf("Finished insertions and deletions. "
         "You should now check the db before I call write_blks...\n");
  sleep(5);
  // write blocks - insert or update
  // function to update block timestamp
  blk_arr = (size_t*)malloc(sizeof(*blk_arr)*4);
  memcpy(blk_arr, (size_t[4]) {1024, 1948, 0000, 2134}, sizeof(blk_arr[0])*4);
  write_blks(db, "newdir/hello/Hello.txt", 4, blk_arr);
  free(blk_arr);  

  // check file in cache
  printf("%d\n", is_file_in_cache(db, "newdir/hello/Hello.txt"));
  printf("%d\n", is_file_in_cache(db, "newdir/hello/Test2.txt"));

  // check block in cache
  printf("%d\n", is_blk_in_cache(db, "newdir/hello/Hello.txt", 1234));
  printf("%d\n", is_blk_in_cache(db, "newdir/hello/Hello.txt", 1024));
  printf("%d\n", is_blk_in_cache(db, "newdir/hello/Test2.txt", 1234));

  // check blocks in cache
  blk_arr = (size_t*)malloc(sizeof(*blk_arr)*4);
  int *bool_arr = (int*)malloc(sizeof(*bool_arr)*4);
  memcpy(blk_arr, (size_t[4]) {1024, 1948, 0000, 2134}, sizeof(blk_arr[0])*4);
  are_blocks_in_cache(db, "newdir/hello/Hello.txt", 4, blk_arr, bool_arr);
  for (int i = 0; i < 4; ++i){
    printf("%lu:%d\n", blk_arr[i], bool_arr[i]);
  }
  free(blk_arr);
  free(bool_arr);

  evict a certain nunber of blocks
  char **filenames;
  size_t *blocks;
  int *file_ids;
  size_t num_blks = 4;

  filenames = (char **)malloc(sizeof(*filenames)*num_blks);
  memset(filenames, 0, sizeof(*filenames)*num_blks);

  printf("Before block eviction\n");
  for (int i = 0; i < num_blks; ++i){
    if(filenames[i]){
      printf("Filename[%d]: %s\n", i, filenames[i]);
    }
  }

  blocks = (size_t *)malloc(sizeof(*blocks)*num_blks);
  memset(blocks, 0, sizeof(*blocks)*num_blks);

  file_ids = (int*)malloc(sizeof(*file_ids)*num_blks);
  memset(file_ids, 0, sizeof(*file_ids)*num_blks);

  evict_blocks(db, num_blks, file_ids, filenames, blocks);
  printf("Evicted Blocks...\n");

  for (int i = 0; i < num_blks; ++i){
    if(filenames[i]){
      printf("Filename[%d]: %s\n", i, filenames[i]);
      free(filenames[i]);
    }
  }
  printf("Freed each filename.\n");
  free(filenames);
  printf("Freed filenames.\n");
  free(blocks);  
  printf("Freed blocks.\n");
  free(file_ids);
  printf("Freed file_ids.\n");

  // close database
  sqlite3_close(db);

  return EXIT_SUCCESS;
}

// LRU_block* init_lru_blk(){
//   LRU_block *lru_block = malloc(sizeof(LRU_block));
//   lru_block->filename = NULL;
//   lru_block->blk_offset = 0;
//   return lru_block;
// }

void print_cache_used_size(){
  printf("Cache Usage: %lu\n", cache_used_size);
}
size_t get_cache_used_size(){
  return cache_used_size;
}

void set_block_size(size_t blk_size){
  meta_block_size = blk_size;
  if (VERBOSE)
  {
    printf("Block Size Set to %lu\n", meta_block_size);
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
int open_db(char * db_name, sqlite3 ** db){
  int ret = sqlite3_open(db_name, db);
  char *ErrMsg = 0;

  if (ret != SQLITE_OK) {
    fprintf(stderr, "Cannot open database: %s, \n", sqlite3_errmsg(*db));
    sqlite3_close(*db);

    exit(EXIT_FAILURE);
  }

  if (VERBOSE)
  {
    printf("Turning on forign keys pragma...\n");
  }
  ret = sqlite3_exec(*db, "PRAGMA foreign_keys=ON", NULL, 0, &ErrMsg);
  if (ret != SQLITE_OK){
      fprintf(stderr, "Open DB: Forign Key Pragma: SQL error: %s\n", ErrMsg);
      sqlite3_free(ErrMsg);
      return -1;
   } 

  if (VERBOSE) {
    printf("Opened database successfully!\n");
  }
  return 0;
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
    fprintf(stderr, "Create File: Finalize: SQL error: %s\n", sqlite3_errmsg(db));
    return -1;
  }
  /*-----------Insert into Files------------*/

  if (VERBOSE) {
    fprintf(stdout, "File inserted\n");
  }
  return 0;

}

int insert_block(sqlite3* db, char * filename, size_t blk_offset){
  // INSERT INTO DATABLOCKS
  // Then update the local_size of the file
  // and update cache_size_used variable
  char *sql;
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
    fprintf(stderr, "Insert Block: Finalize: SQL error: %s\n", 
      sqlite3_errmsg(db));
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
  sqlite3_bind_int64(stmt, 1, meta_block_size);
  sqlite3_bind_text(stmt, 2, filename, -1, SQLITE_STATIC);
  // STEP
  ret = sqlite3_step(stmt); 
  if (ret != SQLITE_DONE) {
    printf("Insert Block: SQL Error: %s\n", sqlite3_errmsg(db));
    return -1;
  }
  ret = sqlite3_finalize(stmt);
  // check return code for status
   
  if (ret != SQLITE_OK){
    fprintf(stderr, "Insert Block: Finalize: SQL error: %s\n", sqlite3_errmsg(db));
    return -1;
  }
  /*-----------Update local_size in Files------------*/

  /*-------------Update cache_used_size--------------*/
  cache_used_size += meta_block_size;
  /*-------------Update cache_used_size--------------*/

  if (VERBOSE) {
    fprintf(stdout, "Block inserted\n");
  }
  return 0;
}


int insert_blocks(sqlite3* db, char * filename, size_t num_blks, size_t *blk_arr){
  if (VERBOSE){
    printf("Num of blocks to insert: %lu\n", num_blks);
  }

  for (size_t i = 0; i < num_blks; ++i){
    insert_block(db, filename, blk_arr[i]);
  }
  return 0;
}

int delete_file(sqlite3* db, char * filename){
  // DELETE FILE
  char *sql;
  sqlite3_stmt *stmt;

  /*-----------Delete from Files------------*/
  sql = "SELECT local_size FROM Files WHERE relative_path=?1;"
        "DELETE FROM Files WHERE relative_path = ?1;";
  /* 
  Prepare
  Bind
  Step
  Finalize
  */
  // Prepare stmt
  sqlite3_prepare_v2(db, sql, -1, &stmt, (const char**)&sql); // Do the select
  // Bind
  sqlite3_bind_text(stmt, 1, filename, -1, SQLITE_STATIC);
  // STEP
  int ret = sqlite3_step(stmt); 
  cache_used_size -= sqlite3_column_int64(stmt, 0);
  if (VERBOSE){
    print_cache_used_size();
  }
  ret = sqlite3_step(stmt);
  if (ret != SQLITE_DONE) {
    printf("Delete File: SQL Error: %s\n", sqlite3_errmsg(db));
    return -1;
  }
  ret = sqlite3_finalize(stmt);
  // check return code for status
   
  if (ret != SQLITE_OK){
    fprintf(stderr, "Delete File: Finalize: SQL error: %s\n", sqlite3_errmsg(db));
    return -1;
  }

  sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
  sqlite3_bind_text(stmt, 1, filename, -1, SQLITE_STATIC);
  ret = sqlite3_step(stmt);
  if (ret != SQLITE_DONE) {
    printf("Delete File: SQL Error: %s\n", sqlite3_errmsg(db));
    return -1;
  }

  ret = sqlite3_finalize(stmt);
  // check return code for status
   
  if (ret != SQLITE_OK){
    fprintf(stderr, "Delete File: Finalize: SQL error: %s\n", sqlite3_errmsg(db));
    return -1;
  }
  /*-----------Delete from Files------------*/

  if (VERBOSE) {
    fprintf(stdout, "File %s deleted.\n", filename);
  }
  return 0;
}

int delete_block(sqlite3* db, char * filename, size_t blk_offset){
  // DELETE FILE
  char *sql;
  sqlite3_stmt *stmt;

  /*-----------Delete from Datablocks------------*/
  sql = "DELETE FROM Datablocks WHERE blk_start_offset = ?1 and "
        "file_id=(SELECT file_id from Files WHERE relative_path=?2);";
  /* 
  Prepare
  Bind
  Step
  Finalize
  */
  // Prepare stmt
  sqlite3_prepare_v2(db, sql, -1, &stmt, NULL); // Do the select
  // Bind
  sqlite3_bind_int64(stmt, 1, blk_offset);
  sqlite3_bind_text(stmt, 2, filename, -1, SQLITE_STATIC);
  // STEP
  int ret = sqlite3_step(stmt);

  if (ret != SQLITE_DONE) {
    printf("Delete Block: SQL Error: %s\n", sqlite3_errmsg(db));
    return -1;
  }

  ret = sqlite3_finalize(stmt);
   
  if (ret != SQLITE_OK){
    fprintf(stderr, "Delete Block: Finalize: SQL error: %s\n", sqlite3_errmsg(db));
    return -1;
  }
  /*-----------Delete from Datablocks------------*/
  sql = "UPDATE Files SET local_size = local_size - ?1 WHERE relative_path = ?2;";
   // Prepare stmt
  sqlite3_prepare_v2(db, sql, -1, &stmt, NULL); 
  // Bind
  sqlite3_bind_int64(stmt, 1, meta_block_size);
  sqlite3_bind_text(stmt, 2, filename, -1, SQLITE_STATIC);
  // STEP
  ret = sqlite3_step(stmt); 
  if (ret != SQLITE_DONE) {
    printf("Delete Block: SQL Error: %s\n", sqlite3_errmsg(db));
    return -1;
  }
  ret = sqlite3_finalize(stmt);
  // check return code for status
   
  if (ret != SQLITE_OK){
    fprintf(stderr, "Delete Block: Finalize: SQL error: %s\n", sqlite3_errmsg(db));
    return -1;
  }
  /*---------Reduce local_size in Files----------*/

  /*---------Reduce local_size in Files----------*/
  // If block deletion successful, reduce used space count
  cache_used_size -= meta_block_size;
  if (VERBOSE){
    print_cache_used_size();
  }
  if (VERBOSE) {
    fprintf(stdout, "Block %s: %lu deleted.\n", filename, blk_offset);
  }
  return 0;
}

int delete_blocks(sqlite3* db, char * filename, size_t num_blks, size_t *blk_arr){
  if (VERBOSE){
    printf("Num of blocks to delete: %lu\n", num_blks);
  }
  for (size_t i = 0; i < num_blks; ++i){
    if (VERBOSE){
      printf("Deleting block %s:%lu\n", filename, blk_arr[i]);
    }
    delete_block(db, filename, blk_arr[i]);
  }
  return 0;
}

int is_file_in_cache(sqlite3* db, char * filename /*,[datatype] mtime */){
  char *sql;
  sqlite3_stmt *stmt;
  int ret;

  /*-----------Check if file is in cache------------*/
  sql = "SELECT file_id FROM Files WHERE relative_path=?1;"; // select anything and check if any row is returned
  /* 
  Prepare
  Bind
  Step
  Finalize
  */
  // Prepare stmt
  sqlite3_prepare_v2(db, sql, -1, &stmt, NULL); // Do the select
  // Bind
  sqlite3_bind_text(stmt, 1, filename, -1, SQLITE_STATIC);
  // Check if rows are returned on execution
  if(SQLITE_ROW != (ret = sqlite3_step(stmt))){
    if(SQLITE_DONE == ret){
      // No rows returned => File not found
      return 0;
    }
    else {
      printf("Check File: SQL Error: %s\n", sqlite3_errmsg(db));
      return -1; // SQL Error
    }
  }
  // step to next row?
  ret = sqlite3_step(stmt);
  if (ret != SQLITE_DONE) {
    printf("Check File: SQL Error: %s\n", sqlite3_errmsg(db));
    return -1;
  }
  // destructor of the sql stmt
  ret = sqlite3_finalize(stmt);
  // check return code for status
   
  if (ret != SQLITE_OK){
    fprintf(stderr, "Check File: Finalize: SQL error: %s\n", sqlite3_errmsg(db));
    return -1;
  }
  /*-----------Check if file is in cache------------*/

  if (VERBOSE) {
    fprintf(stdout, "File %s found.\n", filename);
  }
  // file found
  return 1;
}

int is_blk_in_cache(sqlite3* db, char * filename, size_t blk_offset){
  char *sql;
  sqlite3_stmt *stmt;
  int ret;

  /*-----------Check if block is in cache------------*/
  sql = "SELECT block_id FROM Datablocks "
        "WHERE blk_start_offset=?1 AND "
        "file_id=(SELECT file_id from files WHERE relative_path=?2);"; // select anything and check if any row is returned
  /* 
  Prepare
  Bind
  Step
  Finalize
  */
  // Prepare stmt
  sqlite3_prepare_v2(db, sql, -1, &stmt, NULL); // Do the select
  // Bind
  sqlite3_bind_int64(stmt, 1, blk_offset);
  sqlite3_bind_text(stmt, 2, filename, -1, SQLITE_STATIC);
  // Check if rows are returned on execution
  if(SQLITE_ROW != (ret = sqlite3_step(stmt))){
    if(SQLITE_DONE == ret){
      // No rows returned => File not found
      return 0;
    }
    else {
      printf("Check block: SQL Error: %s\n", sqlite3_errmsg(db));
      return -1; // SQL Error
    }
  }
  // step to next row?
  ret = sqlite3_step(stmt);
  if (ret != SQLITE_DONE) {
    printf("Check block: SQL Error: %s\n", sqlite3_errmsg(db));
    return -1;
  }
  // destructor of the sql stmt
  ret = sqlite3_finalize(stmt);
  // check return code for status
   
  if (ret != SQLITE_OK){
    fprintf(stderr, "Check block: Finalize: SQL error: %s\n", sqlite3_errmsg(db));
    return -1;
  }
  /*-----------Check if block is in cache------------*/

  if (VERBOSE) {
    fprintf(stdout, "Block %s:%lu found.\n", filename, blk_offset);
  }
  // file found
  return 1;
}

int are_blocks_in_cache(sqlite3* db, char * filename, size_t num_blks, 
  size_t *blk_arr, int *bool_arr){
  int total_hit = 1;
  if (VERBOSE) printf("Num of blocks to check: %lu\n", num_blks);
  for (size_t i = 0; i < num_blks; ++i){
    if (VERBOSE){
      printf("Checking block %s:%lu\n", filename, blk_arr[i]);
    }
    bool_arr[i] = is_blk_in_cache(db, filename, blk_arr[i]);
    if(bool_arr[i] == 0) total_hit = 0;
  }
  return total_hit;
}


// are_blks_in_cache()
// YES->update_blks()
// NO ->create_blks()
// USE UPSERT INSTEAD
int write_blks(sqlite3* db, char * filename, size_t num_blks, size_t *blk_arr){
  printf("In write_blks\n");
  int *bool_arr = (int*)malloc(sizeof(*bool_arr)*num_blks);
  memset(bool_arr, 0, sizeof(*bool_arr)*num_blks);
  are_blocks_in_cache(db, filename, num_blks, blk_arr, bool_arr);
  for (int i = 0; i < num_blks; ++i){
    if(bool_arr[i]){
      printf("Write block: Update Block\n");
      update_blk_time(db, filename, blk_arr[i]);
    }else{
      printf("Write block: Insert Block\n");
      insert_block(db, filename, blk_arr[i]);
    }
  }
  free(bool_arr);
  return 0;
}

int update_lru_blk(sqlite3* db, char * filename, size_t blk_offset){
  return 0;
}

int update_blk_time(sqlite3* db, char * filename, size_t blk_offset){
  char *sql;
  sqlite3_stmt *stmt;

  /*-----------Update Block in Datablocks------------*/
  sql = "UPDATE Datablocks SET timestamp=(DATETIME('now')) "
        "WHERE blk_start_offset=?1 AND "
        "file_id=(SELECT file_id from files WHERE relative_path=?2);";
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
    printf("Update Block: SQL Error: %s\n", sqlite3_errmsg(db));
    return -1;
  }
  ret = sqlite3_finalize(stmt);
  // check return code for status
  if (ret != SQLITE_OK){
    fprintf(stderr, "Update Block: Finalize: SQL error: %s\n", 
      sqlite3_errmsg(db));
    return -1;
  }
  /*-----------Update block in Datablocks------------*/

  if (VERBOSE) {
    fprintf(stdout, "Block updated\n");
  }

  return 0;
}

// Get blocks with oldest timestamps
// Get filename for each block
// Delete blocks
ssize_t evict_blocks(sqlite3 *db, size_t num_blks, int *file_ids, char **filenames, 
  size_t *blk_offsets){
  char *sql;
  sqlite3_stmt *stmt;

  /*-----------Get oldest num_blks blocks------------*/
  sql="SELECT blk_start_offset, file_id FROM Datablocks "
      "ORDER BY timestamp ASC LIMIT ?1;";
  /* 
  Prepare
  Bind
  Step
  Finalize
  */
  // Prepare stmt
  sqlite3_prepare_v2(db, sql, -1, &stmt, NULL); 
  // Bind
  sqlite3_bind_int64(stmt, 1, num_blks);
  // STEP
  int ret = 0; //sqlite3_step(stmt); 
  int row = 0; // row number
  if(VERBOSE) printf("Evicting blocks:\n");
  while(SQLITE_ROW == (ret = sqlite3_step(stmt))) {
    int col;
    for(col=0; col < sqlite3_column_count(stmt); col++) {
      if(col == 0) blk_offsets[row] = sqlite3_column_int64(stmt, col);
      if(col == 1) file_ids[row] = sqlite3_column_int(stmt, col);
    }
    if(filenames[file_ids[row]]){
      printf("Filename queried: %s\n", filenames[file_ids[row]]);
    }
    else{
      get_filename_from_fileid(db, file_ids[row], (char **)&filenames[row]);
    }
    printf("\tBlock Offset: %lu\tFile ID: %d\n", blk_offsets[row], file_ids[row]);
    row++; // track row number
  }

  if (ret != SQLITE_DONE) {
    printf("Evict Block: Get oldest blocks: SQL Error: %s\n", sqlite3_errmsg(db));
    return -1;
  }
  ret = sqlite3_finalize(stmt);
  // check return code for status
  if (ret != SQLITE_OK){
    fprintf(stderr, "Evict Block: Get oldest blocks: Finalize: SQL error: %s\n", 
      sqlite3_errmsg(db));
    return -1;
  }
  /*-----------Get oldest num_blks blocks------------*/
  return 0; // should return number of successfully evicted blocks
}

int get_filename_from_fileid(sqlite3 *db, int file_id, char **filename){
  char *sql;
  sqlite3_stmt *stmt;

  printf("Get filename for file_id:%d\n", file_id);
  /*-----------Get filename using the file_id------------*/
  sql = "SELECT relative_path FROM Files WHERE file_id=?1;";
  /* 
  Prepare
  Bind
  Step
  Finalize
  */
  // Prepare stmt
  sqlite3_prepare_v2(db, sql, -1, &stmt, NULL); 
  // Bind
  sqlite3_bind_int(stmt, 1, file_id);
  // STEP
  int ret = sqlite3_step(stmt); 
  if(ret == SQLITE_ROW){
    const char * filename_on_stack = (char *)sqlite3_column_text(stmt, 0);
    printf("Filename[%d]:%s\n", file_id, sqlite3_column_text(stmt, 0));
    *filename = (char *)malloc(sizeof(filename_on_stack));
    strcpy(*filename, filename_on_stack);
    printf("Filename variable after assigning sql column text: %s\n", *filename);
  }
  ret = sqlite3_step(stmt);

  if (ret != SQLITE_DONE) {
    printf("Evict Block: Get Filename: SQL Error: %s\n", sqlite3_errmsg(db));
    return -1;
  }
  ret = sqlite3_finalize(stmt);
  // check return code for status
  if (ret != SQLITE_OK){
    fprintf(stderr, "Evict Block: Get Filename: Finalize: SQL error: %s\n", 
      sqlite3_errmsg(db));
    return -1;
  }
  /*-----------Get filename using the file_id------------*/
  return 0;
}

// for(int col=0; col<sqlite3_column_count(stmt); col++) {
//     // Note that by using sqlite3_column_text, sqlite will coerce the value into a string
//     printf("\tColumn %s(%i): '%s'\n",
//       sqlite3_column_name(stmt, col), col,
//       sqlite3_column_text(stmt, col));
//   }

// while(SQLITE_ROW == (rc = sqlite3_step(stmt))) {
//     int col;
//     printf("Found row\n");
//     for(col=0; col<sqlite3_column_count(stmt); col++) {
//       // Note that by using sqlite3_column_text, sqlite will coerce the value into a string
//       printf("\tColumn %s(%i): '%s'\n",
//         sqlite3_column_name(stmt, col), col,
//         sqlite3_column_text(stmt, col));
//     }
//   }
