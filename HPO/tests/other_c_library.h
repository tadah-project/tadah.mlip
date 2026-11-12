/* What of other_c_library.c, the stand-in for another C library that four
 * CTest entries load before the C library, is in effect in this process. A
 * test run by such an entry checks that the stand-in is loaded and that the
 * choice it was given changed what the program got from the C library: an
 * entry whose stand-in was not loaded would pass without testing anything
 * (the dynamic linker only warns of a file it cannot load). */
#ifndef TADAH_HPO_TESTS_OTHER_C_LIBRARY_H
#define TADAH_HPO_TESTS_OTHER_C_LIBRARY_H

#include <dlfcn.h>

#include <cstdlib>
#include <string>

/* True when the entry asked for the choice @p what of other_c_library.c
 * ("strerror", "libm", "sort_other", "sort_keep"). */
inline bool other_c_library(const char* what)
{
    const char* asked = std::getenv("TADAH_TEST_C_LIBRARY");
    return asked && std::string(asked) == what;
}

/* True when other_c_library.c is loaded in this process. */
inline bool other_c_library_loaded()
{
    return ::dlsym(RTLD_DEFAULT, "tadah_test_other_c_library") != nullptr;
}

#endif
