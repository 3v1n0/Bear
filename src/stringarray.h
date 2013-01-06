// This file is distributed under MIT-LICENSE. See COPYING for details.

#ifndef COMMON_STRINGARRAY_H
#define COMMON_STRINGARRAY_H

#include <stddef.h>
#include <stdarg.h>

typedef char const *    String;
typedef String *        Strings;

#ifdef CLIENT
Strings sa_copy(Strings const in);
Strings sa_build(String arg, va_list ap);
#endif

void    sa_release(Strings);

Strings sa_append(Strings const in, String e);
Strings sa_remove(Strings const in, String e);

size_t  sa_length(Strings const in);
int     sa_find(Strings const in, String e);

#ifdef SERVER
String  sa_fold(Strings const in, char const sep);
#endif

#endif
