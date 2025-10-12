# Comments for autoconf: use dnl for comments, not as a macro
AC_CONFIG_MACRO_DIR([m4])
m4_define([WINE_VERSION], regexp(m4_include([VERSION]), [version \([-.0-9A-Za-z]+\)], [\1]))

# Fallback for missing macros (no-op if not found)
m4_ifndef([AS_VAR_APPEND], [m4_define([AS_VAR_APPEND], [])])
m4_ifndef([AS_VAR_IF], [m4_define([AS_VAR_IF], [])])

AC_PREREQ([2.63b])
AC_INIT([Wine],[WINE_VERSION],[wine-devel@winehq.org],[wine],[https://www.winehq.org])
AC_CONFIG_SRCDIR([server/atom.c])
AC_CONFIG_HEADERS([include/config.h])
AC_CONFIG_AUX_DIR([tools])

libwine_version="1.0"

AM_INIT_AUTOMAKE([foreign subdir-objects])
LT_INIT
dnl Check for flex
AC_CHECK_PROGS(FLEX,flex,none)
if test "$FLEX" = "none"
then
  AC_MSG_ERROR([no suitable flex found. Please install the 'flex' package.])
fi
AC_CACHE_CHECK([whether flex is recent enough],wine_cv_recent_flex,
    [cat >conftest.l <<EOF
%top{
#include "prediluvian.h"
}
%%
EOF
    if $FLEX -t conftest.l >/dev/null 2>&AS_MESSAGE_LOG_FD
    then
      wine_cv_recent_flex=yes
    else
      wine_cv_recent_flex=no
    fi])
test $wine_cv_recent_flex != no || AC_MSG_ERROR([Your flex version is too old. Please install flex version 2.5.33 or newer.])

dnl Check for bison
AC_CHECK_PROGS(BISON,bison,none)
if test "$BISON" = "none"
then
  AC_MSG_ERROR([no suitable bison found. Please install the 'bison' package.])
fi
AC_CACHE_CHECK([whether bison is recent enough],wine_cv_recent_bison,
    [cat >conftest.y <<EOF
%define parse.error verbose
%%
empty: ;
EOF
    if $BISON conftest.y >/dev/null 2>&AS_MESSAGE_LOG_FD
    then
      wine_cv_recent_bison=yes
    else
      wine_cv_recent_bison=no
    fi])
test $wine_cv_recent_bison != no || AC_MSG_ERROR([Your bison version is too old. Please install bison version 3.0 or newer.])

AC_CHECK_TOOL(STRIP,strip,strip)
AC_PROG_LN_S
WINE_PATH_PKG_CONFIG

if test "x$enable_maintainer_mode" != "xyes"
then
  FONTFORGE=""
  RSVG=""
  CONVERT=""
  ICOTOOL=""
else
  test "$srcdir" = . || AC_MSG_ERROR([Maintainer mode cannot work out of tree.])
  AC_CHECK_PROGS(FONTFORGE, fontforge, false)
  AC_CHECK_PROGS(RSVG, rsvg-convert rsvg, false)
  AC_CHECK_PROGS(CONVERT, convert, false)
  AC_CHECK_PROGS(ICOTOOL, icotool, false)
  test "$FONTFORGE" != "false" || AC_MSG_ERROR([You need fontforge to rebuild fonts in maintainer mode.])
  test "$RSVG" != "false" || AC_MSG_ERROR([You need rsvg to rebuild icons in maintainer mode.])

  dnl Check the imagemagick version
  if test "$CONVERT" = false
  then
    AC_MSG_ERROR([You need imagemagick to rebuild icons in maintainer mode.])
  else
    AC_MSG_CHECKING([for recent enough imagemagick])
    convert_version=`convert --version | head -n1`
    if test "x$convert_version" != "x"
    then
      convert_version_major=`expr "$convert_version" : '.* \([[0-9]]*\)\.[[0-9]]*'`
      convert_version_minor=`expr "$convert_version" : '.* [[0-9]]*\.\([[0-9]]*\)'`
      if test "$convert_version_major" -eq 6 -a "$convert_version_minor" -lt 6
      then
        CONVERT=false
      fi
    fi
    if test "$CONVERT" = false
    then
      AC_MSG_RESULT([no ($convert_version_major.$convert_version_minor)])
      AC_MSG_ERROR([You need imagemagick version 6.6 or newer to rebuild icons in maintainer mode.])
    else
      AC_MSG_RESULT([yes ($convert_version_major.$convert_version_minor)])
    fi
  fi

  dnl Check the icotool version
  if test "$ICOTOOL" = false
  then
    AC_MSG_ERROR([You need icotool to rebuild icons in maintainer mode.])
  else
    AC_MSG_CHECKING([for recent enough icotool])
    icotool_version=`icotool --version | head -n1`
    if test "x$icotool_version" != "x"
    then
      icotool_version_major=`expr "$icotool_version" : '.* \([[0-9]]*\)\.[[0-9]]*'`
      icotool_version_minor=`expr "$icotool_version" : '.* [[0-9]]*\.\([[0-9]]*\)'`
      if test "$icotool_version_major" -eq 0 -a "$icotool_version_minor" -lt 29
      then
        ICOTOOL=false
        WINE_WARNING([icotool version 0.29.0 or newer is needed to rebuild icons.])
      fi
    fi
    if test "$ICOTOOL" = false
    then
      AC_MSG_RESULT([no ($icotool_version_major.$icotool_version_minor)])
      AC_MSG_ERROR([You need icotool version 0.29.0 or newer to rebuild icons in maintainer mode.])
    else
      AC_MSG_RESULT([yes ($icotool_version_major.$icotool_version_minor)])
    fi
  fi

  dnl Maintainer mode requires gettext
  with_gettext=yes
  with_gettextpo=yes

  dnl Maintainer mode wants -Werror
  enable_werror=yes
fi

if test "x$with_gettext" != xno
then
  AC_CHECK_PROGS(MSGFMT, msgfmt, false)
  if test "$MSGFMT" != "false"
  then
    AC_CACHE_CHECK([whether msgfmt supports contexts],wine_cv_msgfmt_contexts,
    [cat >conftest.po <<EOF
# comment
msgctxt "ctxt"
msgid "id"
msgstr "str"
EOF
    if $MSGFMT -o /dev/null conftest.po 2>&AS_MESSAGE_LOG_FD
    then
      wine_cv_msgfmt_contexts=yes
    else
      wine_cv_msgfmt_contexts=no
    fi])
    test $wine_cv_msgfmt_contexts != no || MSGFMT=false
  fi
else
  MSGFMT=false
fi
WINE_WARNING_WITH(gettext,[test "$MSGFMT" = false],
                  [gettext tools not found (or too old), translations won't be built.],
                  [enable_po])

dnl **** Check for some libraries ****

dnl Check for -li386 for NetBSD and OpenBSD
AC_CHECK_LIB(i386,i386_set_ldt,[AC_SUBST(I386_LIBS, "-li386")])

AC_SUBST(OPENGL_LIBS,"")

dnl **** Check for header files ****

AC_SYS_LARGEFILE()
m4_ifdef([AC_SYS_YEAR2038],
         [AS_IF(false,[AC_SYS_YEAR2038()])], dnl bypass misguided autoconf error on wow64 builds
         [test "$ac_cv_sys_file_offset_bits" = 64 && AC_DEFINE(_TIME_BITS,64,[Define to 64 to enable 64-bit time_t])])

AC_CHECK_HEADERS(\
        CL/cl.h \
        EGL/egl.h \
        OpenCL/opencl.h \
        arpa/inet.h \
        arpa/nameser.h \
        asm/termbits.h \
        asm/types.h \
        asm/user.h \
        elf.h \
        gettext-po.h \
        link.h \
        linux/cdrom.h \
        linux/filter.h \
        linux/futex.h \
        linux/hdreg.h \
        linux/hidraw.h \
        linux/input.h \
        linux/ioctl.h \
        linux/major.h \
        linux/param.h \
        linux/seccomp.h \
        linux/serial.h \
        linux/types.h \
        linux/ucdrom.h \
        linux/wireless.h \
        lwp.h \
        mach-o/loader.h \
        mach/mach.h \
        machine/cpu.h \
        machine/sysarch.h \
        mntent.h \
        netdb.h \
        netinet/in.h \
        netinet/in_systm.h \
        netinet/tcp.h \
        netinet/tcp_fsm.h \
        pcap/pcap.h \
        port.h \
        pwd.h \
        sched.h \
        scsi/scsi.h \
        scsi/scsi_ioctl.h \
        scsi/sg.h \
        sys/attr.h \
        sys/auxv.h \
        sys/cdio.h \
        sys/epoll.h \
        sys/event.h \
        sys/eventfd.h \
        sys/extattr.h \
        sys/filio.h \
        sys/ipc.h \
        sys/link.h \
        sys/modem.h \
        sys/mtio.h \
        sys/param.h \
        sys/prctl.h \
        sys/ptrace.h \
        sys/queue.h \
        sys/random.h \
        sys/resource.h \
        sys/scsiio.h \
        sys/shm.h \
        sys/signal.h \
        sys/socketvar.h \
        sys/sockio.h \
        sys/statvfs.h \
        sys/strtio.h \
        sys/syscall.h \
        sys/sysinfo.h \
        sys/times.h \
        sys/uio.h \
        sys/user.h \
        sys/utsname.h \
        sys/vnode.h \
        sys/xattr.h \
        syscall.h \
        utime.h \
        valgrind/memcheck.h \
        valgrind/valgrind.h
)
AC_HEADER_MAJOR()

dnl **** Checks for headers that depend on other ones ****

AC_CHECK_HEADERS([sys/conf.h sys/mount.h sys/statfs.h sys/user.h sys/vfs.h],,,
    [#include <sys/types.h>
     #ifdef HAVE_SYS_PARAM_H
     # include <sys/param.h>
     #endif])

dnl check for broken Linux sysctl.h that causes deprecation warnings
saved_sysctl_h_CFLAGS=$CFLAGS
test "x${GCC}" != xyes || CFLAGS="$CFLAGS -Werror"
AC_CHECK_HEADERS([sys/sysctl.h],,,
    [#include <sys/types.h>
     #ifdef HAVE_SYS_PARAM_H
     # include <sys/param.h>
     #endif])
CFLAGS=$saved_sysctl_h_CFLAGS

AC_CHECK_HEADERS(\
        netinet/ip.h \
        net/if.h \
        net/if_arp.h \
        net/if_dl.h \
        net/if_types.h \
        net/route.h \
        netinet/if_ether.h \
        netinet/in_pcb.h \
        netinet/ip_icmp.h \
        netinet/ip_var.h \
        netinet/udp.h \
        netinet6/ip6_var.h \
        netipx/ipx.h \
        sys/un.h \
,,,[#include <sys/types.h>
     #include <sys/socket.h>
     #ifdef HAVE_SYS_SOCKETVAR_H
     # include <sys/socketvar.h>
     #endif
     #ifdef HAVE_NET_ROUTE_H
     # include <net/route.h>
     #endif
     #ifdef HAVE_NETINET_IN_H
     # include <netinet/in.h>
     #endif
     #ifdef HAVE_NETINET_IN_SYSTM_H
     # include <netinet/in_systm.h>
     #endif
     #ifdef HAVE_NET_IF_H
     # include <net/if.h>
     #endif
     #ifdef HAVE_NETINET_IP_H
     # include <netinet/ip.h>
     #endif])

AC_CHECK_HEADERS([netinet/udp_var.h netinet/icmp_var.h netinet/icmp6.h netinet/tcp_var.h ],,,
    [#include <sys/types.h>
     #include <sys/socket.h>
     #ifdef HAVE_SYS_SOCKETVAR_H
     # include <sys/socketvar.h>
     #endif
     #ifdef HAVE_NETINET_IN_H
     # include <netinet/in.h>
     #endif
     #ifdef HAVE_NETINET_IN_SYSTM_H
     # include <netinet/in_systm.h>
     #endif
     #ifdef HAVE_NETINET_IP_H
     # include <netinet/ip.h>
     #endif
     #ifdef HAVE_NETINET_IP_VAR_H
     # include <netinet/ip_var.h>
     #endif
     #ifdef HAVE_NETINET_IP_ICMP_H
     # include <netinet/ip_icmp.h>
     #endif
     #ifdef HAVE_NETINET_UDP_H
     # include <netinet/udp.h>
     #endif
     #ifdef HAVE_NETINET_TCP_H
     # include <netinet/tcp.h>
     #endif])

AC_CHECK_HEADERS([linux/ipx.h linux/irda.h linux/rtnetlink.h],,,
    [#include <sys/types.h>
     #include <sys/socket.h>
     #ifdef HAVE_ASM_TYPES_H
     # include <asm/types.h>
     #endif
     #ifdef HAVE_LINUX_TYPES_H
     # include <linux/types.h>
     #endif])

AC_CHECK_HEADERS([resolv.h],,,
    [#include <sys/types.h>
     #include <sys/socket.h>
     #ifdef HAVE_NETINET_IN_H
     # include <netinet/in.h>
     #endif
     #ifdef HAVE_ARPA_NAMESER_H
     # include <arpa/nameser.h>
     #endif])

AC_CHECK_HEADERS([ifaddrs.h],,,[#include <sys/types.h>])

AC_CHECK_HEADERS(sys/ucontext.h,,,[#include <signal.h>])

AC_CHECK_HEADERS([sys/thr.h],,,
[#include <sys/types.h>
#ifdef HAVE_SYS_UCONTEXT_H
#include <sys/ucontext.h>
#endif])

AC_CHECK_HEADERS([pthread_np.h],,,[#include <pthread.h>])

AC_CHECK_HEADERS([linux/videodev2.h],,,
[#include <sys/time.h>
#include <sys/types.h>
#ifdef HAVE_ASM_TYPES_H
#include <asm/types.h>
#endif])

AC_CHECK_HEADERS([libprocstat.h],,,
[#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#include <sys/socket.h>
#ifdef HAVE_SYS_QUEUE_H
#include <sys/queue.h>
#endif])

if test "x$ac_cv_header_sys_xattr_h" = xyes
then
    AC_CACHE_CHECK([whether getxattr takes additional arguments], wine_cv_xattr_extra_args,
        [AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <sys/xattr.h>]], [[getxattr("", "", "", 0, 0, 0);]])],
                           [wine_cv_xattr_extra_args=yes],[wine_cv_xattr_extra_args=no])])
    test $wine_cv_xattr_extra_args != yes || AC_DEFINE(XATTR_ADDITIONAL_OPTIONS, 1, [Define if xattr functions take additional arguments (macOS)])
fi

dnl **** Check for working dll ****

AC_SUBST(DLLFLAGS,"")
AC_SUBST(LDDLLFLAGS,"")
AC_SUBST(LDEXECFLAGS,"")
AC_SUBST(EXTRACFLAGS,"")
AC_SUBST(UNIXDLLFLAGS,"-fPIC")
AC_SUBST(UNIXLDFLAGS,["-shared -Wl,-Bsymbolic -Wl,-soname,\$(UNIXLIB)"])
AC_SUBST(TOP_INSTALL_LIB,"")
AC_SUBST(TOP_INSTALL_DEV,"")
AC_SUBST(WINELOADER_LDFLAGS,"")
AC_SUBST(WINEPRELOADER_LDFLAGS,"")
AC_SUBST(DLLEXT,".so")
LIBEXT="so"
WINE_PATH_SONAME_TOOLS
WINE_CONFIG_HELPERS

case $host_os in
  cygwin*|mingw32*)
    LIBEXT="dll"
    DLLEXT=""
    EXTRACFLAGS="-D__WINE_PE_BUILD"
    dnl Disable modules that can't be used on Windows
    enable_loader=${enable_loader:-no}
    enable_server=${enable_server:-no}
    dnl Disable dependencies that are not useful on Windows
    with_x=${with_x:-no}
    with_pthread=${with_pthread:-no}
    ;;

  darwin*|macosx*)
    LIBEXT="dylib"
    DLLFLAGS="$DLLFLAGS -fPIC"
    LDDLLFLAGS="-fPIC"
    enable_winemac_drv=${enable_winemac_drv:-yes}
    dnl declare needed frameworks
    AC_SUBST(CARBON_LIBS,"-framework Carbon")
    AC_SUBST(COREFOUNDATION_LIBS,"-framework CoreFoundation")
    AC_SUBST(DISKARBITRATION_LIBS,"-framework DiskArbitration -framework CoreFoundation")
    AC_SUBST(IOKIT_LIBS,"-framework IOKit -framework CoreFoundation")
    AC_SUBST(METAL_LIBS,"-framework Metal")
    AC_SUBST(APPLICATIONSERVICES_LIBS,"-framework ApplicationServices")
    AC_SUBST(CORESERVICES_LIBS,"-framework CoreServices")
    AC_SUBST(APPKIT_LIBS,"-framework AppKit")
    AC_SUBST(SECURITY_LIBS,"-framework Security -framework CoreFoundation")
    AC_SUBST(SYSTEMCONFIGURATION_LIBS,"-framework SystemConfiguration")

    WINELOADER_LDFLAGS="-Wl,-segalign,0x1000,-pagezero_size,0x1000,-sectcreate,__TEXT,__info_plist,loader/wine_info.plist"

    case $HOST_ARCH in
      i386) wine_use_preloader=yes ;;
      x86_64)
        dnl If the -no_huge linker option is present (added in Xcode 15.3), use zerofill sections instead of the preloader
        WINE_TRY_CFLAGS([-Wl,-no_huge],
           [wine_use_preloader=no
            WINELOADER_LDFLAGS="$WINELOADER_LDFLAGS -Wl,-no_pie,-image_base,0x200000000,-no_huge,-no_fixup_chains,-segalign,0x1000,-segaddr,WINE_RESERVE,0x1000,-segaddr,WINE_TOP_DOWN,0x7ff000000000"],
           [wine_use_preloader=yes])
        ;;
      *)    wine_use_preloader=no ;;
    esac

    if test "$wine_use_preloader" = "yes"
    then
        WINEPRELOADER_LDFLAGS="-nostartfiles -nodefaultlibs -e _start -ldylib1.o -mmacosx-version-min=10.7 -Wl,-no_new_main,-segalign,0x1000,-pagezero_size,0x1000,-sectcreate,__TEXT,__info_plist,loader/wine_info.plist"
        WINE_TRY_CFLAGS([-Wl,-no_pie],
                        [WINEPRELOADER_LDFLAGS="-Wl,-no_pie $WINEPRELOADER_LDFLAGS"])
        case $HOST_ARCH in
          i386)
            WINEPRELOADER_LDFLAGS="-Wl,-image_base,0x7d400000 $WINEPRELOADER_LDFLAGS"
            ;;
          x86_64)
            WINEPRELOADER_LDFLAGS="-Wl,-image_base,0x200000000,-segalign,0x1000,-segaddr,WINE_RESERVE,0x1000 $WINEPRELOADER_LDFLAGS"
            ;;
        esac
        dnl If preloader is used, the loader needs to be an LC_UNIXTHREAD binary to avoid AppKit/Core Animation problems.
        WINELOADER_LDFLAGS="$WINELOADER_LDFLAGS -mmacosx-version-min=10.7"
    fi

    if test "x$with_coreaudio" != "xno";
    then
        AC_SUBST(COREAUDIO_LIBS,"-framework CoreFoundation -framework CoreAudio -framework AudioUnit -framework AudioToolbox -framework CoreMIDI")
        enable_winecoreaudio_drv=${enable_winecoreaudio_drv:-yes}
    fi
    if test "$ac_cv_header_OpenCL_opencl_h" = "yes"
    then
        AC_SUBST(OPENCL_LIBS,"-framework OpenCL")
        ac_cv_lib_OpenCL_clGetPlatformInfo=yes
    fi

    dnl Check for MTLDevice registryID property
    AC_MSG_CHECKING([whether MTLDevice protocol supports registryID property])
    AC_LANG_PUSH([Objective C])
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <Metal/Metal.h>]], [[id<MTLDevice> device; device.registryID;]])],
                          [AC_MSG_RESULT(yes)
                           AC_DEFINE(HAVE_MTLDEVICE_REGISTRYID, 1, [Define if MTLDevice protocol has registryID property.])],
                          [AC_MSG_RESULT(no)])
    AC_LANG_POP([Objective C])

    UNIXLDFLAGS="-dynamiclib -install_name @rpath/\$(UNIXLIB) -Wl,-rpath,@loader_path\/"
    AC_SUBST(WINELOADER_DEPENDS,"wine_info.plist")
    ;;

  linux-android*)
    AC_DEFINE(_GNU_SOURCE,1,[Define to 1 to enable GNU extensions on Linux])
    DLLFLAGS="$DLLFLAGS -fPIC"
    LDDLLFLAGS="-fPIC"
    LDEXECFLAGS="-Wl,-pie"
    enable_wineandroid_drv=${enable_wineandroid_drv:-yes}
    WINE_TRY_CFLAGS([-Wl,-z,defs],[UNIXLDFLAGS="$UNIXLDFLAGS -Wl,-z,defs"])
    WINE_TRY_CFLAGS([-fPIC -Wl,--export-dynamic],[WINELOADER_LDFLAGS="-Wl,--export-dynamic"])
    WINEPRELOADER_LDFLAGS="-static -nostartfiles -nodefaultlibs -Wl,-Ttext=0x7d400000"

    WINE_CHECK_SONAME(EGL,eglGetProcAddress)
    WINE_CHECK_SONAME(GLESv2,glFlush)

    if test "x$exec_prefix" = xNONE
    then
      case $HOST_ARCH in
        i386)    exec_prefix='${prefix}/x86' ;;
        x86_64)  exec_prefix='${prefix}/x86_64' ;;
        arm)     exec_prefix='${prefix}/armeabi-v7a' ;;
        aarch64) exec_prefix='${prefix}/arm64-v8a' ;;
      esac
    fi
    ;;

  *)
    AC_DEFINE(_GNU_SOURCE,1,[Define to 1 to enable GNU extensions on Linux])
    if test $HOST_ARCH = i386
    then
        DLLFLAGS="$DLLFLAGS -fno-PIC"
        LDDLLFLAGS="-fno-PIC"
        WINE_TRY_CFLAGS([-fno-PIC -Wl,-z,notext],[LDDLLFLAGS="$LDDLLFLAGS -Wl,-z,notext"])
    else
        DLLFLAGS="$DLLFLAGS -fPIC"
        LDDLLFLAGS="-fPIC"
    fi

    AC_CACHE_CHECK([whether -Wl,-z,defs works correctly], ac_cv_wl_z_defs,
                   [ac_save_CFLAGS="$CFLAGS"
                    CFLAGS="$CFLAGS $UNIXDLLFLAGS $UNIXLDFLAGS -Wl,-z,defs"
                    dnl On FreeBSD, shared libraries using environ fail to link with -Wl,-z,defs
                    AC_LINK_IFELSE([AC_LANG_SOURCE([[extern char **environ; char **envp; void myfunc(void) { envp = environ; }]])],
                                                   [ac_cv_wl_z_defs=yes],[ac_cv_wl_z_defs=no])
                    CFLAGS=$ac_save_CFLAGS])
    test $ac_cv_wl_z_defs != yes || AS_VAR_APPEND([UNIXLDFLAGS],[" -Wl,-z,defs"])

    WINE_TRY_CFLAGS([-Wl,--export-dynamic],[WINELOADER_LDFLAGS="-Wl,--export-dynamic"])
    WINEPRELOADER_LDFLAGS="-nostartfiles -nodefaultlibs"

    case $host_os in
      linux*)
        AS_VAR_APPEND([WINELOADER_LDFLAGS],[" -pie"])
        case $HOST_ARCH in
          i386|arm)
            AS_VAR_APPEND([WINEPRELOADER_LDFLAGS],[" -static -Wl,-Ttext=0x7d400000"]) ;;
          *)
            WINE_TRY_CFLAGS([-static-pie],
                            [AS_VAR_APPEND([WINEPRELOADER_LDFLAGS],[" -static-pie"])],
                            [AS_VAR_APPEND([WINEPRELOADER_LDFLAGS],[" -static -Wl,-Ttext=0x7d7d00000000"])])
            ;;
        esac
        ;;
      *)
        case $HOST_ARCH in
          i386|arm)
            WINE_TRY_CFLAGS([-Wl,-Ttext-segment=0x60000000],
                            [AS_VAR_APPEND([WINELOADER_LDFLAGS],[" -Wl,-Ttext-segment=0x60000000"])],
                            [WINE_TRY_CFLAGS([-Wl,--section-start,.interp=0x60000400],
                                [AS_VAR_APPEND([WINELOADER_LDFLAGS],[" -Wl,--section-start,.interp=0x60000400"])])])
            ;;
          *)
            AS_VAR_APPEND([WINELOADER_LDFLAGS],[" -pie"])
            ;;
        esac
        ;;
    esac
    ;;
