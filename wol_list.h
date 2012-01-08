/*
 * Copyright (c) 2011 Toni Spets <toni.spets@iki.fi>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
   These are from wolapi.h of the client project, they were handy and reusable.
*/

#define WOL_ALLOC(size)                                     \
    calloc(1, size)

#define WOL_FREE(ptr)                                       \
    if (ptr)                                                \
        free(ptr)

/* linked list handling macros for the structures above */

#define WOL_LIST_NEW(type)                                  \
    WOL_ALLOC(sizeof(type))

#define WOL_LIST_FREE(el)                                   \
    while (el)                                              \
    {                                                       \
        void *_eltmp = (el);                                \
        (el) = (el)->next;                                  \
        free(_eltmp);                                       \
    }                                                       \
    (el) = NULL

#define WOL_LIST_INSERT(list, el)                           \
    if ((list) == NULL)                                     \
    {                                                       \
        (list) = (el);                                      \
    }                                                       \
    else                                                    \
    {                                                       \
        void *_eltmp = (el);                                \
        WOL_LIST_FOREACH(list, el)                          \
        {                                                   \
            if ((el)->next == NULL)                         \
            {                                               \
                (el)->next = _eltmp;                        \
                (el) = _eltmp;                              \
                break;                                      \
            }                                               \
        }                                                   \
    }

#define WOL_LIST_FOREACH(list, el)                          \
    for ((el) = (list); (el) != NULL; (el) = (el)->next)

#define WOL_LIST_REMOVE(list, el)                           \
    if ((el) && (list) == (el))                             \
    {                                                       \
        (list) = (el)->next;                                \
    }                                                       \
    else if ((list) && (el))                                \
    {                                                       \
        void *_eltmp = (el);                                \
        (el) = (list);                                      \
        do {                                                \
            if ((el)->next == _eltmp)                       \
            {                                               \
                (el)->next = (el)->next->next;              \
                (el) = _eltmp;                              \
                (el)->next = NULL;                          \
                break;                                      \
            }                                               \
            (el) = (el)->next;                              \
        } while(el);                                        \
        (el) = _eltmp;                                      \
    }
