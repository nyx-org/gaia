/*
 * Copyright (c) 2022, lg.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <gaia/base.h>
#include <gaia/charon.h>
#include <limine.h>
#include <stdc-shim/string.h>
#include <stddef.h>
#include <stdint.h>

// The Limine requests can be placed anywhere, but it is important that
// the compiler does not optimise them away, so, usually, they should
// be made volatile or equivalent.

static void done(void)
{
    for (;;)
    {
        __asm__("hlt");
    }
}

Charon limine_to_charon();

void gaia_main(Charon *charon);

void _start(void)
{
    Charon charon = limine_to_charon();
    gaia_main(&charon);
    done();
}
