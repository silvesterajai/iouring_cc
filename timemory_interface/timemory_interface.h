/**
 * MIT License
 *
 * Copyright (c) Ajai Silvester  <silvesterajai@gmail.com>
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
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#pragma once

#if defined(__cplusplus)
extern "C"
{
#endif

#if defined(USE_TIMEMORY)

    void set_papi_events(int nevents, char const** events);
    void initialize(int* argc, char*** argv);
    void finalize();
    void push_region(char const*);
    void pop_region(char const*);

#else

static inline void
set_papi_events(int, char const**)
{}

static inline void
initialize(int*, char***)
{}

static inline void
finalize()
{}

static inline void
push_region(char const*)
{}

static inline void
pop_region(char const*)
{}

#endif

#if defined(__cplusplus)
}
#endif
