#ifndef LOCALE_IMPL_H
#define LOCALE_IMPL_H

#include <locale.h>

#define LOCALE_NAME_MAX 23

#include <bits/alltypes.h>
typedef unsigned long size_t;
struct __locale_map {
	const void *map;
	size_t map_size;
	char name[LOCALE_NAME_MAX+1];
	const struct __locale_map *next;
};

struct __locale_struct {
    struct __locale_data *__locales[13];
    const unsigned short int *__ctype_b;
    const int* __ctype_tolower;
    const int* __ctype_toupper;
    const char *__names[13];
    const struct __locale_map *cat[6];
};

/*
struct __locale_struct {
	const struct __locale_map *cat[6];
};*/

typedef struct __locale_struct *__locale_t;

typedef __locale_t locale_t;

#define hidden __attribute__((__visibility__("hidden")))
extern hidden const struct __locale_map __c_dot_utf8;
extern hidden const struct __locale_struct __c_locale;
extern hidden const struct __locale_struct __c_dot_utf8_locale;

hidden const struct __locale_map *__get_locale(int, const char *);
hidden const char *__mo_lookup(const void *, size_t, const char *);
hidden const char *__lctrans(const char *, const struct __locale_map *);
hidden const char *__lctrans_cur(const char *);
hidden const char *__lctrans_impl(const char *, const struct __locale_map *);
hidden int __loc_is_allocated(locale_t);
hidden char *__gettextdomain(void);

#define LOC_MAP_FAILED ((const struct __locale_map *)-1)

#define LCTRANS(msg, lc, loc) __lctrans(msg, (loc)->cat[(lc)])
#define LCTRANS_CUR(msg) __lctrans_cur(msg)

#define C_LOCALE ((locale_t)&__c_locale)
#define UTF8_LOCALE ((locale_t)&__c_dot_utf8_locale)

//#define CURRENT_LOCALE (__pthread_self()->locale)
//#define CURRENT_UTF8 (!!__pthread_self()->locale->cat[LC_CTYPE])
extern locale_t the_locale;
#define CURRENT_LOCALE (the_locale)
#define CURRENT_UTF8 (the_locale)

#undef MB_CUR_MAX
//#define MB_CUR_MAX (CURRENT_UTF8 ? 4 : 1)
#define MB_CUR_MAX (1)

#endif
