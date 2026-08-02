// Copyright (c) 2026 Christopher Antos
// Derived from clink/test/main.cpp, portions Copyright (c) 2015 Martin Ridgers
// License: http://opensource.org/licenses/MIT

#include "tib.h"
#include "test.h"

#include <list>
#include <assert.h>
#include <crtdbg.h>

#ifdef DISABLE_CRT_INVALID_PARAMETER_FAILFAST
static void __cdecl do_nothing(wchar_t const*, wchar_t const*, wchar_t const*, unsigned int, uintptr_t)
{
}

static void install_crt_invalid_parameter_handler()
{
    _set_invalid_parameter_handler(do_nothing);
}
#endif

int main(int argc, char** argv)
{
    --argc, ++argv;

#ifdef DISABLE_CRT_INVALID_PARAMETER_FAILFAST
    install_crt_invalid_parameter_handler();
#endif

    // TODO:  Override ding preference, otherwise the tests may be vocal.

    bool list = false;
    bool times = false;

    while (argc > 0)
    {
        if (!strcmp(argv[0], "-?") || !strcmp(argv[0], "--help"))
        {
            puts(  "Options:\n"
                   "  -?            Show this help.\n"
                   "  -t            Show individual test times.\n"
                   "  --list-tests  List test names.");
            return 1;
        }
        else if (!strcmp(argv[0], "-t"))
        {
            times = true;
        }
        else if (!strcmp(argv[0], "--list-tests"))
        {
            list = true;
        }
        else if (!strcmp(argv[0], "--"))
        {
        }
        else
        {
            break;
        }

        --argc, ++argv;
    }

    if (list)
    {
        test::list();
        return 0;
    }

    // TODO:  Initialize tib appropriately.

    DWORD start = GetTickCount();

    test::colors::initialize();

    const char* prefix = (argc > 0) ? argv[0] : "";
    int32_t result = (test::run(prefix, times) != true);

    DWORD elapsed = GetTickCount() - start;
    printf("\nElapsed time %u.%03u seconds.\n", elapsed / 1000, elapsed % 1000);

    return result;
}
