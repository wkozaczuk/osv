/*
 * Copyright (C) 2016 The Android Open Source Project
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

#define BOOST_TEST_MODULE tst-wctype

#include <boost/test/unit_test.hpp>
namespace utf = boost::unit_test;

#include <wctype.h>

//#include <dlfcn.h>

class UtfLocale {
 public:
  UtfLocale() : l(newlocale(LC_ALL, "C.UTF-8", nullptr)) {}
  ~UtfLocale() { freelocale(l); }
  locale_t l;
};

static void TestIsWideFn(int fn(wint_t),
                         int fn_l(wint_t, locale_t),
                         const wchar_t* trues,
                         const wchar_t* falses) {
  UtfLocale l;
  for (const wchar_t* p = trues; *p; ++p) {
    /*if (!have_dl() && *p > 0x7f) {
      GTEST_LOG_(INFO) << "skipping unicode test " << *p;
      continue;
    }*/
    BOOST_REQUIRE_MESSAGE(fn(*p), "Failed for " << *p);
    BOOST_REQUIRE_MESSAGE(fn_l(*p, l.l), "Failed for " << *p);
  }
  for (const wchar_t* p = falses; *p; ++p) {
    /*if (!have_dl() && *p > 0x7f) {
      GTEST_LOG_(INFO) << "skipping unicode test " << *p;
      continue;
    }*/
    BOOST_REQUIRE_MESSAGE(!fn(*p), "Failed for " << *p);
    BOOST_REQUIRE_MESSAGE(!fn_l(*p, l.l), "Failed for " << *p);
  }
}

BOOST_AUTO_TEST_CASE(wctype_iswalnum) {
  TestIsWideFn(iswalnum, iswalnum_l, L"1aAÇçΔδ", L"! \b");
}

BOOST_AUTO_TEST_CASE(wctype_iswalpha) {
  TestIsWideFn(iswalpha, iswalpha_l, L"aAÇçΔδ", L"1! \b");
}

BOOST_AUTO_TEST_CASE(wctype_iswblank) {
  TestIsWideFn(iswblank, iswblank_l, L" \t", L"1aA!\bÇçΔδ");
}

BOOST_AUTO_TEST_CASE(wctype_iswcntrl) {
  TestIsWideFn(iswcntrl, iswcntrl_l, L"\b\u009f", L"1aA! ÇçΔδ");
}

BOOST_AUTO_TEST_CASE(wctype_iswdigit) {
  TestIsWideFn(iswdigit, iswdigit_l, L"1", L"aA! \bÇçΔδ");
}

BOOST_AUTO_TEST_CASE(wctype_iswgraph) {
  TestIsWideFn(iswgraph, iswgraph_l, L"1aA!ÇçΔδ", L" \b");
}

BOOST_AUTO_TEST_CASE(wctype_iswlower) {
  TestIsWideFn(iswlower, iswlower_l, L"açδ", L"1A! \bÇΔ");
}

BOOST_AUTO_TEST_CASE(wctype_iswprint) {
  TestIsWideFn(iswprint, iswprint_l, L"1aA! ÇçΔδ", L"\b");
}

BOOST_AUTO_TEST_CASE(wctype_iswpunct) {
  TestIsWideFn(iswpunct, iswpunct_l, L"!", L"1aA \bÇçΔδ");
}

BOOST_AUTO_TEST_CASE(wctype_iswspace) {
  TestIsWideFn(iswspace, iswspace_l, L" \f\t", L"1aA!\bÇçΔδ");
}

BOOST_AUTO_TEST_CASE(wctype_iswupper) {
  TestIsWideFn(iswupper, iswupper_l, L"AÇΔ", L"1a! \bçδ");
}

BOOST_AUTO_TEST_CASE(wctype_iswxdigit) {
  TestIsWideFn(iswxdigit, iswxdigit_l, L"01aA", L"xg! \b");
}

