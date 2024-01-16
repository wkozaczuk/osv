/*
 * Copyright (C) 2013 Nodalink, SARL.
 * Copyright (C) 2014 Cloudius Systems.
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#include <iterator>
#include <fstream>
#include <osv/debug.hh>

#include <osv/power.hh>
#include <osv/commands.hh>
#include <osv/align.hh>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <cassert>
#include <unistd.h>

namespace osv {

// std::string is not fully functional when parse_cmdline is used for the first
// time.  So let's go for a more traditional memory management to avoid testing
// early / not early, etc
char *osv_cmdline = nullptr;
static char* parsed_cmdline = nullptr;

std::string getcmdline()
{
    return std::string(osv_cmdline);
}

#define MY_DEBUG(args...) if(0) printf(args)
/*
loader_parse_cmdline accepts input str - OSv commandline, say:
--env=AA=aa --env=BB=bb1\ bb2 app.so arg1 arg2

The loader options are parsed and saved into argv, up to first not-loader-option
token. argc is set to number of loader options.
app_cmdline is set to unconsumed part of input str.
Example output:
argc = 2
argv[0] = "--env=AA=aa"
argv[1] = "--env=BB=bb1 bb2"
argv[2] = NULL
app_cmdline = "app.so arg1 arg2"

The whitespaces can be escaped with '\' to allow options with spaces.
Notes:
 - _quoting_ loader options with space is not supported.
 - input str is modified.
 - output argv has to be free-d by caller.
 - the strings pointed to by output argv and app_cmdline are in same memory as
   original input str. The caller is not permited to modify or free data at str
   after the call to loader_parse_cmdline, as that would corrupt returned
   results in argv and app_cmdline.

Note that std::string is intentionly not used, as it is not fully functional when
called early during boot.
*/
void loader_parse_cmdline(char* str, int *pargc, char*** pargv, char** app_cmdline) {
    *pargv = nullptr;
    *pargc = 0;
    *app_cmdline = nullptr;

    const char *delim = " \t\n";
    char esc = '\\';

    // parse string
    char *ap;
    char *ap0=nullptr, *apE=nullptr; // first and last token.
    int ntoken = 0;
    ap0 = nullptr;
    while(1) {
        // Did we already consume all loader options?
        // Look at first non-space char - if =='-', than this is loader option.
        // Otherwise, it is application command.
        char *ch = str;
        while (ch && *ch != '\0') {
            if (strchr(delim, *ch)) {
                ch++;
                continue;
            }
            else if (*ch == '-') {
                // this is a loader option, continue with loader parsing
                break;
            }
            else {
                // ch is not space or '-', it is start of application command
                // Save current position and stop loader parsing.
                *app_cmdline = str;
                break;
            }
        }
        if (ch && *ch == '\0') {
            // empty str, contains only spaces
            *app_cmdline = str;
        }
        if (*app_cmdline) {
            break;
        }
        // there are loader options, continue with parsing

        ap = stresep(&str, delim, esc);
        assert(ap);

        MY_DEBUG("  ap = %p %s, *ap=%d\n", ap, ap, *ap);
        if (*ap != '\0') {
            // valid token found
            ntoken++;
            if (ap0 == nullptr) {
                ap0 = ap;
            }
            apE = ap;
        }
        else {
            // Multiple consecutive delimiters found. Stresep will write multiple
            // '\0' into str. Squash them into one, so that argv will be 'nice',
            // in memory consecutive array of C strings.
            if (str) {
                MY_DEBUG("    shift   str %p '%s' <- %p '%s'\n", str-1, str-1, str, str);
                memmove(str-1, str, strlen(str) + 1);
                str--;
            }
        }
        if (str == nullptr) {
            // end of string, last char was delimiter
            *app_cmdline = ap + strlen(ap); // make app_cmdline valid pointer to '\0'.
            MY_DEBUG("    make app_cmdline valid pointer to '\\0' ap=%p '%s', app_cmdline=%p '%s'\n",
                ap, ap, app_cmdline, app_cmdline);
            break;
        }

    }
    MY_DEBUG("  ap0 = %p '%s', apE = %p '%s', ntoken = %d, app_cmdline=%p '%s'\n",
        ap0, ap0, apE, apE, ntoken, *app_cmdline, *app_cmdline);
    *pargv = (char**)malloc(sizeof(char*) * (ntoken+1));
    // str was modified, tokes are separated by exactly one '\0'
    int ii;
    for(ap = ap0, ii = 0; ii < ntoken; ap += strlen(ap)+1, ii++) {
        assert(ap != nullptr);
        assert(*ap != '\0');
        MY_DEBUG("  argv[%d] = %p %s\n", ii, ap, ap);
        (*pargv)[ii] = ap;
    }
    MY_DEBUG("  ntoken = %d, ii = %d\n", ntoken, ii);
    assert(ii == ntoken);
    (*pargv)[ii] = nullptr;
    *pargc = ntoken;
}
#undef MY_DEBUG

int parse_cmdline(const char *p)
{
    if (__loader_argv) {
        // __loader_argv was allocated by loader_parse_cmdline
        free(__loader_argv);
    }

    if (osv_cmdline) {
        free(osv_cmdline);
    }
    osv_cmdline = strdup(p);

    if (parsed_cmdline) {
        free(parsed_cmdline);
    }
    parsed_cmdline = strdup(p);

    loader_parse_cmdline(parsed_cmdline, &__loader_argc, &__loader_argv, &__app_cmdline);
    return 0;
}

void save_cmdline(std::string newcmd)
{
    if (newcmd.size() >= max_cmdline) {
        throw std::length_error("command line too long");
    }

    int fd = open("/dev/vblk0", O_WRONLY);
    if (fd < 0) {
        throw std::system_error(std::error_code(errno, std::system_category()), "error opening block device");
    }

    lseek(fd, 512, SEEK_SET);

    // writes to the block device must be 512-byte aligned
    int size = align_up(newcmd.size() + 1, 512UL);
    int ret = write(fd, newcmd.c_str(), size);
    close(fd);

    if (ret != size) {
        throw std::system_error(std::error_code(errno, std::system_category()), "error writing command line to disk");
    }

    osv::parse_cmdline(newcmd.c_str());
}

}
