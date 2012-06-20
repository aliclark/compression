
#include <stdlib.h>
#include <stdio.h>

#include "compression.h"

static lzi process_bit (lzctable* tab, bitbuf* sp, lzi fbase, int bit)
{
    lzi base = ctable_handle_bit(tab, fbase, bit);

    /*
     * If true, indicates a new prefix has been seen,
     * and we can print it out.
     */
    if (base == fbase) {
        bitbuf_enqueue_bits(sp, base, ctable_pointer_size(tab));

        bitbuf_enqueue_bits(sp, bit, 1);

        while (bitbuf_numbits(sp) >= 8) {
            putchar(bitbuf_dequeue_bits(sp, 8));
        }

        base = 0;
    }

    return base;
}

static void compress_stdin (void)
{
    int b, bitsin, shi;

    lzi base = 0;

    lzctable* tab = ctable_create();

    bitbuf s;
    bitbuf* sp = bitbuf_initialise(&s);

    int fracn = 0;

    while (true) {
        b = getchar();

        if (b == EOF) {

            /*
             * We have read the bits of a previously observed prefix,
             * enqueue the pointer representing that entire prefix
             */
            if (base != 0) {
                bitbuf_enqueue_bits(sp, base, ctable_pointer_size(tab));
            }

            while (bitbuf_numbits(sp) >= 8) {
                putchar(bitbuf_dequeue_bits(sp, 8));
            }

            /* Indicate how many bits of the final byte are used */
            fracn = bitbuf_numbits(sp);

            if (fracn > 0) {
                putchar(bitbuf_dequeue_bits(sp, fracn) << (8 - fracn));
                putchar(fracn);
            } else {
                /* all bits were used, or nothing was compressed */
                if (ctable_count(tab) != 1) {
                    putchar(8);
                }
            }

            ctable_destroy(tab);

            return;
        }

        bitsin = 128;
        shi = 7;

        while (bitsin != 0) {
            base = process_bit(tab, sp, base, (b & bitsin) >> shi);

            bitsin >>= 1;
            --shi;
        }
    }
}

int main (void)
{
    compress_stdin();

    return EXIT_SUCCESS;
}
