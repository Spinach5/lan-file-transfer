/* getopt_long() replacement for MSVC (no getopt.h).
   On MinGW (__GNUC__) and Linux, the system <getopt.h> is used. */
#ifndef COMPAT_GETOPT_H
#define COMPAT_GETOPT_H

#if defined(_WIN32) && !defined(__GNUC__)

#include <string.h>
#include <stdio.h>

#define no_argument       0
#define required_argument 1
#define optional_argument 2

struct option {
    const char *name;
    int         has_arg;
    int        *flag;
    int         val;
};

extern int opterr;
extern int optind;
extern int optopt;
extern char *optarg;

int getopt_long(int argc, char *const argv[], const char *optstring,
                const struct option *longopts, int *longindex);

#endif /* _WIN32 && !__GNUC__ */

#endif /* COMPAT_GETOPT_H */