BOOST_AUTO_TEST_CASE(wctype_towlower) {
  BOOST_CHECK_EQUAL(WEOF, towlower(WEOF));
  BOOST_CHECK_EQUAL(wint_t('!'), towlower(L'!'));
  BOOST_CHECK_EQUAL(wint_t('a'), towlower(L'a'));
  BOOST_CHECK_EQUAL(wint_t('a'), towlower(L'A'));
  //if (have_dl()) {
    BOOST_CHECK_EQUAL(wint_t(L'ç'), towlower(L'ç'));
    BOOST_CHECK_EQUAL(wint_t(L'ç'), towlower(L'Ç'));
    BOOST_CHECK_EQUAL(wint_t(L'δ'), towlower(L'δ'));
    BOOST_CHECK_EQUAL(wint_t(L'δ'), towlower(L'Δ'));
  //} else {
  //  GTEST_SKIP() << "icu not available";
  //}
}

BOOST_AUTO_TEST_CASE(wctype_towlower_l) {
  UtfLocale l;
  BOOST_CHECK_EQUAL(WEOF, towlower(WEOF));
  BOOST_CHECK_EQUAL(wint_t('!'), towlower_l(L'!', l.l));
  BOOST_CHECK_EQUAL(wint_t('a'), towlower_l(L'a', l.l));
  BOOST_CHECK_EQUAL(wint_t('a'), towlower_l(L'A', l.l));
  //if (have_dl()) {
    BOOST_CHECK_EQUAL(wint_t(L'ç'), towlower_l(L'ç', l.l));
    BOOST_CHECK_EQUAL(wint_t(L'ç'), towlower_l(L'Ç', l.l));
    BOOST_CHECK_EQUAL(wint_t(L'δ'), towlower_l(L'δ', l.l));
    BOOST_CHECK_EQUAL(wint_t(L'δ'), towlower_l(L'Δ', l.l));
  // else {
  //  GTEST_SKIP() << "icu not available";
  //}
}

BOOST_AUTO_TEST_CASE(wctype_towupper) {
  BOOST_CHECK_EQUAL(WEOF, towupper(WEOF));
  BOOST_CHECK_EQUAL(wint_t('!'), towupper(L'!'));
  BOOST_CHECK_EQUAL(wint_t('A'), towupper(L'a'));
  BOOST_CHECK_EQUAL(wint_t('A'), towupper(L'A'));
  //if (have_dl()) {
    BOOST_CHECK_EQUAL(wint_t(L'Ç'), towupper(L'ç'));
    BOOST_CHECK_EQUAL(wint_t(L'Ç'), towupper(L'Ç'));
    BOOST_CHECK_EQUAL(wint_t(L'Δ'), towupper(L'δ'));
    BOOST_CHECK_EQUAL(wint_t(L'Δ'), towupper(L'Δ'));
  //} else {
  //  GTEST_SKIP() << "icu not available";
  //}
}

BOOST_AUTO_TEST_CASE(wctype_towupper_l) {
  UtfLocale l;
  BOOST_CHECK_EQUAL(WEOF, towupper_l(WEOF, l.l));
  BOOST_CHECK_EQUAL(wint_t('!'), towupper_l(L'!', l.l));
  BOOST_CHECK_EQUAL(wint_t('A'), towupper_l(L'a', l.l));
  BOOST_CHECK_EQUAL(wint_t('A'), towupper_l(L'A', l.l));
  //if (have_dl()) {
    BOOST_CHECK_EQUAL(wint_t(L'Ç'), towupper_l(L'ç', l.l));
    BOOST_CHECK_EQUAL(wint_t(L'Ç'), towupper_l(L'Ç', l.l));
    BOOST_CHECK_EQUAL(wint_t(L'Δ'), towupper_l(L'δ', l.l));
    BOOST_CHECK_EQUAL(wint_t(L'Δ'), towupper_l(L'Δ', l.l));
  //} else {
  //  GTEST_SKIP() << "icu not available";
  //}
}

BOOST_AUTO_TEST_CASE(wctype_wctype) {
  BOOST_REQUIRE(wctype("alnum") != 0);
  BOOST_REQUIRE(wctype("alpha") != 0);
  BOOST_REQUIRE(wctype("blank") != 0);
  BOOST_REQUIRE(wctype("cntrl") != 0);
  BOOST_REQUIRE(wctype("digit") != 0);
  BOOST_REQUIRE(wctype("graph") != 0);
  BOOST_REQUIRE(wctype("lower") != 0);
  BOOST_REQUIRE(wctype("print") != 0);
  BOOST_REQUIRE(wctype("punct") != 0);
  BOOST_REQUIRE(wctype("space") != 0);
  BOOST_REQUIRE(wctype("upper") != 0);
  BOOST_REQUIRE(wctype("xdigit") != 0);

  BOOST_REQUIRE(wctype("monkeys") == 0);
}

