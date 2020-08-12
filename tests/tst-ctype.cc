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

#define BOOST_TEST_MODULE tst-ctype

#include <boost/test/unit_test.hpp>
namespace utf = boost::unit_test;

#include <ctype.h>

// We test from -1 (EOF) to 0xff, because that's the range for which behavior
// is actually defined. (It's explicitly undefined below or above that.) Most
// of our routines are no longer table-based and behave correctly for the
// entire int range, but that's not true of other C libraries that we might
// want to compare against, nor of our isalnum(3) and ispunt(3).
static constexpr int kMin = -1;
static constexpr int kMax = 256;

BOOST_AUTO_TEST_CASE(ctype_isalnum) {
  for (int i = kMin; i < kMax; ++i) {
    if ((i >= '0' && i <= '9') ||
        (i >= 'A' && i <= 'Z') ||
        (i >= 'a' && i <= 'z')) {
      BOOST_REQUIRE_MESSAGE(isalnum(i), "Failed for " << i);
    } else {
      BOOST_REQUIRE_MESSAGE(!isalnum(i), "Failed for " << i);
    }
  }
}

BOOST_AUTO_TEST_CASE(ctype_isalnum_l) {
  for (int i = kMin; i < kMax; ++i) {
    if ((i >= '0' && i <= '9') ||
        (i >= 'A' && i <= 'Z') ||
        (i >= 'a' && i <= 'z')) {
      BOOST_REQUIRE_MESSAGE(isalnum_l(i, LC_GLOBAL_LOCALE), "Failed for " << i);
    } else {
      BOOST_REQUIRE_MESSAGE(!isalnum_l(i, LC_GLOBAL_LOCALE), "Failed for " << i);
    }
  }
}

BOOST_AUTO_TEST_CASE(ctype_isalpha) {
  for (int i = kMin; i < kMax; ++i) {
    if ((i >= 'A' && i <= 'Z') ||
        (i >= 'a' && i <= 'z')) {
      BOOST_REQUIRE_MESSAGE(isalpha(i), "Failed for " << i);
    } else {
      BOOST_REQUIRE_MESSAGE(!isalpha(i), "Failed for " << i);
    }
  }
}

BOOST_AUTO_TEST_CASE(ctype_isalpha_l) {
  for (int i = kMin; i < kMax; ++i) {
    if ((i >= 'A' && i <= 'Z') ||
        (i >= 'a' && i <= 'z')) {
      BOOST_REQUIRE_MESSAGE(isalpha_l(i, LC_GLOBAL_LOCALE), "Failed for " << i);
    } else {
      BOOST_REQUIRE_MESSAGE(!isalpha_l(i, LC_GLOBAL_LOCALE), "Failed for " << i);
    }
  }
}

BOOST_AUTO_TEST_CASE(ctype_isascii) {
  for (int i = kMin; i < kMax; ++i) {
    if (i >= 0 && i <= 0x7f) {
      BOOST_REQUIRE_MESSAGE(isascii(i), "Failed for " << i);
    } else {
      BOOST_REQUIRE_MESSAGE(!isascii(i), "Failed for " << i);
    }
  }
}

BOOST_AUTO_TEST_CASE(ctype_isblank) {
  for (int i = kMin; i < kMax; ++i) {
    if (i == '\t' || i == ' ') {
      BOOST_REQUIRE_MESSAGE(isblank(i), "Failed for " << i);
    } else {
      BOOST_REQUIRE_MESSAGE(!isblank(i), "Failed for " << i);
    }
  }
}

BOOST_AUTO_TEST_CASE(ctype_isblank_l) {
  for (int i = kMin; i < kMax; ++i) {
    if (i == '\t' || i == ' ') {
      BOOST_REQUIRE_MESSAGE(isblank_l(i, LC_GLOBAL_LOCALE), "Failed for " << i);
    } else {
      BOOST_REQUIRE_MESSAGE(!isblank_l(i, LC_GLOBAL_LOCALE), "Failed for " << i);
    }
  }
}

BOOST_AUTO_TEST_CASE(ctype_iscntrl) {
  for (int i = kMin; i < kMax; ++i) {
    if ((i >= 0 && i < ' ') || i == 0x7f) {
      BOOST_REQUIRE_MESSAGE(iscntrl(i), "Failed for " << i);
    } else {
      BOOST_REQUIRE_MESSAGE(!iscntrl(i), "Failed for " << i);
    }
  }
}

BOOST_AUTO_TEST_CASE(ctype_iscntrl_l) {
  for (int i = kMin; i < kMax; ++i) {
    if ((i >= 0 && i < ' ') || i == 0x7f) {
      BOOST_REQUIRE_MESSAGE(iscntrl_l(i, LC_GLOBAL_LOCALE), "Failed for " << i);
    } else {
      BOOST_REQUIRE_MESSAGE(!iscntrl_l(i, LC_GLOBAL_LOCALE), "Failed for " << i);
    }
  }
}

