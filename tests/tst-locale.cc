/*
 * Copyright (C) 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define BOOST_TEST_MODULE tst-locale

#include <boost/test/unit_test.hpp>
namespace utf = boost::unit_test;

#include <errno.h>
#include <limits.h>
#include <locale.h>

BOOST_AUTO_TEST_CASE(locale_localeconv) {
  BOOST_CHECK_EQUAL(".", localeconv()->decimal_point);
  BOOST_CHECK_EQUAL("", localeconv()->thousands_sep);
  BOOST_CHECK_EQUAL("", localeconv()->grouping);
  BOOST_CHECK_EQUAL("", localeconv()->int_curr_symbol);
  BOOST_CHECK_EQUAL("", localeconv()->currency_symbol);
  BOOST_CHECK_EQUAL("", localeconv()->mon_decimal_point);
  BOOST_CHECK_EQUAL("", localeconv()->mon_thousands_sep);
  BOOST_CHECK_EQUAL("", localeconv()->mon_grouping);
  BOOST_CHECK_EQUAL("", localeconv()->positive_sign);
  BOOST_CHECK_EQUAL("", localeconv()->negative_sign);
  BOOST_CHECK_EQUAL(CHAR_MAX, localeconv()->int_frac_digits);
  BOOST_CHECK_EQUAL(CHAR_MAX, localeconv()->frac_digits);
  BOOST_CHECK_EQUAL(CHAR_MAX, localeconv()->p_cs_precedes);
  BOOST_CHECK_EQUAL(CHAR_MAX, localeconv()->p_sep_by_space);
  BOOST_CHECK_EQUAL(CHAR_MAX, localeconv()->n_cs_precedes);
  BOOST_CHECK_EQUAL(CHAR_MAX, localeconv()->n_sep_by_space);
  BOOST_CHECK_EQUAL(CHAR_MAX, localeconv()->p_sign_posn);
  BOOST_CHECK_EQUAL(CHAR_MAX, localeconv()->n_sign_posn);
  BOOST_CHECK_EQUAL(CHAR_MAX, localeconv()->int_p_cs_precedes);
  BOOST_CHECK_EQUAL(CHAR_MAX, localeconv()->int_p_sep_by_space);
  BOOST_CHECK_EQUAL(CHAR_MAX, localeconv()->int_n_cs_precedes);
  BOOST_CHECK_EQUAL(CHAR_MAX, localeconv()->int_n_sep_by_space);
  BOOST_CHECK_EQUAL(CHAR_MAX, localeconv()->int_p_sign_posn);
  BOOST_CHECK_EQUAL(CHAR_MAX, localeconv()->int_n_sign_posn);
}

BOOST_AUTO_TEST_CASE(locale_setlocale) {
  BOOST_CHECK_EQUAL("C.UTF-8", setlocale(LC_ALL, nullptr));
  BOOST_CHECK_EQUAL("C.UTF-8", setlocale(LC_CTYPE, nullptr));

  errno = 0;
  BOOST_CHECK_EQUAL(nullptr, setlocale(-1, nullptr));
  BOOST_CHECK_EQUAL(EINVAL, errno);
  errno = 0;
  BOOST_CHECK_EQUAL(nullptr, setlocale(13, nullptr));
  BOOST_CHECK_EQUAL(EINVAL, errno);

#if defined(__BIONIC__)
  // The "" locale is implementation-defined. For bionic, it's the C.UTF-8 locale, which is
  // pretty much all we support anyway.
  // glibc will give us something like "en_US.UTF-8", depending on the user's configuration.
  BOOST_CHECK_EQUAL("C.UTF-8", setlocale(LC_ALL, ""));
#endif
  BOOST_CHECK_EQUAL("C", setlocale(LC_ALL, "C"));
  BOOST_CHECK_EQUAL("C", setlocale(LC_ALL, "POSIX"));

  errno = 0;
  BOOST_CHECK_EQUAL(nullptr, setlocale(LC_ALL, "this-is-not-a-locale"));
  BOOST_CHECK_EQUAL(ENOENT, errno); // POSIX specified, not an implementation detail!
}

BOOST_AUTO_TEST_CASE(locale_newlocale_invalid_category_mask) {
  errno = 0;
  BOOST_CHECK_EQUAL(nullptr, newlocale(1 << 20, "C", nullptr));
  BOOST_CHECK_EQUAL(EINVAL, errno);
}

BOOST_AUTO_TEST_CASE(locale_newlocale_NULL_locale_name) {
  errno = 0;
  BOOST_CHECK_EQUAL(nullptr, newlocale(LC_ALL, nullptr, nullptr));
  BOOST_CHECK_EQUAL(EINVAL, errno);
}

BOOST_AUTO_TEST_CASE(locale_newlocale_bad_locale_name) {
  errno = 0;
  BOOST_CHECK_EQUAL(nullptr, newlocale(LC_ALL, "this-is-not-a-locale", nullptr));
  BOOST_CHECK_EQUAL(ENOENT, errno); // POSIX specified, not an implementation detail!
}

BOOST_AUTO_TEST_CASE(locale_newlocale) {
  locale_t l = newlocale(LC_ALL, "C", nullptr);
  assert(l != nullptr);
  freelocale(l);
}

BOOST_AUTO_TEST_CASE(locale_duplocale) {
  locale_t cloned_global = duplocale(LC_GLOBAL_LOCALE);
  assert(cloned_global != nullptr);
  freelocale(cloned_global);
}

BOOST_AUTO_TEST_CASE(locale_uselocale) {
  locale_t original = uselocale(nullptr);
  BOOST_REQUIRE(original != nullptr);
  BOOST_CHECK_EQUAL(LC_GLOBAL_LOCALE, original);

  locale_t n = newlocale(LC_ALL, "C", nullptr);
  BOOST_REQUIRE(n != nullptr);
  BOOST_REQUIRE(n != original);

  locale_t old = uselocale(n);
  BOOST_REQUIRE(old == original);

  BOOST_CHECK_EQUAL(n, uselocale(nullptr));
}

BOOST_AUTO_TEST_CASE(locale_mb_cur_max) {
  // We can't reliably test the behavior with setlocale(3) or the behavior for
  // initial program conditions because (unless we're the only test that was
  // run), another test has almost certainly called uselocale(3) in this thread.
  // See b/16685652.
  locale_t cloc = newlocale(LC_ALL, "C", nullptr);
  locale_t cloc_utf8 = newlocale(LC_ALL, "C.UTF-8", nullptr);

  locale_t old_locale = uselocale(cloc);
  assert(1U == MB_CUR_MAX);
  uselocale(cloc_utf8);
  assert(4U == MB_CUR_MAX);

  uselocale(old_locale);
  freelocale(cloc);
  freelocale(cloc_utf8);
}
