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

#if defined(USE_TIMEMORY)
#include "timemory_interface/timemory_interface.h"
#include <timemory/library.h>
#include <timemory/timemory.h>
#include <timemory/tools/timemory-mallocp.h>
#include <timemory/tools/timemory-mpip.h>

extern "C"
{
    void initialize(int* argc, char*** argv)
    {
        tim::settings::precision()         = 6;
        tim::settings::cout_output()       = false;
        tim::settings::flamegraph_output() = false;
        tim::settings::global_components() = "wall_clock, cpu_clock, peak_rss";
        tim::settings::papi_events() =
            "PAPI_TOT_CYC, PAPI_TOT_INS, PAPI_RES_STL, PAPI_STL_CCY, PAPI_LD_INS, "
            "PAPI_SR_INS, PAPI_LST_INS";

        if(!tim::settings::papi_events().empty())
            tim::settings::global_components() += ", papi_array";

        timemory_init_library(*argc, *argv);
        tim::timemory_argparse(argc, argv);
        timemory_set_default(tim::settings::global_components().c_str());
        timemory_register_mpip();
    }

    void finalize()
    {
        timemory_deregister_mpip();
        timemory_finalize_library();
    }

    void push_region(char const* _region_name) { timemory_push_region(_region_name); }

    void pop_region(char const* _region_name) { timemory_pop_region(_region_name); }

}  // extern "C"
# endif
