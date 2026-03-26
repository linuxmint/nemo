/*
 * fzy fuzzy matching algorithm - bonus scoring tables
 *
 * Copyright (c) 2014 John Hawthorn
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef FZY_BONUS_H
#define FZY_BONUS_H

#include "fzy-match.h"

#define ASSIGN_LOWER(v) \
    ['a'] = (v), \
    ['b'] = (v), \
    ['c'] = (v), \
    ['d'] = (v), \
    ['e'] = (v), \
    ['f'] = (v), \
    ['g'] = (v), \
    ['h'] = (v), \
    ['i'] = (v), \
    ['j'] = (v), \
    ['k'] = (v), \
    ['l'] = (v), \
    ['m'] = (v), \
    ['n'] = (v), \
    ['o'] = (v), \
    ['p'] = (v), \
    ['q'] = (v), \
    ['r'] = (v), \
    ['s'] = (v), \
    ['t'] = (v), \
    ['u'] = (v), \
    ['v'] = (v), \
    ['w'] = (v), \
    ['x'] = (v), \
    ['y'] = (v), \
    ['z'] = (v)

#define ASSIGN_UPPER(v) \
    ['A'] = (v), \
    ['B'] = (v), \
    ['C'] = (v), \
    ['D'] = (v), \
    ['E'] = (v), \
    ['F'] = (v), \
    ['G'] = (v), \
    ['H'] = (v), \
    ['I'] = (v), \
    ['J'] = (v), \
    ['K'] = (v), \
    ['L'] = (v), \
    ['M'] = (v), \
    ['N'] = (v), \
    ['O'] = (v), \
    ['P'] = (v), \
    ['Q'] = (v), \
    ['R'] = (v), \
    ['S'] = (v), \
    ['T'] = (v), \
    ['U'] = (v), \
    ['V'] = (v), \
    ['W'] = (v), \
    ['X'] = (v), \
    ['Y'] = (v), \
    ['Z'] = (v)

#define ASSIGN_DIGIT(v) \
    ['0'] = (v), \
    ['1'] = (v), \
    ['2'] = (v), \
    ['3'] = (v), \
    ['4'] = (v), \
    ['5'] = (v), \
    ['6'] = (v), \
    ['7'] = (v), \
    ['8'] = (v), \
    ['9'] = (v)

const score_t bonus_states[3][256] = {
    { 0 },
    {
        ['/'] = SCORE_MATCH_SLASH,
        ['-'] = SCORE_MATCH_WORD,
        ['_'] = SCORE_MATCH_WORD,
        [' '] = SCORE_MATCH_WORD,
        ['.'] = SCORE_MATCH_DOT,
    },
    {
        ['/'] = SCORE_MATCH_SLASH,
        ['-'] = SCORE_MATCH_WORD,
        ['_'] = SCORE_MATCH_WORD,
        [' '] = SCORE_MATCH_WORD,
        ['.'] = SCORE_MATCH_DOT,

        /* ['a' ... 'z'] = SCORE_MATCH_CAPITAL, */
        ASSIGN_LOWER(SCORE_MATCH_CAPITAL)
    }
};

const size_t bonus_index[256] = {
    /* ['A' ... 'Z'] = 2 */
    ASSIGN_UPPER(2),

    /* ['a' ... 'z'] = 1 */
    ASSIGN_LOWER(1),

    /* ['0' ... '9'] = 1 */
    ASSIGN_DIGIT(1)
};

#define COMPUTE_BONUS(last_ch, ch) (bonus_states[bonus_index[(unsigned char)(ch)]][(unsigned char)(last_ch)])

#endif
