#second is version number
# third is bug report address
AC_INIT([cortx-motr-app], [1.0])

# i.e. this program doesn't follow the gnu coding standards, and doesn't have
# ChangeLog, COPYING, AUTHORS, INSTALL, README etc. files.
AM_INIT_AUTOMAKE([-Wall -Werror -Wno-portability  foreign])


#
# Checking for Platform support -------------------------------------------{{{1
#

ARCH=
AC_CANONICAL_HOST
AS_CASE([$host_cpu],
        [aarch64], [ARCH=aarch64],
        [x86_64], [ARCH=x86_64],
        [AC_MSG_ERROR("Only X86_64 and aarch64 are platforms supported")]
)


castxml=`which castxml`
src_dir_exist=`test -d "/usr/include/motr"`
M0_SRC_DIR=
AS_IF([test x$src_dir_exist = x1],
   [
    M0_SRC_DIR="/usr/include/motr"
   ],[ M0_SRC_DIR=]
)    


AM_CONDITIONAL(M0_SRC_DIR_EXIST, [test "x$src_dir_exist" = "x1"])
AM_CONDITIONAL(CASTXML, [test "x$castxml" != "x"])

PAGE_SIZE=$(getconf PAGE_SIZE)
PAGE_SHIFT=$(( $((PAGE_SIZE==4096)) ? 12 :  $(($((PAGE_SIZE==65536)) ? 16 :  14)) ))
M0_CFLAGS="-DPAGE_SIZE=$PAGE_SIZE"UL

AC_SUBST(M0_CFLAGS)
AC_SUBST(M0_SRC_DIR)
# Check for C compiler
AC_PROG_CC

# AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile])

# Generate the output
AC_OUTPUT
