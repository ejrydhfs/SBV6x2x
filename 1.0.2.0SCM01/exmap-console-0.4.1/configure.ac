AC_PREREQ(2.53)
AC_INIT([exmap-console], 0.4.1, [tf@o-hand.com])
AC_CONFIG_SRCDIR([src/exmap.c])

AM_INIT_AUTOMAKE()

# version of the exmap packege from which we are using the 
# kernel module
EXMAP_VERSION=0.9
EXMAP_PROTOCOL_VERSION=0.1

AC_ARG_WITH(readline, AS_HELP_STRING([--with-readline], 
                                   [With readline for interactive mode]),
              [WITH_READLINE=$enableval], [WITH_READLINE=yes])

WORKING_READLINE=no
# these b*****ds cannot be called conditionally as they do AM_CONDITIONAL
AC_CHECK_LIB(readline, readline, [WORKING_READLINE=yes], [WORKING_READLINE=no])
AC_CHECK_LIB(history,add_history,[WORKING_READLINE=yes],[WORKING_READLINE=no])

if test x$WITH_READLINE = xyes ; then
   if test x$cross_compiling != xyes ; then
      if test x$WORKING_READLINE != xyes ; then
	 WITH_READLINE=error
      fi
   fi
fi

AM_CONDITIONAL(INTERACTIVE, test "x$WITH_READLINE" = "xyes")

if test "x$WITH_READLINE" = "xyes" ; then
   READLINE_LIBS="-lreadline -lhistory"
   READLINE_CFLAGS="-DINTERACTIVE"
fi

AC_SUBST(READLINE_LIBS)
AC_SUBST(READLINE_CFLAGS)

AC_ARG_ENABLE(doc, AS_HELP_STRING([--disable-doc], 
                                   [Do not build documentation]),
              [DO_DOCS=$enableval], [DO_DOCS=$enableval])

AC_ARG_WITH(help2man, AS_HELP_STRING([--without-help2man], 
                                   [Rebuild manpage using help2doc]),
              [WITH_H2MAN=$withval], [WITH_H2MAN=yes])

AM_CONDITIONAL(DODOCS, test "x$DO_DOCS" != "xno")

AC_ARG_ENABLE(debug, AS_HELP_STRING([--enable-debug], 
                                   [Enable debuggin information]),
              [CFLAGS="-g -O0"], [CFLAGS="-g -O2"])

CFLAGS="$CFLAGS -DEXMAP_VERSION=\\\"$EXMAP_VERSION\\\" -DEXMAP_PROTOCOL_VERSION=\\\"$EXMAP_PROTOCOL_VERSION\\\""

AC_SUBST(CFLAGS)

AC_PROG_CC

PKG_CHECK_MODULES(GLIB, glib-2.0)
AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GLIB_LIBS)

HELPTOMAN=no

if test "x$cross_compiling" != "xyes" && test "x$DO_DOCS" != "xno" ; then
   if test x$WITH_H2MAN = xyes ; then
      #autodetect, error if not found
      AC_PATH_PROG(HELPTOMAN, help2man, no)
      if test "x$HELPTOMAN" = "xno" ; then
	 AC_MSG_ERROR([help2man not found -- either install it or use --without-help2man or --disable-doc])
      fi
   else
      if test x$WITH_H2MAN = xno ; then
	 HELPTOMAN=no
      else
	 #check that the passed in argument point to a program that exists
	 H2M_PATH=${WITH_H2MAN%/*}
	 H2M_PROG=${WITH_H2MAN##*/}
	 if test x$H2M_PATH = x$WITH_H2MAN ; then
	     AC_PATH_PROG(HELPTOMAN, [$H2M_PROG], no)
	 else
	     AC_PATH_PROG(HELPTOMAN, [$H2M_PROG], no, [$H2M_PATH])
	 fi

	 if test "x$HELPTOMAN" = "xno" ; then
	    AC_MSG_ERROR([$WITH_H2MAN not found])
         fi
      fi
   fi
fi

AM_CONDITIONAL(HAVE_HELPTOMAN, test "x$HELPTOMAN" != "xno")
AC_SUBST(HELPTOMAN)

if test "x$GCC" = "xyes"; then
        GCC_CFLAGS="-g -Wall"
fi

AC_SUBST(GCC_CFLAGS)

AC_CONFIG_FILES([Makefile
 src/Makefile
 doc/Makefile
])

AC_OUTPUT

if test x$WITH_READLINE = xerror ; then
   AC_MSG_RESULT([WARNING: Could not link against readline libraries; interactive mode will not be available.])
fi

AC_MSG_RESULT([Type make to build exmap-console.])
