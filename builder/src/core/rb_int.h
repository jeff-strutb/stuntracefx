/* rb_int.h: what the core's files share. */
#ifndef RB_INT_H
#define RB_INT_H

#include <stdio.h>
#include "rb.h"

#ifdef _WIN32
#define RB_SEP '\\'
#else
#define RB_SEP '/'
#endif

/* ---- files (util.c): UTF-8 paths on every system ---------------------------------- */
FILE    *rb_fopen(const char *path, const char *mode);
int      rb_seek(FILE *f, uint64_t off);              /* 1 on success */
uint64_t rb_file_size(const char *path);              /* 0 when missing */
int      rb_is_dir(const char *path);
int      rb_exists(const char *path);
int      rb_mkdirs(const char *path);                 /* mkdir -p; 1 on success */
int      rb_rmtree(const char *path);                 /* rm -rf; 1 on success */
int      rb_rename(const char *from, const char *to);
int      rb_write_file(const char *path, const void *p, size_t n);
void    *rb_read_file(const char *path, size_t *n);   /* malloc'd; NULL when unreadable */
void     rb_join(char *out, size_t n, const char *a, const char *b);
void     rb_dirname(char *out, size_t n, const char *path);
int      rb_set_exec(const char *path);               /* chmod 755 (POSIX) */
int      rb_copy_file(const char *from, const char *to);
int      rb_copy_tree(const char *from, const char *to);   /* cp -R of a folder (POSIX) */

void rb_err(char *err, size_t errlen, const char *fmt, ...);

#endif
