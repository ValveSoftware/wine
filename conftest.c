/* confdefs.h */
#define PACKAGE_NAME "Wine"
#define PACKAGE_TARNAME "wine"
#define PACKAGE_VERSION "10.0"
#define PACKAGE_STRING "Wine 10.0"
#define PACKAGE_BUGREPORT "wine-devel@winehq.org"
#define PACKAGE_URL "https://www.winehq.org"
#define HAVE_LINUX_GETHOSTBYNAME_R_6 1
#define HAVE_SIGINFO_T_SI_FD 1
#define STDC_HEADERS 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRING_H 1
#define HAVE_MEMORY_H 1
#define HAVE_STRINGS_H 1
#define HAVE_INTTYPES_H 1
#define HAVE_STDINT_H 1
#define HAVE_UNISTD_H 1
#define HAVE_STRUCT_STAT_ST_MTIM 1
#define HAVE_STRUCT_STAT_ST_CTIM 1
#define HAVE_STRUCT_STAT_ST_ATIM 1
/* end confdefs.h.  */
#include <sys/types.h>
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

int
main ()
{
static struct sockaddr_in6 ac_aggr;
if (sizeof ac_aggr.sin6_scope_id)
return 0;
  ;
  return 0;
}
