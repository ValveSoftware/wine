AC_DEFUN([WINE_IGNORE_FILE], [dnl])

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

AC_CHECK_TYPES([request_sense],,,[#include <linux/cdrom.h>])

AC_CACHE_CHECK([whether we can use re-entrant gethostbyname_r Linux style],
   	wine_cv_linux_gethostbyname_r_6,
	AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <netdb.h>]],[[
    char *name=0;
    struct hostent he;
    struct hostent *result;
    char *buf=0;
    int bufsize=0;
    int errnr;
    char *addr=0;
    int addrlen=0;
    int addrtype=0;
    gethostbyname_r(name,&he,buf,bufsize,&result,&errnr);
    gethostbyaddr_r(addr, addrlen, addrtype,&he,buf,bufsize,&result,&errnr);
    ]])],[wine_cv_linux_gethostbyname_r_6=yes],[wine_cv_linux_gethostbyname_r_6=no
	])
   )
   if test "$wine_cv_linux_gethostbyname_r_6" = "yes"
   then
      AC_DEFINE(HAVE_LINUX_GETHOSTBYNAME_R_6, 1,
                [Define if Linux-style gethostbyname_r and gethostbyaddr_r are available])
   fi

dnl Check for socket structure members
AC_CHECK_MEMBERS([struct sockaddr_un.sun_len],,,
[#include <sys/types.h>
#include <sys/socket.h>
#ifdef HAVE_SYS_UN_H
# include <sys/un.h>
#endif])

dnl Check for scsireq_t and sg_io_hdr_t members
AC_CHECK_MEMBERS([scsireq_t.cmd, sg_io_hdr_t.interface_id],,,
[#include <sys/types.h>
#ifdef HAVE_SCSI_SG_H
#include <scsi/sg.h>
#endif])

dnl Check for siginfo_t members
AC_CHECK_MEMBERS([siginfo_t.si_fd],,,[#include <signal.h>])

dnl Check for struct mtget members
AC_CHECK_MEMBERS([struct mtget.mt_blksiz, struct mtget.mt_gstat, struct mtget.mt_blkno],,,
[#include <sys/types.h>
#ifdef HAVE_SYS_MTIO_H
#include <sys/mtio.h>
#endif])

dnl Check for stat.st_blocks and ns-resolved times
AC_CHECK_MEMBERS([
	struct stat.st_mtim,
	struct stat.st_mtimespec,
	struct stat.st_ctim,
	struct stat.st_ctimespec,
	struct stat.st_atim,
	struct stat.st_atimespec,
	struct stat.st_birthtime,
	struct stat.st_birthtim,
	struct stat.st_birthtimespec,
	struct stat.__st_birthtime,
	struct stat.__st_birthtim])

dnl Check for sin6_scope_id
AC_CHECK_MEMBERS([struct sockaddr_in6.sin6_scope_id],,,
[#include <sys/types.h>
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif])

dnl Check for struct __res_state._u._ext.nscount6
AC_CHECK_MEMBERS([struct __res_state._u._ext.nscount6],,,
[#ifdef HAVE_RESOLV_H
#include <resolv.h>
#endif])

dnl Check for struct in6_pktinfo
AC_CHECK_MEMBERS([struct in6_pktinfo.ipi6_addr],,,
[#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif])

dnl Check for struct ipstat
AC_CHECK_MEMBERS([struct ipstat.ips_total],,,
[#include <sys/types.h>
#ifdef HAVE_SYS_SOCKETVAR_H
#include <sys/socketvar.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_NETINET_IP_VAR_H
#include <netinet/ip_var.h>
#endif])

dnl Check for struct ip_stats
AC_CHECK_MEMBERS([struct ip_stats.ips_total],,,
[#ifdef HAVE_NETINET_IP_VAR_H
#include <netinet/ip_var.h>
#endif])

dnl Check for struct ip6stat
AC_CHECK_MEMBERS([struct ip6stat.ip6s_total],,,
[#include <sys/types.h>
#ifdef HAVE_SYS_SOCKETVAR_H
#include <sys/socketvar.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_NETINET6_IP6_VAR_H
#include <netinet6/ip6_var.h>
#endif])

dnl Check for struct icmpstat
AC_CHECK_MEMBERS([struct icmpstat.icps_error],,,
[#include <sys/types.h>
#ifdef HAVE_SYS_SOCKETVAR_H
#include <sys/socketvar.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_NETINET_IP_H
#include <netinet/ip.h>
#endif
#ifdef HAVE_NETINET_IP_ICMP_H
#include <netinet/ip_icmp.h>
#endif
#ifdef HAVE_NETINET_ICMP_VAR_H
#include <netinet/icmp_var.h>
#endif])

dnl Check for struct icmp6stat
AC_CHECK_MEMBERS([struct icmp6stat.icp6s_error],,,
[#include <sys/types.h>
#ifdef HAVE_SYS_SOCKETVAR_H
#include <sys/socketvar.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_NETINET_ICMP6_H
#include <netinet/icmp6.h>
#endif])

dnl Check for struct tcpstat
AC_CHECK_MEMBERS([struct tcpstat.tcps_connattempt],,,
[#include <sys/types.h>
#ifdef HAVE_SYS_SOCKETVAR_H
#include <sys/socketvar.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_NETINET_TCP_H
#include <netinet/tcp.h>
#endif
#ifdef HAVE_NETINET_TCP_VAR_H
#include <netinet/tcp_var.h>
#endif])

dnl Check for struct tcp_stats
AC_CHECK_MEMBERS([struct tcp_stats.tcps_connattempt],,,
[#ifdef HAVE_NETINET_TCP_VAR_H
#include <netinet/tcp_var.h>
#endif])

dnl Check for struct udpstat
AC_CHECK_MEMBERS([struct udpstat.udps_ipackets],,,
[#include <sys/types.h>
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_NETINET_IP_VAR_H
#include <netinet/ip_var.h>
#endif
#ifdef HAVE_NETINET_UDP_H
#include <netinet/udp.h>
#endif
#ifdef HAVE_NETINET_UDP_VAR_H
#include <netinet/udp_var.h>
#endif])

dnl Check for struct ifreq.ifr_hwaddr
AC_CHECK_MEMBERS([struct ifreq.ifr_hwaddr],,,
[#include <sys/types.h>
#ifdef HAVE_NET_IF_H
# include <net/if.h>
#endif])

dnl Check for struct sysinfo members totalram and mem_unit
AC_CHECK_MEMBERS([struct sysinfo.totalram, struct sysinfo.mem_unit],,,
[#ifdef HAVE_SYS_SYSINFO_H
# include <sys/sysinfo.h>
#endif])

LIBS="$ac_save_LIBS"

dnl Check for __builtin_popcount
AC_CACHE_CHECK([for __builtin_popcount], ac_cv_have___builtin_popcount,
               AC_LINK_IFELSE([AC_LANG_PROGRAM(,[[return __builtin_popcount(1)]])],
               [ac_cv_have___builtin_popcount="yes"], [ac_cv_have___builtin_popcount="no"]))
if test "$ac_cv_have___builtin_popcount" = "yes"
then
    AC_DEFINE(HAVE___BUILTIN_POPCOUNT, 1, [Define to 1 if you have the '__builtin_popcount' built-in function.])
fi

AC_CACHE_CHECK([for __clear_cache], ac_cv_have___clear_cache,
               AC_LINK_IFELSE([AC_LANG_PROGRAM(,[[__clear_cache((void*)0, (void*)0); return 0;]])],
               [ac_cv_have___clear_cache="yes"], [ac_cv_have___clear_cache="no"]))
if test "$ac_cv_have___clear_cache" = "yes"
then
    AC_DEFINE(HAVE___CLEAR_CACHE, 1, [Define to 1 if you have the '__clear_cache' (potentially built-in) function.])
fi

dnl *** check for the need to define platform-specific symbols

case $host_cpu in
  *i[[3456789]]86*) WINE_CHECK_DEFINE([__i386__]) ;;
  *x86_64*)         WINE_CHECK_DEFINE([__x86_64__]) ;;
  *powerpc64*)      WINE_CHECK_DEFINE([__powerpc64__]) ;;
  *aarch64*)        WINE_CHECK_DEFINE([__aarch64__]) ;;
  *arm*)            WINE_CHECK_DEFINE([__arm__]) ;;
esac

dnl *** Check for modules to disable by default

enable_win16=${enable_win16:-i386}
enable_vcruntime140_1=${enable_vcruntime140_1:-x86_64,arm64ec}

if test -n "$PE_ARCHS"
then
    enable_wow64=${enable_wow64:-aarch64,x86_64}
    enable_wow64win=${enable_wow64win:-aarch64,x86_64}
    enable_wow64cpu=${enable_wow64cpu:-x86_64}
    enable_xtajit64=${enable_xtajit64:-arm64ec}
else
    enable_wow64=${enable_wow64:-no}
    enable_wow64win=${enable_wow64win:-no}
    enable_wow64cpu=${enable_wow64cpu:-no}
    enable_xtajit64=${enable_xtajit64:-no}
fi

dnl Explicitly enable some programs for arm64ec
enable_cmd=${enable_cmd:-yes}
enable_dllhost=${enable_dllhost:-yes}
enable_dpnsvr=${enable_dpnsvr:-i386,x86_64,arm64ec}
enable_dxdiag=${enable_dxdiag:-yes}
enable_msiexec=${enable_msiexec:-yes}
enable_netsh=${enable_netsh:-yes}
enable_regsvr32=${enable_regsvr32:-yes}
enable_rundll32=${enable_rundll32:-yes}

dnl Disable winetest too if tests are disabled
enable_winetest=${enable_winetest:-$enable_tests}

dnl **** Generate output files ****

AH_TOP([#ifndef __WINE_CONFIG_H
#define __WINE_CONFIG_H])
AH_BOTTOM([#endif /* __WINE_CONFIG_H */])

AC_CONFIG_COMMANDS([include/stamp-h], [echo timestamp > include/stamp-h])
AS_ECHO_N("creating Makefile rules...") >&AS_MESSAGE_FD

makedep_flags=" -C"
test "x$enable_silent_rules" = xyes && makedep_flags="$makedep_flags -S"

wine_srcdir=
test "$srcdir" = . || wine_srcdir="$srcdir/"

WINE_CONFIG_SYMLINK(wine,tools/winewrapper)
WINE_CONFIG_SYMLINK(wine64,tools/winewrapper,["$wine_binary" = wine64 -o -n "$with_wine64"])

WINE_CONFIG_MAKEFILE(dlls/acledit)
WINE_CONFIG_MAKEFILE(dlls/aclui)
WINE_CONFIG_MAKEFILE(dlls/activeds.tlb)
WINE_CONFIG_MAKEFILE(dlls/activeds)
WINE_CONFIG_MAKEFILE(dlls/activeds/tests)
WINE_CONFIG_MAKEFILE(dlls/actxprxy)
WINE_CONFIG_MAKEFILE(dlls/adsldp)
WINE_CONFIG_MAKEFILE(dlls/adsldp/tests)
WINE_CONFIG_MAKEFILE(dlls/adsldpc)
WINE_CONFIG_MAKEFILE(dlls/advapi32)
WINE_CONFIG_MAKEFILE(dlls/advapi32/tests)
WINE_CONFIG_MAKEFILE(dlls/advpack)
WINE_CONFIG_MAKEFILE(dlls/advpack/tests)
WINE_CONFIG_MAKEFILE(dlls/amsi)
WINE_CONFIG_MAKEFILE(dlls/amd_ags_x64)
WINE_CONFIG_MAKEFILE(dlls/amstream)
WINE_CONFIG_MAKEFILE(dlls/amstream/tests)
WINE_CONFIG_MAKEFILE(dlls/apisetschema)
WINE_CONFIG_MAKEFILE(dlls/apphelp)
WINE_CONFIG_MAKEFILE(dlls/apphelp/tests)
WINE_CONFIG_MAKEFILE(dlls/appwiz.cpl)
WINE_CONFIG_MAKEFILE(dlls/appxdeploymentclient)
WINE_CONFIG_MAKEFILE(dlls/atiadlxx)
WINE_CONFIG_MAKEFILE(dlls/atl)
WINE_CONFIG_MAKEFILE(dlls/atl/tests)
WINE_CONFIG_MAKEFILE(dlls/atl100)
WINE_CONFIG_MAKEFILE(dlls/atl100/tests)
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
WINE_CONFIG_MAKEFILE(dlls/concrt140/tests)
WINE_CONFIG_MAKEFILE(dlls/connect)
WINE_CONFIG_MAKEFILE(dlls/coremessaging)
WINE_CONFIG_MAKEFILE(dlls/credui)
WINE_CONFIG_MAKEFILE(dlls/credui/tests)
WINE_CONFIG_MAKEFILE(dlls/crtdll)
WINE_CONFIG_MAKEFILE(dlls/crypt32)
WINE_CONFIG_MAKEFILE(dlls/crypt32/tests)
WINE_CONFIG_MAKEFILE(dlls/cryptdlg)
WINE_CONFIG_MAKEFILE(dlls/cryptdll)
WINE_CONFIG_MAKEFILE(dlls/cryptext)
WINE_CONFIG_MAKEFILE(dlls/cryptnet)
WINE_CONFIG_MAKEFILE(dlls/cryptnet/tests)
WINE_CONFIG_MAKEFILE(dlls/cryptowinrt)
WINE_CONFIG_MAKEFILE(dlls/cryptowinrt/tests)
WINE_CONFIG_MAKEFILE(dlls/cryptsp)
WINE_CONFIG_MAKEFILE(dlls/cryptui)
WINE_CONFIG_MAKEFILE(dlls/cryptui/tests)
WINE_CONFIG_MAKEFILE(dlls/ctapi32)
WINE_CONFIG_MAKEFILE(dlls/ctl3d.dll16,enable_win16)
WINE_CONFIG_MAKEFILE(dlls/ctl3d32)
WINE_CONFIG_MAKEFILE(dlls/ctl3dv2.dll16,enable_win16)
WINE_CONFIG_MAKEFILE(dlls/d2d1)
WINE_CONFIG_MAKEFILE(dlls/d2d1/tests)
WINE_CONFIG_MAKEFILE(dlls/d3d10)
WINE_CONFIG_MAKEFILE(dlls/d3d10/tests)
WINE_CONFIG_MAKEFILE(dlls/d3d10_1)
WINE_CONFIG_MAKEFILE(dlls/d3d10_1/tests)
WINE_CONFIG_MAKEFILE(dlls/d3d10core)
WINE_CONFIG_MAKEFILE(dlls/d3d10core/tests)
WINE_CONFIG_MAKEFILE(dlls/d3d11)
WINE_CONFIG_MAKEFILE(dlls/d3d11/tests)
WINE_CONFIG_MAKEFILE(dlls/d3d12)
WINE_CONFIG_MAKEFILE(dlls/d3d12/tests)
WINE_CONFIG_MAKEFILE(dlls/d3d12core)
WINE_CONFIG_MAKEFILE(dlls/d3d8)
WINE_CONFIG_MAKEFILE(dlls/d3d8/tests)
WINE_CONFIG_MAKEFILE(dlls/d3d8thk)
WINE_CONFIG_MAKEFILE(dlls/d3d9)
WINE_CONFIG_MAKEFILE(dlls/d3d9/tests)
WINE_CONFIG_MAKEFILE(dlls/d3dcompiler_33)
WINE_CONFIG_MAKEFILE(dlls/d3dcompiler_34)
WINE_CONFIG_MAKEFILE(dlls/d3dcompiler_35)
WINE_CONFIG_MAKEFILE(dlls/d3dcompiler_36)
WINE_CONFIG_MAKEFILE(dlls/d3dcompiler_37)
WINE_CONFIG_MAKEFILE(dlls/d3dcompiler_38)
WINE_CONFIG_MAKEFILE(dlls/d3dcompiler_39)
WINE_CONFIG_MAKEFILE(dlls/d3dcompiler_40)
WINE_CONFIG_MAKEFILE(dlls/d3dcompiler_41)
WINE_CONFIG_MAKEFILE(dlls/d3dcompiler_42)
WINE_CONFIG_MAKEFILE(dlls/d3dcompiler_43)
WINE_CONFIG_MAKEFILE(dlls/d3dcompiler_43/tests)
WINE_CONFIG_MAKEFILE(dlls/d3dcompiler_46)
WINE_CONFIG_MAKEFILE(dlls/d3dcompiler_46/tests)
WINE_CONFIG_MAKEFILE(dlls/d3dcompiler_47)
WINE_CONFIG_MAKEFILE(dlls/d3dcompiler_47/tests)
WINE_CONFIG_MAKEFILE(dlls/d3dim)
WINE_CONFIG_MAKEFILE(dlls/d3dim700)
WINE_CONFIG_MAKEFILE(dlls/d3drm)
WINE_CONFIG_MAKEFILE(dlls/d3drm/tests)
WINE_CONFIG_MAKEFILE(dlls/d3dx10_33)
WINE_CONFIG_MAKEFILE(dlls/d3dx10_34)
WINE_CONFIG_MAKEFILE(dlls/d3dx10_34/tests)
WINE_CONFIG_MAKEFILE(dlls/d3dx10_35)
WINE_CONFIG_MAKEFILE(dlls/d3dx10_35/tests)
WINE_CONFIG_MAKEFILE(dlls/d3dx10_36)
WINE_CONFIG_MAKEFILE(dlls/d3dx10_36/tests)
WINE_CONFIG_MAKEFILE(dlls/d3dx10_37)
WINE_CONFIG_MAKEFILE(dlls/d3dx10_37/tests)
WINE_CONFIG_MAKEFILE(dlls/d3dx10_38)
WINE_CONFIG_MAKEFILE(dlls/d3dx10_38/tests)
WINE_CONFIG_MAKEFILE(dlls/d3dx10_39)
WINE_CONFIG_MAKEFILE(dlls/d3dx10_39/tests)
WINE_CONFIG_MAKEFILE(dlls/d3dx10_40)
WINE_CONFIG_MAKEFILE(dlls/d3dx10_40/tests)
WINE_CONFIG_MAKEFILE(dlls/d3dx10_41)
WINE_CONFIG_MAKEFILE(dlls/d3dx10_41/tests)
WINE_CONFIG_MAKEFILE(dlls/d3dx10_42)
WINE_CONFIG_MAKEFILE(dlls/d3dx10_42/tests)
WINE_CONFIG_MAKEFILE(dlls/d3dx10_43)
WINE_CONFIG_MAKEFILE(dlls/d3dx10_43/tests)
WINE_CONFIG_MAKEFILE(dlls/d3dx11_42)
WINE_CONFIG_MAKEFILE(dlls/d3dx11_42/tests)
WINE_CONFIG_MAKEFILE(dlls/d3dx11_43)
WINE_CONFIG_MAKEFILE(dlls/d3dx11_43/tests)
WINE_CONFIG_MAKEFILE(dlls/d3dx9_24)
WINE_CONFIG_MAKEFILE(dlls/d3dx9_25)
WINE_CONFIG_MAKEFILE(dlls/d3dx9_26)
WINE_CONFIG_MAKEFILE(dlls/d3dx9_27)
WINE_CONFIG_MAKEFILE(dlls/d3dx9_28)
WINE_CONFIG_MAKEFILE(dlls/d3dx9_29)
WINE_CONFIG_MAKEFILE(dlls/d3dx9_30)
WINE_CONFIG_MAKEFILE(dlls/d3dx9_31)
WINE_CONFIG_MAKEFILE(dlls/d3dx9_32)
WINE_CONFIG_MAKEFILE(dlls/d3dx9_33)
WINE_CONFIG_MAKEFILE(dlls/d3dx9_34)
WINE_CONFIG_MAKEFILE(dlls/d3dx9_35)
WINE_CONFIG_MAKEFILE(dlls/d3dx9_36)
WINE_CONFIG_MAKEFILE(dlls/d3dx9_36/tests)
WINE_CONFIG_MAKEFILE(dlls/d3dx9_37)
WINE_CONFIG_MAKEFILE(dlls/d3dx9_38)
WINE_CONFIG_MAKEFILE(dlls/d3dx9_39)
WINE_CONFIG_MAKEFILE(dlls/d3dx9_40)
WINE_CONFIG_MAKEFILE(dlls/d3dx9_41)
WINE_CONFIG_MAKEFILE(dlls/d3dx9_42)
WINE_CONFIG_MAKEFILE(dlls/d3dx9_43)
WINE_CONFIG_MAKEFILE(dlls/d3dx9_43/tests)
WINE_CONFIG_MAKEFILE(dlls/d3dxof)
WINE_CONFIG_MAKEFILE(dlls/d3dxof/tests)
WINE_CONFIG_MAKEFILE(dlls/dataexchange)
WINE_CONFIG_MAKEFILE(dlls/dataexchange/tests)
WINE_CONFIG_MAKEFILE(dlls/davclnt)
WINE_CONFIG_MAKEFILE(dlls/dbgeng)
WINE_CONFIG_MAKEFILE(dlls/dbgeng/tests)
WINE_CONFIG_MAKEFILE(dlls/dbghelp)
WINE_CONFIG_MAKEFILE(dlls/dbghelp/tests)
WINE_CONFIG_MAKEFILE(dlls/dciman32)
WINE_CONFIG_MAKEFILE(dlls/dcomp)
WINE_CONFIG_MAKEFILE(dlls/ddeml.dll16,enable_win16)
WINE_CONFIG_MAKEFILE(dlls/ddraw)
WINE_CONFIG_MAKEFILE(dlls/ddraw/tests)
WINE_CONFIG_MAKEFILE(dlls/ddrawex)
WINE_CONFIG_MAKEFILE(dlls/ddrawex/tests)
WINE_CONFIG_MAKEFILE(dlls/desk.cpl)
WINE_CONFIG_MAKEFILE(dlls/devenum)
WINE_CONFIG_MAKEFILE(dlls/devenum/tests)
WINE_CONFIG_MAKEFILE(dlls/dhcpcsvc)
WINE_CONFIG_MAKEFILE(dlls/dhcpcsvc/tests)
WINE_CONFIG_MAKEFILE(dlls/dhcpcsvc6)
WINE_CONFIG_MAKEFILE(dlls/dhtmled.ocx)
WINE_CONFIG_MAKEFILE(dlls/diasymreader)
WINE_CONFIG_MAKEFILE(dlls/difxapi)
WINE_CONFIG_MAKEFILE(dlls/dinput)
WINE_CONFIG_MAKEFILE(dlls/dinput/tests)
WINE_CONFIG_MAKEFILE(dlls/dinput8)
WINE_CONFIG_MAKEFILE(dlls/directmanipulation)
WINE_CONFIG_MAKEFILE(dlls/directmanipulation/tests)
WINE_CONFIG_MAKEFILE(dlls/dispdib.dll16,enable_win16)
WINE_CONFIG_MAKEFILE(dlls/dispex)
WINE_CONFIG_MAKEFILE(dlls/dispex/tests)
WINE_CONFIG_MAKEFILE(dlls/display.drv16,enable_win16)
WINE_CONFIG_MAKEFILE(dlls/dmband)
WINE_CONFIG_MAKEFILE(dlls/dmband/tests)
WINE_CONFIG_MAKEFILE(dlls/dmcompos)
WINE_CONFIG_MAKEFILE(dlls/dmcompos/tests)
WINE_CONFIG_MAKEFILE(dlls/dmime)
WINE_CONFIG_MAKEFILE(dlls/dmime/tests)
WINE_CONFIG_MAKEFILE(dlls/dmloader)
WINE_CONFIG_MAKEFILE(dlls/dmloader/tests)
WINE_CONFIG_MAKEFILE(dlls/dmscript)
WINE_CONFIG_MAKEFILE(dlls/dmscript/tests)
WINE_CONFIG_MAKEFILE(dlls/dmstyle)
WINE_CONFIG_MAKEFILE(dlls/dmstyle/tests)
WINE_CONFIG_MAKEFILE(dlls/dmsynth)
WINE_CONFIG_MAKEFILE(dlls/dmsynth/tests)
WINE_CONFIG_MAKEFILE(dlls/dmusic)
WINE_CONFIG_MAKEFILE(dlls/dmusic/tests)
WINE_CONFIG_MAKEFILE(dlls/dmusic32)
WINE_CONFIG_MAKEFILE(dlls/dnsapi)
WINE_CONFIG_MAKEFILE(dlls/dnsapi/tests)
WINE_CONFIG_MAKEFILE(dlls/dplay)
WINE_CONFIG_MAKEFILE(dlls/dplayx)
WINE_CONFIG_MAKEFILE(dlls/dplayx/tests)
WINE_CONFIG_MAKEFILE(dlls/dpnaddr)
WINE_CONFIG_MAKEFILE(dlls/dpnet)
WINE_CONFIG_MAKEFILE(dlls/dpnet/tests)
WINE_CONFIG_MAKEFILE(dlls/dpnhpast)
WINE_CONFIG_MAKEFILE(dlls/dpnhupnp)
WINE_CONFIG_MAKEFILE(dlls/dpnlobby)
WINE_CONFIG_MAKEFILE(dlls/dpvoice)
WINE_CONFIG_MAKEFILE(dlls/dpvoice/tests)
WINE_CONFIG_MAKEFILE(dlls/dpwsockx)
WINE_CONFIG_MAKEFILE(dlls/drmclien)
WINE_CONFIG_MAKEFILE(dlls/dsdmo)
WINE_CONFIG_MAKEFILE(dlls/dsdmo/tests)
WINE_CONFIG_MAKEFILE(dlls/dsound)
WINE_CONFIG_MAKEFILE(dlls/dsound/tests)
WINE_CONFIG_MAKEFILE(dlls/dsquery)
WINE_CONFIG_MAKEFILE(dlls/dssenh)
WINE_CONFIG_MAKEFILE(dlls/dssenh/tests)
WINE_CONFIG_MAKEFILE(dlls/dsuiext)
WINE_CONFIG_MAKEFILE(dlls/dswave)
WINE_CONFIG_MAKEFILE(dlls/dswave/tests)
WINE_CONFIG_MAKEFILE(dlls/dwmapi)
WINE_CONFIG_MAKEFILE(dlls/dwmapi/tests)
WINE_CONFIG_MAKEFILE(dlls/dwrite)
WINE_CONFIG_MAKEFILE(dlls/dwrite/tests)
WINE_CONFIG_MAKEFILE(dlls/dx8vb)
WINE_CONFIG_MAKEFILE(dlls/dxcore)
WINE_CONFIG_MAKEFILE(dlls/dxdiagn)
WINE_CONFIG_MAKEFILE(dlls/dxdiagn/tests)
WINE_CONFIG_MAKEFILE(dlls/dxgi)
WINE_CONFIG_MAKEFILE(dlls/dxgi/tests)
WINE_CONFIG_MAKEFILE(dlls/dxtrans)
WINE_CONFIG_MAKEFILE(dlls/dxva2)
WINE_CONFIG_MAKEFILE(dlls/dxva2/tests)
WINE_CONFIG_MAKEFILE(dlls/esent)
WINE_CONFIG_MAKEFILE(dlls/evr)
WINE_CONFIG_MAKEFILE(dlls/evr/tests)
WINE_CONFIG_MAKEFILE(dlls/explorerframe)
WINE_CONFIG_MAKEFILE(dlls/explorerframe/tests)
WINE_CONFIG_MAKEFILE(dlls/faultrep)
WINE_CONFIG_MAKEFILE(dlls/faultrep/tests)
WINE_CONFIG_MAKEFILE(dlls/feclient)
WINE_CONFIG_MAKEFILE(dlls/fltlib)
WINE_CONFIG_MAKEFILE(dlls/fltmgr.sys)
WINE_CONFIG_MAKEFILE(dlls/fntcache)
WINE_CONFIG_MAKEFILE(dlls/fontsub)
WINE_CONFIG_MAKEFILE(dlls/fusion)
WINE_CONFIG_MAKEFILE(dlls/fusion/tests)
WINE_CONFIG_MAKEFILE(dlls/fwpuclnt)
WINE_CONFIG_MAKEFILE(dlls/gameux)
WINE_CONFIG_MAKEFILE(dlls/gameux/tests)
WINE_CONFIG_MAKEFILE(dlls/gamingtcui)
WINE_CONFIG_MAKEFILE(dlls/gdi.exe16,enable_win16)
WINE_CONFIG_MAKEFILE(dlls/gdi32)
WINE_CONFIG_MAKEFILE(dlls/gdi32/tests)
WINE_CONFIG_MAKEFILE(dlls/gdiplus)
WINE_CONFIG_MAKEFILE(dlls/gdiplus/tests)
WINE_CONFIG_MAKEFILE(dlls/geolocation)
WINE_CONFIG_MAKEFILE(dlls/geolocation/tests)
WINE_CONFIG_MAKEFILE(dlls/glu32)
WINE_CONFIG_MAKEFILE(dlls/glu32/tests)
WINE_CONFIG_MAKEFILE(dlls/gphoto2.ds)
WINE_CONFIG_MAKEFILE(dlls/gpkcsp)
WINE_CONFIG_MAKEFILE(dlls/graphicscapture)
WINE_CONFIG_MAKEFILE(dlls/graphicscapture/tests)
WINE_CONFIG_MAKEFILE(dlls/hal)
WINE_CONFIG_MAKEFILE(dlls/hhctrl.ocx)
WINE_CONFIG_MAKEFILE(dlls/hid)
WINE_CONFIG_MAKEFILE(dlls/hid/tests)
WINE_CONFIG_MAKEFILE(dlls/hidclass.sys)
WINE_CONFIG_MAKEFILE(dlls/hidparse.sys)
WINE_CONFIG_MAKEFILE(dlls/hlink)
WINE_CONFIG_MAKEFILE(dlls/hlink/tests)
WINE_CONFIG_MAKEFILE(dlls/hnetcfg)
WINE_CONFIG_MAKEFILE(dlls/hnetcfg/tests)
WINE_CONFIG_MAKEFILE(dlls/hrtfapo)
WINE_CONFIG_MAKEFILE(dlls/http.sys)
WINE_CONFIG_MAKEFILE(dlls/httpapi)
WINE_CONFIG_MAKEFILE(dlls/httpapi/tests)
WINE_CONFIG_MAKEFILE(dlls/hvsimanagementapi)
WINE_CONFIG_MAKEFILE(dlls/hvsimanagementapi/tests)
WINE_CONFIG_MAKEFILE(dlls/ia2comproxy)
WINE_CONFIG_MAKEFILE(dlls/iccvid)
WINE_CONFIG_MAKEFILE(dlls/icmp)
WINE_CONFIG_MAKEFILE(dlls/icmui)
WINE_CONFIG_MAKEFILE(dlls/icmui/tests)
WINE_CONFIG_MAKEFILE(dlls/icu)
WINE_CONFIG_MAKEFILE(dlls/ieframe)
WINE_CONFIG_MAKEFILE(dlls/ieframe/tests)
WINE_CONFIG_MAKEFILE(dlls/ieproxy)
WINE_CONFIG_MAKEFILE(dlls/iertutil)
WINE_CONFIG_MAKEFILE(dlls/iertutil/tests)
WINE_CONFIG_MAKEFILE(dlls/ifsmgr.vxd,enable_win16)
WINE_CONFIG_MAKEFILE(dlls/imaadp32.acm)
WINE_CONFIG_MAKEFILE(dlls/imagehlp)
WINE_CONFIG_MAKEFILE(dlls/imagehlp/tests)
WINE_CONFIG_MAKEFILE(dlls/imm.dll16,enable_win16)
WINE_CONFIG_MAKEFILE(dlls/imm32)
WINE_CONFIG_MAKEFILE(dlls/imm32/tests)
WINE_CONFIG_MAKEFILE(dlls/inetcomm)
WINE_CONFIG_MAKEFILE(dlls/inetcomm/tests)
WINE_CONFIG_MAKEFILE(dlls/inetcpl.cpl)
WINE_CONFIG_MAKEFILE(dlls/inetmib1)
WINE_CONFIG_MAKEFILE(dlls/inetmib1/tests)
WINE_CONFIG_MAKEFILE(dlls/infosoft)
WINE_CONFIG_MAKEFILE(dlls/infosoft/tests)
WINE_CONFIG_MAKEFILE(dlls/initpki)
WINE_CONFIG_MAKEFILE(dlls/inkobj)
WINE_CONFIG_MAKEFILE(dlls/inseng)
WINE_CONFIG_MAKEFILE(dlls/iphlpapi)
WINE_CONFIG_MAKEFILE(dlls/iphlpapi/tests)
WINE_CONFIG_MAKEFILE(dlls/iprop)
WINE_CONFIG_MAKEFILE(dlls/irprops.cpl)
WINE_CONFIG_MAKEFILE(dlls/itircl)
WINE_CONFIG_MAKEFILE(dlls/itss)
WINE_CONFIG_MAKEFILE(dlls/itss/tests)
WINE_CONFIG_MAKEFILE(dlls/joy.cpl)
WINE_CONFIG_MAKEFILE(dlls/jscript)
WINE_CONFIG_MAKEFILE(dlls/jscript/tests)
WINE_CONFIG_MAKEFILE(dlls/jsproxy)
WINE_CONFIG_MAKEFILE(dlls/jsproxy/tests)
WINE_CONFIG_MAKEFILE(dlls/kerberos)
WINE_CONFIG_MAKEFILE(dlls/kernel32)
WINE_CONFIG_MAKEFILE(dlls/kernel32/tests)
WINE_CONFIG_MAKEFILE(dlls/kernelbase)
WINE_CONFIG_MAKEFILE(dlls/kernelbase/tests)
WINE_CONFIG_MAKEFILE(dlls/keyboard.drv16,enable_win16)
WINE_CONFIG_MAKEFILE(dlls/krnl386.exe16,enable_win16)
WINE_CONFIG_MAKEFILE(dlls/ksecdd.sys)
WINE_CONFIG_MAKEFILE(dlls/ksproxy.ax)
WINE_CONFIG_MAKEFILE(dlls/ksuser)
WINE_CONFIG_MAKEFILE(dlls/ktmw32)
WINE_CONFIG_MAKEFILE(dlls/l3codeca.acm)
WINE_CONFIG_MAKEFILE(dlls/l3codecx.ax)
WINE_CONFIG_MAKEFILE(dlls/light.msstyles)
WINE_CONFIG_MAKEFILE(dlls/loadperf)
WINE_CONFIG_MAKEFILE(dlls/localspl)
WINE_CONFIG_MAKEFILE(dlls/localspl/tests)
WINE_CONFIG_MAKEFILE(dlls/localui)
WINE_CONFIG_MAKEFILE(dlls/localui/tests)
WINE_CONFIG_MAKEFILE(dlls/lz32)
WINE_CONFIG_MAKEFILE(dlls/lz32/tests)
WINE_CONFIG_MAKEFILE(dlls/lzexpand.dll16,enable_win16)
WINE_CONFIG_MAKEFILE(dlls/magnification)
WINE_CONFIG_MAKEFILE(dlls/mapi32)
WINE_CONFIG_MAKEFILE(dlls/mapi32/tests)
WINE_CONFIG_MAKEFILE(dlls/mapistub)
WINE_CONFIG_MAKEFILE(dlls/mciavi32)
WINE_CONFIG_MAKEFILE(dlls/mcicda)
WINE_CONFIG_MAKEFILE(dlls/mciqtz32)
WINE_CONFIG_MAKEFILE(dlls/mciseq)
WINE_CONFIG_MAKEFILE(dlls/mciwave)
WINE_CONFIG_MAKEFILE(dlls/mf)
WINE_CONFIG_MAKEFILE(dlls/mf/tests)
WINE_CONFIG_MAKEFILE(dlls/mf3216)
WINE_CONFIG_MAKEFILE(dlls/mfasfsrcsnk)
WINE_CONFIG_MAKEFILE(dlls/mferror)
WINE_CONFIG_MAKEFILE(dlls/mfh264enc)
WINE_CONFIG_MAKEFILE(dlls/mfmediaengine)
WINE_CONFIG_MAKEFILE(dlls/mfmediaengine/tests)
WINE_CONFIG_MAKEFILE(dlls/mfmp4srcsnk)
WINE_CONFIG_MAKEFILE(dlls/mfplat)
WINE_CONFIG_MAKEFILE(dlls/mfplat/tests)
WINE_CONFIG_MAKEFILE(dlls/mfplay)
WINE_CONFIG_MAKEFILE(dlls/mfplay/tests)
WINE_CONFIG_MAKEFILE(dlls/mfreadwrite)
WINE_CONFIG_MAKEFILE(dlls/mfreadwrite/tests)
WINE_CONFIG_MAKEFILE(dlls/mfsrcsnk)
WINE_CONFIG_MAKEFILE(dlls/mfsrcsnk/tests)
WINE_CONFIG_MAKEFILE(dlls/mgmtapi)
WINE_CONFIG_MAKEFILE(dlls/midimap)
WINE_CONFIG_MAKEFILE(dlls/mlang)
WINE_CONFIG_MAKEFILE(dlls/mlang/tests)
WINE_CONFIG_MAKEFILE(dlls/mmcndmgr)
WINE_CONFIG_MAKEFILE(dlls/mmcndmgr/tests)
WINE_CONFIG_MAKEFILE(dlls/mmdevapi)
WINE_CONFIG_MAKEFILE(dlls/mmdevapi/tests)
WINE_CONFIG_MAKEFILE(dlls/mmdevldr.vxd,enable_win16)
WINE_CONFIG_MAKEFILE(dlls/mmsystem.dll16,enable_win16)
WINE_CONFIG_MAKEFILE(dlls/monodebg.vxd,enable_win16)
WINE_CONFIG_MAKEFILE(dlls/mouhid.sys)
WINE_CONFIG_MAKEFILE(dlls/mountmgr.sys)
WINE_CONFIG_MAKEFILE(dlls/mouse.drv16,enable_win16)
WINE_CONFIG_MAKEFILE(dlls/mp3dmod)
WINE_CONFIG_MAKEFILE(dlls/mp3dmod/tests)
WINE_CONFIG_MAKEFILE(dlls/mpr)
WINE_CONFIG_MAKEFILE(dlls/mpr/tests)
WINE_CONFIG_MAKEFILE(dlls/mprapi)
WINE_CONFIG_MAKEFILE(dlls/msacm.dll16,enable_win16)
WINE_CONFIG_MAKEFILE(dlls/msacm32.drv)
WINE_CONFIG_MAKEFILE(dlls/msacm32)
WINE_CONFIG_MAKEFILE(dlls/msacm32/tests)
WINE_CONFIG_MAKEFILE(dlls/msado15)
WINE_CONFIG_MAKEFILE(dlls/msado15/tests)
WINE_CONFIG_MAKEFILE(dlls/msadp32.acm)
WINE_CONFIG_MAKEFILE(dlls/msasn1)
WINE_CONFIG_MAKEFILE(dlls/msasn1/tests)
WINE_CONFIG_MAKEFILE(dlls/msauddecmft)
WINE_CONFIG_MAKEFILE(dlls/mscat32)
WINE_CONFIG_MAKEFILE(dlls/mscms)
WINE_CONFIG_MAKEFILE(dlls/mscms/tests)
WINE_CONFIG_MAKEFILE(dlls/mscoree)
WINE_CONFIG_MAKEFILE(dlls/mscoree/tests)
WINE_CONFIG_MAKEFILE(dlls/mscorwks)
WINE_CONFIG_MAKEFILE(dlls/msctf)
WINE_CONFIG_MAKEFILE(dlls/msctf/tests)
WINE_CONFIG_MAKEFILE(dlls/msctfmonitor)
WINE_CONFIG_MAKEFILE(dlls/msctfp)
WINE_CONFIG_MAKEFILE(dlls/msdaps)
WINE_CONFIG_MAKEFILE(dlls/msdasql)
WINE_CONFIG_MAKEFILE(dlls/msdasql/tests)
WINE_CONFIG_MAKEFILE(dlls/msdelta)
WINE_CONFIG_MAKEFILE(dlls/msdmo)
WINE_CONFIG_MAKEFILE(dlls/msdmo/tests)
WINE_CONFIG_MAKEFILE(dlls/msdrm)
WINE_CONFIG_MAKEFILE(dlls/msftedit)
WINE_CONFIG_MAKEFILE(dlls/msftedit/tests)
WINE_CONFIG_MAKEFILE(dlls/msg711.acm)
WINE_CONFIG_MAKEFILE(dlls/msgsm32.acm)
WINE_CONFIG_MAKEFILE(dlls/mshtml.tlb)
WINE_CONFIG_MAKEFILE(dlls/mshtml)
WINE_CONFIG_MAKEFILE(dlls/mshtml/tests)
WINE_CONFIG_MAKEFILE(dlls/msi)
WINE_CONFIG_MAKEFILE(dlls/msi/tests)
WINE_CONFIG_MAKEFILE(dlls/msident)
WINE_CONFIG_MAKEFILE(dlls/msimg32)
WINE_CONFIG_MAKEFILE(dlls/msimsg)
WINE_CONFIG_MAKEFILE(dlls/msimtf)
WINE_CONFIG_MAKEFILE(dlls/msisip)
WINE_CONFIG_MAKEFILE(dlls/msisys.ocx)
WINE_CONFIG_MAKEFILE(dlls/msls31)
WINE_CONFIG_MAKEFILE(dlls/msmpeg2vdec)
WINE_CONFIG_MAKEFILE(dlls/msnet32)
WINE_CONFIG_MAKEFILE(dlls/mspatcha)
WINE_CONFIG_MAKEFILE(dlls/mspatcha/tests)
WINE_CONFIG_MAKEFILE(dlls/msports)
WINE_CONFIG_MAKEFILE(dlls/msrle32)
WINE_CONFIG_MAKEFILE(dlls/msrle32/tests)
WINE_CONFIG_MAKEFILE(dlls/msscript.ocx)
WINE_CONFIG_MAKEFILE(dlls/msscript.ocx/tests)
WINE_CONFIG_MAKEFILE(dlls/mssign32)
WINE_CONFIG_MAKEFILE(dlls/mssip32)
WINE_CONFIG_MAKEFILE(dlls/mstask)
WINE_CONFIG_MAKEFILE(dlls/mstask/tests)
WINE_CONFIG_MAKEFILE(dlls/msttsengine)
WINE_CONFIG_MAKEFILE(dlls/msv1_0)
WINE_CONFIG_MAKEFILE(dlls/msvcirt)
WINE_CONFIG_MAKEFILE(dlls/msvcirt/tests)
WINE_CONFIG_MAKEFILE(dlls/msvcm80)
WINE_CONFIG_MAKEFILE(dlls/msvcm90)
WINE_CONFIG_MAKEFILE(dlls/msvcp100)
WINE_CONFIG_MAKEFILE(dlls/msvcp100/tests)
WINE_CONFIG_MAKEFILE(dlls/msvcp110)
WINE_CONFIG_MAKEFILE(dlls/msvcp110/tests)
WINE_CONFIG_MAKEFILE(dlls/msvcp120)
WINE_CONFIG_MAKEFILE(dlls/msvcp120/tests)
WINE_CONFIG_MAKEFILE(dlls/msvcp120_app)
WINE_CONFIG_MAKEFILE(dlls/msvcp140)
WINE_CONFIG_MAKEFILE(dlls/msvcp140/tests)
WINE_CONFIG_MAKEFILE(dlls/msvcp140_1)
WINE_CONFIG_MAKEFILE(dlls/msvcp140_1/tests)
WINE_CONFIG_MAKEFILE(dlls/msvcp140_2)
WINE_CONFIG_MAKEFILE(dlls/msvcp140_2/tests)
WINE_CONFIG_MAKEFILE(dlls/msvcp140_atomic_wait)
WINE_CONFIG_MAKEFILE(dlls/msvcp140_atomic_wait/tests)
WINE_CONFIG_MAKEFILE(dlls/msvcp140_codecvt_ids)
WINE_CONFIG_MAKEFILE(dlls/msvcp60)
WINE_CONFIG_MAKEFILE(dlls/msvcp60/tests)
WINE_CONFIG_MAKEFILE(dlls/msvcp70)
WINE_CONFIG_MAKEFILE(dlls/msvcp71)
WINE_CONFIG_MAKEFILE(dlls/msvcp80)
WINE_CONFIG_MAKEFILE(dlls/msvcp90)
WINE_CONFIG_MAKEFILE(dlls/msvcp90/tests)
WINE_CONFIG_MAKEFILE(dlls/msvcp_win)
WINE_CONFIG_MAKEFILE(dlls/msvcr100)
WINE_CONFIG_MAKEFILE(dlls/msvcr100/tests)
WINE_CONFIG_MAKEFILE(dlls/msvcr110)
WINE_CONFIG_MAKEFILE(dlls/msvcr110/tests)
WINE_CONFIG_MAKEFILE(dlls/msvcr120)
WINE_CONFIG_MAKEFILE(dlls/msvcr120/tests)
WINE_CONFIG_MAKEFILE(dlls/msvcr120_app)
WINE_CONFIG_MAKEFILE(dlls/msvcr70)
WINE_CONFIG_MAKEFILE(dlls/msvcr70/tests)
WINE_CONFIG_MAKEFILE(dlls/msvcr71)
WINE_CONFIG_MAKEFILE(dlls/msvcr71/tests)
WINE_CONFIG_MAKEFILE(dlls/msvcr80)
WINE_CONFIG_MAKEFILE(dlls/msvcr80/tests)
WINE_CONFIG_MAKEFILE(dlls/msvcr90)
WINE_CONFIG_MAKEFILE(dlls/msvcr90/tests)
WINE_CONFIG_MAKEFILE(dlls/msvcrt)
WINE_CONFIG_MAKEFILE(dlls/msvcrt/tests)
WINE_CONFIG_MAKEFILE(dlls/msvcrt20)
WINE_CONFIG_MAKEFILE(dlls/msvcrt40)
WINE_CONFIG_MAKEFILE(dlls/msvcrtd)
WINE_CONFIG_MAKEFILE(dlls/msvcrtd/tests)
WINE_CONFIG_MAKEFILE(dlls/msvfw32)
WINE_CONFIG_MAKEFILE(dlls/msvfw32/tests)
WINE_CONFIG_MAKEFILE(dlls/msvidc32)
WINE_CONFIG_MAKEFILE(dlls/msvideo.dll16,enable_win16)
WINE_CONFIG_MAKEFILE(dlls/msvdsp)
WINE_CONFIG_MAKEFILE(dlls/msvproc)
WINE_CONFIG_MAKEFILE(dlls/mswsock)
WINE_CONFIG_MAKEFILE(dlls/msxml)
WINE_CONFIG_MAKEFILE(dlls/msxml2)
WINE_CONFIG_MAKEFILE(dlls/msxml3)
WINE_CONFIG_MAKEFILE(dlls/msxml3/tests)
WINE_CONFIG_MAKEFILE(dlls/msxml4)
WINE_CONFIG_MAKEFILE(dlls/msxml4/tests)
WINE_CONFIG_MAKEFILE(dlls/msxml6)
WINE_CONFIG_MAKEFILE(dlls/msxml6/tests)
WINE_CONFIG_MAKEFILE(dlls/mtxdm)
WINE_CONFIG_MAKEFILE(dlls/ncrypt)
WINE_CONFIG_MAKEFILE(dlls/ncrypt/tests)
WINE_CONFIG_MAKEFILE(dlls/nddeapi)
WINE_CONFIG_MAKEFILE(dlls/ndis.sys)
WINE_CONFIG_MAKEFILE(dlls/ndis.sys/tests)
WINE_CONFIG_MAKEFILE(dlls/netapi32)
WINE_CONFIG_MAKEFILE(dlls/netapi32/tests)
WINE_CONFIG_MAKEFILE(dlls/netcfgx)
WINE_CONFIG_MAKEFILE(dlls/netcfgx/tests)
WINE_CONFIG_MAKEFILE(dlls/netio.sys)
WINE_CONFIG_MAKEFILE(dlls/netprofm)
WINE_CONFIG_MAKEFILE(dlls/netprofm/tests)
WINE_CONFIG_MAKEFILE(dlls/netutils)
WINE_CONFIG_MAKEFILE(dlls/newdev)
WINE_CONFIG_MAKEFILE(dlls/ninput)
WINE_CONFIG_MAKEFILE(dlls/ninput/tests)
WINE_CONFIG_MAKEFILE(dlls/normaliz)
WINE_CONFIG_MAKEFILE(dlls/npmshtml)
WINE_CONFIG_MAKEFILE(dlls/npptools)
WINE_CONFIG_MAKEFILE(dlls/nsi)
WINE_CONFIG_MAKEFILE(dlls/nsi/tests)
WINE_CONFIG_MAKEFILE(dlls/nsiproxy.sys)
WINE_CONFIG_MAKEFILE(dlls/ntdll)
WINE_CONFIG_MAKEFILE(dlls/ntdll/tests)
WINE_CONFIG_MAKEFILE(dlls/ntdsapi)
WINE_CONFIG_MAKEFILE(dlls/ntdsapi/tests)
WINE_CONFIG_MAKEFILE(dlls/ntoskrnl.exe)
WINE_CONFIG_MAKEFILE(dlls/ntoskrnl.exe/tests)
WINE_CONFIG_MAKEFILE(dlls/ntprint)
WINE_CONFIG_MAKEFILE(dlls/ntprint/tests)
WINE_CONFIG_MAKEFILE(dlls/nvcuda)
WINE_CONFIG_MAKEFILE(dlls/objsel)
WINE_CONFIG_MAKEFILE(dlls/odbc32)
WINE_CONFIG_MAKEFILE(dlls/odbc32/tests)
WINE_CONFIG_MAKEFILE(dlls/odbcbcp)
WINE_CONFIG_MAKEFILE(dlls/odbccp32)
WINE_CONFIG_MAKEFILE(dlls/odbccp32/tests)
WINE_CONFIG_MAKEFILE(dlls/odbccu32)
WINE_CONFIG_MAKEFILE(dlls/ole2.dll16,enable_win16)
WINE_CONFIG_MAKEFILE(dlls/ole2conv.dll16,enable_win16)
WINE_CONFIG_MAKEFILE(dlls/ole2disp.dll16,enable_win16)
WINE_CONFIG_MAKEFILE(dlls/ole2nls.dll16,enable_win16)
WINE_CONFIG_MAKEFILE(dlls/ole2prox.dll16,enable_win16)
WINE_CONFIG_MAKEFILE(dlls/ole2thk.dll16,enable_win16)
WINE_CONFIG_MAKEFILE(dlls/ole32)
WINE_CONFIG_MAKEFILE(dlls/ole32/tests)
WINE_CONFIG_MAKEFILE(dlls/oleacc)
WINE_CONFIG_MAKEFILE(dlls/oleacc/tests)
WINE_CONFIG_MAKEFILE(dlls/oleaut32)
WINE_CONFIG_MAKEFILE(dlls/oleaut32/tests)
WINE_CONFIG_MAKEFILE(dlls/olecli.dll16,enable_win16)
WINE_CONFIG_MAKEFILE(dlls/olecli32)
WINE_CONFIG_MAKEFILE(dlls/oledb32)
WINE_CONFIG_MAKEFILE(dlls/oledb32/tests)
WINE_CONFIG_MAKEFILE(dlls/oledlg)
WINE_CONFIG_MAKEFILE(dlls/oledlg/tests)
WINE_CONFIG_MAKEFILE(dlls/olepro32)
WINE_CONFIG_MAKEFILE(dlls/olesvr.dll16,enable_win16)
WINE_CONFIG_MAKEFILE(dlls/olesvr32)
WINE_CONFIG_MAKEFILE(dlls/olethk32)
WINE_CONFIG_MAKEFILE(dlls/opcservices)
WINE_CONFIG_MAKEFILE(dlls/opcservices/tests)
WINE_CONFIG_MAKEFILE(dlls/opencl)
WINE_CONFIG_MAKEFILE(dlls/opengl32)
WINE_CONFIG_MAKEFILE(dlls/opengl32/tests)
WINE_CONFIG_MAKEFILE(dlls/packager)
WINE_CONFIG_MAKEFILE(dlls/packager/tests)
WINE_CONFIG_MAKEFILE(dlls/pdh)
WINE_CONFIG_MAKEFILE(dlls/pdh/tests)
WINE_CONFIG_MAKEFILE(dlls/photometadatahandler)
WINE_CONFIG_MAKEFILE(dlls/pidgen)
WINE_CONFIG_MAKEFILE(dlls/powrprof)
WINE_CONFIG_MAKEFILE(dlls/printui)
WINE_CONFIG_MAKEFILE(dlls/prntvpt)
WINE_CONFIG_MAKEFILE(dlls/prntvpt/tests)
WINE_CONFIG_MAKEFILE(dlls/profapi)
WINE_CONFIG_MAKEFILE(dlls/propsys)
WINE_CONFIG_MAKEFILE(dlls/propsys/tests)
WINE_CONFIG_MAKEFILE(dlls/protontts)
WINE_CONFIG_MAKEFILE(dlls/psapi)
WINE_CONFIG_MAKEFILE(dlls/psapi/tests)
WINE_CONFIG_MAKEFILE(dlls/pstorec)
WINE_CONFIG_MAKEFILE(dlls/pstorec/tests)
WINE_CONFIG_MAKEFILE(dlls/pwrshplugin)
WINE_CONFIG_MAKEFILE(dlls/qasf)
WINE_CONFIG_MAKEFILE(dlls/qasf/tests)
WINE_CONFIG_MAKEFILE(dlls/qcap)
WINE_CONFIG_MAKEFILE(dlls/qcap/tests)
WINE_CONFIG_MAKEFILE(dlls/qdvd)
WINE_CONFIG_MAKEFILE(dlls/qdvd/tests)
WINE_CONFIG_MAKEFILE(dlls/qedit)
WINE_CONFIG_MAKEFILE(dlls/qedit/tests)
WINE_CONFIG_MAKEFILE(dlls/qmgr)
WINE_CONFIG_MAKEFILE(dlls/qmgr/tests)
WINE_CONFIG_MAKEFILE(dlls/qmgrprxy)
WINE_CONFIG_MAKEFILE(dlls/quartz)
WINE_CONFIG_MAKEFILE(dlls/quartz/tests)
WINE_CONFIG_MAKEFILE(dlls/query)
WINE_CONFIG_MAKEFILE(dlls/qwave)
WINE_CONFIG_MAKEFILE(dlls/qwave/tests)
WINE_CONFIG_MAKEFILE(dlls/rasapi16.dll16,enable_win16)
WINE_CONFIG_MAKEFILE(dlls/rasapi32)
WINE_CONFIG_MAKEFILE(dlls/rasapi32/tests)
WINE_CONFIG_MAKEFILE(dlls/rasdlg)
WINE_CONFIG_MAKEFILE(dlls/regapi)
WINE_CONFIG_MAKEFILE(dlls/resampledmo)
WINE_CONFIG_MAKEFILE(dlls/resutils)
WINE_CONFIG_MAKEFILE(dlls/riched20)
WINE_CONFIG_MAKEFILE(dlls/riched20/tests)
WINE_CONFIG_MAKEFILE(dlls/riched32)
WINE_CONFIG_MAKEFILE(dlls/riched32/tests)
WINE_CONFIG_MAKEFILE(dlls/rometadata)
WINE_CONFIG_MAKEFILE(dlls/rometadata/tests)
WINE_CONFIG_MAKEFILE(dlls/rpcrt4)
WINE_CONFIG_MAKEFILE(dlls/rpcrt4/tests)
WINE_CONFIG_MAKEFILE(dlls/rsabase)
WINE_CONFIG_MAKEFILE(dlls/rsaenh)
WINE_CONFIG_MAKEFILE(dlls/rsaenh/tests)
WINE_CONFIG_MAKEFILE(dlls/rstrtmgr)
WINE_CONFIG_MAKEFILE(dlls/rtutils)
WINE_CONFIG_MAKEFILE(dlls/rtworkq)
WINE_CONFIG_MAKEFILE(dlls/rtworkq/tests)
WINE_CONFIG_MAKEFILE(dlls/samlib)
WINE_CONFIG_MAKEFILE(dlls/sane.ds)
WINE_CONFIG_MAKEFILE(dlls/sapi)
WINE_CONFIG_MAKEFILE(dlls/sapi/tests)
WINE_CONFIG_MAKEFILE(dlls/sas)
WINE_CONFIG_MAKEFILE(dlls/scarddlg)
WINE_CONFIG_MAKEFILE(dlls/scardsvr)
WINE_CONFIG_MAKEFILE(dlls/sccbase)
WINE_CONFIG_MAKEFILE(dlls/schannel)
WINE_CONFIG_MAKEFILE(dlls/schannel/tests)
WINE_CONFIG_MAKEFILE(dlls/schedsvc)
WINE_CONFIG_MAKEFILE(dlls/schedsvc/tests)
WINE_CONFIG_MAKEFILE(dlls/scrobj)
WINE_CONFIG_MAKEFILE(dlls/scrobj/tests)
WINE_CONFIG_MAKEFILE(dlls/scrrun)
WINE_CONFIG_MAKEFILE(dlls/scrrun/tests)
WINE_CONFIG_MAKEFILE(dlls/scsiport.sys)
WINE_CONFIG_MAKEFILE(dlls/sechost)
WINE_CONFIG_MAKEFILE(dlls/secur32)
WINE_CONFIG_MAKEFILE(dlls/secur32/tests)
WINE_CONFIG_MAKEFILE(dlls/security)
WINE_CONFIG_MAKEFILE(dlls/sensapi)
WINE_CONFIG_MAKEFILE(dlls/serialui)
WINE_CONFIG_MAKEFILE(dlls/serialui/tests)
WINE_CONFIG_MAKEFILE(dlls/setupapi)
WINE_CONFIG_MAKEFILE(dlls/setupapi/tests)
WINE_CONFIG_MAKEFILE(dlls/setupx.dll16,enable_win16)
WINE_CONFIG_MAKEFILE(dlls/sfc)
WINE_CONFIG_MAKEFILE(dlls/sfc_os)
WINE_CONFIG_MAKEFILE(dlls/sharedgpures.sys)
WINE_CONFIG_MAKEFILE(dlls/shcore)
WINE_CONFIG_MAKEFILE(dlls/shcore/tests)
WINE_CONFIG_MAKEFILE(dlls/shdoclc)
WINE_CONFIG_MAKEFILE(dlls/shdocvw)
WINE_CONFIG_MAKEFILE(dlls/shdocvw/tests)
WINE_CONFIG_MAKEFILE(dlls/shell.dll16,enable_win16)
WINE_CONFIG_MAKEFILE(dlls/shell32)
WINE_CONFIG_MAKEFILE(dlls/shell32/tests)
WINE_CONFIG_MAKEFILE(dlls/shfolder)
WINE_CONFIG_MAKEFILE(dlls/shlwapi)
WINE_CONFIG_MAKEFILE(dlls/shlwapi/tests)
WINE_CONFIG_MAKEFILE(dlls/slbcsp)
WINE_CONFIG_MAKEFILE(dlls/slc)
WINE_CONFIG_MAKEFILE(dlls/slc/tests)
WINE_CONFIG_MAKEFILE(dlls/snmpapi)
WINE_CONFIG_MAKEFILE(dlls/snmpapi/tests)
WINE_CONFIG_MAKEFILE(dlls/softpub)
WINE_CONFIG_MAKEFILE(dlls/sound.drv16,enable_win16)
WINE_CONFIG_MAKEFILE(dlls/spoolss)
WINE_CONFIG_MAKEFILE(dlls/spoolss/tests)
WINE_CONFIG_MAKEFILE(dlls/sppc)
WINE_CONFIG_MAKEFILE(dlls/srclient)
WINE_CONFIG_MAKEFILE(dlls/srvcli)
WINE_CONFIG_MAKEFILE(dlls/srvsvc)
WINE_CONFIG_MAKEFILE(dlls/sspicli)
WINE_CONFIG_MAKEFILE(dlls/stdole2.tlb)
WINE_CONFIG_MAKEFILE(dlls/stdole32.tlb)
WINE_CONFIG_MAKEFILE(dlls/sti)
WINE_CONFIG_MAKEFILE(dlls/sti/tests)
WINE_CONFIG_MAKEFILE(dlls/storage.dll16,enable_win16)
WINE_CONFIG_MAKEFILE(dlls/stress.dll16,enable_win16)
WINE_CONFIG_MAKEFILE(dlls/strmdll)
WINE_CONFIG_MAKEFILE(dlls/svrapi)
WINE_CONFIG_MAKEFILE(dlls/sxs)
WINE_CONFIG_MAKEFILE(dlls/sxs/tests)
WINE_CONFIG_MAKEFILE(dlls/system.drv16,enable_win16)
WINE_CONFIG_MAKEFILE(dlls/t2embed)
WINE_CONFIG_MAKEFILE(dlls/t2embed/tests)
WINE_CONFIG_MAKEFILE(dlls/tapi32)
WINE_CONFIG_MAKEFILE(dlls/tapi32/tests)
WINE_CONFIG_MAKEFILE(dlls/taskschd)
WINE_CONFIG_MAKEFILE(dlls/taskschd/tests)
WINE_CONFIG_MAKEFILE(dlls/tbs)
WINE_CONFIG_MAKEFILE(dlls/tdh)
WINE_CONFIG_MAKEFILE(dlls/tdi.sys)
WINE_CONFIG_MAKEFILE(dlls/threadpoolwinrt)
WINE_CONFIG_MAKEFILE(dlls/threadpoolwinrt/tests)
WINE_CONFIG_MAKEFILE(dlls/toolhelp.dll16,enable_win16)
WINE_CONFIG_MAKEFILE(dlls/traffic)
WINE_CONFIG_MAKEFILE(dlls/twain.dll16,enable_win16)
WINE_CONFIG_MAKEFILE(dlls/twain_32)
WINE_CONFIG_MAKEFILE(dlls/twain_32/tests)
WINE_CONFIG_MAKEFILE(dlls/twinapi.appcore)
WINE_CONFIG_MAKEFILE(dlls/twinapi.appcore/tests)
WINE_CONFIG_MAKEFILE(dlls/typelib.dll16,enable_win16)
WINE_CONFIG_MAKEFILE(dlls/tzres)
WINE_CONFIG_MAKEFILE(dlls/ucrtbase)
WINE_CONFIG_MAKEFILE(dlls/ucrtbase/tests)
WINE_CONFIG_MAKEFILE(dlls/uianimation)
WINE_CONFIG_MAKEFILE(dlls/uianimation/tests)
WINE_CONFIG_MAKEFILE(dlls/uiautomationcore)
WINE_CONFIG_MAKEFILE(dlls/uiautomationcore/tests)
WINE_CONFIG_MAKEFILE(dlls/uiribbon)
WINE_CONFIG_MAKEFILE(dlls/unicows)
WINE_CONFIG_MAKEFILE(dlls/updspapi)
WINE_CONFIG_MAKEFILE(dlls/url)
WINE_CONFIG_MAKEFILE(dlls/urlmon)
WINE_CONFIG_MAKEFILE(dlls/urlmon/tests)
WINE_CONFIG_MAKEFILE(dlls/usbd.sys)
WINE_CONFIG_MAKEFILE(dlls/user.exe16,enable_win16)
WINE_CONFIG_MAKEFILE(dlls/user32)
WINE_CONFIG_MAKEFILE(dlls/user32/tests)
WINE_CONFIG_MAKEFILE(dlls/userenv)
WINE_CONFIG_MAKEFILE(dlls/userenv/tests)
WINE_CONFIG_MAKEFILE(dlls/usp10)
WINE_CONFIG_MAKEFILE(dlls/usp10/tests)
WINE_CONFIG_MAKEFILE(dlls/utildll)
WINE_CONFIG_MAKEFILE(dlls/uxtheme)
WINE_CONFIG_MAKEFILE(dlls/uxtheme/tests)
WINE_CONFIG_MAKEFILE(dlls/vbscript)
WINE_CONFIG_MAKEFILE(dlls/vbscript/tests)
WINE_CONFIG_MAKEFILE(dlls/vcomp)
WINE_CONFIG_MAKEFILE(dlls/vcomp/tests)
WINE_CONFIG_MAKEFILE(dlls/vcomp100)
WINE_CONFIG_MAKEFILE(dlls/vcomp110)
WINE_CONFIG_MAKEFILE(dlls/vcomp110/tests)
WINE_CONFIG_MAKEFILE(dlls/vcomp120)
WINE_CONFIG_MAKEFILE(dlls/vcomp140)
WINE_CONFIG_MAKEFILE(dlls/vcomp90)
WINE_CONFIG_MAKEFILE(dlls/vcruntime140)
WINE_CONFIG_MAKEFILE(dlls/vcruntime140_1)
WINE_CONFIG_MAKEFILE(dlls/vdhcp.vxd,enable_win16)
WINE_CONFIG_MAKEFILE(dlls/vdmdbg)
WINE_CONFIG_MAKEFILE(dlls/ver.dll16,enable_win16)
WINE_CONFIG_MAKEFILE(dlls/version)
WINE_CONFIG_MAKEFILE(dlls/version/tests)
WINE_CONFIG_MAKEFILE(dlls/vga)
WINE_CONFIG_MAKEFILE(dlls/vidreszr)
WINE_CONFIG_MAKEFILE(dlls/virtdisk)
WINE_CONFIG_MAKEFILE(dlls/virtdisk/tests)
WINE_CONFIG_MAKEFILE(dlls/vmm.vxd,enable_win16)
WINE_CONFIG_MAKEFILE(dlls/vnbt.vxd,enable_win16)
WINE_CONFIG_MAKEFILE(dlls/vnetbios.vxd,enable_win16)
WINE_CONFIG_MAKEFILE(dlls/vssapi)
WINE_CONFIG_MAKEFILE(dlls/vtdapi.vxd,enable_win16)
WINE_CONFIG_MAKEFILE(dlls/vulkan-1)
WINE_CONFIG_MAKEFILE(dlls/vulkan-1/tests)
WINE_CONFIG_MAKEFILE(dlls/vwin32.vxd,enable_win16)
WINE_CONFIG_MAKEFILE(dlls/w32skrnl,enable_win16)
WINE_CONFIG_MAKEFILE(dlls/w32sys.dll16,enable_win16)
WINE_CONFIG_MAKEFILE(dlls/wbemdisp)
WINE_CONFIG_MAKEFILE(dlls/wbemdisp/tests)
WINE_CONFIG_MAKEFILE(dlls/wbemprox)
WINE_CONFIG_MAKEFILE(dlls/wbemprox/tests)
WINE_CONFIG_MAKEFILE(dlls/wdscore)
WINE_CONFIG_MAKEFILE(dlls/webservices)
WINE_CONFIG_MAKEFILE(dlls/webservices/tests)
WINE_CONFIG_MAKEFILE(dlls/websocket)
WINE_CONFIG_MAKEFILE(dlls/wer)
WINE_CONFIG_MAKEFILE(dlls/wer/tests)
WINE_CONFIG_MAKEFILE(dlls/wevtapi)
WINE_CONFIG_MAKEFILE(dlls/wevtapi/tests)
WINE_CONFIG_MAKEFILE(dlls/wevtsvc)
WINE_CONFIG_MAKEFILE(dlls/wiaservc)
WINE_CONFIG_MAKEFILE(dlls/wiaservc/tests)
WINE_CONFIG_MAKEFILE(dlls/wimgapi)
WINE_CONFIG_MAKEFILE(dlls/win32s16.dll16,enable_win16)
WINE_CONFIG_MAKEFILE(dlls/win32u)
WINE_CONFIG_MAKEFILE(dlls/win32u/tests)
WINE_CONFIG_MAKEFILE(dlls/win87em.dll16,enable_win16)
WINE_CONFIG_MAKEFILE(dlls/winaspi.dll16,enable_win16)
WINE_CONFIG_MAKEFILE(dlls/windebug.dll16,enable_win16)
WINE_CONFIG_MAKEFILE(dlls/windows.applicationmodel)
WINE_CONFIG_MAKEFILE(dlls/windows.applicationmodel/tests)
WINE_CONFIG_MAKEFILE(dlls/windows.devices.bluetooth)
WINE_CONFIG_MAKEFILE(dlls/windows.devices.bluetooth/tests)
WINE_CONFIG_MAKEFILE(dlls/windows.devices.enumeration)
WINE_CONFIG_MAKEFILE(dlls/windows.devices.enumeration/tests)
WINE_CONFIG_MAKEFILE(dlls/windows.devices.usb)
WINE_CONFIG_MAKEFILE(dlls/windows.devices.usb/tests)
WINE_CONFIG_MAKEFILE(dlls/windows.gaming.input)
WINE_CONFIG_MAKEFILE(dlls/windows.gaming.input/tests)
WINE_CONFIG_MAKEFILE(dlls/windows.gaming.ui.gamebar)
WINE_CONFIG_MAKEFILE(dlls/windows.gaming.ui.gamebar/tests)
WINE_CONFIG_MAKEFILE(dlls/windows.globalization)
WINE_CONFIG_MAKEFILE(dlls/windows.globalization/tests)
WINE_CONFIG_MAKEFILE(dlls/windows.media.devices)
WINE_CONFIG_MAKEFILE(dlls/windows.media.devices/tests)
WINE_CONFIG_MAKEFILE(dlls/windows.media.mediacontrol)
WINE_CONFIG_MAKEFILE(dlls/windows.media.mediacontrol/tests)
WINE_CONFIG_MAKEFILE(dlls/windows.media.speech)
WINE_CONFIG_MAKEFILE(dlls/windows.media.speech/tests)
WINE_CONFIG_MAKEFILE(dlls/windows.media)
WINE_CONFIG_MAKEFILE(dlls/windows.media/tests)
WINE_CONFIG_MAKEFILE(dlls/windows.networking.connectivity)
WINE_CONFIG_MAKEFILE(dlls/windows.networking.connectivity/tests)
WINE_CONFIG_MAKEFILE(dlls/windows.networking.hostname)
WINE_CONFIG_MAKEFILE(dlls/windows.networking.hostname/tests)
WINE_CONFIG_MAKEFILE(dlls/windows.networking)
WINE_CONFIG_MAKEFILE(dlls/windows.perception.stub)
WINE_CONFIG_MAKEFILE(dlls/windows.perception.stub/tests)
WINE_CONFIG_MAKEFILE(dlls/windows.security.authentication.onlineid)
WINE_CONFIG_MAKEFILE(dlls/windows.security.authentication.onlineid/tests)
WINE_CONFIG_MAKEFILE(dlls/windows.security.credentials.ui.userconsentverifier)
WINE_CONFIG_MAKEFILE(dlls/windows.security.credentials.ui.userconsentverifier/tests)
WINE_CONFIG_MAKEFILE(dlls/windows.storage.applicationdata)
WINE_CONFIG_MAKEFILE(dlls/windows.storage.applicationdata/tests)
WINE_CONFIG_MAKEFILE(dlls/windows.system.profile.systemmanufacturers)
WINE_CONFIG_MAKEFILE(dlls/windows.system.profile.systemmanufacturers/tests)
WINE_CONFIG_MAKEFILE(dlls/windows.ui)
WINE_CONFIG_MAKEFILE(dlls/windows.ui/tests)
WINE_CONFIG_MAKEFILE(dlls/windows.web)
WINE_CONFIG_MAKEFILE(dlls/windows.web/tests)
WINE_CONFIG_MAKEFILE(dlls/windowscodecs)
WINE_CONFIG_MAKEFILE(dlls/windowscodecs/tests)
WINE_CONFIG_MAKEFILE(dlls/windowscodecsext)
WINE_CONFIG_MAKEFILE(dlls/windowscodecsext/tests)
WINE_CONFIG_MAKEFILE(dlls/winealsa.drv)
WINE_CONFIG_MAKEFILE(dlls/wineandroid.drv)
WINE_CONFIG_MAKEFILE(dlls/winebth.sys)
WINE_CONFIG_MAKEFILE(dlls/winebus.sys)
WINE_CONFIG_MAKEFILE(dlls/winecoreaudio.drv)
WINE_CONFIG_MAKEFILE(dlls/winecrt0)
WINE_CONFIG_MAKEFILE(dlls/wined3d)
WINE_CONFIG_MAKEFILE(dlls/winedmo)
WINE_CONFIG_MAKEFILE(dlls/winegstreamer)
WINE_CONFIG_MAKEFILE(dlls/winehid.sys)
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
WINE_CONFIG_MAKEFILE(programs/winedevice)
WINE_CONFIG_MAKEFILE(programs/winefile)
WINE_CONFIG_MAKEFILE(programs/winemenubuilder)
WINE_CONFIG_MAKEFILE(programs/winemine)
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
