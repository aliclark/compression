
#ifndef _COMPRESSION_H
#define _COMPRESSION_H

#include <stdlib.h> /* size_t */
#include <stdint.h> /* uint_least32_t */

/******************************************************/

#ifndef __cplusplus
#define false 0
#define true  1

typedef int bool;
#endif /* __cplusplus */

typedef size_t lzi;

/******************************************************/

typedef struct lzctable lzctable;

lzctable* ctable_create (void);
int       ctable_pointer_size (lzctable* tab);
lzi       ctable_count (lzctable* tab);
lzi       ctable_handle_bit (lzctable* tab, lzi base, int bit);
void      ctable_destroy (lzctable* tab);

/******************************************************/

typedef struct lzdtable lzdtable;

lzdtable* dtable_create (void);
void      dtable_set_child (lzdtable* tab, lzi base, int bit);
int       dtable_get (lzdtable* tab, lzi base);
lzi       dtable_get_parent (lzdtable* tab, lzi base);
void      dtable_destroy (lzdtable* tab);

/******************************************************/

typedef struct bitbuf {
    uint_least32_t buf;
    int    len;
} bitbuf;

bitbuf* bitbuf_initialise (bitbuf* s);
void    bitbuf_enqueue_bits (bitbuf* dest, size_t src, int nbits);
size_t  bitbuf_numbits (bitbuf* src);
size_t  bitbuf_dequeue_bits (bitbuf* src, int nbits);

/******************************************************/

typedef struct bitstack bitstack;

bitstack* bitstack_create (void);
void      bitstack_save_rem (bitstack* bs);
void      bitstack_push (bitstack* bs, int bit);
int       bitstack_pop (bitstack* bs);
void      bitstack_restore_rem (bitstack* bs);
int       bitstack_numbits (bitstack* bs);
void      bitstack_destroy (bitstack* bs);

/******************************************************/

#endif /* _COMPRESSION_H */
