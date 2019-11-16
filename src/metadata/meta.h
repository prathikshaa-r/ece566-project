#include <sqlite3.h>
#include <assert.h>
#include <string.h>
#include <errno.h>

#define VERBOSE 1
#define UNUSED(x) (void)(x)

struct LRU_block{
  char * filename;
  int blk_offset;
} typedef LRU_block;

static size_t meta_block_size = 0;
static size_t cache_used_size = 0;

void set_block_size(size_t blk_size);
void init_cache_used_size(sqlite3 *db);
void print_cache_used_size();
size_t get_cache_used_size();

size_t str_to_num(const char *str);

// LRU_block* init_lru_blk(){
//   LRU_block *lru_block = malloc(sizeof(LRU_block));
//   lru_block->filename = NULL;
//   lru_block->blk_offset = 0;
//   return lru_block;
// }

// callback function used to execute sql statements
static int callback(void *NotUsed, int argc, char **argv, char **azColName);
// open database, populates the db pointer with the opened database
void open_db(char * db_name, sqlite3 ** db);
// create the FILES and DATABLOCKS database tables
int create_tables(sqlite3 * db);

// inserts a new file into the database
int create_file(sqlite3* db, char * filename, size_t remote_size);
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
int is_blk_in_cache(sqlite3* db, char * filename, int blk_offset);

// blk array should be malloced appropriately and will be used to set boolean 
// values for each offset in the blk_arr
int are_blocks_in_cache(sqlite3* db, char * filename, int num_blks, 
	int *blk_arr, int *bool_arr);