esac

enable_winecoreaudio_drv=${enable_winecoreaudio_drv:-no}
enable_wineandroid_drv=${enable_wineandroid_drv:-no}
enable_winemac_drv=${enable_winemac_drv:-no}

dnl Check for cross compiler(s)
AC_SUBST(PE_ARCHS,"")
cross_archs=
AS_VAR_SET_IF([enable_archs],
    [test "x$with_system_dllpath" = "x" || AC_MSG_ERROR("The --with-system-dllpath option is not compatible with --enable-archs")
     ac_save_IFS=$IFS
     IFS=' ,'
     set x $enable_archs
     IFS=$ac_save_IFS
     shift
     for arch
     do
         case $arch in
           i386|x86_64|arm|aarch64|arm64ec) cross_archs="$cross_archs $arch" ;;
           *) AC_MSG_ERROR([Unknown cross-compilation architecture '$arch']) ;;
         esac
     done],
    [if test "x$with_mingw" != xno
     then
         test $HOST_ARCH = unknown || cross_archs=$HOST_ARCH
     fi])

saved_CC=$CC
saved_CFLAGS=$CFLAGS
saved_CPPFLAGS=$CPPFLAGS
saved_LDFLAGS=$LDFLAGS

AS_UNSET(extra_arch)
for arch in $cross_archs
do
  case $arch in
    arm64ec) test ${extra_arch+y} || extra_arch=x86_64 ;;
    x86_64) extra_arch="" ;;
  esac