BOOST_AUTO_TEST_CASE(ctype_isdigit) {
  for (int i = kMin; i < kMax; ++i) {
    if (i >= '0' && i <= '9') {
      BOOST_REQUIRE_MESSAGE(isdigit(i), "Failed for " << i);
    } else {
      BOOST_REQUIRE_MESSAGE(!isdigit(i), "Failed for " << i);
    }
  }
}

BOOST_AUTO_TEST_CASE(ctype_isdigit_l) {
  for (int i = kMin; i < kMax; ++i) {
    if (i >= '0' && i <= '9') {
      BOOST_REQUIRE_MESSAGE(isdigit_l(i, LC_GLOBAL_LOCALE), "Failed for " << i);
    } else {
      BOOST_REQUIRE_MESSAGE(!isdigit_l(i, LC_GLOBAL_LOCALE), "Failed for " << i);
    }
  }
}

BOOST_AUTO_TEST_CASE(ctype_isgraph) {
  for (int i = kMin; i < kMax; ++i) {
    if (i >= '!' && i <= '~') {
      BOOST_REQUIRE_MESSAGE(isgraph(i), "Failed for " << i);
    } else {
      BOOST_REQUIRE_MESSAGE(!isgraph(i), "Failed for " << i);
    }
  }
}

BOOST_AUTO_TEST_CASE(ctype_isgraph_l) {
  for (int i = kMin; i < kMax; ++i) {
    if (i >= '!' && i <= '~') {
      BOOST_REQUIRE_MESSAGE(isgraph_l(i, LC_GLOBAL_LOCALE), "Failed for " << i);
    } else {
      BOOST_REQUIRE_MESSAGE(!isgraph_l(i, LC_GLOBAL_LOCALE), "Failed for " << i);
    }
  }
}

BOOST_AUTO_TEST_CASE(ctype_islower) {
  for (int i = kMin; i < kMax; ++i) {
    if (i >= 'a' && i <= 'z') {
      BOOST_REQUIRE_MESSAGE(islower(i), "Failed for " << i);
    } else {
      BOOST_REQUIRE_MESSAGE(!islower(i), "Failed for " << i);
    }
  }
}

BOOST_AUTO_TEST_CASE(ctype_islower_l) {
  for (int i = kMin; i < kMax; ++i) {
    if (i >= 'a' && i <= 'z') {
      BOOST_REQUIRE_MESSAGE(islower_l(i, LC_GLOBAL_LOCALE), "Failed for " << i);
    } else {
      BOOST_REQUIRE_MESSAGE(!islower_l(i, LC_GLOBAL_LOCALE), "Failed for " << i);
    }
  }
}

BOOST_AUTO_TEST_CASE(ctype_isprint) {
  for (int i = kMin; i < kMax; ++i) {
    if (i >= ' ' && i <= '~') {
      BOOST_REQUIRE_MESSAGE(isprint(i), "Failed for " << i);
    } else {
      BOOST_REQUIRE_MESSAGE(!isprint(i), "Failed for " << i);
    }
  }
}

BOOST_AUTO_TEST_CASE(ctype_isprint_l) {
  for (int i = kMin; i < kMax; ++i) {
    if (i >= ' ' && i <= '~') {
      BOOST_REQUIRE_MESSAGE(isprint_l(i, LC_GLOBAL_LOCALE), "Failed for " << i);
    } else {
      BOOST_REQUIRE_MESSAGE(!isprint_l(i, LC_GLOBAL_LOCALE), "Failed for " << i);
    }
  }
}

BOOST_AUTO_TEST_CASE(ctype_ispunct) {
  for (int i = kMin; i < kMax; ++i) {
    if ((i >= '!' && i <= '/') ||
        (i >= ':' && i <= '@') ||
        (i >= '[' && i <= '`') ||
        (i >= '{' && i <= '~')) {
      BOOST_REQUIRE_MESSAGE(ispunct(i), "Failed for " << i);
    } else {
      BOOST_REQUIRE_MESSAGE(!ispunct(i), "Failed for " << i);
    }
  }
}

BOOST_AUTO_TEST_CASE(ctype_ispunct_l) {
  for (int i = kMin; i < kMax; ++i) {
    if ((i >= '!' && i <= '/') ||
        (i >= ':' && i <= '@') ||
        (i >= '[' && i <= '`') ||
        (i >= '{' && i <= '~')) {
      BOOST_REQUIRE_MESSAGE(ispunct_l(i, LC_GLOBAL_LOCALE), "Failed for " << i);
    } else {
      BOOST_REQUIRE_MESSAGE(!ispunct_l(i, LC_GLOBAL_LOCALE), "Failed for " << i);
    }
  }
}

