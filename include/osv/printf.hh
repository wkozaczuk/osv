/*
 * Copyright (C) 2013 Cloudius Systems, Ltd.
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#ifndef PRINTF_HH_
#define PRINTF_HH_

#include <string>
#include <sstream>
#include <iostream>

namespace osv {

template <typename... args>
std::string sprintf(const char* fmt, args... as);

// implementation

//TODO: Change it to not to use ostream
template <typename... args>
std::ostream& fprintf(std::ostream& os, const char* fmt, args... as)
{
    //boost::format f(fmt);
    //return fprintf(os, f, as...);
    return os;
}

template <typename... args>
std::string sprintf(const char* fmt, args... as)
{
    //TODO: Re-implement
    /*boost::format f(fmt);
    std::ostringstream os;
    fprintf(os, f, as...);
    return os.str();*/
    return "";
}

} // namespace osv

#endif /* PRINTF_HH_ */
