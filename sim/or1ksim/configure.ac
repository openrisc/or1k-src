dnl Process this file with autoconf to produce a configure script.
AC_PREREQ(2.59)dnl
AC_INIT(Makefile.in)

sinclude(../common/acinclude.m4)

SIM_AC_COMMON

# Invoke the autoconf macros most often used by the simulators
# README_HACKING says to use SIM_AC_COMMON, but that breaks in autoconf 2.59
sinclude(../common/common.m4)

# Option to explicitly specify the location of the Or1ksim installation
AC_ARG_WITH([or1ksim],
            [AS_HELP_STRING([--with-or1ksim=PATH],
            [specify the Or1ksim installation directory explicitly (otherwise use default search paths)])],
            [],
            [])

if test "x$enable_or1ksim" != xno; then

  if test "x$with_or1ksim" != x
  then
    SIM_CPPFLAGS="$SIM_CPPFLAGS -I$with_or1ksim/include"
    SIM_LDFLAGS="$SIM_LDFLAGS -Wl,-rpath -Wl,$with_or1ksim/lib -L$with_or1ksim/lib"
  else
    SIM_CPPFLAGS="$SIM_CPPFLAGS -I/opt/or1ksim/include"
    SIM_LDFLAGS="$SIM_LDFLAGS -Wl,-rpath -Wl,/opt/or1ksim/lib -L/opt/or1ksim/lib"
  fi

fi

# No target specific stuff as yet...
AC_CHECK_HEADERS(stdlib.h unistd.h stdint.h)

# Set up simulator output
AC_SUBST(SIM_CPPFLAGS)
AC_SUBST(SIM_LDFLAGS)

SIM_AC_OUTPUT
