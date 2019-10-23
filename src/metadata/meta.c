#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>

#define VERBOSE 1
#define UNUSED(x) (void)(x)

int main(void) {
  // printf("%s\n", sqlite3_libversion());

  sqlite3 *db;
  sqlite3_stmt *res;

  char *remainder;
  UNUSED(remainder);

  // use mem or file for metadata - configurable
  int rc = sqlite3_open(":memory:", &db);

  if (rc != SQLITE_OK) {
    fprintf(stderr, "Cannot open database: %s, \n", sqlite3_errmsg(db));
    sqlite3_close(db);

    return EXIT_FAILURE;
  }

  if (VERBOSE)
    printf("sqliteok = %d\n", SQLITE_OK);

  // precompile the sql statement
  // -1 param takes len of query| -1 => read till 0 or null terminator
  // 0 in end: param takes pointer to part of the query that is not compiled
  rc = sqlite3_prepare_v2(db, "SELECT SQLITE_VERSION()", -1, &res, 0);

  if (rc != SQLITE_OK) {
    fprintf(stderr, "Statement failed: %s\n", sqlite3_errmsg(db));
  }

  rc = sqlite3_step(res);

  if (rc == SQLITE_ROW) {
    printf("%s\n", sqlite3_column_text(res, 0));
  }

  sqlite3_finalize(res);
  sqlite3_close(db);

  return EXIT_SUCCESS;
}