BOOST_AUTO_TEST_CASE(ctype_isspace) {
  for (int i = kMin; i < kMax; ++i) {
    if ((i >= '\t' && i <= '\r') || i == ' ') {
      BOOST_REQUIRE_MESSAGE(isspace(i), "Failed for " << i);
    } else {
      BOOST_REQUIRE_MESSAGE(!isspace(i), "Failed for " << i);
    }
  }
}

BOOST_AUTO_TEST_CASE(ctype_isspace_l) {
  for (int i = kMin; i < kMax; ++i) {
    if ((i >= '\t' && i <= '\r') || i == ' ') {
      BOOST_REQUIRE_MESSAGE(isspace_l(i, LC_GLOBAL_LOCALE), "Failed for " << i);
    } else {
      BOOST_REQUIRE_MESSAGE(!isspace_l(i, LC_GLOBAL_LOCALE), "Failed for " << i);
    }
  }
}

BOOST_AUTO_TEST_CASE(ctype_isupper) {
  for (int i = kMin; i < kMax; ++i) {
    if (i >= 'A' && i <= 'Z') {
      BOOST_REQUIRE_MESSAGE(isupper(i), "Failed for " << i);
    } else {
      BOOST_REQUIRE_MESSAGE(!isupper(i), "Failed for " << i);
    }
  }
}

BOOST_AUTO_TEST_CASE(ctype_isupper_l) {
  for (int i = kMin; i < kMax; ++i) {
    if (i >= 'A' && i <= 'Z') {
      BOOST_REQUIRE_MESSAGE(isupper_l(i, LC_GLOBAL_LOCALE), "Failed for " << i);
    } else {
      BOOST_REQUIRE_MESSAGE(!isupper_l(i, LC_GLOBAL_LOCALE), "Failed for " << i);
    }
  }
}

BOOST_AUTO_TEST_CASE(ctype_isxdigit) {
  for (int i = kMin; i < kMax; ++i) {
    if ((i >= '0' && i <= '9') ||
        (i >= 'A' && i <= 'F') ||
        (i >= 'a' && i <= 'f')) {
      BOOST_REQUIRE_MESSAGE(isxdigit(i), "Failed for " << i);
    } else {
      BOOST_REQUIRE_MESSAGE(!isxdigit(i), "Failed for " << i);
    }
  }
}

BOOST_AUTO_TEST_CASE(ctype_isxdigit_l) {
  for (int i = kMin; i < kMax; ++i) {
    if ((i >= '0' && i <= '9') ||
        (i >= 'A' && i <= 'F') ||
        (i >= 'a' && i <= 'f')) {
      BOOST_REQUIRE_MESSAGE(isxdigit_l(i, LC_GLOBAL_LOCALE), "Failed for " << i);
    } else {
      BOOST_REQUIRE_MESSAGE(!isxdigit_l(i, LC_GLOBAL_LOCALE), "Failed for " << i);
    }
  }
}

BOOST_AUTO_TEST_CASE(ctype_toascii) {
  BOOST_CHECK_EQUAL('a', toascii('a'));
  BOOST_CHECK_EQUAL('a', toascii(0x80 | 'a'));
}

BOOST_AUTO_TEST_CASE(ctype_tolower) {
  BOOST_CHECK_EQUAL('!', tolower('!'));
  BOOST_CHECK_EQUAL('a', tolower('a'));
  BOOST_CHECK_EQUAL('a', tolower('A'));
}

BOOST_AUTO_TEST_CASE(ctype_tolower_l) {
  BOOST_CHECK_EQUAL('!', tolower_l('!', LC_GLOBAL_LOCALE));
  BOOST_CHECK_EQUAL('a', tolower_l('a', LC_GLOBAL_LOCALE));
  BOOST_CHECK_EQUAL('a', tolower_l('A', LC_GLOBAL_LOCALE));
}

BOOST_AUTO_TEST_CASE(ctype__tolower) {
  // _tolower may mangle characters for which isupper is false.
  BOOST_CHECK_EQUAL('a', _tolower('A'));
}

BOOST_AUTO_TEST_CASE(ctype_toupper) {
  BOOST_CHECK_EQUAL('!', toupper('!'));
  BOOST_CHECK_EQUAL('A', toupper('a'));
  BOOST_CHECK_EQUAL('A', toupper('A'));
}

BOOST_AUTO_TEST_CASE(ctype_toupper_l) {
  BOOST_CHECK_EQUAL('!', toupper_l('!', LC_GLOBAL_LOCALE));
  BOOST_CHECK_EQUAL('A', toupper_l('a', LC_GLOBAL_LOCALE));
  BOOST_CHECK_EQUAL('A', toupper_l('A', LC_GLOBAL_LOCALE));
}

BOOST_AUTO_TEST_CASE(ctype__toupper) {
  // _toupper may mangle characters for which islower is false.
  BOOST_CHECK_EQUAL('A', _toupper('a'));
}
