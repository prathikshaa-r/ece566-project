#include <sqlite3.h>

#define VERBOSE 1
#define UNUSED(x) (void)(x)

struct LRU_block{
  char * filename;
  int blk_offset;
} typedef LRU_block;


LRU_block* init_lru_blk(){
  LRU_block *lru_block = malloc(sizeof(LRU_block));
  lru_block->filename = NULL;
  lru_block->blk_offset=0;
  return lru_block;
}

// callback function used to execute sql statements
static int callback(void *NotUsed, int argc, char **argv, char **azColName);
// open database, populates the db pointer with the opened database
void open_db(char * db_name, sqlite3 ** db);
// create the FILES and DATABLOCKS database tables
int create_tables(sqlite3 * db);

// inserts a new file into the database
int create_file(sqlite3* db, char * filename, size_t remote_size, size_t local_size);
// remove deleted file
int delete_file(sqlite3* db, char * filename);

// inserts new block into database
int insert_block(sqlite3* db, char * filename, int blk_offset);
// delete evicted block from file
int delete_block(sqlite3* db, char * filename, int blk_offset);

// change what the LRU block points to
int update_lru_blk(sqlite3* db, char* filename, int blk_offset);
// call on every write to block
int update_blk_time(sqlite3* db, char * filename, int blk_offset);

int is_file_in_cache(sqlite3* db, char * filename);
int is_blk_in_cache(sqlite3* db, char * filename);
int are_blocks_in_cache(sqlite3* db, char * filename);

