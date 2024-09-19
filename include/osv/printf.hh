/*
 * Copyright (C) 2013 Cloudius Systems, Ltd.
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#ifndef PRINTF_HH_
#define PRINTF_HH_

#define BOOST_NO_STD_LOCALE 1

#include <string>
//#include <sstream>
//#include <iostream>

namespace osv {

std::string sprintf(const char* fmt...);
std::string vsprintf(const char* fmt, va_list ap);

// implementation

//TODO: Change it to not to use ostream
/*template <typename... args>
std::ostream& fprintf(std::ostream& os, const char* fmt, args... as)
{
    //boost::format f(fmt);
    //return fprintf(os, f, as...);
    return os;
}*/

} // namespace osv

#endif /* PRINTF_HH_ */