BOOST_AUTO_TEST_CASE(wctype_wctype_l) {
  UtfLocale l;
  BOOST_REQUIRE(wctype_l("alnum", l.l) != 0);
  BOOST_REQUIRE(wctype_l("alpha", l.l) != 0);
  BOOST_REQUIRE(wctype_l("blank", l.l) != 0);
  BOOST_REQUIRE(wctype_l("cntrl", l.l) != 0);
  BOOST_REQUIRE(wctype_l("digit", l.l) != 0);
  BOOST_REQUIRE(wctype_l("graph", l.l) != 0);
  BOOST_REQUIRE(wctype_l("lower", l.l) != 0);
  BOOST_REQUIRE(wctype_l("print", l.l) != 0);
  BOOST_REQUIRE(wctype_l("punct", l.l) != 0);
  BOOST_REQUIRE(wctype_l("space", l.l) != 0);
  BOOST_REQUIRE(wctype_l("upper", l.l) != 0);
  BOOST_REQUIRE(wctype_l("xdigit", l.l) != 0);

  BOOST_REQUIRE(wctype_l("monkeys", l.l) == 0);
}

BOOST_AUTO_TEST_CASE(wctype_iswctype) {
  BOOST_REQUIRE(iswctype(L'a', wctype("alnum")));
  BOOST_REQUIRE(iswctype(L'1', wctype("alnum")));
  BOOST_REQUIRE(!iswctype(L' ', wctype("alnum")));

  BOOST_CHECK_EQUAL(0, iswctype(WEOF, wctype("alnum")));
}

BOOST_AUTO_TEST_CASE(wctype_iswctype_l) {
  UtfLocale l;
  BOOST_REQUIRE(iswctype_l(L'a', wctype_l("alnum", l.l), l.l));
  BOOST_REQUIRE(iswctype_l(L'1', wctype_l("alnum", l.l), l.l));
  BOOST_REQUIRE(!iswctype_l(L' ', wctype_l("alnum", l.l), l.l));

  BOOST_CHECK_EQUAL(0, iswctype_l(WEOF, wctype_l("alnum", l.l), l.l));
}

BOOST_AUTO_TEST_CASE(wctype_towctrans) {
  BOOST_REQUIRE(wctrans("tolower") != nullptr);
  BOOST_REQUIRE(wctrans("toupper") != nullptr);

  BOOST_REQUIRE(wctrans("monkeys") == nullptr);
}

BOOST_AUTO_TEST_CASE(wctype_towctrans_l) {
  UtfLocale l;
  BOOST_REQUIRE(wctrans_l("tolower", l.l) != nullptr);
  BOOST_REQUIRE(wctrans_l("toupper", l.l) != nullptr);

  BOOST_REQUIRE(wctrans_l("monkeys", l.l) == nullptr);
}

BOOST_AUTO_TEST_CASE(wctype_wctrans) {
  BOOST_CHECK_EQUAL(wint_t('a'), towctrans(L'A', wctrans("tolower")));
  BOOST_CHECK_EQUAL(WEOF, towctrans(WEOF, wctrans("tolower")));

  BOOST_CHECK_EQUAL(wint_t('A'), towctrans(L'a', wctrans("toupper")));
  BOOST_CHECK_EQUAL(WEOF, towctrans(WEOF, wctrans("toupper")));
}

BOOST_AUTO_TEST_CASE(wctype_wctrans_l) {
  UtfLocale l;
  BOOST_CHECK_EQUAL(wint_t('a'), towctrans_l(L'A', wctrans_l("tolower", l.l), l.l));
  BOOST_CHECK_EQUAL(WEOF, towctrans_l(WEOF, wctrans_l("tolower", l.l), l.l));

  BOOST_CHECK_EQUAL(wint_t('A'), towctrans_l(L'a', wctrans_l("toupper", l.l), l.l));
  BOOST_CHECK_EQUAL(WEOF, towctrans_l(WEOF, wctrans_l("toupper", l.l), l.l));
}
