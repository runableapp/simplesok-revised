# This file is part of the 'Simple Sokoban' project.
#
# MIT LICENSE
#
# Copyright (C) 2014-2025 Mateusz Viste
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.

AC_INIT([simplesok], [1.0.7])
AC_CONFIG_SRCDIR([simplesok.c])
AC_LANG(C)
AM_INIT_AUTOMAKE([dist-xz])

AC_CONFIG_HEADERS([config.h])

: ${CFLAGS=""}		# Do not use autotools default.
AC_PROG_CC

AC_CHECK_HEADERS([dirent.h errno.h stddef.h stdio.h stdlib.h string.h])
AC_CHECK_HEADERS([time.h unistd.h sys/stat.h])

AC_TYPE_SIZE_T
AC_TYPE_UINT32_T

AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([memset strdup strerror strstr])

dnl ----------------------------------------------------------------------------
dnl			Native Windows target check
dnl ----------------------------------------------------------------------------

AC_PATH_PROG(WINDRES, [windres])
AC_SUBST([WINDRES])

AC_MSG_CHECKING([if target is native Windows])
NATIVE_WINDOWS=no
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <windows.h>]], [[
#if !defined(__MINGW32__) && !defined(__MINGW32CE__) &&			\
   !(defined(_MSC_VER) && (defined(_WIN32) || defined(_WIN64)))
#error Not native Windows
#endif
  int dummy = 2 * WINVER;
]])], [
	test "${WINDRES}" || AC_MSG_ERROR([windres not found])
	NATIVE_WINDOWS=yes
])
AC_MSG_RESULT(${NATIVE_WINDOWS})
AM_CONDITIONAL([NATIVE_WINDOWS], [test "${NATIVE_WINDOWS}" = yes])

dnl ----------------------------------------------------------------------------
dnl				package configuration
dnl ----------------------------------------------------------------------------

AC_ARG_WITH(pkgdatadir,
            AS_HELP_STRING([--with-pkgdatadir=DIR, package data path]),
            [pkgdatadir=$withval],
            [pkgdatadir="${datadir}/simplesok"]
)
# Expand references.
prefix_save="${prefix}"
exec_prefix_save="${exec_prefix}"
test "${prefix}" = NONE && prefix="${ac_default_prefix}"
test "${exec_prefix}" = NONE && exec_prefix="${prefix}"
while true
do	new_pkgdatadir="`eval echo \\"\"${pkgdatadir}\"\\"`"
	test "${pkgdatadir}" = "${new_pkgdatadir}" && break
	pkgdatadir="${new_pkgdatadir}"
done
prefix="${prefix_save}"
exec_prefix="${exec_prefix_save}"
AC_DEFINE_UNQUOTED([PKGDATADIR], ["${pkgdatadir}"], [Package data path])
PACKAGE_YEAR=`date '+%Y'`
AC_SUBST([PACKAGE_YEAR])
AC_DEFINE_UNQUOTED([PACKAGE_YEAR], ${PACKAGE_YEAR}, [Last copyright year])

dnl ----------------------------------------------------------------------------
dnl				gzip compression tool
dnl ----------------------------------------------------------------------------

AC_CHECK_PROG(GZIPIT, [zopfli], [zopfli --gzip])
AC_CHECK_PROG(GZIPIT, [gzip], [gzip -k])
test -z "${GZIPIT}" && {
				AC_MSG_WARN([No gzip compression tool found])
				GZIPIT=:
			}


dnl ----------------------------------------------------------------------------
dnl				zlib configuration
dnl ----------------------------------------------------------------------------

PKG_CHECK_MODULES(ZLIB, [zlib],,
		  AC_MSG_ERROR([zlib development files not found]))

dnl ----------------------------------------------------------------------------
dnl				SDL configuration
dnl ----------------------------------------------------------------------------

sdl_version=
AC_ARG_WITH(sdl,
	    AS_HELP_STRING([--with-sdl=<version>], [Force SDL version to use]),
	    [
		case "${withval}" in
		2|3)
			sdl_version="$withval"
			;;
		auto|'')
			;;
		*)	AC_MSG_ERROR([SDL supported versions: auto, 2, 3])
		esac
	    ]
)
WITH_SDL=
AS_IF([test "${sdl_version}" != 2], [
      PKG_CHECK_MODULES(SDL, [sdl3 sdl3-image], [
	WITH_SDL=3
      ], [:])
])
AS_IF([test "${sdl_version}" != 3 -a -z "${WITH_SDL}"], [
      PKG_CHECK_MODULES(SDL, [sdl2 SDL2_image], [WITH_SDL=2], [:])
])
AS_IF([test -z "${WITH_SDL}"], [
      AC_MSG_ERROR([SDL development files not found])
])
AC_DEFINE_UNQUOTED([WITH_SDL], ${WITH_SDL}, [SDL version to use])

WITH_SOFTWARE_RENDERER=0
AC_ARG_WITH(software-renderer,
	    AS_HELP_STRING([--with-software-renderer],
			   [Do not use graphics hardware acceleration]),
	    [
		case "${withval}" in
		yes|'')	WITH_SOFTWARE_RENDERER=1
			;;
		esac
	    ]
)
AC_DEFINE_UNQUOTED([WITH_SOFTWARE_RENDERER], ${WITH_SOFTWARE_RENDERER},
		   [Use software renderer])

dnl ----------------------------------------------------------------------------
dnl				libcurl configuration
dnl ----------------------------------------------------------------------------

has_libcurl=
PKG_CHECK_MODULES(LIBCURL, [libcurl], [has_libcurl=yes])
AC_ARG_ENABLE(libcurl, AS_HELP_STRING([--disable-libcurl],
				      [Don't use libcurl for internet access]),,
	      enable_libcurl="${has_libcurl}")
if test "${enable_libcurl}" != yes
then	AC_CHECK_HEADERS([netdb.h])
	AC_CHECK_FUNCS([gethostbyname socket])
	AC_MSG_WARN([libcurl is disabled: redirection and https not supported])
elif test "${has_libcurl}" != yes
then	AC_MSG_ERROR([libcurl not found])
fi
AM_CONDITIONAL([DISABLE_LIBCURL], [test "${enable_libcurl}" != yes])

dnl ----------------------------------------------------------------------------
dnl			packging specification support
dnl ----------------------------------------------------------------------------

RPM_VERSION="${VERSION%%-*}"
RPM_RELEASE="${VERSION:${#RPM_VERSION}}"
RPM_RELEASE="${RPM_RELEASE/#-/.}"
RPM_DATE=`date '+%a %b %e %Y'`
RPM_USER=`whoami`
RPM_FULLNAME=`getent passwd "${RPM_USER}" | cut -d ':' -f 5 | cut -d ',' -f 1`
RPM_HOST=`uname -n`
RPM_KIND='upstream release'
test -n "${RPM_RELEASE}" && RPM_KIND='snapshot'
AC_SUBST([RPM_VERSION])
AC_SUBST([RPM_RELEASE])
AC_SUBST([RPM_DATE])
AC_SUBST([RPM_FULLNAME])
AC_SUBST([RPM_USER])
AC_SUBST([RPM_HOST])
AC_SUBST([RPM_KIND])


AC_CONFIG_FILES([
	Makefile							\
	skins/Makefile							\
	simplesok.6							\
	pkgbuild/rpm/simplesok.spec					\
	pkgbuild/rpm_fedora/simplesok.spec				\
])
AC_OUTPUT
