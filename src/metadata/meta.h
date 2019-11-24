#ifndef __META_H__
#define __META_H__

#include <sqlite3.h>
#include <assert.h>
#include <string.h>
#include <errno.h>

#define VERBOSE 1
#define UNUSED(x) (void)(x)

// struct LRU_block{
//   char * filename;
//   int blk_offset;
// } typedef LRU_block;

static size_t meta_block_size = 0; // num of bytes
static size_t cache_used_size = 0; // num of bytes

void set_block_size(size_t blk_size);
void init_cache_used_size(sqlite3 *db);
void print_cache_used_size();
size_t get_cache_used_size(); // tracked in num of bytes

size_t str_to_num(const char *str);

// LRU_block* init_lru_blk();

// callback function used to execute sql statements
static int callback(void *NotUsed, int argc, char **argv, char **azColName);
// FUSE: open database, populates the db pointer with the opened database
int open_db(char * db_name, sqlite3 ** db);
// FUSE: create the FILES and DATABLOCKS database tables
int create_tables(sqlite3 * db);

// FUSE: inserts a new file into the database
int create_file(sqlite3* db, char * filename, size_t remote_size);
// FUSE: remove deleted file
int delete_file(sqlite3* db, char * filename);

// inserts one block and several blocks into database
int insert_block(sqlite3* db, char * filename, size_t blk_offset);
int insert_blocks(sqlite3* db, char * filename, size_t num_blks, size_t *blk_arr);


// FUSE: write blocks from fuse side
int write_blks(sqlite3* db, char * filename, size_t num_blks, size_t *blk_arr);

// delete evicted block/blocks from file
/*
inputs: filename, blk_offset to delete
return value: check for -1 (indicating failure) or 0 (indicating success)
*/
int delete_block(sqlite3* db, char * filename, size_t blk_offset);
int delete_blocks(sqlite3* db, char * filename, size_t num_blks, size_t *blk_arr);

// change what the LRU block points to
int update_lru_blk(sqlite3* db, char * filename, size_t blk_offset);
// call on every write to block
int update_blk_time(sqlite3* db, char * filename, size_t blk_offset);


// FUSE: check for cache hits
int is_file_in_cache(sqlite3* db, char * filename /*,[datatype] mtime */);
int is_blk_in_cache(sqlite3* db, char * filename, size_t blk_offset);

// blk array should be malloced appropriately and will be used to set boolean 
// blk_arr = [] of blk_offsets
int are_blocks_in_cache(sqlite3* db, char * filename, size_t num_blks, 
	size_t *blk_arr, int *bool_arr);

/*
inputs:
* db: database handle
* num_blks: num of blocks to evict
* file_ids: file_id corresponding to each file.
* 			use this as index to lookup filename from char filenames[][]
* filenames: filenames array corresponding to each evicted block
*	-POINTER ARRAY MUST BE MALLOCED BY USER
*	-POINTER ARRAY + ALL NON_NULL POINTERS IN ARRAY MUST BE FREED
* blk_offsets: array of each blk evicted from the corresponding file in filenames
*	-MUST BE MALLOCED BY USER

return value:
* The number of blocks actually evicted. Will be less than or equal to num_blks.
* Returns -1 in case of error and printts error to stderr.

Desc:
* Evict the LRU num_blks, # of blks.
*/
//ssize_t evict_blocks(sqlite3 *db, size_t num_blks, char **filenames, size_t *blk_offsets);
ssize_t evict_blocks(sqlite3 *db, size_t num_blks, int *file_ids, char **filenames, size_t *blk_offsets);

int get_filename_from_fileid(sqlite3 *db, int file_id, char **filename);
/*
Alternately to evict LRU file
*/
int evict_file(sqlite3 *db, char **filename);

#endif // __META_H_ 