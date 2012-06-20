
#include <stdlib.h>
#include <stdio.h>

#include "compression.h"

static void process_bit (lzdtable* tab, bitbuf* sp, bitstack* bs, int bit, bool last)
{
    /* for tracking pointer size */
    static int curpsize = 0;
    static int curpmax  = 1;

    static int curpos = 0;

    /* for reading individual phrases */
    static bool inphr = false;
    static int phrrem = 0;
    static int curbuf = 0;

    lzi base;

    if (!inphr) {
        ++curpos;

        if (curpos > curpmax) {
            curpsize += 1;
            curpmax  *= 2;
        }

        inphr  = true;
        phrrem = curpsize + 1;
        curbuf = 0;
    }

    bitbuf_enqueue_bits(sp, bit, 1);
    --phrrem;

    if (!last && (phrrem != 0)) {
        return;
    }

    base = bitbuf_dequeue_bits(sp, curpsize);
    bitstack_save_rem(bs);

    /*
     * If phrrem != 0, this is the last bit,
     * and phrrem should be 1
     */
    if (phrrem == 0) {
        bit = bitbuf_dequeue_bits(sp, 1);
        dtable_set_child(tab, base, bit);
        bitstack_push(bs, bit);
    }

    while (base != 0) {
        bitstack_push(bs, dtable_get(tab, base));
        base = dtable_get_parent(tab, base);
    }

    bitstack_restore_rem(bs);

    while (bitstack_numbits(bs) >= 8) {
        putchar(bitstack_pop(bs));
    }

    inphr = false;
}

static void decompress_stdin (void)
{
    int bitsin, shi, c, b, a;

    lzdtable* tab = dtable_create();

    bitbuf s;
    bitbuf* sp = bitbuf_initialise(&s);

    bitstack* bs = bitstack_create();

    c = getchar();
    if (c == EOF) {
        dtable_destroy(tab);
        bitstack_destroy(bs);
        return;
    }
    b = getchar();
    if (b == EOF) {
        /* should not happen */
        dtable_destroy(tab);
        bitstack_destroy(bs);
        return;
    }

    while (true) {
        a = getchar();

        if (a == EOF) {
            /* process the top b bits of c */

            bitsin = 128;
            shi = 7;

            while (b > 1) {
                process_bit(tab, sp, bs, (c & bitsin) >> shi, false);

                bitsin >>= 1;
                --shi;
                --b;
            }

            /* this is the last bit */
            process_bit(tab, sp, bs, (c & bitsin) >> shi, true);

            /*
             * It should be impossible for process_bit
             * to have remainder here, if the input is valid
             */

            dtable_destroy(tab);
            bitstack_destroy(bs);
            return;
        }

        /* process all bits of c */

        bitsin = 128;
        shi = 7;

        while (bitsin != 0) {
            process_bit(tab, sp, bs, (c & bitsin) >> shi, false);

            bitsin >>= 1;
            --shi;
        }

        c = b;
        b = a;
    }
}

int main (void)
{
    decompress_stdin();

    return EXIT_SUCCESS;
}