done

for wine_arch in $cross_archs $extra_arch
do
    case "x$with_mingw" in
      xclang|x*/clang) AS_VAR_SET(${wine_arch}_CC,$with_mingw) ;;
    esac
    AS_VAR_IF([${wine_arch}_CC],[],
        [case $wine_arch in
          aarch64)
            AC_CHECK_PROGS(aarch64_CC,[aarch64-w64-mingw32-clang aarch64-w64-mingw32-gcc clang],[false])
            ;;
          arm64ec)
            AC_CHECK_PROGS(arm64ec_CC,[arm64ec-w64-mingw32-clang arm64ec-w64-mingw32-gcc clang],[false])
            ;;
          arm)
            AC_CHECK_PROGS(arm_CC,[armv7-w64-mingw32-clang armv7-w64-mingw32-gcc clang],[false])
            ;;
         i386)
            ac_prefix_list="m4_foreach([cc],[gcc,clang],m4_foreach([cpu],[i686,i586,i486,i386],[cpu-w64-mingw32-cc ]))"
            AC_CHECK_PROGS(i386_CC,[$ac_prefix_list clang],[false])
            ;;
         x86_64)
            ac_prefix_list="m4_foreach([cc],[gcc,clang],m4_foreach([cpu],[x86_64,amd64],[cpu-w64-mingw32-cc ]))"
            AC_CHECK_PROGS(x86_64_CC,[$ac_prefix_list clang],[false])
            ;;
        esac])

    CPPFLAGS=""
    AS_VAR_SET_IF([${wine_arch}_CFLAGS],[],[AS_VAR_SET([${wine_arch}_CFLAGS],[${CROSSCFLAGS:-"-g -O2"}])])
    AS_VAR_SET_IF([${wine_arch}_LDFLAGS],[],[AS_VAR_SET([${wine_arch}_LDFLAGS],[$CROSSLDFLAGS])])
    AS_VAR_COPY([CC],[${wine_arch}_CC])
    AS_VAR_COPY([CFLAGS],[${wine_arch}_CFLAGS])
    AS_VAR_COPY([LDFLAGS],[${wine_arch}_LDFLAGS])
    AS_VAR_SET([${wine_arch}_EXTRACFLAGS],["-D__WINE_PE_BUILD -Wall"])

    target=""
    set x $CC
    shift
    while test $# -ge 1
    do
        case "$1" in
            */*-gcc) target=`expr "$1" : '.*/\(.*\)-gcc'` ;;
            */*-clang) target=`expr "$1" : '.*/\(.*\)-clang'` ;;
            *-gcc) target=`expr "$1" : '\(.*\)-gcc'` ;;
            *-clang) target=`expr "$1" : '\(.*\)-clang'` ;;
        esac
        shift
    done

    dnl Check if cross compiler supports -target argument
    llvm_target=$target
    target_strip=${target:-llvm}-strip
    if test -z "$llvm_target"
    then
        case $wine_arch in
        i386) llvm_target=i686-windows ;;
        arm)  llvm_target=armv7-windows ;;
        *)    llvm_target=$wine_arch-windows ;;
        esac
    fi
    llvm_extra_cflags="-target $llvm_target -fuse-ld=lld"
    llvm_extra_ldflags=""
    llvm_cflags=""
    case $llvm_target in
      *windows) llvm_cflags="-Wl,-subsystem:console -Wl,-WX" ;;
    esac
    WINE_TRY_PE_CFLAGS([$llvm_extra_cflags $llvm_cflags --no-default-config],
                       [llvm_extra_cflags="$llvm_extra_cflags --no-default-config"
                        llvm_extra_ldflags="--no-default-config"],
                       [WINE_TRY_PE_CFLAGS([$llvm_extra_cflags $llvm_cflags],[:],
                                           [llvm_extra_cflags=""])])
    if test -n "$llvm_extra_cflags"
    then
        target=$llvm_target
        AS_VAR_SET([${wine_arch}_DELAYLOADFLAG],["-Wl,-delayload,"])
        AS_VAR_APPEND([${wine_arch}_EXTRACFLAGS],[" $llvm_extra_cflags"])
        AS_VAR_APPEND([${wine_arch}_LDFLAGS],[" $llvm_extra_ldflags"])
        CFLAGS="$CFLAGS $llvm_extra_cflags $llvm_cflags"
    else
        AS_VAR_PUSHDEF([wine_cv_crosscc], [wine_cv_${wine_arch}_crosscc])
        AC_CACHE_CHECK([whether $CC works], wine_cv_crosscc,
            [AC_COMPILE_IFELSE([AC_LANG_PROGRAM([])], [AS_VAR_SET([wine_cv_crosscc],[yes])], [AS_VAR_SET([wine_cv_crosscc],[no])])])
        AS_VAR_IF([wine_cv_crosscc], [no], [AS_VAR_SET([target],[])])
        AS_VAR_POPDEF([wine_cv_crosscc])
    fi

    if test -z "$target"
    then
        AS_VAR_SET_IF([enable_archs],
            [AC_MSG_ERROR([$wine_arch PE cross-compiler not found.
This is an error since --enable-archs=$wine_arch was requested.])])
        continue
    fi

    AS_VAR_SET([${wine_arch}_TARGET],[$target])
    AS_VAR_SET([${wine_arch}_STRIP],[$target_strip])
    AS_VAR_PUSHDEF([wine_cv_crosscc_c99],[ac_cv_${wine_arch}_crosscc_c99])
    AC_MSG_CHECKING([for $CC option to enable C99 features])
    AC_CACHE_VAL([wine_cv_crosscc_c99],
        [AS_VAR_SET([wine_cv_crosscc_c99],[no])
         for arg in '' '-std=gnu99' '-D__STDC__'
         do
             test -z "$arg" || CC="$CC $arg"
             AC_COMPILE_IFELSE([AC_LANG_SOURCE([$ac_c_conftest_c99_program])],
                               [AS_VAR_SET([wine_cv_crosscc_c99],[$arg])],
                               [AS_VAR_SET([wine_cv_crosscc_c99],[no])])
             AS_VAR_COPY([CC],[${wine_arch}_CC])
             AS_VAR_IF([wine_cv_crosscc_c99],[no],[],[break])
         done])
    AS_VAR_COPY([res],[wine_cv_crosscc_c99])
    AS_VAR_POPDEF([wine_cv_crosscc_c99])
    case "x$res" in
      x) AC_MSG_RESULT([none needed]) ;;
      xno) AC_MSG_RESULT([unsupported])
           AS_VAR_SET_IF([enable_archs],
                [AC_MSG_ERROR([$wine_arch PE cross-compiler supporting C99 not found.
This is an error since --enable-archs=$wine_arch was requested.])])
           continue
           ;;
      x*) AC_MSG_RESULT([$res])
          AS_VAR_APPEND([${wine_arch}_CC],[" $res"]) ;;
    esac

    AS_VAR_IF([wine_arch],[i386],[],
              [AS_VAR_PUSHDEF([wine_cv_seh_support],[ac_cv_${wine_arch}_seh_support])
               AC_CACHE_CHECK([whether $CC supports SEH directives], wine_cv_seh_support,
                   AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[asm(".text\nac_test:\t.seh_proc ac_test\n\tnop\n\t.seh_stackalloc 16\n\t.seh_endprologue\n\t.seh_endproc");]])],
                                     [AS_VAR_SET([wine_cv_seh_support],[yes])],[AS_VAR_SET([wine_cv_seh_support],[no])]))
               AS_VAR_COPY([res],[wine_cv_seh_support])
               AS_VAR_POPDEF([wine_cv_seh_support])
               AS_VAR_IF([res],[yes],[],
                         [AS_VAR_SET_IF([enable_archs],
                             [AC_MSG_ERROR([The $wine_arch PE cross-compiler doesn't support SEH directives.
This is an error since --enable-archs=$wine_arch was requested.])])
                          continue])])

    AS_VAR_IF([wine_arch],[$extra_arch],[],[AS_VAR_APPEND([PE_ARCHS],[" $wine_arch"])])

    WINE_TRY_PE_CFLAGS([-fno-strict-aliasing])
    dnl clang needs to be told to fail on unknown options
    WINE_TRY_PE_CFLAGS([-Werror=unknown-warning-option],[CFLAGS="$CFLAGS -Werror=unknown-warning-option"])
    WINE_TRY_PE_CFLAGS([-Werror=ignored-optimization-argument],[CFLAGS="$CFLAGS -Werror=ignored-optimization-argument"])
    WINE_TRY_PE_CFLAGS([-Wdeclaration-after-statement])
    WINE_TRY_PE_CFLAGS([-Wempty-body])
    WINE_TRY_PE_CFLAGS([-Wignored-qualifiers])
    WINE_TRY_PE_CFLAGS([-Winit-self])
    WINE_TRY_PE_CFLAGS([-Wpacked-not-aligned],[AS_VAR_APPEND(${wine_arch}_EXTRACFLAGS,[" -Wno-packed-not-aligned"])])
    WINE_TRY_PE_CFLAGS([-Wpragma-pack],[AS_VAR_APPEND(${wine_arch}_EXTRACFLAGS,[" -Wno-pragma-pack"])])
    WINE_TRY_PE_CFLAGS([-Wmicrosoft-enum-forward-reference],[AS_VAR_APPEND(${wine_arch}_EXTRACFLAGS,[" -Wno-microsoft-enum-forward-reference"])])
    WINE_TRY_PE_CFLAGS([-Wshift-overflow=2])
    WINE_TRY_PE_CFLAGS([-Wstrict-prototypes])
    WINE_TRY_PE_CFLAGS([-Wtype-limits])
    WINE_TRY_PE_CFLAGS([-Wunused-but-set-parameter])
    WINE_TRY_PE_CFLAGS([-Wvla])
    WINE_TRY_PE_CFLAGS([-Wwrite-strings])
    WINE_TRY_PE_CFLAGS([-Wpointer-arith])
    WINE_TRY_PE_CFLAGS([-Wlogical-op])
    WINE_TRY_PE_CFLAGS([-Wabsolute-value])
    WINE_TRY_PE_CFLAGS([-Wenum-enum-conversion],[:],WINE_TRY_PE_CFLAGS([-Wenum-conversion]))
    WINE_TRY_PE_CFLAGS([-ffunction-sections])

    dnl clang had broken -fms-hotpatch support before version 18 (https://github.com/llvm/llvm-project/pull/77245)
    WINE_TRY_PE_CFLAGS([-fms-hotpatch -DMIN_CLANG_VERSION=18],
                       [AS_VAR_APPEND([${wine_arch}_EXTRACFLAGS],[" -fms-hotpatch"])
                        AS_VAR_APPEND([${wine_arch}_LDFLAGS],[" -fms-hotpatch"])])

    dnl GCC can't handle large files when -Wmisleading-indentation is enabled (https://gcc.gnu.org/bugzilla/show_bug.cgi?id=89549)
    WINE_TRY_PE_CFLAGS([-flarge-source-files -Wmisleading-indentation],[AS_VAR_APPEND(${wine_arch}_EXTRACFLAGS,[" -Wno-misleading-indentation"])])

    case $wine_arch in
      i386) WINE_TRY_PE_CFLAGS([-fno-omit-frame-pointer])
            WINE_TRY_PE_CFLAGS([-mpreferred-stack-boundary=2])
            WINE_TRY_PE_CFLAGS([-Wl,--disable-stdcall-fixup],
                               [AS_VAR_APPEND([${wine_arch}_LDFLAGS],[" -Wl,--disable-stdcall-fixup"])]) ;;
      x86_64) WINE_TRY_PE_CFLAGS([-Wformat-overflow])
              WINE_TRY_PE_CFLAGS([-Wnonnull])
              WINE_TRY_PE_CFLAGS([-mcx16])
              WINE_TRY_PE_CFLAGS([-mcmodel=small]) ;;
    esac

    dnl Determine debug info format
    wine_crossdebug=$CROSSDEBUG
    if test -z "$wine_crossdebug"
    then
      for ac_flag in $CFLAGS; do
        case $ac_flag in
         -gdwarf*)    wine_crossdebug=dwarf ;;
         -gcodeview)  wine_crossdebug=pdb ;;
         -g)          wine_crossdebug=${wine_crossdebug:-dwarf} ;;
        esac
      done
    fi

    ac_debug_format_seen=""
    for ac_flag in $CFLAGS; do
      case $ac_flag in
       -gdwarf*|-gcodeview)  ac_debug_format_seen=$ac_flag ;;
      esac
    done
    if test "x$ac_debug_format_seen" = x
    then
      case $wine_crossdebug in
        *dwarf)  dnl clang emits a warning without -Wl,-debug:dwarf, so ensure linker option is present
                 WINE_TRY_PE_CFLAGS([-Wl,-debug:dwarf],[AS_VAR_APPEND([${wine_arch}_LDFLAGS],[" -Wl,-debug:dwarf"])
                                                        CFLAGS="$CFLAGS -Wl,-debug:dwarf"])
                 WINE_TRY_PE_CFLAGS([-gdwarf-4]) ;;
        pdb)     WINE_TRY_PE_CFLAGS([-gcodeview]) ;;
      esac
    fi
    AS_VAR_SET([${wine_arch}_DEBUG],[$wine_crossdebug])

    test "x$enable_werror" != xyes || WINE_TRY_PE_CFLAGS([-Werror])
    dnl clang for PE target wants -build-id (and not --build-id as gcc/mingw or clang in unix target)
    dnl don't bother testing flags, compilation will fail afterwards
    test "x$enable_build_id" != xyes || AS_VAR_APPEND([${wine_arch}_LDFLAGS],[" -Wl,--build-id"])

