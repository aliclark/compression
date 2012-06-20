
#include <stdlib.h> /* size_t */
#include <string.h> /* memcpy */
#include <assert.h>

#include "compression.h"

typedef unsigned char byte;

#define LZTABLE_INITIAL (2048)

/******************************************************/

static size_t select_mask (int nbits) {
    return (1 << nbits) - 1;
}

/******************************************************/

/* Can be used to build a tree of all bit strings we have seen */
typedef struct lzc {
    byte bit;
    /* The pointers for child bits 0 and 1 at those offsets */
    lzi  ch[2];
} lzc;

/* Holds the bit string tree entries, added in any order */
struct lzctable {
    lzi count; /* how many entries */
    int pbits; /* min. pointer size needed to address any current entry */
    lzi pmax;  /* follows from the pointer size, the max addressable entry, incl. */
    lzi max;   /* the max number of elements arr can hold */
    lzc* arr;
};

static void ctable_grow (lzctable* tab)
{
    lzc* arr = (lzc*) malloc(sizeof(lzc) * (tab->max * 2));
    memcpy(arr, tab->arr, sizeof(lzc) * tab->max);
    free(tab->arr);
    tab->arr = arr;
    tab->max *= 2;
}

/* zero is the null child */
static lzi ctable_get_child (lzctable* tab, lzi base, int bit)
{
    return tab->arr[base].ch[bit];
}

static bool ctable_has_child (lzctable* tab, lzi base, int bit)
{
    return ctable_get_child(tab, base, bit) != 0;
}

static lzi ctable_set_child (lzctable* tab, lzi base, int bit)
{
    lzi i = tab->count;

    if (i >= tab->max) {
        ctable_grow(tab);
    }

    if (tab->count > tab->pmax) {
        ++tab->pbits;
        tab->pmax *= 2;
    }

    ++tab->count;

    tab->arr[i].bit   = bit;
    tab->arr[i].ch[0] = 0;
    tab->arr[i].ch[1] = 0;

    /* point to the new bit entry from the prefix base */
    tab->arr[base].ch[bit] = i;

    return i;
}

lzctable* ctable_create (void)
{
    lzctable* tab = (lzctable*) malloc(sizeof(lzctable));

    tab->count = 0;
    tab->pbits = 0;
    tab->pmax  = 1;

    tab->max = LZTABLE_INITIAL;
    tab->arr = (lzc*) malloc(sizeof(lzc) * LZTABLE_INITIAL);

    /* The 0 index by convention represents the NULL bit string */
    tab->arr[0].bit   = 0;
    tab->arr[0].ch[0] = 0;
    tab->arr[0].ch[1] = 0;
    ++tab->count;

    return tab;
}

lzi ctable_count (lzctable* tab)
{
    return tab->count;
}

/* Only valid until the next ctable_handle_bit() call */
int ctable_pointer_size (lzctable* tab)
{
    return tab->pbits;
}

/*
 * base is the pointer of a prefix string
 * bit  is an additional bit of the string
 *
 * If the prefix and bit have previously been observed,
 * return the base pointer that represents the entire string.
 *
 * If not, add the bit as a child of this base prefix,
 * return the base argument unchanged to indicate this.
 */
lzi ctable_handle_bit (lzctable* tab, lzi base, int bit)
{
    if (ctable_has_child(tab, base, bit)) {
        return ctable_get_child(tab, base, bit);
    }

    ctable_set_child(tab, base, bit);

    return base;
}

void ctable_destroy (lzctable* tab)
{
    free(tab->arr);
    tab->arr = NULL;
    free(tab);
}

/******************************************************/

/* Used to build bit strings from the leaves to the root */
typedef struct lzd {
    lzi  parent;
    byte bit;
} lzd;

/* same convention as lzctable */
struct lzdtable {
    lzi count;
    lzi max;
    lzd* arr;
};

static void dtable_grow (lzdtable* tab)
{
    lzd* arr = (lzd*) malloc(sizeof(lzd) * (tab->max * 2));
    memcpy(arr, tab->arr, sizeof(lzd) * tab->max);
    free(tab->arr);
    tab->arr = arr;
    tab->max *= 2;
}

lzdtable* dtable_create (void)
{
    lzdtable* tab = (lzdtable*) malloc(sizeof(lzdtable));

    tab->count = 1;
    tab->max   = LZTABLE_INITIAL;
    tab->arr   = (lzd*) malloc(sizeof(lzd) * LZTABLE_INITIAL);

    /* The NULL bit string, by convention */
    tab->arr[0].parent = 0;
    tab->arr[0].bit    = 0;

    return tab;
}

void dtable_set_child (lzdtable* tab, lzi base, int bit)
{
    lzi i = tab->count;

    if (i >= tab->max) {
        dtable_grow(tab);
    }

    ++tab->count;

    tab->arr[i].parent = base;
    tab->arr[i].bit    = bit;
}

int dtable_get (lzdtable* tab, lzi base)
{
    return tab->arr[base].bit;
}

lzi dtable_get_parent (lzdtable* tab, lzi base)
{
    return tab->arr[base].parent;
}

void dtable_destroy (lzdtable* tab)
{
    free(tab->arr);
    tab->arr = NULL;
    free(tab);
}

/******************************************************/

/*
 * Bitbuf is used to enqueue a small number of bits,
 * currently this is a maximum of 32 bits at a time.
 *
 * Bits are enqueud and dequeued using size_t
 *
 * The ordering of bits of the 'src' is retained when enqueueing,
 * with the MSB considered the first bit enqueued, LSB the last.
 *
 * This means if two enqueueings overlap in a dequeud byte,
 * the LSB of the first enqueuing is one bit left shifted from
 * the MSB of the second.
 */

bitbuf* bitbuf_initialise (bitbuf* s)
{
    s->len = 0;
    s->buf = 0;
    return s;
}

void bitbuf_enqueue_bits (bitbuf* dest, size_t src, int nbits)
{
    if (nbits <= 0) {
        return;
    }

    dest->len +=  nbits;
    dest->buf <<= nbits;
    dest->buf +=  src & ((1 << nbits) - 1);
}

size_t bitbuf_numbits (bitbuf* src)
{
    return src->len;
}

size_t bitbuf_dequeue_bits (bitbuf* src, int nbits)
{
    size_t rv = 0;
    int len   = src->len;
    int diff  = len - nbits;

    if (nbits <= 0) {
        return 0;
    }

    if (diff < 0) {
        nbits = len;
        diff  = 0;
    }

    rv = (src->buf & (select_mask(nbits) << diff)) >> diff;

    src->len -= nbits;
    src->buf &= select_mask(src->len);

    return rv;
}

/******************************************************/

struct bitstack {
    /* each bit of the byte is filled from lsb to msb */
    byte* arr;
    int lenrem;  /* number of remainder bits in the top byte */
    int lenfull; /* number of full bytes */
    int size;    /* number of bytes available at arr */
    int save;    /* a handy place to save a remainder byte */
    int numsave; /* number of remainder bits saved */
};

static void bitstack_grow (bitstack* bs)
{
    byte* arr = (byte*) calloc(bs->size * 2, sizeof(byte));
    memcpy(arr, bs->arr, sizeof(byte) * bs->size);
    free(bs->arr);
    bs->arr = arr;
    bs->size *= 2;
}

bitstack* bitstack_create (void)
{
    bitstack* bs = (bitstack*) malloc(sizeof(bitstack));

    bs->arr     = (byte*) calloc(1, 1);
    bs->size    = 1;
    bs->lenrem  = 0;
    bs->lenfull = 0;
    bs->save    = 0;
    bs->numsave = 0;

    return bs;
}

/*
 * Once all usable bytes have been dequeued, it is often useful
 * to save the remainder bits, push new bits, then push the saved bits again.
 * This should only be called when there are no full bytes left.
 */
void bitstack_save_rem (bitstack* bs)
{
    assert(bs->lenfull == 0);

    bs->save    = bs->arr[0];
    bs->numsave = bs->lenrem;

    bs->lenrem = 0;
    bs->arr[0] = 0;
}

/* Push a single bit */
void bitstack_push (bitstack* bs, int bit)
{
    if ((bs->lenfull + 1) >= bs->size) {
        bitstack_grow(bs);
    }

    bs->arr[bs->lenfull] |= bit << bs->lenrem;

    ++bs->lenrem;

    if (bs->lenrem == 8) {
        bs->lenrem = 0;
        ++bs->lenfull;

        bs->arr[bs->lenfull] = 0;
    }
}

/* Pop a byte */
int bitstack_pop (bitstack* bs)
{
    int val;

    assert(bs->lenfull > 0);

    val = ((bs->arr[bs->lenfull] << (8 - bs->lenrem)) +
           (bs->arr[bs->lenfull - 1] >> bs->lenrem));

    bs->lenfull -= 1;
    bs->arr[bs->lenfull] &= (1 << bs->lenrem) - 1;

    return val;
}

/* Loop through the saved bits, pushing them on again */
void bitstack_restore_rem (bitstack* bs)
{
    int ns = bs->numsave;
    int s  = bs->save;
    int m;
    int n = 0;

    if (ns == 0) {
        return;
    }

    m = 1;

    while (n < ns) {
        bitstack_push(bs, (s & m) >> n);
        m <<= 1;
        ++n;
    }
}

int bitstack_numbits (bitstack* bs)
{
    return (bs->lenfull * 8) + bs->lenrem;
}

void bitstack_destroy (bitstack* bs)
{
    free(bs->arr);
    bs->arr = NULL;
    free(bs);
}

/******************************************************/