done

CC=$saved_CC
CFLAGS=$saved_CFLAGS
CPPFLAGS=$saved_CPPFLAGS
LDFLAGS=$saved_LDFLAGS

case $HOST_ARCH in
  arm|aarch64)
    test "x$PE_ARCHS" != x || AC_MSG_ERROR([PE cross-compilation is required for $HOST_ARCH, please install clang/llvm-dlltool/lld, or llvm-mingw.])
    DLLEXT="" ;;
  *)
    WINE_NOTICE_WITH(mingw,[test "x$PE_ARCHS" = "x"],
                     [Suitable PE cross-compiler not found, PE files won't be built.]) ;;
esac

dnl **** External libraries ****

if test "x$with_system_dllpath" != "x" -a -n "$PE_ARCHS"
then
    WINE_MINGW_PACKAGE_FLAGS(FAUDIO,[FAudio],[-lFAudio],
        [WINE_CHECK_MINGW_HEADER(FAudio.h,
            [WINE_CHECK_MINGW_LIB(FAudio,FAudio_CommitOperationSet,[:],[FAUDIO_PE_CFLAGS=""; FAUDIO_PE_LIBS=""],[$FAUDIO_PE_LIBS])],
            [FAUDIO_PE_CFLAGS=""; FAUDIO_PE_LIBS=""])])
    if test "x$FAUDIO_PE_LIBS" = "x"
    then
        WINE_NOTICE([FAudio ${notice_platform}MinGW development files not found (or too old); using bundled version.])
    fi

    WINE_MINGW_PACKAGE_FLAGS(FLUIDSYNTH,[fluidsynth],[-lfluidsynth],
        [WINE_CHECK_MINGW_HEADER(fluidsynth.h,
            [WINE_CHECK_MINGW_LIB(fluidsynth,new_fluid_synth,[:] tl100/tests)
WINE_CONFIG_MAKEFILE(dlls/atl110)
WINE_CONFIG_MAKEFILE(dlls/atl110/tests)
WINE_CONFIG_MAKEFILE(dlls/atl80)
WINE_CONFIG_MAKEFILE(dlls/atl80/tests)
WINE_CONFIG_MAKEFILE(dlls/atl90)
WINE_CONFIG_MAKEFILE(dlls/atlthunk)
WINE_CONFIG_MAKEFILE(dlls/atlthunk/tests)
WINE_CONFIG_MAKEFILE(dlls/atmlib)
WINE_CONFIG_MAKEFILE(dlls/audioses)
WINE_CONFIG_MAKEFILE(dlls/authz)
WINE_CONFIG_MAKEFILE(dlls/avicap32)
WINE_CONFIG_MAKEFILE(dlls/avifil32)
WINE_CONFIG_MAKEFILE(dlls/avifil32/tests)
WINE_CONFIG_MAKEFILE(dlls/avifile.dll16,enable_win16)
WINE_CONFIG_MAKEFILE(dlls/avrt)
WINE_CONFIG_MAKEFILE(dlls/bcp47langs)
WINE_CONFIG_MAKEFILE(dlls/bcrypt)
WINE_CONFIG_MAKEFILE(dlls/bcrypt/tests)
WINE_CONFIG_MAKEFILE(dlls/bcryptprimitives)
WINE_CONFIG_MAKEFILE(dlls/bluetoothapis)
WINE_CONFIG_MAKEFILE(dlls/bluetoothapis/tests)
WINE_CONFIG_MAKEFILE(dlls/browseui)
WINE_CONFIG_MAKEFILE(dlls/browseui/tests)
WINE_CONFIG_MAKEFILE(dlls/bthprops.cpl)
WINE_CONFIG_MAKEFILE(dlls/cabinet)
WINE_CONFIG_MAKEFILE(dlls/cabinet/tests)
WINE_CONFIG_MAKEFILE(dlls/capi2032)
WINE_CONFIG_MAKEFILE(dlls/cards)
WINE_CONFIG_MAKEFILE(dlls/cdosys)
WINE_CONFIG_MAKEFILE(dlls/cfgmgr32)
WINE_CONFIG_MAKEFILE(dlls/cfgmgr32/tests)
WINE_CONFIG_MAKEFILE(dlls/clusapi)
WINE_CONFIG_MAKEFILE(dlls/cng.sys)
WINE_CONFIG_MAKEFILE(dlls/colorcnv)
WINE_CONFIG_MAKEFILE(dlls/combase)
WINE_CONFIG_MAKEFILE(dlls/combase/tests)
WINE_CONFIG_MAKEFILE(dlls/comcat)
WINE_CONFIG_MAKEFILE(dlls/comcat/tests)
WINE_CONFIG_MAKEFILE(dlls/comctl32)
WINE_CONFIG_MAKEFILE(dlls/comctl32/tests)
WINE_CONFIG_MAKEFILE(dlls/comdlg32)
WINE_CONFIG_MAKEFILE(dlls/comdlg32/tests)
WINE_CONFIG_MAKEFILE(dlls/coml2)
WINE_CONFIG_MAKEFILE(dlls/comm.drv16,enable_win16)
WINE_CONFIG_MAKEFILE(dlls/commdlg.dll16,enable_win16)
WINE_CONFIG_MAKEFILE(dlls/compobj.dll16,enable_win16)
WINE_CONFIG_MAKEFILE(dlls/compstui)
WINE_CONFIG_MAKEFILE(dlls/compstui/tests)
WINE_CONFIG_MAKEFILE(dlls/comsvcs)
WINE_CONFIG_MAKEFILE(dlls/comsvcs/tests)
WINE_CONFIG_MAKEFILE(dlls/concrt140)
WINE_CONFIG_MAKEFILE(dlls/concrt14dlls/winehid.sys)
WINE_CONFIG_MAKEFILE(dlls/winemac.drv)
WINE_CONFIG_MAKEFILE(dlls/winemapi)
WINE_CONFIG_MAKEFILE(dlls/wineoss.drv)
WINE_CONFIG_MAKEFILE(dlls/wineps.drv)
WINE_CONFIG_MAKEFILE(dlls/wineps16.drv16,enable_win16)
WINE_CONFIG_MAKEFILE(dlls/winepulse.drv)
WINE_CONFIG_MAKEFILE(dlls/wineusb.sys)
WINE_CONFIG_MAKEFILE(dlls/winevulkan)
WINE_CONFIG_MAKEFILE(dlls/winewayland.drv)
WINE_CONFIG_MAKEFILE(dlls/winex11.drv)
WINE_CONFIG_MAKEFILE(dlls/winexinput.sys)
WINE_CONFIG_MAKEFILE(dlls/wing.dll16,enable_win16)
WINE_CONFIG_MAKEFILE(dlls/wing32)
WINE_CONFIG_MAKEFILE(dlls/wing32/tests)
WINE_CONFIG_MAKEFILE(dlls/winhttp)
WINE_CONFIG_MAKEFILE(dlls/winhttp/tests)
WINE_CONFIG_MAKEFILE(dlls/wininet)
WINE_CONFIG_MAKEFILE(dlls/wininet/tests)
WINE_CONFIG_MAKEFILE(dlls/winmm)
WINE_CONFIG_MAKEFILE(dlls/winmm/tests)
WINE_CONFIG_MAKEFILE(dlls/winnls.dll16,enable_win16)
WINE_CONFIG_MAKEFILE(dlls/winnls32)
WINE_CONFIG_MAKEFILE(dlls/winprint)
WINE_CONFIG_MAKEFILE(dlls/winscard)
WINE_CONFIG_MAKEFILE(dlls/winscard/tests)
WINE_CONFIG_MAKEFILE(dlls/winsock.dll16,enable_win16)
WINE_CONFIG_MAKEFILE(dlls/winspool.drv)
WINE_CONFIG_MAKEFILE(dlls/winspool.drv/tests)
WINE_CONFIG_MAKEFILE(dlls/winsta)
WINE_CONFIG_MAKEFILE(dlls/wintab.dll16,enable_win16)
WINE_CONFIG_MAKEFILE(dlls/wintab32)
WINE_CONFIG_MAKEFILE(dlls/wintab32/tests)
WINE_CONFIG_MAKEFILE(dlls/wintrust)
WINE_CONFIG_MAKEFILE(dlls/wintrust/tests)
WINE_CONFIG_MAKEFILE(dlls/wintypes)
WINE_CONFIG_MAKEFILE(dlls/wintypes/tests)
WINE_CONFIG_MAKEFILE(dlls/winusb)
WINE_CONFIG_MAKEFILE(dlls/wlanapi)
WINE_CONFIG_MAKEFILE(dlls/wlanapi/tests)
WINE_CONFIG_MAKEFILE(dlls/wlanui)
WINE_CONFIG_MAKEFILE(dlls/wldap32)
WINE_CONFIG_MAKEFILE(dlls/wldap32/tests)
WINE_CONFIG_MAKEFILE(dlls/wldp)
WINE_CONFIG_MAKEFILE(dlls/wldp/tests)
WINE_CONFIG_MAKEFILE(dlls/wmadmod)
WINE_CONFIG_MAKEFILE(dlls/wmasf)
WINE_CONFIG_MAKEFILE(dlls/wmi)
WINE_CONFIG_MAKEFILE(dlls/wmilib.sys)
WINE_CONFIG_MAKEFILE(dlls/wmiutils)
WINE_CONFIG_MAKEFILE(dlls/wmiutils/tests)
WINE_CONFIG_MAKEFILE(dlls/wmp)
WINE_CONFIG_MAKEFILE(dlls/wmp/tests)
WINE_CONFIG_MAKEFILE(dlls/wmphoto)
WINE_CONFIG_MAKEFILE(dlls/wmvcore)
WINE_CONFIG_MAKEFILE(dlls/wmvcore/tests)
WINE_CONFIG_MAKEFILE(dlls/wmvdecod)
WINE_CONFIG_MAKEFILE(dlls/wnaspi32)
WINE_CONFIG_MAKEFILE(dlls/wofutil)
WINE_CONFIG_MAKEFILE(dlls/wow32,enable_win16)
WINE_CONFIG_MAKEFILE(dlls/wow64)
WINE_CONFIG_MAKEFILE(dlls/wow64cpu)
WINE_CONFIG_MAKEFILE(dlls/wow64win)
WINE_CONFIG_MAKEFILE(dlls/wpc)
WINE_CONFIG_MAKEFILE(dlls/wpc/tests)
WINE_CONFIG_MAKEFILE(dlls/wpcap)
WINE_CONFIG_MAKEFILE(dlls/wpcap/tests)
WINE_CONFIG_MAKEFILE(dlls/ws2_32)
WINE_CONFIG_MAKEFILE(dlls/ws2_32/tests)
WINE_CONFIG_MAKEFILE(dlls/wsdapi)
WINE_CONFIG_MAKEFILE(dlls/wsdapi/tests)
WINE_CONFIG_MAKEFILE(dlls/wshom.ocx)
WINE_CONFIG_MAKEFILE(dlls/wshom.ocx/tests)
WINE_CONFIG_MAKEFILE(dlls/wsnmp32)
WINE_CONFIG_MAKEFILE(dlls/wsnmp32/tests)
WINE_CONFIG_MAKEFILE(dlls/wsock32)
WINE_CONFIG_MAKEFILE(dlls/wtsapi32)
WINE_CONFIG_MAKEFILE(dlls/wtsapi32/tests)
WINE_CONFIG_MAKEFILE(dlls/wuapi)
WINE_CONFIG_MAKEFILE(dlls/wuaueng)
WINE_CONFIG_MAKEFILE(dlls/x3daudio1_0)
WINE_CONFIG_MAKEFILE(dlls/x3daudio1_1)
WINE_CONFIG_MAKEFILE(dlls/x3daudio1_2)
WINE_CONFIG_MAKEFILE(dlls/x3daudio1_3)
WINE_CONFIG_MAKEFILE(dlls/x3daudio1_4)
WINE_CONFIG_MAKEFILE(dlls/x3daudio1_5)
WINE_CONFIG_MAKEFILE(dlls/x3daudio1_6)
WINE_CONFIG_MAKEFILE(dlls/x3daudio1_7)
WINE_CONFIG_MAKEFILE(dlls/xactengine2_0)
WINE_CONFIG_MAKEFILE(dlls/xactengine2_4)
WINE_CONFIG_MAKEFILE(dlls/xactengine2_7)
WINE_CONFIG_MAKEFILE(dlls/xactengine2_9)
WINE_CONFIG_MAKEFILE(dlls/xactengine3_0)
WINE_CONFIG_MAKEFILE(dlls/xactengine3_1)
WINE_CONFIG_MAKEFILE(dlls/xactengine3_2)
WINE_CONFIG_MAKEFILE(dlls/xactengine3_3)
WINE_CONFIG_MAKEFILE(dlls/xactengine3_4)
WINE_CONFIG_MAKEFILE(dlls/xactengine3_5)
WINE_CONFIG_MAKEFILE(dlls/xactengine3_6)
WINE_CONFIG_MAKEFILE(dlls/xactengine3_7)
WINE_CONFIG_MAKEFILE(dlls/xactengine3_7/tests)
WINE_CONFIG_MAKEFILE(dlls/xapofx1_1)
WINE_CONFIG_MAKEFILE(dlls/xapofx1_2)
WINE_CONFIG_MAKEFILE(dlls/xapofx1_3)
WINE_CONFIG_MAKEFILE(dlls/xapofx1_4)
WINE_CONFIG_MAKEFILE(dlls/xapofx1_5)
WINE_CONFIG_MAKEFILE(dlls/xaudio2_0)
WINE_CONFIG_MAKEFILE(dlls/xaudio2_1)
WINE_CONFIG_MAKEFILE(dlls/xaudio2_2)
WINE_CONFIG_MAKEFILE(dlls/xaudio2_3)
WINE_CONFIG_MAKEFILE(dlls/xaudio2_4)
WINE_CONFIG_MAKEFILE(dlls/xaudio2_5)
WINE_CONFIG_MAKEFILE(dlls/xaudio2_6)
WINE_CONFIG_MAKEFILE(dlls/xaudio2_7)
WINE_CONFIG_MAKEFILE(dlls/xaudio2_7/tests)
WINE_CONFIG_MAKEFILE(dlls/xaudio2_8)
WINE_CONFIG_MAKEFILE(dlls/xaudio2_8/tests)
WINE_CONFIG_MAKEFILE(dlls/xaudio2_9)
WINE_CONFIG_MAKEFILE(dlls/xinput1_1)
WINE_CONFIG_MAKEFILE(dlls/xinput1_2)
WINE_CONFIG_MAKEFILE(dlls/xinput1_3)
WINE_CONFIG_MAKEFILE(dlls/xinput1_3/tests)
WINE_CONFIG_MAKEFILE(dlls/xinput1_4)
WINE_CONFIG_MAKEFILE(dlls/xinput9_1_0)
WINE_CONFIG_MAKEFILE(dlls/xinputuap)
WINE_CONFIG_MAKEFILE(dlls/xmllite)
WINE_CONFIG_MAKEFILE(dlls/xmllite/tests)
WINE_CONFIG_MAKEFILE(dlls/xolehlp)
WINE_CONFIG_MAKEFILE(dlls/xpsprint)
WINE_CONFIG_MAKEFILE(dlls/xpssvcs)
WINE_CONFIG_MAKEFILE(dlls/xtajit64)
WINE_CONFIG_MAKEFILE(fonts)
WINE_CONFIG_MAKEFILE(include)
WINE_CONFIG_MAKEFILE(libs/adsiid)
WINE_CONFIG_MAKEFILE(libs/capstone)
WINE_CONFIG_MAKEFILE(libs/darling)
WINE_CONFIG_MAKEFILE(libs/dmoguids)
WINE_CONFIG_MAKEFILE(libs/dxerr8)
WINE_CONFIG_MAKEFILE(libs/dxerr9)
WINE_CONFIG_MAKEFILE(libs/dxguid)
WINE_CONFIG_MAKEFILE(libs/faudio)
WINE_CONFIG_MAKEFILE(libs/fluidsynth)
WINE_CONFIG_MAKEFILE(libs/gsm)
WINE_CONFIG_MAKEFILE(libs/jpeg)
WINE_CONFIG_MAKEFILE(libs/jxr)
WINE_CONFIG_MAKEFILE(libs/lcms2)
WINE_CONFIG_MAKEFILE(libs/ldap)
WINE_CONFIG_MAKEFILE(libs/mfuuid)
WINE_CONFIG_MAKEFILE(libs/mpg123)
WINE_CONFIG_MAKEFILE(libs/musl)
WINE_CONFIG_MAKEFILE(libs/png)
WINE_CONFIG_MAKEFILE(libs/strmbase)
WINE_CONFIG_MAKEFILE(libs/strmiids)
WINE_CONFIG_MAKEFILE(libs/tiff)
WINE_CONFIG_MAKEFILE(libs/tomcrypt)
WINE_CONFIG_MAKEFILE(libs/uuid)
WINE_CONFIG_MAKEFILE(libs/vkd3d)
WINE_CONFIG_MAKEFILE(libs/wbemuuid)
WINE_CONFIG_MAKEFILE(libs/wmcodecdspuuid)
WINE_CONFIG_MAKEFILE(libs/xml2)
WINE_CONFIG_MAKEFILE(libs/xslt)
WINE_CONFIG_MAKEFILE(libs/zlib)
WINE_CONFIG_MAKEFILE(loader)
WINE_CONFIG_MAKEFILE(nls)
WINE_CONFIG_MAKEFILE(po)
WINE_CONFIG_MAKEFILE(programs/arp)
WINE_CONFIG_MAKEFILE(programs/aspnet_regiis)
WINE_CONFIG_MAKEFILE(programs/attrib)
WINE_CONFIG_MAKEFILE(programs/belauncher)
WINE_CONFIG_MAKEFILE(programs/cabarc)
WINE_CONFIG_MAKEFILE(programs/cacls)
WINE_CONFIG_MAKEFILE(programs/certutil)
WINE_CONFIG_MAKEFILE(programs/chcp.com)
WINE_CONFIG_MAKEFILE(programs/clock)
WINE_CONFIG_MAKEFILE(programs/cmd)
WINE_CONFIG_MAKEFILE(programs/cmd/tests)
WINE_CONFIG_MAKEFILE(programs/conhost)
WINE_CONFIG_MAKEFILE(programs/conhost/tests)
WINE_CONFIG_MAKEFILE(programs/control)
WINE_CONFIG_MAKEFILE(programs/cscript)
WINE_CONFIG_MAKEFILE(programs/dism)
WINE_CONFIG_MAKEFILE(programs/dllhost)
WINE_CONFIG_MAKEFILE(programs/dplaysvr)
WINE_CONFIG_MAKEFILE(programs/dotnetfx35)
WINE_CONFIG_MAKEFILE(programs/dpnsvr)
WINE_CONFIG_MAKEFILE(programs/dpvsetup)
WINE_CONFIG_MAKEFILE(programs/dxdiag)
WINE_CONFIG_MAKEFILE(programs/eject)
WINE_CONFIG_MAKEFILE(programs/expand)
WINE_CONFIG_MAKEFILE(programs/explorer)
WINE_CONFIG_MAKEFILE(programs/explorer/tests)
WINE_CONFIG_MAKEFILE(programs/extrac32)
WINE_CONFIG_MAKEFILE(programs/fc)
WINE_CONFIG_MAKEFILE(programs/fc/tests)
WINE_CONFIG_MAKEFILE(programs/find)
WINE_CONFIG_MAKEFILE(programs/find/tests)
WINE_CONFIG_MAKEFILE(programs/findstr)
WINE_CONFIG_MAKEFILE(programs/findstr/tests)
WINE_CONFIG_MAKEFILE(programs/fsutil)
WINE_CONFIG_MAKEFILE(programs/fsutil/tests)
WINE_CONFIG_MAKEFILE(programs/getminidump)
WINE_CONFIG_MAKEFILE(programs/hh)
WINE_CONFIG_MAKEFILE(programs/hostname)
WINE_CONFIG_MAKEFILE(programs/icacls)
WINE_CONFIG_MAKEFILE(programs/icinfo)
WINE_CONFIG_MAKEFILE(programs/iexplore)
WINE_CONFIG_MAKEFILE(programs/ipconfig)
WINE_CONFIG_MAKEFILE(programs/klist)
WINE_CONFIG_MAKEFILE(programs/lodctr)
WINE_CONFIG_MAKEFILE(programs/mofcomp)
WINE_CONFIG_MAKEFILE(programs/mshta)
WINE_CONFIG_MAKEFILE(programs/msidb)
WINE_CONFIG_MAKEFILE(programs/msiexec)
WINE_CONFIG_MAKEFILE(programs/msinfo32)
WINE_CONFIG_MAKEFILE(programs/net)
WINE_CONFIG_MAKEFILE(programs/net/tests)
WINE_CONFIG_MAKEFILE(programs/netsh)
WINE_CONFIG_MAKEFILE(programs/netstat)
WINE_CONFIG_MAKEFILE(programs/ngen)
WINE_CONFIG_MAKEFILE(programs/notepad)
WINE_CONFIG_MAKEFILE(programs/oleview)
WINE_CONFIG_MAKEFILE(programs/ping)
WINE_CONFIG_MAKEFILE(programs/plugplay)
WINE_CONFIG_MAKEFILE(programs/pnputil)
WINE_CONFIG_MAKEFILE(programs/powershell)
WINE_CONFIG_MAKEFILE(programs/presentationfontcache)
WINE_CONFIG_MAKEFILE(programs/progman)
WINE_CONFIG_MAKEFILE(programs/reg)
WINE_CONFIG_MAKEFILE(programs/reg/tests)
WINE_CONFIG_MAKEFILE(programs/regasm)
WINE_CONFIG_MAKEFILE(programs/regedit)
WINE_CONFIG_MAKEFILE(programs/regedit/tests)
WINE_CONFIG_MAKEFILE(programs/regini)
WINE_CONFIG_MAKEFILE(programs/regsvcs)
WINE_CONFIG_MAKEFILE(programs/regsvr32)
WINE_CONFIG_MAKEFILE(programs/robocopy)
WINE_CONFIG_MAKEFILE(programs/rpcss)
WINE_CONFIG_MAKEFILE(programs/rundll.exe16,enable_win16)
WINE_CONFIG_MAKEFILE(programs/rundll32)
WINE_CONFIG_MAKEFILE(programs/sc)
WINE_CONFIG_MAKEFILE(programs/sc/tests)
WINE_CONFIG_MAKEFILE(programs/schtasks)
WINE_CONFIG_MAKEFILE(programs/schtasks/tests)
WINE_CONFIG_MAKEFILE(programs/sdbinst)
WINE_CONFIG_MAKEFILE(programs/secedit)
WINE_CONFIG_MAKEFILE(programs/servicemodelreg)
WINE_CONFIG_MAKEFILE(programs/services)
WINE_CONFIG_MAKEFILE(programs/services/tests)
WINE_CONFIG_MAKEFILE(programs/setx)
WINE_CONFIG_MAKEFILE(programs/shutdown)
WINE_CONFIG_MAKEFILE(programs/sort)
WINE_CONFIG_MAKEFILE(programs/spoolsv)
WINE_CONFIG_MAKEFILE(programs/start)
WINE_CONFIG_MAKEFILE(programs/subst)
WINE_CONFIG_MAKEFILE(programs/svchost)
WINE_CONFIG_MAKEFILE(programs/systeminfo)
WINE_CONFIG_MAKEFILE(programs/tabtip)
WINE_CONFIG_MAKEFILE(programs/taskkill)
WINE_CONFIG_MAKEFILE(programs/tasklist)
WINE_CONFIG_MAKEFILE(programs/tasklist/tests)
WINE_CONFIG_MAKEFILE(programs/taskmgr)
WINE_CONFIG_MAKEFILE(programs/termsv)
WINE_CONFIG_MAKEFILE(programs/uninstaller)
WINE_CONFIG_MAKEFILE(programs/unlodctr)
WINE_CONFIG_MAKEFILE(programs/view)
WINE_CONFIG_MAKEFILE(programs/wevtutil)
WINE_CONFIG_MAKEFILE(programs/where)
WINE_CONFIG_MAKEFILE(programs/whoami)
WINE_CONFIG_MAKEFILE(programs/wineboot)
WINE_CONFIG_MAKEFILE(programs/winebrowser)
WINE_CONFIG_MAKEFILE(programs/winecfg)
WINE_CONFIG_MAKEFILE(programs/wineconsole)
WINE_CONFIG_MAKEFILE(programs/winedbg)
WINE_CONFIG_MAKEFILE(programs/winefile)
WINE_CONFIG_MAKEFILE(programs/winemenubuilder)
WINE_CONFIG_MAKEFILE(programs/winemsibuilder)
WINE_CONFIG_MAKEFILE(programs/winepath)
WINE_CONFIG_MAKEFILE(programs/winetest)
WINE_CONFIG_MAKEFILE(programs/winevdm,enable_win16)
WINE_CONFIG_MAKEFILE(programs/winhelp.exe16,enable_win16)
WINE_CONFIG_MAKEFILE(programs/winhlp32)
WINE_CONFIG_MAKEFILE(programs/winmgmt)
WINE_CONFIG_MAKEFILE(programs/winoldap.mod16,enable_win16)
WINE_CONFIG_MAKEFILE(programs/winver)
WINE_CONFIG_MAKEFILE(programs/wmic)
WINE_CONFIG_MAKEFILE(programs/wmplayer)
WINE_CONFIG_MAKEFILE(programs/wordpad)
WINE_CONFIG_MAKEFILE(programs/write)
WINE_CONFIG_MAKEFILE(programs/wscript)
WINE_CONFIG_MAKEFILE(programs/wscript/tests)
WINE_CONFIG_MAKEFILE(programs/wuauserv)
WINE_CONFIG_MAKEFILE(programs/wusa)
WINE_CONFIG_MAKEFILE(programs/xcopy)
WINE_CONFIG_MAKEFILE(programs/xcopy/tests)
WINE_CONFIG_MAKEFILE(server)
WINE_CONFIG_MAKEFILE(tools,,[test "x$enable_tools" = xno])
WINE_CONFIG_MAKEFILE(tools/sfnt2fon,,[test "x$enable_tools" = xno])
WINE_CONFIG_MAKEFILE(tools/widl,,[test "x$enable_tools" = xno])
WINE_CONFIG_MAKEFILE(tools/winebuild,,[test "x$enable_tools" = xno])
WINE_CONFIG_MAKEFILE(tools/winedump,,[test "x$enable_tools" = xno])
WINE_CONFIG_MAKEFILE(tools/winegcc,,[test "x$enable_tools" = xno])
WINE_CONFIG_MAKEFILE(tools/winemaker,,[test "x$enable_tools" = xno])
WINE_CONFIG_MAKEFILE(tools/wmc,,[test "x$enable_tools" = xno])
WINE_CONFIG_MAKEFILE(tools/wrc,,[test "x$enable_tools" = xno])
WINE_CONFIG_MAKEFILE(libs/atl_android)
dnl End of auto-generated output commands

dnl Main ignore file

WINE_IGNORE_FILE(TAGS)
WINE_IGNORE_FILE(tags)
WINE_IGNORE_FILE(autom4te.cache)
WINE_IGNORE_FILE(compile_commands.json)
WINE_IGNORE_FILE(config.log)
WINE_IGNORE_FILE(config.status)
WINE_IGNORE_FILE(include/config.h)
WINE_IGNORE_FILE(include/stamp-h)
test "$wine_binary" = wine || WINE_IGNORE_FILE(loader/wine)

if test "x$enable_tools" != xno
then
    AC_CONFIG_COMMANDS([tools/makedep],[wine_fn_output_makedep || AS_EXIT],
[wine_fn_output_makedep ()
{
    AS_MKDIR_P(tools)
    $CC -c -I${wine_srcdir}tools -Iinclude -I${wine_srcdir}include -D__WINESRC__ -DWINE_UNIX_LIB $EXTRACFLAGS $CPPFLAGS $CFLAGS -o tools/makedep.o ${wine_srcdir}tools/makedep.c
    $CC -o tools/makedep$ac_exeext tools/makedep.o $LDFLAGS
}])
fi

AC_CONFIG_COMMANDS([Makefile], [wine_fn_output_makefile Makefile],
[wine_fn_output_makefile ()
{
    cat <<\_WINE_EOF >\$tmp/makefile && mv -f \$tmp/makefile \$[]1 && "$wine_makedep"$makedep_flags && return
# This Makefile understands the following targets:
#
# all (default):   build wine
# clean:           remove all intermediate files
# distclean:       also remove all files created by configure
# test:            run tests
# testclean:       clean test results to force running all tests again
# install-lib:     install libraries needed to run applications
# install-dev:     install development environment
# install:         install everything
# uninstall:       uninstall everything
# ctags:           create a tags file for vim and others.
# etags:           create a TAGS file for Emacs.

m4_set_foreach([_AC_SUBST_VARS],[var],[var = $var
])$SET_MAKE
$wine_rules
_WINE_EOF
    AS_ERROR([could not create Makefile])
}])

dnl Main makefile

AC_SUBST(SHELL,[/bin/sh])

dnl Rules for re-running configure

WINE_APPEND_RULE(
[all: wine
        @echo \"Wine build complete.\"
Makefile: config.status
        @./config.status Makefile
config.status: ${wine_srcdir}configure
        @./config.status --recheck
include/config.h: include/stamp-h
include/stamp-h: ${wine_srcdir}include/config.h.in config.status
        @./config.status include/config.h include/stamp-h])

if test "x$enable_maintainer_mode" = xyes
then
    WINE_APPEND_RULE(
[configure: configure.ac aclocal.m4
        autoconf --warnings=all
include/config.h.in: include/stamp-h.in
include/stamp-h.in: configure.ac aclocal.m4
        autoheader --warnings=all
        @echo timestamp > \$[@]])
fi

dnl Rules for wineloader

if test -n "$with_wine64"
then
    case "$with_wine64" in
      /*) reldir="" ;;
      *)  reldir="../" ;;
    esac
    dnl Get rid of old symlinks
    rm -f fonts server 2>/dev/null
    WINE_APPEND_RULE(
[all: loader/wine64 loader/wine64-preloader $with_wine64/loader/wine $with_wine64/loader/wine-preloader
loader/wine64 loader/wine64-preloader:
        rm -f \$[@] && \$(LN_S) $reldir$with_wine64/\$[@] \$[@]
$with_wine64/loader/wine:
        rm -f \$[@] && \$(LN_S) $ac_pwd/loader/wine \$[@]
$with_wine64/loader/wine-preloader:
        rm -f \$[@] && \$(LN_S) $ac_pwd/loader/wine-preloader \$[@]
clean::
        rm -f loader/wine64 loader/wine64-preloader $with_wine64/loader/wine $with_wine64/loader/wine-preloader])
else
    TOP_INSTALL_DEV="$TOP_INSTALL_DEV include"
    TOP_INSTALL_LIB="$TOP_INSTALL_LIB \
fonts \
loader/wine.inf \
nls \
programs/msidb/msidb \
programs/msiexec/msiexec \
programs/notepad/notepad \
programs/regedit/regedit \
programs/regsvr32/regsvr32 \
programs/wineboot/wineboot \
programs/winecfg/winecfg \
programs/wineconsole/wineconsole \
programs/winedbg/winedbg \
programs/winefile/winefile \
programs/winemine/winemine \
programs/winepath/winepath \
server/wineserver"

    case $host_os in
      cygwin*|mingw32*|darwin*|macosx*|linux-android*) ;;
      *) TOP_INSTALL_LIB="$TOP_INSTALL_LIB loader/wine.desktop" ;;
    esac
fi

dnl Rules for cleaning

WINE_APPEND_RULE(
[distclean:: clean
        rm -rf autom4te.cache
maintainer-clean::
        rm -f configure include/config.h.in])

dnl Rules for generated source files

WINE_APPEND_RULE(
[dlls/ntdll/unix/version.c: dummy
        @version=\`(GIT_DIR=${wine_srcdir}.git git describe HEAD 2>/dev/null || echo \"wine-\$(PACKAGE_VERSION)\") | sed -n -e '\$\$s/\(.*\)/const char wine_build[[]] = \"\\1\";/p'\` && (echo \$\$version | cmp -s - \$[@]) || echo \$\$version >\$[@] || (rm -f \$[@] && exit 1)
programs/winetest/build.rc: dummy
        @build=\"STRINGTABLE { 1 \\\"\`GIT_DIR=${wine_srcdir}.git git rev-parse HEAD 2>/dev/null\`\\\" }\" && (echo \$\$build | cmp -s - \$[@]) || echo \$\$build >\$[@] || (rm -f \$[@] && exit 1)
programs/winetest/build.nfo:
        @-\$(CC) -v 2>\$[@]
dlls/wineandroid.drv/wine-debug.apk: dlls/wineandroid.drv/build.gradle ${wine_srcdir}dlls/wineandroid.drv/AndroidManifest.xml ${wine_srcdir}dlls/wineandroid.drv/WineActivity.java ${wine_srcdir}dlls/wineandroid.drv/wine.svg
        cd dlls/wineandroid.drv && gradle -q -Psrcdir=$srcdir assembleDebug
        mv dlls/wineandroid.drv/build/outputs/apk/wine-debug.apk \$[@]])

dnl Misc rules

AC_SUBST(TAGSFLAGS,["--langmap='c:+.idl.l.rh,make:(Make*.in)'"])
WINE_APPEND_RULE(
[TAGS etags:
        rm -f TAGS
        (test -d .git && git ls-files || find -L $srcdir -name '*.[[ch]]' -print) | xargs etags -a \$(TAGSFLAGS)
tags ctags:
        rm -f tags
        (test -d .git && git ls-files || find -L $srcdir -name '*.[[ch]]' -print) | xargs ctags -a \$(TAGSFLAGS)
dummy:
.PHONY: dummy])

AS_ECHO(" done") >&AS_MESSAGE_FD
AC_OUTPUT

if test "$no_create" = "yes"
then
  exit 0
fi

WINE_PRINT_MESSAGES

AS_ECHO("
$as_me: Finished.  Do '${MAKE-make}' to compile Wine.
") >&AS_MESSAGE_FD

dnl Local Variables:
dnl comment-start: "dnl "
dnl comment-end: ""
dnl comment-start-skip: "\\bdnl\\b\\s *"
dnl compile-command: "autoreconf --warnings=all"
dnl End:
