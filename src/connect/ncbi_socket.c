/*  $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software/database is freely available
 *  to the public for use. The National Library of Medicine and the U.S.
 *  Government have not placed any restriction on its use or reproduction.
 *
 *  Although all reasonable efforts have been taken to ensure the accuracy
 *  and reliability of the software and data, the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties, express or implied, including
 *  warranties of performance, merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Author:  Denis Vakatov
 *
 * File Description:
 *   Plain portable TCP/IP socket API for:  UNIX, MS-Win, MacOS
 *     [UNIX ]   -DNCBI_OS_UNIX     -lresolv -lsocket -lnsl
 *     [MSWIN]   -DNCBI_OS_MSWIN    wsock32.lib
 *     [MacOS]   -DNCBI_OS_MAC      NCSASOCK -- BSD-style socket emulation lib
 *
 */

#include "ncbi_config.h"

/* OS must be specified in the command-line ("-D....") or in the conf. header
 */
#if !defined(NCBI_OS_UNIX) && !defined(NCBI_OS_MSWIN) && !defined(NCBI_OS_MAC)
#  error "Unknown OS, must be one of NCBI_OS_UNIX, NCBI_OS_MSWIN, NCBI_OS_MAC!"
#endif /*supported platforms*/


/* Uncomment these(or specify "-DHAVE_GETADDRINFO -DHAVE_GETNAMEINFO") only if:
 * 0) you are compiling this outside of the NCBI C or C++ Toolkits
 *    (USE_NCBICONF is not #define'd), and
 * 1) your platform has "getaddrinfo()" and "getnameinfo()", and
 * 2) you are going to use this API code in multi-thread application, and
 * 3) "gethostbyname()" gets called somewhere else in your code
 */

/* #define HAVE_GETADDRINFO 1 */
/* #define HAVE_GETNAMEINFO 1 */


/* Uncomment this (or specify "-DHAVE_GETHOSTBY***_R=") only if:
 * 0) you are compiling this outside of the NCBI C or C++ Toolkits
 *    (USE_NCBICONF is not #define'd), and
 * 1) your platform has "gethostbyname_r()" but not "getnameinfo()", and
 * 2) you are going to use this API code in multi-thread application, and
 * 3) "gethostbyname()" gets called somewhere else in your code
 */

/*   Solaris: */
/* #define HAVE_GETHOSTBYNAME_R 5 */
/* #define HAVE_GETHOSTBYADDR_R 7 */

/*   Linux, IRIX: */
/* #define HAVE_GETHOSTBYNAME_R 6 */
/* #define HAVE_GETHOSTBYADDR_R 8 */


/* Uncomment this (or specify "-DHAVE_SIN_LEN") only if:
 * 0) you are compiling this outside of the NCBI C or C++ Toolkits
 *    (USE_NCBICONF is not #define'd), and
 * 1) on your platform, struct sockaddr_in contains a field called "sin_len"
 */

/* #define HAVE_SIN_LEN 1 */


/* Platform-specific system headers
 */

#if defined(NCBI_OS_UNIX)
#  include <sys/time.h>
#  include <unistd.h>
#  ifdef NCBI_COMPILER_MW_MSL
#    include <ncbi_mslextras.h>
#  else
#    include <netdb.h>
#  endif
#  include <fcntl.h>
#  include <sys/socket.h>
#  include <netinet/in.h>
#  if !defined(NCBI_OS_BEOS) && !defined(NCBI_COMPILER_MW_MSL)
#    include <arpa/inet.h>
#  endif /*NCBI_OS_BEOS*/
#  include <signal.h>
#  include <sys/un.h>

#elif defined(NCBI_OS_MSWIN)
#  ifndef COMP_METRO
#    include <winsock2.h>
#  else
#    define SD_RECEIVE      0x00
#    define SD_SEND         0x01
#    define SD_BOTH         0x02
#  endif
#elif defined(NCBI_OS_MAC)
#  include <unistd.h>
#  include <sock_ext.h>
#  include <netdb.h>
#  include <s_types.h>
#  include <s_socket.h>
#  include <neti_in.h>
#  include <a_inet.h>
#  include <neterrno.h> /* missing error numbers on Mac */

#else
#  error "Unsupported platform, must be one of NCBI_OS_UNIX, NCBI_OS_MSWIN, NCBI_OS_MAC !!!"
#endif /* platform-specific headers (for UNIX, MSWIN, MAC) */


/* NCBI core headers
 */
#include "ncbi_ansi_ext.h"
#include "ncbi_priv.h"
#include <connect/ncbi_buffer.h>
#include <connect/ncbi_connutil.h>
#include <connect/ncbi_socket.h>


/* Portable standard C headers
 */
#include <errno.h>
#include <stdlib.h>
#include <string.h>




/******************************************************************************
 *  TYPEDEFS & MACROS
 */


/* Minimal size of the data buffer chunk in the socket internal buffer(s) */
#define SOCK_BUF_CHUNK_SIZE 4096


/* Macro #def for the platform-dependent constants, error codes and functions
 */
#if defined(NCBI_OS_MSWIN)

typedef SOCKET TSOCK_Handle;
#  define SOCK_INVALID        INVALID_SOCKET
#  define SOCK_ERRNO          WSAGetLastError()
#  define SOCK_EINTR          WSAEINTR
#  define SOCK_EWOULDBLOCK    WSAEWOULDBLOCK
#  define SOCK_ECONNRESET     WSAECONNRESET
#  define SOCK_EPIPE          WSAESHUTDOWN
#  define SOCK_EAGAIN         WSAEINPROGRESS
#  define SOCK_EINPROGRESS    WSAEINPROGRESS
#  define SOCK_ENOTCONN       WSAENOTCONN
#  define SOCK_ECONNABORTED   WSAECONNABORTED
#  define SOCK_ECONNREFUSED   WSAECONNREFUSED
#  define SOCK_ENETRESET      WSAENETRESET
#  define SOCK_NFDS(s)        0
#  define SOCK_CLOSE(s)       closesocket(s)
#  define SOCK_SHUTDOWN(s,h)  shutdown(s,h)
#  define SOCK_SHUTDOWN_RD    SD_RECEIVE
#  define SOCK_SHUTDOWN_WR    SD_SEND
#  define SOCK_SHUTDOWN_RDWR  SD_BOTH
#  define SOCK_STRERROR(err)  s_StrError(err)
/* NCBI_OS_MSWIN */

#elif defined(NCBI_OS_UNIX)

typedef int TSOCK_Handle;
#  define SOCK_INVALID        (-1)
#  define SOCK_ERRNO          errno
#  define SOCK_EINTR          EINTR
#  define SOCK_EWOULDBLOCK    EWOULDBLOCK
#  define SOCK_ECONNRESET     ECONNRESET
#  define SOCK_EPIPE          EPIPE
#  define SOCK_EAGAIN         EAGAIN
#  define SOCK_EINPROGRESS    EINPROGRESS
#  define SOCK_ENOTCONN       ENOTCONN
#  define SOCK_ECONNABORTED   ECONNABORTED
#  define SOCK_ECONNREFUSED   ECONNREFUSED
#  define SOCK_ENETRESET      ENETRESET
#  define SOCK_NFDS(s)        (s + 1)
#  ifdef NCBI_OS_BEOS
#    define SOCK_CLOSE(s)     closesocket(s)
#  else
#    define SOCK_CLOSE(s)     close(s)	
#  endif /*NCBI_OS_BEOS*/
#  define SOCK_SHUTDOWN(s,h)  shutdown(s,h)
#  ifndef SHUT_RD
#    define SHUT_RD           0
#  endif /*SHUT_RD*/
#  define SOCK_SHUTDOWN_RD    SHUT_RD
#  ifndef SHUT_WR
#    define SHUT_WR           1
#  endif /*SHUT_WR*/
#  define SOCK_SHUTDOWN_WR    SHUT_WR
#  ifndef SHUT_RDWR
#    define SHUT_RDWR         2
#  endif /*SHUT_RDWR*/
#  define SOCK_SHUTDOWN_RDWR  SHUT_RDWR
#  ifndef INADDR_NONE
#    define INADDR_NONE       (unsigned int)(-1)
#  endif /*INADDR_NONE*/
#  define SOCK_STRERROR(err)  s_StrError(err)
/* NCBI_OS_UNIX */

#elif defined(NCBI_OS_MAC)

#  if TARGET_API_MAC_CARBON
#    define O_NONBLOCK kO_NONBLOCK
#  endif /*TARGET_API_MAC_CARBON*/

typedef int TSOCK_Handle;
#  define SOCK_INVALID        (-1)
#  ifndef SOCK_ERRNO
#    define SOCK_ERRNO        errno
#  endif /*SOCK_ERRNO*/
#  define SOCK_EINTR          EINTR
#  define SOCK_EWOULDBLOCK    EWOULDBLOCK
#  define SOCK_ECONNRESET     ECONNRESET
#  define SOCK_EPIPE          EPIPE
#  define SOCK_EAGAIN         EAGAIN
#  define SOCK_EINPROGRESS    EINPROGRESS
#  define SOCK_ENOTCONN       ENOTCONN
#  define SOCK_ECONNABORTED   ECONNABORTED
#  define SOCK_ECONNREFUSED   ECONNREFUSED
#  define SOCK_ENETRESET      ENETRESET
#  define SOCK_NFDS(s)        (s + 1)
#  define SOCK_CLOSE(s)       close(s)
#  define SOCK_SHUTDOWN(s,h)  shutdown(s,h)
#  define SOCK_SHUTDOWN_RD    0
#  define SOCK_SHUTDOWN_WR    1
#  define SOCK_SHUTDOWN_RDWR  2
#  define SOCK_STRERROR(err)  s_StrError(err)
#  ifdef NETDB_INTERNAL
#    undef NETDB_INTERNAL
#  endif /*NETDB_INTERNAL*/

#endif /*NCBI_OS_MSWIN, NCBI_OS_UNIX, NCBI_OS_MAC*/


#ifdef HAVE_SOCKLEN_T
typedef socklen_t  SOCK_socklen_t;
#else
typedef int        SOCK_socklen_t;
#endif /*HAVE_SOCKLEN_T*/


/* Listening socket
 */
typedef struct LSOCK_tag {
    TSOCK_Handle    sock;       /* OS-specific socket handle                 */
    unsigned int    id;         /* the internal ID (see also "s_ID_Counter") */

    unsigned int    n_accept;   /* total number of accepted clients          */
    ESwitch         log;
} LSOCK_struct;


/* Type of socket (except listening)
 */
typedef enum {
    eSOCK_Datagram = 0,
    eSOCK_ClientSide,
    eSOCK_ServerSide,
    eSOCK_ServerSideKeep
} ESockType;


#if 0/*defined(__GNUC__)*/
typedef ESwitch    EBSwitch;
typedef EIO_Status EBIO_Status;
typedef ESockType  EBSockType;
#else
typedef unsigned   EBSwitch;
typedef unsigned   EBIO_Status;
typedef unsigned   EBSockType;
#endif


/* Socket
 */
typedef struct SOCK_tag {
    TSOCK_Handle    sock;       /* OS-specific socket handle                 */
    unsigned int    id;         /* the internal ID (see also "s_ID_Counter") */

    /* connection point */
    unsigned int    host;       /* peer host (in the network byte order)     */
    unsigned short  port;       /* peer port (in the network byte order)     */

    /* type, status, EOF, log, read-on-write etc bit-field indicators */
    EBSwitch             log:2; /* how to log events and data for this socket*/
    EBSockType          type:2; /* socket type: client- or server-side, dgram*/
    EBSwitch          r_on_w:2; /* enable/disable automatic read-on-write    */
    EBSwitch        i_on_sig:2; /* enable/disable I/O restart on signals     */
    EBIO_Status     r_status:3; /* read  status:  eIO_Closed if was shut down*/
    unsigned/*bool*/     eof:1; /* Stream sockets: 'End of file' seen on read
                                   Datagram socks: 'End of message' written  */
    EBIO_Status     w_status:3; /* write status:  eIO_Closed if was shut down*/
    unsigned/*bool*/ pending:1; /* != 0 if connection is still pending       */

    /* timeouts */
    const struct timeval* r_timeout;/* NULL if infinite, or points to "r_tv" */
    struct timeval  r_tv;       /* finite read timeout value                 */
    STimeout        r_to;       /* finite read timeout value (aux., temp.)   */
    const struct timeval* w_timeout;/* NULL if infinite, or points to "w_tv" */
    struct timeval  w_tv;       /* finite write timeout value                */
    STimeout        w_to;       /* finite write timeout value (aux., temp.)  */
    const struct timeval* c_timeout;/* NULL if infinite, or points to "c_tv" */
    struct timeval  c_tv;       /* finite close timeout value                */
    STimeout        c_to;       /* finite close timeout value (aux., temp.)  */

    /* aux I/O data */
    BUF             r_buf;      /* read  buffer                              */
    BUF             w_buf;      /* write buffer                              */
    size_t          w_len;      /* SOCK: how much data is pending for output */

    /* statistics */
    size_t          n_read;     /* DSOCK: total #; SOCK: last connect/ only  */
    size_t          n_written;  /* DSOCK: total #; SOCK: last /session only  */
    size_t          n_in;       /* DSOCK: msg #; SOCK: total # of bytes read */
    size_t          n_out;      /* DSOCK: msg #; SOCK: total # of bytes sent */

#ifdef NCBI_OS_UNIX
    /* filename for UNIX socket */
    char            file[1];
#endif /*NCBI_OS_UNIX*/
} SOCK_struct;


/*
 * Please note the following implementation details:
 *
 * 1. w_buf is used for stream sockets to keep initial data segment
 *    that has to be sent upon the connection establishment.
 *
 * 2. eof is used differently for stream and datagram sockets:
 *    =1 for stream sockets means that read had hit EOF;
 *    =1 for datagram sockets means that the message in w_buf is complete.
 *
 * 3. r_status keeps completion code of the last low-level read call;
 *    however, eIO_Closed is there when the socket is shut down for reading;
 *    see the table below for full details on stream sockets.
 *
 * 4. w_status keeps completion code of the last low-level write call;
 *    however, eIO_Closed is there when the socket is shut down for writing.
 *
 * 5. The following table depicts r_status and eof combinations and their
 *    meanings for stream sockets:
 * -------------------------------+------------------------------------------
 *              Field             |
 * ---------------+---------------+                  Meaning
 * sock->r_status |   sock->eof   |           (stream sockets only)
 * ---------------+---------------+------------------------------------------
 * eIO_Closed     |       0       |  Socket shut down for reading
 * eIO_Closed     |       1       |  Read severely failed
 * not eIO_Closed |       0       |  Read completed with r_status error
 * not eIO_Closed |       1       |  Read hit EOF and completed with r_status
 * ---------------+---------------+------------------------------------------
 */


/* Globals:
 */


/* Flag to indicate whether the API has been initialized */
static int/*bool*/ s_Initialized = 0/*false*/;

/* SOCK counter */
static unsigned int s_ID_Counter = 0;

/* Read-while-writing switch */
static ESwitch s_ReadOnWrite = eOff;       /* no read-on-write by default */

/* Reuse address flag for newly created stream sockets */
static int/*bool*/ s_ReuseAddress = 0;     /* off by default              */

/* I/O restart on signals */
static ESwitch s_InterruptOnSignal = eOff; /* restart I/O by default      */

/* Data/event logging */
static ESwitch s_Log = eOff;               /* no logging by default       */

/* Flag to indicate whether API should mask SIGPIPE (during initialization) */
#ifdef NCBI_OS_UNIX
static int/*bool*/ s_AllowSigPipe = 0/*false - mask SIGPIPE out*/;
#endif /*NCBI_OS_UNIX*/


/******************************************************************************
 *   Error reporting
 */


static const char* s_StrError(int error)
{
    static struct {
        int         errnum;
        const char* errtxt;
    } errmap[] = {
#ifdef NCBI_OS_MSWIN
        {WSAEINTR,  "Interrupted system call"},
        {WSAEBADF,  "Bad file number"},
        {WSAEACCES, "Access denied"},
        {WSAEFAULT, "Segmentation fault"},
        {WSAEINVAL, "Invalid agrument"},
        {WSAEMFILE, "Too many open files"},
        /*
         * Windows Sockets definitions of regular Berkeley error constants
         */
        {WSAEWOULDBLOCK,     "Resource temporarily unavailable"},
        {WSAEINPROGRESS,     "Operation now in progress"},
        {WSAEALREADY,        "Operation already in progress"},
        {WSAENOTSOCK,        "Not a socket"},
        {WSAEDESTADDRREQ,    "Destination address required"},
        {WSAEMSGSIZE,        "Invalid message size"},
        {WSAEPROTOTYPE,      "Wrong protocol type"},
        {WSAENOPROTOOPT,     "Bad protocol option"},
        {WSAEPROTONOSUPPORT, "Protocol not supported"},
        {WSAESOCKTNOSUPPORT, "Socket type not supported"},
        {WSAEOPNOTSUPP,      "Operation not supported"},
        {WSAEPFNOSUPPORT,    "Protocol family not supported"},
        {WSAEAFNOSUPPORT,    "Address family not supported"},
        {WSAEADDRINUSE,      "Address already in use"},
        {WSAEADDRNOTAVAIL,   "Cannot assign requested address"},
        {WSAENETDOWN,        "Network is down"},
        {WSAENETUNREACH,     "Network is unreachable"},
        {WSAENETRESET,       "Connection dropped on network reset"},
        {WSAECONNABORTED,    "Software caused connection abort"},
        {WSAECONNRESET,      "Connection reset by peer"},
        {WSAENOBUFS,         "No buffer space available"},
        {WSAEISCONN,         "Socket is already connected"},
        {WSAENOTCONN,        "Socket is not connected"},
        {WSAESHUTDOWN,       "Cannot send after socket shutdown"},
        {WSAETOOMANYREFS,    "Too many references"},
        {WSAETIMEDOUT,       "Operation timed out"},
        {WSAECONNREFUSED,    "Connection refused"},
        {WSAELOOP,           "Infinite loop"},
        {WSAENAMETOOLONG,    "Name too long"},
        {WSAEHOSTDOWN,       "Host is down"},
        {WSAEHOSTUNREACH,    "Host unreachable"},
        {WSAENOTEMPTY,       "Not empty"},
        {WSAEPROCLIM,        "Too many processes"},
        {WSAEUSERS,          "Too many users"},
        {WSAEDQUOT,          "Quota exceeded"},
        {WSAESTALE,          "Stale descriptor"},
        {WSAEREMOTE,         "Remote error"},
        /*
         * Extended Windows Sockets error constant definitions
         */
        {WSASYSNOTREADY,         "Network subsystem is unavailable"},
        {WSAVERNOTSUPPORTED,     "Winsock.dll version out of range"},
        {WSANOTINITIALISED,      "Not yet initialized"},
        {WSAEDISCON,             "Graceful shutdown in progress"},
        {WSAENOMORE,             "No more retries"},
        {WSAECANCELLED,          "Cancelled"},
        {WSAEINVALIDPROCTABLE,   "Invalid procedure table"},
        {WSAEINVALIDPROVIDER,    "Invalid provider version number"},
        {WSAEPROVIDERFAILEDINIT, "Cannot init provider"},
        {WSASYSCALLFAILURE,      "System call failed"},
        {WSASERVICE_NOT_FOUND,   "Service not found"},
        {WSATYPE_NOT_FOUND,      "Class type not found"},
        {WSA_E_NO_MORE,          "WSA_E_NO_MORE"},
        {WSA_E_CANCELLED,        "WSA_E_CANCELLED"},
        {WSAEREFUSED,            "Refused"},
#endif /*NCBI_OS_MSWIN*/
#ifdef NCBI_OS_MSWIN
#  define EAI_BASE 0
#else
#  define EAI_BASE 100000
#endif /*NCBI_OS_MSWIN*/
#ifdef EAI_ADDRFAMILY
        {EAI_ADDRFAMILY + EAI_BASE,
                                 "Address family not supported"},
#endif /*EAI_ADDRFAMILY*/
#ifdef EAI_AGAIN
        {EAI_AGAIN + EAI_BASE,
                                 "Temporary failure in name resolution"},
#endif /*EAI_AGAIN*/
#ifdef EAI_BADFLAGS
        {EAI_BADFLAGS + EAI_BASE,
                                 "Invalid value for lookup flags"},
#endif /*EAI_BADFLAGS*/
#ifdef EAI_FAIL
        {EAI_FAIL + EAI_BASE,
                                 "Non-recoverable failure in name resolution"},
#endif /*EAI_FAIL*/
#ifdef EAI_FAMILY
        {EAI_FAMILY + EAI_BASE,
                                 "Address family not supported"},
#endif /*EAI_FAMILY*/
#ifdef EAI_MEMORY
        {EAI_MEMORY + EAI_BASE,
                                 "Memory allocation failure"},
#endif /*EAI_MEMORY*/
#ifdef EAI_NODATA
        {EAI_NODATA + EAI_BASE,
                                 "No address associated with nodename"},
#endif /*EAI_NODATA*/
#ifdef EAI_NONAME
        {EAI_NONAME + EAI_BASE,
                                 "Host/service name not known"},
#endif /*EAI_NONAME*/
#ifdef EAI_SERVICE
        {EAI_SERVICE + EAI_BASE,
                                 "Service name not supported for socket type"},
#endif /*EAI_SERVICE*/
#ifdef EAI_SOCKTYPE
        {EAI_SOCKTYPE + EAI_BASE,
                                 "Socket type not supported"},
#endif /*EAI_SOCKTYPE*/
#ifdef NCBI_OS_MSWIN
#  define DNS_BASE 0
#else
#  define DNS_BASE 200000
#endif /*NCBI_OS_MSWIN*/
#ifdef HOST_NOT_FOUND
        {HOST_NOT_FOUND + DNS_BASE,
                                 "Host not found"},
#endif /*HOST_NOT_FOUND*/
#ifdef TRY_AGAIN
        {TRY_AGAIN + DNS_BASE,
                                 "DNS server failure"},
#endif /*TRY_AGAIN*/
#ifdef NO_RECOVERY
        {NO_RECOVERY + DNS_BASE,
                                 "Unrecoverable DNS error"},
#endif /*NO_RECOVERY*/
#ifdef NO_DATA
        {NO_DATA + DNS_BASE,
                                 "No DNS data of requested type"},
#endif /*NO_DATA*/
#ifdef NO_ADDRESS
        {NO_ADDRESS + DNS_BASE,
                                 "No address record found in DNS"},
#endif /*NO_ADDRESS*/

        /* Last dummy entry - must present */
        {0, 0}
    };
    size_t i, n = sizeof(errmap)/sizeof(errmap[0]) - 1/*dummy entry*/;

    /* always called on error, so get error number here if not having already*/
    if ( !error )
        error = errno;
    for (i = 0; i < n; i++) {
        if (errmap[i].errnum == error)
            return errmap[i].errtxt;
    }
    return strerror(error);
}


/******************************************************************************
 *   Data Logging
 */


static const char* s_ID(const SOCK sock, char* buf)
{
    const char* sname;

    if ( !sock )
        return "";
    sname = sock->id < 1000 ? "LSOCK" : "SOCK";
    if (sock->sock == SOCK_INVALID)
        sprintf(buf, "%s#%u[?]: ",  sname, sock->id);
    else
        sprintf(buf, "%s#%u[%u]: ", sname, sock->id, (unsigned int)sock->sock);
    return buf;
}


/* Put socket description to the message, then log the transferred data
 */
static void s_DoLog
(const SOCK  sock, EIO_Event event,
 const void* data, size_t    size,  const struct sockaddr* sa)
{
    char head[128];
    char tail[128];
    char _id[32];

    if ( !CORE_GetLOG() )
        return;

    assert(sock);
    switch (event) {
    case eIO_Open:
        if (sock->type == eSOCK_Datagram) {
            if ( !sa ) {
                strcpy(head, "Datagram socket created");
            } else {
                const struct sockaddr_in* sin = (const struct sockaddr_in*) sa;
                if ( !data ) {
                    strcpy(head, "Datagram socket bound to port :");
                    sprintf(tail, "%hu", ntohs(sin->sin_port));
                } else {
                    strcpy(head, "Datagram socket connected to ");
                    HostPortToString(sin->sin_addr.s_addr,ntohs(sin->sin_port),
                                     tail, sizeof(tail));
                }
            }
        } else {
            if (sock->type == eSOCK_ClientSide)
                strcpy(head, "Connecting to ");
            else if ( data )
                strcpy(head, "Connected to ");
            else
                strcpy(head, "Accepted from ");
            if (sa->sa_family == AF_INET) {
                const struct sockaddr_in* sin = (const struct sockaddr_in*) sa;
                HostPortToString(sin->sin_addr.s_addr, ntohs(sin->sin_port),
                                 tail, sizeof(tail));
            }
#ifdef NCBI_OS_UNIX
            else if (sa->sa_family == AF_UNIX) {
                const struct sockaddr_un* un = (const struct sockaddr_un*) sa;
                strncpy0(tail, un->sun_path, sizeof(tail) - 1);
            }
#endif /*NCBI_OS_UNIX*/
            else
                strcpy(tail, "???");
        }
        CORE_LOGF(eLOG_Trace, ("%s%s%s", s_ID(sock, _id), head, tail));
        break;
    case eIO_Read:
    case eIO_Write:
        if (sock->type == eSOCK_Datagram) {
            const struct sockaddr_in* sin = (const struct sockaddr_in*) sa;
            assert(sa && sa->sa_family == AF_INET);
            HostPortToString(sin->sin_addr.s_addr, ntohs(sin->sin_port),
                             tail, sizeof(tail));
            sprintf(tail + strlen(tail), ", msg# %u",
                    (unsigned)(event == eIO_Read ? sock->n_in : sock->n_out));
        } else {
            assert(sa == 0);
            *tail = 0;
        }
        sprintf(head, "%s%s%s at offset %lu%s%s", s_ID(sock, _id),
                event == eIO_Read
                ? (sock->type != eSOCK_Datagram  &&  !size
                   ? (data ? "EOF hit" : SOCK_STRERROR(SOCK_ERRNO))
                   : "Read")
                : (sock->type != eSOCK_Datagram  &&  !size
                   ? SOCK_STRERROR(SOCK_ERRNO) : "Written"),
                sock->type == eSOCK_Datagram  ||  size ? "" :
                (event == eIO_Read ? " while reading" : " while writing"),
                (unsigned long) (event == eIO_Read
                                 ? sock->n_read : sock->n_written),
                sa ? (event == eIO_Read ? " from " : " to ") : "", tail);
        CORE_DATA(data, size, head);
        break;
    case eIO_Close:
        {{
            int n = sprintf(head, "%lu byte%s",
                            (unsigned long) sock->n_written,
                            sock->n_written == 1 ? "" : "s");
            if (sock->type == eSOCK_Datagram  ||
                sock->n_out != sock->n_written) {
                sprintf(head + n, "/%lu %s%s",
                        (unsigned long) sock->n_out,
                        sock->type == eSOCK_Datagram ? "msg" : "total byte",
                        sock->n_out == 1 ? "" : "s");
            }
        }}
        {{
            int n = sprintf(tail, "%lu byte%s",
                            (unsigned long) sock->n_read,
                            sock->n_read == 1 ? "" : "s");
            if (sock->type == eSOCK_Datagram  ||
                sock->n_in != sock->n_read) {
                sprintf(tail + n, "/%lu %s%s",
                        (unsigned long) sock->n_in,
                        sock->type == eSOCK_Datagram ? "msg" : "total byte",
                        sock->n_in == 1 ? "" : "s");
            }
        }}
        CORE_LOGF(eLOG_Trace, ("%s%s (out: %s, in: %s)", s_ID(sock, _id),
                               sock->type == eSOCK_ServerSideKeep
                               ? "Leaving" : "Closing", head,tail));
        break;
    default:
        CORE_LOGF(eLOG_Error, ("%s[SOCK::s_DoLog]  Invalid event %u",
                               s_ID(sock, _id), (unsigned int) event));
        assert(0);
        break;
    }
}


extern void SOCK_SetDataLoggingAPI(ESwitch log)
{
    if (log == eDefault)
        log = eOff;
    s_Log = log;
}


extern void SOCK_SetDataLogging(SOCK sock, ESwitch log)
{
    sock->log = log;
}



/******************************************************************************
 *  API Initialization and Shutdown/Cleanup
 */


extern void SOCK_AllowSigPipeAPI(void)
{
#ifdef NCBI_OS_UNIX
    s_AllowSigPipe = 1/*true - API will not mask SIGPIPE out at init*/;
#endif /*NCBI_OS_UNIX*/
    return;
}


#if 0/*defined(_DEBUG) && !defined(NDEBUG)*/

#  if !defined(__GNUC__) && !defined(offsetof)
#    define offsetof(T, F) ((unsigned int)((char*) &(((T*) 0)->F) - (char*) 0))
#  endif

static void s_ShowDataLayout(void)
{
    CORE_LOGF(eLOG_Note, ("SOCK data layout:\n"
                          "    Sizeof(SOCK_struct) = %u, offsets follow\n"
                          "\tsock:      %u\n"
                          "\tid:        %u\n"
                          "\thost:      %u\n"
                          "\tport:      %u\n"
                          "\tbitfield:  16 bits\n"
                          "\tr_timeout: %u\n"
                          "\tr_tv:      %u\n"
                          "\tr_to:      %u\n"
                          "\tw_timeout: %u\n"
                          "\tw_tv:      %u\n"
                          "\tw_to:      %u\n"
                          "\tc_timeout: %u\n"
                          "\tc_tv:      %u\n"
                          "\tc_to:      %u\n"
                          "\tr_buf:     %u\n"
                          "\tw_buf:     %u\n"
                          "\tw_len:     %u\n"
                          "\tn_read:    %u\n"
                          "\tn_written: %u\n"
                          "\tn_in:      %u\n"
                          "\tn_out:     %u"
#  ifdef NCBI_OS_UNIX
                          "\n\tfile:      %u"
#  endif /*NCBI_OS_UNIX*/
                          , (unsigned int) sizeof(SOCK_struct),
                          offsetof(SOCK_struct, sock),
                          offsetof(SOCK_struct, id),
                          offsetof(SOCK_struct, host),
                          offsetof(SOCK_struct, port),
                          offsetof(SOCK_struct, r_timeout),
                          offsetof(SOCK_struct, r_tv),
                          offsetof(SOCK_struct, r_to),
                          offsetof(SOCK_struct, w_timeout),
                          offsetof(SOCK_struct, w_tv),
                          offsetof(SOCK_struct, w_to),
                          offsetof(SOCK_struct, c_timeout),
                          offsetof(SOCK_struct, c_tv),
                          offsetof(SOCK_struct, c_to),
                          offsetof(SOCK_struct, r_buf),
                          offsetof(SOCK_struct, w_buf),
                          offsetof(SOCK_struct, w_len),
                          offsetof(SOCK_struct, n_read),
                          offsetof(SOCK_struct, n_written),
                          offsetof(SOCK_struct, n_in),
                          offsetof(SOCK_struct, n_out)
#  ifdef NCBI_OS_UNIX
                          , offsetoff(SOCK_struct, file)
#  endif /*NCBI_OS_UNIX*/
                          ));
}

#endif


extern EIO_Status SOCK_InitializeAPI(void)
{
    static int/*bool*/ s_AtExitSet = 0;

    CORE_LOCK_WRITE;
    if ( s_Initialized ) {
        CORE_UNLOCK;
        return eIO_Success;
    }

#if 0/*defined(_DEBUG) && !defined(NDEBUG)*/
    s_ShowDataLayout();
#endif

#if defined(NCBI_OS_MSWIN)
    {{
        WSADATA wsadata;
        int x_errno = WSAStartup(MAKEWORD(1,1), &wsadata);
        if (x_errno != 0) {
            CORE_UNLOCK;
            CORE_LOG_ERRNO_EX(eLOG_Error, x_errno, SOCK_STRERROR(x_errno),
                              "[SOCK_InitializeAPI]  Failed WSAStartup()");
            return eIO_Unknown;
        }
    }}
#elif defined(NCBI_OS_UNIX)
    if ( !s_AllowSigPipe ) {
        struct sigaction sa;
        if (sigaction(SIGPIPE, 0, &sa) < 0  ||  sa.sa_handler == SIG_DFL) {
            memset(&sa, 0, sizeof(sa));
            sa.sa_handler = SIG_IGN;
            sigaction(SIGPIPE, &sa, 0);
        }
    }
#endif /*platform-specific init*/

    s_Initialized = 1/*true*/;
    if ( !s_AtExitSet ) {
        atexit((void (*)(void)) SOCK_ShutdownAPI);
        s_AtExitSet = 1;
    }
    CORE_UNLOCK;
    return eIO_Success;
}


extern EIO_Status SOCK_ShutdownAPI(void)
{
    CORE_LOCK_WRITE;
    if ( !s_Initialized ) {
        CORE_UNLOCK;
        return eIO_Success;
    }

    s_Initialized = 0/*false*/;
#if defined(NCBI_OS_MSWIN)
    {{
        int x_errno = WSACleanup() ? SOCK_ERRNO : 0;
        CORE_UNLOCK;
        if ( x_errno ) {
            CORE_LOG_ERRNO_EX(eLOG_Warning, x_errno, SOCK_STRERROR(x_errno),
                              "[SOCK_ShutdownAPI]  Failed WSACleanup()");
            return eIO_Unknown;
        }
    }}
#else
    CORE_UNLOCK;
#endif /*NCBI_OS_MSWIN*/

    return eIO_Success;
}



/******************************************************************************
 *  LSOCK & SOCK AUXILIARIES
 */


/* STimeout <--> struct timeval  conversions
 */
static STimeout *s_tv2to(const struct timeval* tv, STimeout* to)
{
    if ( !tv )
        return 0;

    to->sec  = (unsigned int) tv->tv_sec;
    to->usec = (unsigned int) tv->tv_usec;
    return to;
}

static struct timeval* s_to2tv(const STimeout* to, struct timeval* tv)
{
    if ( !to )
        return 0;

    tv->tv_sec  = to->usec / 1000000 + to->sec;
    tv->tv_usec = to->usec % 1000000;
    return tv;
}


/* Switch the specified socket i/o between blocking and non-blocking mode
 */
static int/*bool*/ s_SetNonblock(TSOCK_Handle sock, int/*bool*/ nonblock)
{
#if defined(NCBI_OS_MSWIN)
    unsigned long argp = nonblock ? 1 : 0;
    return ioctlsocket(sock, FIONBIO, &argp) == 0;
#elif defined(NCBI_OS_UNIX)  ||  defined(NCBI_OS_MAC)
    return fcntl(sock, F_SETFL,
                 nonblock ?
                 fcntl(sock, F_GETFL, 0) | O_NONBLOCK :
                 fcntl(sock, F_GETFL, 0) & (int) ~O_NONBLOCK) != -1;
#else
#   error "Unsupported platform"
#endif /*platform-specific ioctl*/
}


static int/*bool*/ s_SetReuseAddress(TSOCK_Handle x_sock, int/*bool*/ on_off)
{
#if defined(NCBI_OS_UNIX)  ||  defined(NCBI_OS_MSWIN)
    /* setsockopt() is not implemented for MAC (in MIT socket emulation lib) */
#  ifdef NCBI_OS_MSWIN
    BOOL reuse_addr = on_off ? TRUE : FALSE;
#  else
    int  reuse_addr = on_off ? 1 : 0;
#  endif /*NCBI_OS_MSWIN*/
    return !setsockopt(x_sock, SOL_SOCKET, SO_REUSEADDR, 
                       (const char*) &reuse_addr, sizeof(reuse_addr));
#else
    return 1;
#endif /*NCBI_OS_UNIX || NCBI_OS_MSWIN*/
}


static EIO_Status s_Status(SOCK sock, EIO_Event direction)
{
    assert(sock  &&  sock->sock != SOCK_INVALID);
    switch ( direction ) {
    case eIO_Read:
        if (sock->type != eSOCK_Datagram) {
            return sock->r_status != eIO_Success
                ? sock->r_status : (sock->eof ? eIO_Closed : eIO_Success);
        }
        return sock->r_status;
    case eIO_Write:
        return sock->w_status;
    default:
        /*should never get here*/
        assert(0);
        break;
    }
    return eIO_InvalidArg;
}


/* Select on the socket I/O (multiple sockets).
 * "Event" field is not considered for entries, whose "sock" field is 0,
 * "revent" for those entries is always "eIO_Open". For all other entries
 * only those sockets will be considered, whose "revent" field does not
 * contain "eIO_Open" value. If at least one non-"eIO_Open" status found
 * in "revent", the call terminates with "eIO_Success" status (after,
 * however checking all other entries for validity). No additional checks
 * are made for the pre-ready entries. 
 *
 * This function does not check datagram sockets with the select() system call
 * at all if the number of requested sockets is more than 1 (cf. SOCK_Poll()).
 *
 * If "eIO_Write" event is inquired on a stream socket, and the socket is
 * marked for upread, then returned "revent" may also include "eIO_Read" to
 * indicate that some input is available on that socket.
 * If "eIO_Read" event is inquired on an array (n != 1) including stream
 * socket (n != 1) and some sockets still have connection/data pending,
 * those "revent" field may then include "eIO_Write" to indicate that
 * connection can be completed/data sent.
 *
 * Return eIO_Success when at least one socket is found either ready 
 * (including "eIO_Read" event on "eIO_Write" for upreadable sockets
 * and "eIO_Write" on "eIO_Read" for sockets in pending state)
 * or failing ("revent" contains "eIO_Close").
 * Return "eIO_Timeout", if timeout expired before any socket became available.
 * Any other return code indicates some failure.
 */
static EIO_Status s_Select(size_t                n,
                           SSOCK_Poll            polls[],
                           const struct timeval* tv)
{
    int/*bool*/ write_only = 1;
    int/*bool*/ read_only = 1;
    int/*bool*/ ready = 0;
    int/*bool*/ bad = 0;
    fd_set      r_fds, w_fds, e_fds;
    int         n_fds;
    size_t      i;

    for (;;) { /* (optionally) auto-resume if interrupted by a signal */
        struct timeval x_tv;

        n_fds = 0;
        FD_ZERO(&r_fds);
        FD_ZERO(&w_fds);
        FD_ZERO(&e_fds);
        for (i = 0; i < n; i++) {
            if ( !polls[i].sock ) {
                polls[i].revent = eIO_Open;
                continue;
            }
            if ( polls[i].revent ) {
                ready = 1;
                continue;
            }
            if (polls[i].event  &&
                (EIO_Event)(polls[i].event | eIO_ReadWrite) == eIO_ReadWrite) {
                if (polls[i].sock->sock != SOCK_INVALID) {
                    if (n != 1  &&  polls[i].sock->type == eSOCK_Datagram)
                        continue;
                    if (ready  ||  bad)
                        continue;
                    switch (polls[i].event) {
                    case eIO_Write:
                    case eIO_ReadWrite:
                        if (polls[i].sock->type == eSOCK_Datagram  ||
                            polls[i].sock->w_status != eIO_Closed) {
                            read_only = 0;
                            FD_SET(polls[i].sock->sock, &w_fds);
                            if (polls[i].sock->type == eSOCK_Datagram  ||
                                polls[i].sock->pending)
                                break;
                            if (polls[i].event == eIO_Write  &&
                                (polls[i].sock->r_on_w == eOff
                                 ||  (polls[i].sock->r_on_w == eDefault
                                      &&  s_ReadOnWrite != eOn)))
                                break;
                        } else if (polls[i].event == eIO_Write)
                            break;
                        /*FALLTHRU*/
                    case eIO_Read:
                        if (polls[i].sock->type != eSOCK_Datagram
                            &&  (polls[i].sock->r_status == eIO_Closed  ||
                                 polls[i].sock->eof))
                            break;
                        write_only = 0;
                        FD_SET(polls[i].sock->sock, &r_fds);
                        if (polls[i].sock->type == eSOCK_Datagram  ||
                            polls[i].event != eIO_Read             ||
                            polls[i].sock->w_status == eIO_Closed  ||
                            n == 1  ||  (!polls[i].sock->pending  &&
                                         !polls[i].sock->w_len))
                            break;
                        read_only = 0;
                        FD_SET(polls[i].sock->sock, &w_fds);
                        break;
                    default:
                        /* should never get here */
                        assert(0);
                        break;
                    }
                    FD_SET(polls[i].sock->sock, &e_fds);
                    if (n_fds < (int) polls[i].sock->sock)
                        n_fds = (int) polls[i].sock->sock;
                } else {
                    polls[i].revent = eIO_Close;
                    ready = 1;
                }
            } else {
                polls[i].revent = eIO_Close;
                bad = 1;
            }
        }

        if ( bad )
            return eIO_InvalidArg;

        if ( ready )
            return eIO_Success;

        if ( tv )
            x_tv = *tv;

        n_fds = select(SOCK_NFDS((TSOCK_Handle) n_fds),
                       write_only ? 0 : &r_fds, read_only ? 0 : &w_fds,
                       &e_fds, tv ? &x_tv : 0);

        /* timeout has expired */
        if (n_fds == 0)
            return eIO_Timeout;

        if (n_fds > 0)
            break;

        /* n_fds < 0 */
        if (SOCK_ERRNO != SOCK_EINTR) {
            int  x_errno = SOCK_ERRNO;
            char _id[32];
            CORE_LOGF_ERRNO_EX(eLOG_Trace, x_errno, SOCK_STRERROR(x_errno),
                               ("%s[SOCK::s_Select]  Failed select()",
                                n == 1 ? s_ID(polls[0].sock, _id) : ""));
            return eIO_Unknown;
        }

        if ((n != 1  &&  s_InterruptOnSignal == eOn)  ||
            (n == 1  &&  (polls[0].sock->i_on_sig == eOn
                          ||  (polls[0].sock->i_on_sig == eDefault
                               &&  s_InterruptOnSignal == eOn)))) {
            return eIO_Interrupt;
        }
    }

    n_fds = 0;
    for (i = 0; i < n; i++) {
        if ( polls[i].sock ) {
            assert(polls[i].revent == eIO_Open);
            if ( !FD_ISSET(polls[i].sock->sock, &e_fds) ) {
                if ( FD_ISSET(polls[i].sock->sock, &r_fds) )
                    polls[i].revent = eIO_Read;
                if ( FD_ISSET(polls[i].sock->sock, &w_fds) )
                    polls[i].revent = (EIO_Event)(polls[i].revent | eIO_Write);
            } else
                polls[i].revent = eIO_Close;
            if (polls[i].revent != eIO_Open)
                n_fds++;
        }
    }
    assert(n_fds);

    /* success; can do I/O now */
    return eIO_Success;
}



/******************************************************************************
 *  LISTENING SOCKET
 */

extern EIO_Status LSOCK_Create(unsigned short port,
                               unsigned short backlog,
                               LSOCK*         lsock)
{
    return LSOCK_CreateEx(port, backlog, lsock, eDefault);
}


extern EIO_Status LSOCK_CreateEx(unsigned short port,
                                 unsigned short backlog,
                                 LSOCK*         lsock,
                                 ESwitch        log)
{
    unsigned int       x_id = ++s_ID_Counter;
    TSOCK_Handle       x_lsock;
    struct sockaddr_in addr;

    *lsock = 0;
    /* initialize internals */
    verify(s_Initialized  ||  SOCK_InitializeAPI() == eIO_Success);

    /* create new(listening) socket */
    if ((x_lsock = socket(AF_INET, SOCK_STREAM, 0)) == SOCK_INVALID) {
        int x_errno = SOCK_ERRNO;
        CORE_LOGF_ERRNO_EX(eLOG_Error, x_errno, SOCK_STRERROR(x_errno),
                           ("LSOCK#%u[?]: [LSOCK::Create] "
                            " Cannot create socket", x_id));
        return eIO_Unknown;
    }

    /*
     * It was confirmed(?) that at least under Solaris 2.5 this precaution:
     * 1) makes the address released immediately after the process
     *    termination;
     * 2) still issue EADDINUSE error on the attempt to bind() to the
     *    same address being in-use by a living process (if SOCK_STREAM).
     */
    if ( !s_SetReuseAddress(x_lsock, 1/*true*/) ) {
        int x_errno = SOCK_ERRNO;
        CORE_LOGF_ERRNO_EX(eLOG_Error, x_errno, SOCK_STRERROR(x_errno),
                           ("LSOCK#%u[%u]: [LSOCK::Create] "
                            " Failed setsockopt(REUSEADDR)",
                            x_id, (unsigned int) x_lsock));
        SOCK_CLOSE(x_lsock);
        return eIO_Unknown;
    }

    /* bind */
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port        = htons(port);
#ifdef HAVE_SIN_LEN
    addr.sin_len         = sizeof(addr);
#endif /*HAVE_SIN_LEN*/
    if (bind(x_lsock, (struct sockaddr*) &addr, sizeof(addr)) != 0) {
        int x_errno = SOCK_ERRNO;
        CORE_LOGF_ERRNO_EX(eLOG_Error, x_errno, SOCK_STRERROR(x_errno),
                           ("LSOCK#%u[%u]: [LSOCK::Create]  Failed bind()",
                            x_id, (unsigned int) x_lsock));
        SOCK_CLOSE(x_lsock);
        return eIO_Unknown;
    }

    /* listen */
    if (listen(x_lsock, backlog) != 0) {
        int x_errno = SOCK_ERRNO;
        CORE_LOGF_ERRNO_EX(eLOG_Error, x_errno, SOCK_STRERROR(x_errno),
                           ("LSOCK#%u[%u]: [LSOCK::Create]  Failed listen()",
                            x_id, (unsigned int) x_lsock));
        SOCK_CLOSE(x_lsock);
        return eIO_Unknown;
    }

    /* set to non-blocking mode */
    if ( !s_SetNonblock(x_lsock, 1/*true*/) ) {
        CORE_LOGF(eLOG_Error, ("LSOCK#%u[%u]: [LSOCK::Create] "
                               " Cannot set socket to non-blocking mode",
                               x_id, (unsigned int) x_lsock));
        SOCK_CLOSE(x_lsock);
        return eIO_Unknown;
    }

    /* allocate memory for the internal socket structure */
    if ( !(*lsock = (LSOCK) calloc(1, sizeof(**lsock))) )
        return eIO_Unknown;
    (*lsock)->sock = x_lsock;
    (*lsock)->id   = x_id;
    (*lsock)->log  = log;

    /* statistics & logging */
    if (log == eOn  ||  (log == eDefault  &&  s_Log == eOn)) {
        CORE_LOGF(eLOG_Trace, ("LSOCK#%u[%u]: Listening on port :%hu",
                               x_id, (unsigned int) x_lsock, port));
    }

    return eIO_Success;
}


extern EIO_Status LSOCK_Accept(LSOCK           lsock,
                               const STimeout* timeout,
                               SOCK*           sock)
{
    struct sockaddr_in addr;
    unsigned int       x_id;
    TSOCK_Handle       x_sock;

    if (lsock->sock == SOCK_INVALID) {
        CORE_LOGF(eLOG_Error, ("LSOCK#%u[?]: [LSOCK::Accept] "
                               " Invalid socket", lsock->id));
        assert(0);
        return eIO_Unknown;
    }

    {{ /* wait for the connection request to come (up to timeout) */
        EIO_Status     status;
        SSOCK_Poll     poll;
        struct timeval tv;
        SOCK_struct    s;

        memset(&s, 0, sizeof(s));
        s.sock      = lsock->sock;
        s.id        = lsock->id;
        s.i_on_sig  = eDefault;
        poll.sock   = &s;
        poll.event  = eIO_Read;
        poll.revent = eIO_Open;
        if ((status = s_Select(1, &poll, s_to2tv(timeout,&tv))) != eIO_Success)
            return status;
        if (poll.revent == eIO_Close)
            return eIO_Unknown;
        assert(poll.event == eIO_Read  &&  poll.revent == eIO_Read);
    }}

    x_id = (lsock->id * 1000 + ++s_ID_Counter) * 1000;
    {{ /* accept next connection */
        SOCK_socklen_t addrlen = (SOCK_socklen_t) sizeof(addr);
        if ((x_sock = accept(lsock->sock, (struct sockaddr*) &addr, &addrlen))
            == SOCK_INVALID) {
            int x_errno = SOCK_ERRNO;
            CORE_LOGF_ERRNO_EX(eLOG_Error, x_errno, SOCK_STRERROR(x_errno),
                               ("LSOCK#%u[%u]: [LSOCK::Accept] "
                                " Failed accept()",
                                lsock->id, (unsigned int) lsock->sock));
            return eIO_Unknown;
        }
        lsock->n_accept++;
        assert(addr.sin_family == AF_INET);
        /* man accept(2) notes that non-blocking state may not be inherited */
        if ( !s_SetNonblock(x_sock, 1/*true*/) ) {
            CORE_LOGF(eLOG_Error, ("SOCK#%u[%u]: [LSOCK::Accept]  Cannot"
                                   " set accepted socket to non-blocking mode",
                                   x_id, (unsigned int) x_sock));
            SOCK_CLOSE(x_sock);
            return eIO_Unknown;
        }

        if (s_ReuseAddress  &&  !s_SetReuseAddress(x_sock, 1/*true*/)) {
            int x_errno = SOCK_ERRNO;
            CORE_LOGF_ERRNO_EX(eLOG_Warning, x_errno, SOCK_STRERROR(x_errno),
                               ("SOCK#%u[%u]: [LSOCK::Accept] "
                                " Failed setsockopt(REUSEADDR)",
                                x_id, (unsigned int) x_sock));
        }
    }}

    /* create new SOCK structure */
    if ( !(*sock = (SOCK) calloc(1, sizeof(**sock))) ) {
        SOCK_CLOSE(x_sock);
        return eIO_Unknown;
    }

    /* success */
    (*sock)->sock     = x_sock;
    (*sock)->id       = x_id;
    (*sock)->host     = addr.sin_addr.s_addr;
    (*sock)->port     = addr.sin_port;
    (*sock)->log      = lsock->log;
    (*sock)->type     = eSOCK_ServerSide;
    (*sock)->r_on_w   = eDefault;
    (*sock)->i_on_sig = eDefault;
    (*sock)->r_status = eIO_Success;
    (*sock)->eof      = 0/*false*/;
    (*sock)->w_status = eIO_Success;
    (*sock)->pending  = 0/*connected*/;
    /* all timeouts zeroed - infinite */
    BUF_SetChunkSize(&(*sock)->r_buf, SOCK_BUF_CHUNK_SIZE);
    /* w_buf is unused for accepted sockets */

    /* statistics & logging */
    if (lsock->log == eOn  ||  (lsock->log == eDefault  &&  s_Log == eOn))
        s_DoLog(*sock, eIO_Open, 0, 0, (struct sockaddr*) &addr);

    return eIO_Success;
}


extern EIO_Status LSOCK_Close(LSOCK lsock)
{
    EIO_Status status;

    if (lsock->sock == SOCK_INVALID) {
        CORE_LOGF(eLOG_Error, ("LSOCK#%u[?]: [LSOCK::Close] "
                               " Invalid socket", lsock->id));
        assert(0);
        return eIO_Unknown;
    }

    /* set the socket back to blocking mode */
    if ( !s_SetNonblock(lsock->sock, 0/*false*/) ) {
        CORE_LOGF(eLOG_Warning, ("LSOCK#%u[%u]: [LSOCK::Close] "
                                 " Cannot set socket back to blocking mode",
                                 lsock->id, (unsigned int) lsock->sock));
    }

    /* statistics & logging */
    if (lsock->log == eOn  ||  (lsock->log == eDefault  &&  s_Log == eOn)) {
        CORE_LOGF(eLOG_Trace, ("LSOCK#%u[%u]: Closing (%u accept%s total)",
                               lsock->id, (unsigned int) lsock->sock,
                               lsock->n_accept, lsock->n_accept == 1? "":"s"));
    }

    status = eIO_Success;
    for (;;) { /* close persistently - retry if interrupted by a signal */
        /* success */
        if (SOCK_CLOSE(lsock->sock) == 0)
            break;

        /* error */
        if (SOCK_ERRNO != SOCK_EINTR) {
            int x_errno = SOCK_ERRNO;
            CORE_LOGF_ERRNO_EX(eLOG_Error, x_errno, SOCK_STRERROR(x_errno),
                               ("LSOCK#%u[%u]: [LSOCK::Close]  Failed close()",
                                lsock->id, (unsigned int) lsock->sock));
            status = eIO_Unknown;
            break;
        }
    }

    /* cleanup & return */
    lsock->sock = SOCK_INVALID;
    free(lsock);
    return status;
}


extern EIO_Status LSOCK_GetOSHandle(LSOCK  lsock,
                                    void*  handle,
                                    size_t handle_size)
{
    if (!handle  ||  handle_size != sizeof(lsock->sock)) {
        CORE_LOGF(eLOG_Error, ("LSOCK#%u[%u]: [LSOCK::GetOSHandle] "
                               " Invalid handle %s%lu", lsock->id,
                               (unsigned int) lsock->sock, handle? "size ": "",
                               handle ? (unsigned long) handle_size : 0));
        assert(0);
        return eIO_InvalidArg;
    }

    memcpy(handle, &lsock->sock, handle_size);
    return eIO_Success;
}



/******************************************************************************
 *  SOCKET
 */

/* connect() could be async/interrupted by a signal or just cannot be
 * established immediately;  yet, it must have been in progress
 * (asynchronous), so wait here for it to succeed (become writable).
 */
static EIO_Status s_IsConnected(SOCK                  sock,
                                const struct timeval* tv,
                                int*                  x_errno,
                                int/*bool*/           writeable)
{
    EIO_Status     status;
#ifdef NCBI_OS_UNIX
    SOCK_socklen_t x_len = (SOCK_socklen_t) sizeof(*x_errno);
#endif /*NCBI_OS_UNIX*/
    SSOCK_Poll     poll;

    *x_errno = 0;
    if (sock->w_status == eIO_Closed)
        return eIO_Closed;
    if ( !writeable ) {
        poll.sock      = sock;
        poll.event     = eIO_Write;
        poll.revent    = eIO_Open;
        status         = s_Select(1, &poll, tv);
    } else {
        status         = eIO_Success;
        poll.revent    = eIO_Write;
    }
#ifdef NCBI_OS_UNIX
    if (status == eIO_Success  &&
        (getsockopt(sock->sock, SOL_SOCKET, SO_ERROR, x_errno, &x_len) != 0  ||
         *x_errno != 0)) {
        status = eIO_Unknown;
    }
#endif /*NCBI_OS_UNIX*/
    if (status != eIO_Success  ||  poll.revent != eIO_Write) {
        if ( !*x_errno )
            *x_errno = SOCK_ERRNO;
        if (*x_errno == SOCK_ECONNREFUSED)
            sock->r_status = sock->w_status = status = eIO_Closed;
        else if (status == eIO_Success)
            status = eIO_Unknown;
    }
    if (status == eIO_Success  &&  s_ReuseAddress) {
        if ( !s_SetReuseAddress(sock->sock, 1/*true*/) ) {
            int x_errno = SOCK_ERRNO;
            char _id[32];
            CORE_LOGF_ERRNO_EX(eLOG_Warning, x_errno, SOCK_STRERROR(x_errno),
                               ("%s[SOCK::s_IsConnected]  Failed "
                                "setsockopt(REUSEADDR)", s_ID(sock, _id)));
        }
    }
    return status;
}


/* Connect the (pre-allocated) socket to the specified "host:port" peer.
 * HINT: if "host" is NULL then assume(!) that the "sock" already exists,
 *       and connect to the same host;  the same is for zero "port".
 * NOTE: Client-side stream sockets only.
 */
static EIO_Status s_Connect(SOCK            sock,
                            const char*     host,
                            unsigned short  port,
                            const STimeout* timeout)
{
    char               _id[32];
    TSOCK_Handle       x_sock;
    unsigned int       x_host;
    unsigned short     x_port;
    struct sockaddr_in peer;

    assert(sock->type == eSOCK_ClientSide);
#ifdef NCBI_OS_UNIX
    assert(!sock->file[0]);
#endif /*NCBI_OS_UNIX*/

    /* initialize internals */
    verify(s_Initialized  ||  SOCK_InitializeAPI() == eIO_Success);

    /* get address of the remote host (assume the same host if it is NULL) */
    x_host = host  &&  *host ? SOCK_gethostbyname(host) : sock->host;
    if ( !x_host ) {
        CORE_LOGF(eLOG_Error, ("%s[SOCK::s_Connect]  Failed "
                               "SOCK_gethostbyname(%.64s)",
                               s_ID(sock, _id), host));
        return eIO_Unknown;
    }

    /* set the port to connect to (assume the same port if "port" is zero) */
    x_port = (unsigned short) (port ? htons(port) : sock->port);

    /* create new socket */
    if ((x_sock = socket(AF_INET, SOCK_STREAM, 0)) == SOCK_INVALID) {
        int x_errno = SOCK_ERRNO;
        CORE_LOGF_ERRNO_EX(eLOG_Error, x_errno, SOCK_STRERROR(x_errno),
                           ("%s[SOCK::s_Connect]  Cannot create socket",
                            s_ID(sock, _id)));
        return eIO_Unknown;
    }
    sock->sock = x_sock;

    /* set the socket i/o to non-blocking mode */
    if ( !s_SetNonblock(x_sock, 1/*true*/) ) {
        CORE_LOGF(eLOG_Error, ("%s[SOCK::s_Connect] "
                               " Cannot set socket to non-blocking mode",
                               s_ID(sock, _id)));
        sock->sock = SOCK_INVALID;
        SOCK_CLOSE(x_sock);
        return eIO_Unknown;
    }

    /* fill in the server "addr" to connect to */
    memset(&peer, 0, sizeof(peer));
    peer.sin_family      = AF_INET;
    peer.sin_addr.s_addr = x_host;
    peer.sin_port        = x_port;
#ifdef HAVE_SIN_LEN
    peer.sin_len         = sizeof(peer);
#endif /*HAVE_SIN_LEN*/

    /* statistics & logging */
    if (sock->log == eOn  ||  (sock->log == eDefault  &&  s_Log == eOn))
        s_DoLog(sock, eIO_Open, 0, 0, (struct sockaddr*) &peer);

    /* establish connection to the peer */
    sock->r_status  = eIO_Success;
    sock->eof       = 0/*false*/;
    sock->w_status  = eIO_Success;
    assert(sock->w_len == 0);
    if (connect(x_sock, (struct sockaddr*) &peer, sizeof(peer)) != 0) {
        if (SOCK_ERRNO != SOCK_EINTR  &&  SOCK_ERRNO != SOCK_EINPROGRESS  &&
            SOCK_ERRNO != SOCK_EWOULDBLOCK) {
            int x_errno = SOCK_ERRNO;
            char addr[80];
            HostPortToString(x_host, ntohs(x_port), addr, sizeof(addr));
            CORE_LOGF_ERRNO_EX(eLOG_Error, x_errno, SOCK_STRERROR(x_errno),
                               ("%s[SOCK::s_Connect]  Failed connect() to %s",
                                s_ID(sock, _id), addr));
            sock->sock = SOCK_INVALID;
            SOCK_CLOSE(x_sock);
            return eIO_Unknown; /* unrecoverable error */
        }

        if (!timeout  ||  timeout->sec  ||  timeout->usec) {
            int            x_errno;
            EIO_Status     status;
            struct timeval tv;

            status = s_IsConnected(sock, s_to2tv(timeout, &tv), &x_errno, 0);
            if (status != eIO_Success) {
                char addr[80];
                HostPortToString(x_host, ntohs(x_port), addr, sizeof(addr));
                CORE_LOGF_ERRNO_EX(eLOG_Error, x_errno, SOCK_STRERROR(x_errno),
                                   ("%s[SOCK::s_Connect]  Failed pending "
                                    "connect() to %s (%s)", s_ID(sock, _id),
                                    addr, IO_StatusStr(status)));
                sock->sock = SOCK_INVALID;
                SOCK_CLOSE(x_sock);
                return status;
            }
            sock->pending = 0/*connected*/;
        } else
            sock->pending = 1/*not yet connected*/;
    } else
        sock->pending = 0/*connected*/;

    /* success: do not change any timeouts */
    sock->host      = x_host;
    sock->port      = x_port;
    sock->n_read    = 0;
    sock->n_written = 0;
    sock->w_len     = BUF_Size(sock->w_buf);
    return eIO_Success;
}


/* To allow emulating "peek" using the NCBI data buffering.
 * (MSG_PEEK is not implemented on Mac, and it is poorly implemented
 * on Win32, so we had to implement this feature by ourselves.)
 * NOTE: This call is for stream sockets only.
 */
static int s_Recv(SOCK        sock,
                  void*       buffer,
                  size_t      size,
                  int/*bool*/ peek)
{
    char*  x_buffer = (char*) buffer;
    char   xx_buffer[4096];
    size_t n_read;

    assert(sock->type != eSOCK_Datagram  &&  !sock->pending);
    if ( !size ) {
        /* internal upread use only */
        assert(peek  &&  !buffer  &&  sock->r_status != eIO_Closed);
        n_read = 0;
    } else {
        /* read (or peek) from the internal buffer */
        n_read = peek ?
            BUF_Peek(sock->r_buf, x_buffer, size) :
            BUF_Read(sock->r_buf, x_buffer, size);
        if ((n_read  &&  (n_read == size  ||  !peek))  ||
            sock->r_status == eIO_Closed  ||  sock->eof) {
            return (int) n_read;
        }
    }

    /* read (not just peek) from the socket */
    do {
        size_t n_todo;
        int    x_read;

        if ( !size ) {
            /* internal upread call -- read out as much as possible */
            n_todo    = sizeof(xx_buffer);
            x_buffer  = xx_buffer;
        } else if ( !buffer ) {
            /* read to the temporary buffer (to store or discard later) */
            n_todo    = size - n_read;
            if (n_todo > sizeof(xx_buffer))
                n_todo = sizeof(xx_buffer);
            x_buffer  = xx_buffer;
        } else {
            /* read to the data buffer provided by user */
            n_todo    = size - n_read;
            x_buffer += n_read;
        }
        /* recv */
        x_read = recv(sock->sock, x_buffer, n_todo, 0);

        /* success */
        if (x_read >= 0  ||
            (x_read < 0  &&  (SOCK_ERRNO == SOCK_ENOTCONN      ||
                              SOCK_ERRNO == SOCK_ECONNRESET    ||
                              SOCK_ERRNO == SOCK_ECONNABORTED  ||
                              SOCK_ERRNO == SOCK_ENETRESET))) {
            /* statistics & logging */
            if (sock->log == eOn  ||  (sock->log == eDefault && s_Log == eOn)){
                s_DoLog(sock, eIO_Read, x_read >= 0 ? x_buffer : 0,
                        (size_t)(x_read < 0 ? 0 : x_read), 0);
            }
            if (x_read <= 0) {
                /* catch EOF/failure */
                sock->eof = 1/*true*/;
                sock->r_status = x_read == 0 ? eIO_Success : eIO_Closed;
                break;
            }
        } else {
            /* some error */
            int x_errno = SOCK_ERRNO;
            if (x_errno != SOCK_EWOULDBLOCK  &&
                x_errno != SOCK_EAGAIN       &&
                x_errno != SOCK_EINTR) {
                /* catch unknown ERROR */
                sock->r_status = eIO_Unknown;
                CORE_LOGF_ERRNO_EX(eLOG_Trace, x_errno, SOCK_STRERROR(x_errno),
                                   ("%s[SOCK::s_Recv]  Failed recv()",
                                   s_ID(sock, xx_buffer)));
            }
            return n_read ? (int) n_read : -1;
        }
        assert(x_read > 0);

        /* if "peek" -- store the new read data in the internal input buffer */
        if (peek  &&  !BUF_Write(&sock->r_buf, x_buffer, (size_t) x_read)) {
            CORE_LOGF_ERRNO(eLOG_Error, errno,
                            ("%s[SOCK::s_Recv]  Failed to store data in "
                             "peek buffer", s_ID(sock, xx_buffer)));
            sock->eof      = 1/*failure*/;
            sock->r_status = eIO_Closed;
            break;
        }

        /* successful read */
        sock->r_status = eIO_Success;
        sock->n_read  += x_read;
        n_read        += x_read;
    } while (!size  ||  (!buffer  &&  n_read < size));

    return (int) n_read;
}


static EIO_Status s_WritePending(SOCK, const struct timeval*, int);

/* s_Select() with stall protection: try pull incoming data from sockets.
 * This method returns array of polls, "revent"s of which are always
 * compatible with requested "event"s. That is, it always strips additional
 * events that s_Select() may have set to indicate additional I/O events
 * some sockets are ready for. Return eIO_Timeout if no compatible events
 * were found (all sockets are not ready for inquired respective I/O) within
 * the specified timeout (and no other socket error was flagged).
 * Return eIO_Success if at least one socket is ready. Return the number
 * of sockets that are ready via pointer argument "n_ready" (may be NULL).
 * Return other error code to indicate error condition.
 */
static EIO_Status s_SelectStallsafe(size_t                n,
                                    SSOCK_Poll            polls[],
                                    const struct timeval* tv,
                                    size_t*               n_ready)
{
    int/*bool*/ pending;
    EIO_Status  status;
    size_t      i, j;

    if ((status = s_Select(n, polls, tv)) != eIO_Success) {
        if ( n_ready )
            *n_ready = 0;
        return status;
    }

    j = 0;
    pending = 0;
    for (i = 0; i < n; i++) {
        if (polls[i].revent == eIO_Close)
            break;
        if (polls[i].revent & polls[i].event)
            break;
        if (polls[i].revent != eIO_Open  &&  !pending) {
            pending++;
            j = i;
        }
    }
    if (i >= n  &&  pending) {
        /* all sockets are not ready for the requested events */
        for (i = j; i < n; i++) {
            /* try to push pending writes */
            if (polls[i].event == eIO_Read  &&  polls[i].revent == eIO_Write) {
                assert(n != 1);
                assert(polls[i].sock->pending  ||  polls[i].sock->w_len);
                status = s_WritePending(polls[i].sock, tv, 1/*writeable*/);
                if (status != eIO_Success  &&  status != eIO_Timeout) {
                    polls[i].revent = eIO_Close;
                    break;
                }
                continue;
            }
            /* try to find an immediately readable socket */
            if (polls[i].event != eIO_Write)
                continue;
            while (polls[i].revent == eIO_Read) {
                assert(polls[i].sock                          &&
                       polls[i].sock->sock != SOCK_INVALID    &&
                       polls[i].sock->type != eSOCK_Datagram  &&
                       polls[i].sock->w_status != eIO_Closed  &&
                       polls[i].sock->r_status != eIO_Closed  &&
                       !polls[i].sock->eof                    &&
                       !polls[i].sock->pending                &&
                       (polls[i].sock->r_on_w == eOn
                        ||  (polls[i].sock->r_on_w == eDefault
                             &&  s_ReadOnWrite == eOn)));

                /* try upread as mush as possible data into internal buffer */
                s_Recv(polls[i].sock, 0, 0/*infinite*/, 1/*peek*/);

                /* then poll if writeable */
                polls[i].revent = eIO_Open;
                if ((status = s_Select(1, &polls[i], tv)) != eIO_Success)
                    break;
            }
            if (status != eIO_Success  ||  polls[i].revent == eIO_Write)
                break; /*error or can write now!*/
        }
    }

    j = 0;
    for (i = 0; i < n; i++) {
        if (polls[i].revent != eIO_Close) {
            polls[i].revent = (EIO_Event)(polls[i].revent & polls[i].event);
            if (polls[i].revent != eIO_Open)
                j++;
        } else
            j++;
    }

    if ( n_ready )
        *n_ready = j;

    return j ? eIO_Success : (status == eIO_Success ? eIO_Timeout : status);
}


static EIO_Status s_WipeRBuf(SOCK sock)
{
    EIO_Status status;
    size_t     size = BUF_Size(sock->r_buf);

    if (size  &&  BUF_Read(sock->r_buf, 0, size) != size) {
        char _id[32];
        CORE_LOGF(eLOG_Error, ("%s[SOCK::s_WipeRBuf] "
                               " Cannot drop aux. data buf", s_ID(sock, _id)));
        assert(0);
        status = eIO_Unknown;
    } else
        status = eIO_Success;
    return status;
}


/* For datagram sockets only! */
static EIO_Status s_WipeWBuf(SOCK sock)
{
    EIO_Status status;
    size_t     size = BUF_Size(sock->w_buf);

    assert(sock->type == eSOCK_Datagram);
    if (size  &&  BUF_Read(sock->w_buf, 0, size) != size) {
        char _id[32];
        CORE_LOGF(eLOG_Error, ("%s[SOCK::s_WipeWBuf] "
                               " Cannot drop aux. data buf", s_ID(sock, _id)));
        assert(0);
        status = eIO_Unknown;
    } else
        status = eIO_Success;
    sock->eof = 0;
    return status;
}


/* Write data to the socket "as is" (as many bytes at once as possible).
 * Return eIO_Success if at least some bytes were written successfully.
 * Otherwise (no bytes written) return an error code to indicate the problem.
 * NOTE: This call is for stream sockets only.
 */
static EIO_Status s_Send(SOCK        sock,
                         const void* buf,
                         size_t      size,
                         size_t*     n_written)
{
    char _id[32];

    assert(size > 0  &&  sock->type != eSOCK_Datagram  &&  *n_written == 0);
    for (;;) { /* optionally retry if interrupted by a signal */
        /* try to write */
        int x_written = send(sock->sock, (void*) buf, size, 0);
        int x_errno;

        if (x_written > 0) {
            /* statistics & logging */
            if (sock->log == eOn  ||  (sock->log == eDefault && s_Log == eOn))
                s_DoLog(sock, eIO_Write, buf, (size_t) x_written, 0);
            sock->n_written += x_written;

            *n_written = x_written;
            sock->w_status = eIO_Success;
            break/*done*/;
        }
        x_errno = SOCK_ERRNO;
        /* don't want to handle all possible errors... let them be "unknown" */
        sock->w_status = eIO_Unknown;

        /* blocked -- retry if unblocked before the timeout expires */
        /* (use stall protection if specified) */
        if (x_errno == SOCK_EWOULDBLOCK  ||  x_errno == SOCK_EAGAIN) {
            EIO_Status status;
            SSOCK_Poll poll;

            poll.sock   = sock;
            poll.event  = eIO_Write;
            poll.revent = eIO_Open;
            /* stall protection:  try pull incoming data from the socket */
            status = s_SelectStallsafe(1, &poll, sock->w_timeout, 0);
            if (status != eIO_Success)
                return status;
            if (poll.revent == eIO_Close)
                return eIO_Unknown;
            assert(poll.revent == eIO_Write);
            continue;
        }

        if (x_errno != SOCK_EINTR) {
            /* forcibly closed by peer or shut down? */
            if (x_errno != SOCK_EPIPE      &&  x_errno != SOCK_ENOTCONN     &&
                x_errno != SOCK_ECONNRESET &&  x_errno != SOCK_ECONNABORTED &&
                x_errno != SOCK_ENETRESET) {
                CORE_LOGF_ERRNO_EX(eLOG_Trace, x_errno, SOCK_STRERROR(x_errno),
                                   ("%s[SOCK::s_Send] "
                                    " Failed send()", s_ID(sock, _id)));
                break;
            }
            sock->w_status = eIO_Closed;
            if (sock->log == eOn  ||  (sock->log == eDefault && s_Log == eOn))
                s_DoLog(sock, eIO_Write, 0, 0, 0);
            break;
        }

        if (sock->i_on_sig == eOn  ||
            (sock->i_on_sig == eDefault  &&  s_InterruptOnSignal == eOn)) {
            sock->w_status = eIO_Interrupt;
            break;
        }
    }

    return sock->w_status;
}


/* Wrapper for s_Send() that slices the output buffer for some brain-dead
 * systems (e.g. old Macs) that cannot handle large data chunks in "send()".
 * Return eIO_Success if some data were successfully sent; other
 * error code if no data were sent at all.
 */
#ifdef SOCK_WRITE_SLICE
static EIO_Status s_WriteSliced(SOCK        sock,
                                const void* buf,
                                size_t      size,
                                size_t*     n_written)
{
    /* split output buffer by slices (of size <= SOCK_WRITE_SLICE)
     * before writing to the socket
     */
    EIO_Status status;

    assert(size > 0  &&  *n_written == 0);
    do {
        size_t n_io = size > SOCK_WRITE_SLICE ? SOCK_WRITE_SLICE : size;
        size_t n_io_done = 0;
        status = s_Send(sock, (char*) buf + *n_written, n_io, &n_io_done);
        if (status != eIO_Success)
            break;
        *n_written += n_io_done;
        if (n_io != n_io_done)
            break;
        size       -= n_io_done;
    } while ( size );

    return status;
}
#else
#  define s_WriteSliced s_Send
#endif /*SOCK_WRITE_SLICE*/


static EIO_Status s_WritePending(SOCK                  sock,
                                 const struct timeval* tv,
                                 int/*bool*/           writeable)
{
    const struct timeval* x_tv;
    EIO_Status status;
    int x_errno;
    size_t off;

    assert(sock->type != eSOCK_Datagram  &&  sock->sock != SOCK_INVALID);
    if ( sock->pending ) {
        status = s_IsConnected(sock, tv, &x_errno, writeable);
        if (status != eIO_Success) {
            if (status != eIO_Timeout) {
                char addr[80];
                char  _id[32];
#ifdef NCBI_OS_UNIX
                if (!sock->file[0])
                    sprintf(addr, "\"%.*s\"", (int) sizeof(addr)-3,sock->file);
                else
#endif /*NCBI_OS_UNIX*/
                    HostPortToString(sock->host, ntohs(sock->port),
                                     addr, sizeof(addr));
                CORE_LOGF_ERRNO_EX(eLOG_Error, x_errno, SOCK_STRERROR(x_errno),
                                   ("%s[SOCK::s_WritePending]  Failed pending "
                                    "connect() to %s (%s)", s_ID(sock, _id),
                                    addr, IO_StatusStr(status)));
            }
            return status;
        }
        sock->pending = 0/*connected*/;
    }
    if (sock->w_len == 0  ||  sock->w_status == eIO_Closed)
        return eIO_Success;

    x_tv = sock->w_timeout;
    sock->w_timeout = tv;
    off = BUF_Size(sock->w_buf) - sock->w_len;
    do {
        char   buf[4096];
        size_t n_written = 0;
        size_t n_write = BUF_PeekAt(sock->w_buf, off, buf, sizeof(buf));
        status = s_WriteSliced(sock, buf, n_write, &n_written);
        if (status != eIO_Success)
            break;
        sock->w_len -= n_written;
        off         += n_written;
    } while ( sock->w_len );
    sock->w_timeout = x_tv;

    assert((sock->w_len != 0)  ==  (status != eIO_Success));
    return status;
}


/* Read/Peek data from the socket. Always return eIO_Success if some data
 * were read (regardless of socket conditions that may include EOF/error).
 * Return other (error) code only if no data at all could be obtained.
 */
static EIO_Status s_Read(SOCK        sock,
                         void*       buf,
                         size_t      size,
                         size_t*     n_read,
                         int/*bool*/ peek)
{
    EIO_Status status;

    *n_read = 0;

    if (size == 0)
        return s_Status(sock, eIO_Read);

    if (sock->type == eSOCK_Datagram) {
        *n_read = peek ?
            BUF_Peek(sock->r_buf, buf, size) :
            BUF_Read(sock->r_buf, buf, size);
        sock->r_status = *n_read ? eIO_Success : eIO_Closed;
        return sock->r_status;
    }

    status = s_WritePending(sock, sock->r_timeout, 0);
    if ( sock->pending )
        return status;

    for (;;) { /* retry if either blocked or interrupted (optional) */
        /* try to read */
        int x_read = s_Recv(sock, buf, size, peek);
        int x_errno;

        if (x_read > 0) {
            assert((size_t) x_read <= size);
            *n_read = x_read;
            return eIO_Success;
        }

        if (sock->r_status == eIO_Unknown)
            break;

        if (sock->r_status == eIO_Closed  ||  sock->eof) {
            if ( !sock->eof ) {
                char _id[32];
                CORE_LOGF(eLOG_Trace, ("%s[SOCK::s_Read]  Socket has been "
                                       "shut down for reading",
                                       s_ID(sock, _id)));
            }
            return eIO_Closed;
        }

        x_errno = SOCK_ERRNO;
        /* blocked -- wait for data to come;  exit if timeout/error */
        if (x_errno == SOCK_EWOULDBLOCK  ||  x_errno == SOCK_EAGAIN) {
            const struct timeval* tv = sock->r_timeout;
            SSOCK_Poll poll;

            if (tv  &&  !tv->tv_sec  &&  !tv->tv_usec)
                return eIO_Timeout;
            poll.sock   = sock;
            poll.event  = eIO_Read;
            poll.revent = eIO_Open;
            if ((status = s_Select(1, &poll, tv)) != eIO_Success)
                return status;
            if (poll.revent == eIO_Close)
                break;
            assert(poll.event == eIO_Read  &&  poll.revent == eIO_Read);
            continue;
        }

        if (x_errno != SOCK_EINTR)
            break;

        if (sock->i_on_sig == eOn  ||
            (sock->i_on_sig == eDefault  &&  s_InterruptOnSignal == eOn)) {
            sock->r_status = eIO_Interrupt;
            break;
        }
    }

    /* don't want to handle all possible errors... let them be "unknown" */
    return eIO_Unknown;
}


/* Write to the socket. Return eIO_Success if some data were written.
 * Return other (error) code only if no data at all could be written.
 */
static EIO_Status s_Write(SOCK        sock,
                          const void* buf,
                          size_t      size,
                          size_t*     n_written)
{
    EIO_Status status;

    *n_written = 0;

    if (sock->type == eSOCK_Datagram) {
        if ( sock->eof )
            s_WipeWBuf(sock);
        if ( BUF_Write(&sock->w_buf, buf, size) ) {
            sock->w_status = eIO_Success;
            *n_written = size;
        } else
            sock->w_status = eIO_Unknown;
        return sock->w_status;
    }

    if (sock->w_status == eIO_Closed) {
        if (size != 0) {
            char _id[32];
            CORE_LOGF(eLOG_Trace, ("%s[SOCK::s_Write]  Socket has been shut "
                                   "down for writing", s_ID(sock, _id)));
        }
        return eIO_Closed;
    }

    if ((status = s_WritePending(sock, sock->w_timeout, 0)) != eIO_Success) {
        if (status == eIO_Timeout  ||  status == eIO_Closed)
            return status;
        return size ? status : eIO_Success;
    }

    assert(sock->w_len == 0);
    return size ? s_WriteSliced(sock, buf, size, n_written) : eIO_Success;
}


/* For non-datagram sockets only */
static EIO_Status s_Shutdown(SOCK                  sock,
                             EIO_Event             how,
                             const struct timeval* tv)
{
    EIO_Status status;
    char      _id[32];
    int        x_how;

    assert(sock->type != eSOCK_Datagram);
    switch ( how ) {
    case eIO_Read:
        if ( sock->eof ) {
            /* hit EOF (and may be not yet shut down) -- so, flag it as been
             * shut down, but do not call the actual system shutdown(),
             * as it can cause smart OS'es like Linux to complain
             */
            sock->eof = 0/*false*/;
            sock->r_status = eIO_Closed;
        }
        if (sock->r_status == eIO_Closed)
            return eIO_Success;  /* has been shut down already */
        x_how = SOCK_SHUTDOWN_RD;
        sock->r_status = eIO_Closed;
#ifdef NCBI_OS_MSWIN
        /* see comments at the bottom of eIO_Write case */
        return eIO_Success;
#endif /*NCBI_OS_MSWIN*/
        break;
    case eIO_Write:
        if (sock->w_status == eIO_Closed)
            return eIO_Success;  /* has been shut down already */
        if ((status = s_WritePending(sock, tv, 0)) != eIO_Success
            &&  (!tv  ||  tv->tv_sec  ||  tv->tv_usec)) {
            CORE_LOGF(eLOG_Warning, ("%s[SOCK::s_Shutdown]  Shutting down for "
                                     "write with some output pending (%s)",
                                     s_ID(sock, _id), IO_StatusStr(status)));
        }
        x_how = SOCK_SHUTDOWN_WR;
        sock->w_status = eIO_Closed;
#ifdef NCBI_OS_MSWIN
        /*  on MS-Win, socket shutdown on writing apparently messes up (?!)  *
         *  with the later reading, esp. when reading a lot of data...       */
        return eIO_Success;
#endif /*NCBI_OS_MSWIN*/
        break;
    case eIO_ReadWrite:
#ifdef NCBI_OS_MSWIN
        if (sock->r_status == eIO_Closed  &&  sock->w_status == eIO_Closed)
            return eIO_Success;
        if (sock->w_status != eIO_Closed  &&
            (status = s_WritePending(sock, tv, 0)) != eIO_Success
            &&  (!tv ||  tv->tv_sec  ||  tv->tv_usec)) {
            CORE_LOGF(eLOG_Warning, ("%s[SOCK::s_Shutdown]  Shutting down for "
                                     "R/W with some output pending (%s)",
                                     s_ID(sock, _id), IO_StatusStr(status)));
        }
        x_how = SOCK_SHUTDOWN_RDWR;
        sock->eof = 0/*false*/;
        sock->r_status = sock->w_status = eIO_Closed;
        break;
#else
        {{
            EIO_Status sw = s_Shutdown(sock, eIO_Write, tv);
            EIO_Status sr = s_Shutdown(sock, eIO_Read,  tv);
            if (sw != eIO_Success  ||  sr != eIO_Success)
                return (sw == eIO_Success ? sr :
                        sr == eIO_Success ? sw : eIO_Unknown);
        }}
        return eIO_Success;
#endif /*NCBI_OS_MSWIN*/
    default:
        CORE_LOGF(eLOG_Error, ("%s[SOCK::s_Shutdown]  Invalid direction %u",
                               s_ID(sock, _id), (unsigned int) how));
        return eIO_InvalidArg;
    }

    if (SOCK_SHUTDOWN(sock->sock, x_how) != 0) {
        int x_errno = SOCK_ERRNO;
        if (
#if defined(NCBI_OS_LINUX)/*bug in the kernel to report*/  || \
    defined(NCBI_OS_IRIX)                                  || \
    defined(NCBI_OS_OSF1)
            x_errno != SOCK_ENOTCONN
#else
            x_errno != SOCK_ENOTCONN  ||  sock->pending
#endif /*UNIX flavors*/
            )
            CORE_LOGF_ERRNO_EX(eLOG_Warning, x_errno, SOCK_STRERROR(x_errno),
                               ("%s[SOCK::s_Shutdown]  Failed shutdown(%s)",
                                s_ID(sock, _id), how == eIO_Read ? "READ" :
                                how == eIO_Write ? "WRITE" : "READ/WRITE"));
    }
    return eIO_Success;
}


/* Close the socket
 */
static EIO_Status s_Close(SOCK sock)
{
    EIO_Status status;
    char      _id[32];

    /* reset the auxiliary data buffers */
    s_WipeRBuf(sock);
    if (sock->type == eSOCK_Datagram) {
        s_WipeWBuf(sock);
    } else if ( sock->type != eSOCK_ServerSideKeep ) {
        /* set the close()'s linger period be equal to the close timeout */
#if (defined(NCBI_OS_UNIX) && !defined(NCBI_OS_BEOS)) || defined(NCBI_OS_MSWIN)
        /* setsockopt() is not implemented for MAC (MIT socket emulation lib)*/
        if (sock->w_status != eIO_Closed  &&  (!sock->c_timeout        ||
                                               sock->c_timeout->tv_sec ||
                                               sock->c_timeout->tv_usec)) {
            const struct timeval* tv = sock->c_timeout;
            unsigned int  tmo;
            struct linger lgr;
            if (!tv)
                tmo = 120; /*this is a standard TCP TTL, 2 minutes*/
            else if (!(tmo = tv->tv_sec + (tv->tv_usec + 500000)/1000000))
                tmo = 1;
            lgr.l_onoff  = 1;
            lgr.l_linger = tmo;
            if (setsockopt(sock->sock, SOL_SOCKET, SO_LINGER,
                           (char*) &lgr, sizeof(lgr)) != 0) {
                int x_errno = SOCK_ERRNO;
                CORE_LOGF_ERRNO_EX(eLOG_Warning,x_errno,SOCK_STRERROR(x_errno),
                                   ("%s[SOCK::s_Close]  Failed "
                                    "setsockopt(SO_LINGER)", s_ID(sock, _id)));
            }
        }
#endif /*(NCBI_OS_UNIX && !NCBI_OS_BEOS) || NCBI_OS_MSWIN*/

        /* shutdown in both directions */
        s_Shutdown(sock, eIO_ReadWrite, sock->c_timeout);

        /* set the socket back to blocking mode */
        if ( !s_SetNonblock(sock->sock, 0/*false*/) ) {
            CORE_LOGF(eLOG_Error, ("%s[SOCK::s_Close]  Failed to set socket "
                                   "back to blocking mode", s_ID(sock, _id)));
        }
    } else {
        status = s_WritePending(sock,sock->c_timeout,0);
        if (status != eIO_Success) {
            CORE_LOGF(eLOG_Warning, ("%s[SOCK::s_Close]  Leaving with some "
                                     "output data pending (%s)",
                                     s_ID(sock, _id), IO_StatusStr(status)));
        }
    }
    sock->w_len = 0;

    /* statistics & logging */
    if (sock->type != eSOCK_Datagram) {
        sock->n_in  += sock->n_read;
        sock->n_out += sock->n_written;
    }
    if (sock->log == eOn  ||  (sock->log == eDefault  &&  s_Log == eOn))
        s_DoLog(sock, eIO_Close, 0, 0, 0);

    status = eIO_Success;
    if ( sock->type != eSOCK_ServerSideKeep ) {
        for (;;) { /* close persistently - retry if interrupted by a signal */
            if (SOCK_CLOSE(sock->sock) == 0)
                break;

            /* error */
            if (SOCK_ERRNO != SOCK_EINTR) {
                int x_errno = SOCK_ERRNO;
                CORE_LOGF_ERRNO_EX(eLOG_Warning,x_errno,SOCK_STRERROR(x_errno),
                                   ("%s[SOCK::s_Close]  Failed close()",
                                    s_ID(sock, _id)));
                status = eIO_Unknown;
                break;
            }
        }
    }

    /* return */
    sock->sock = SOCK_INVALID;
    return status;
}



extern EIO_Status SOCK_Create(const char*     host,
                              unsigned short  port, 
                              const STimeout* timeout,
                              SOCK*           sock)
{
    return SOCK_CreateEx(host, port, timeout, sock, 0, 0, eDefault);
}


extern EIO_Status SOCK_CreateEx(const char*     host,
                                unsigned short  port,
                                const STimeout* timeout,
                                SOCK*           sock,
                                const void*     data,
                                size_t          datalen,
                                ESwitch         log)
{
    unsigned int x_id = ++s_ID_Counter * 1000;
    SOCK         x_sock;

    *sock = 0;
    /* allocate memory for the internal socket structure */
    if (!(x_sock = (SOCK) calloc(1, sizeof(*x_sock))))
        return eIO_Unknown;
    x_sock->sock     = SOCK_INVALID;
    x_sock->id       = x_id;
    x_sock->log      = log;
    x_sock->type     = eSOCK_ClientSide;

    /* setup the I/O data buffer properties */
    BUF_SetChunkSize(&x_sock->r_buf, SOCK_BUF_CHUNK_SIZE);
    if (datalen) {
        if (!BUF_SetChunkSize(&x_sock->w_buf, datalen) ||
            !BUF_Write(&x_sock->w_buf, data, datalen)) {
            char _id[32];
            CORE_LOGF_ERRNO(eLOG_Error, errno,
                            ("%s[SOCK::CreateEx]  Cannot store initial data",
                             s_ID(x_sock, _id)));
            SOCK_Close(x_sock);
            return eIO_Unknown;
        }
    }

    /* connect */
    {{
        EIO_Status status;
        if ((status = s_Connect(x_sock, host, port, timeout)) != eIO_Success) {
            SOCK_Close(x_sock);
            return status;
        }
    }}

    /* success */
    x_sock->r_on_w   = eDefault;
    x_sock->i_on_sig = eDefault;
    *sock = x_sock;
    return eIO_Success;
}


extern EIO_Status SOCK_CreateOnTop(const void*   handle,
                                   size_t        handle_size,
                                   SOCK*         sock)
{
    return SOCK_CreateOnTopEx(handle, handle_size, sock, 0, 0,
                              eDefault, eSCOT_CloseOnClose);
}


extern EIO_Status SOCK_CreateOnTopEx(const void*   handle,
                                     size_t        handle_size,
                                     SOCK*         sock,
                                     const void*   data,
                                     size_t        datalen,
                                     ESwitch       log,
                                     ESCOT_OnClose on_close)
{
    union {
        struct sockaddr    sa;
        struct sockaddr_in in;
#ifdef NCBI_OS_UNIX
        struct sockaddr_un un;
#endif /*NCBI_OS_UNIX*/
    } peer;
    SOCK               x_sock;
    TSOCK_Handle       xx_sock;
    BUF                w_buf = 0;
    unsigned int       x_id = ++s_ID_Counter * 1000;
    SOCK_socklen_t     peerlen = (SOCK_socklen_t) sizeof(peer);
    size_t             socklen;

    *sock = 0;
    assert(!datalen  ||  data);

    if (!handle  ||  handle_size != sizeof(xx_sock)) {
        CORE_LOGF(eLOG_Error, ("SOCK#%u[?]: [SOCK::CreateOnTopEx] "
                               " Invalid handle %s%lu", x_id,
                               handle ? "size "                     : "",
                               handle ? (unsigned long) handle_size : 0));
        assert(0);
        return eIO_InvalidArg;
    }
    memcpy(&xx_sock, handle, sizeof(xx_sock));

    /* initialize internals */
    verify(s_Initialized  ||  SOCK_InitializeAPI() == eIO_Success);

    /* get peer's address */
    if (getpeername(xx_sock, (struct sockaddr*) &peer, &peerlen) != 0 ||
        peerlen != sizeof(peer))
        return eIO_Closed;
#ifdef NCBI_OS_UNIX
    if (peer.sa.sa_family != AF_INET  &&  peer.sa.sa_family != AF_UNIX)
        return eIO_InvalidArg;
#else
    if (peer.sa.sa_family != AF_INET)
        return eIO_InvalidArg;
#endif /*NCBI_OS_UNIX*/

    /* store initial data */
    if (datalen  &&  (!BUF_SetChunkSize(&w_buf, datalen)  ||
                      !BUF_Write(&w_buf, data, datalen))) {
        CORE_LOGF_ERRNO(eLOG_Error, errno,
                        ("SOCK#%u[%u]: [SOCK::CreateOnTopEx] "
                         " Cannot store initial data", x_id,
                         (unsigned int) xx_sock));
        BUF_Destroy(w_buf);
        return eIO_Unknown;
    }

    /* create and fill socket handle */
#ifdef NCBI_OS_UNIX
    if (peer.sa.sa_family == AF_UNIX) {
        if (!peer.un.sun_path[0]) {
            CORE_LOGF(eLOG_Error, ("SOCK#%u[%u]: [SOCK::CreateOnTopEx] "
                                   " Cannot deal with anonymous UNIX sockets",
                                   x_id, (unsigned int) xx_sock));
            assert(0);
            BUF_Destroy(w_buf);
            return eIO_InvalidArg;
        }
        socklen = strlen(peer.un.sun_path);
    } else
#endif /*NCBI_OS_UNIX*/
        socklen = 0;
    if (!(x_sock = (SOCK) calloc(1, sizeof(*x_sock) + socklen))) {
        BUF_Destroy(w_buf);
        return eIO_Unknown;
    }
    x_sock->sock     = xx_sock;
    x_sock->id       = x_id;
#ifdef NCBI_OS_UNIX
    if (peer.sa.sa_family != AF_UNIX) {
        x_sock->host = peer.in.sin_addr.s_addr;
        x_sock->port = peer.in.sin_port;
    } else
        strcpy(x_sock->file, peer.un.sun_path);
#else
    x_sock->host     = peer.in.sin_addr.s_addr;
    x_sock->port     = peer.in.sin_port;
#endif /*NCBI_OS_UNIX*/
    x_sock->log      = log;
    x_sock->type     = (on_close != eSCOT_KeepOnClose
                        ? eSOCK_ServerSide
                        : eSOCK_ServerSideKeep);
    x_sock->r_on_w   = eDefault;
    x_sock->i_on_sig = eDefault;
    x_sock->r_status = eIO_Success;
    x_sock->eof      = 0/*false*/;
    x_sock->w_status = eIO_Success;
    x_sock->pending  = 1/*have to check at the nearest I/O*/;
    /* all timeouts zeroed - infinite */
    BUF_SetChunkSize(&x_sock->r_buf, SOCK_BUF_CHUNK_SIZE);
    x_sock->w_buf    = w_buf;
    x_sock->w_len    = datalen;

    /* set to non-blocking mode */
    if ( !s_SetNonblock(xx_sock, 1/*true*/) ) {
        char _id[32];
        CORE_LOGF(eLOG_Error, ("%s[SOCK::CreateOnTopEx] "
                               " Failed to set socket to non-blocking mode",
                               s_ID(x_sock, _id)));
        x_sock->sock = SOCK_INVALID;
        SOCK_Close(x_sock);
        return eIO_Unknown;
    }

    /* statistics & logging */
    if (log == eOn  ||  (log == eDefault  &&  s_Log == eOn))
        s_DoLog(x_sock, eIO_Open, &peer, 0, &peer.sa);

    /* success */
    *sock = x_sock;
    return eIO_Success;
}


extern EIO_Status SOCK_Reconnect(SOCK            sock,
                                 const char*     host,
                                 unsigned short  port,
                                 const STimeout* timeout)
{
    char _id[32];

    if (sock->sock == eSOCK_Datagram) {
        CORE_LOGF(eLOG_Error, ("%s[SOCK::Reconnect]  Called for "
                               "datagram socket", s_ID(sock, _id)));
        assert(0);
        return eIO_InvalidArg;
    }

#ifdef NCBI_OS_UNIX
    if (sock->file[0]) {
        CORE_LOGF(eLOG_Error, ("%s[SOCK::Reconnect]  Called for UNIX socket "
                               "\"%s\"", s_ID(sock, _id), sock->file));
        assert(0);
        return eIO_InvalidArg;
    }
#endif /*NCBI_OS_UNIX*/

    /* close the socket if necessary */
    if (sock->sock != SOCK_INVALID)
        s_Close(sock);

    /* special treatment for server-side socket */
    if (sock->type & eSOCK_ServerSide) {
        if (!host  ||  !*host  ||  !port) {
            CORE_LOGF(eLOG_Error, ("%s[SOCK::Reconnect]  Attempt to reconnect "
                                   "server-side socket as client one to "
                                   "its peer address", s_ID(sock, _id)));
            return eIO_InvalidArg;
        }
        sock->type = eSOCK_ClientSide;
    }

    /* connect */
    sock->id++;
    return s_Connect(sock, host, port, timeout);
}


extern EIO_Status SOCK_Shutdown(SOCK      sock,
                                EIO_Event how)
{
    char _id[32];

    if (sock->sock == SOCK_INVALID) {
        CORE_LOGF(eLOG_Error, ("%s[SOCK::Shutdown] "
                               " Invalid socket", s_ID(sock, _id)));
        assert(0);
        return eIO_Unknown;
    }
    if (sock->type == eSOCK_Datagram) {
        CORE_LOGF(eLOG_Error, ("%s[SOCK::Shutdown] "
                               " Datagram socket", s_ID(sock, _id)));
        assert(0);
        return eIO_InvalidArg;
    }

    return s_Shutdown(sock, how, sock->w_timeout);
}


extern EIO_Status SOCK_Close(SOCK sock)
{
    EIO_Status status = sock->sock==SOCK_INVALID ? eIO_Success : s_Close(sock);

    assert(sock->sock == SOCK_INVALID);
    BUF_Destroy(sock->r_buf);
    BUF_Destroy(sock->w_buf);
    free(sock);
    return status;
}


extern EIO_Status SOCK_Wait(SOCK            sock,
                            EIO_Event       event,
                            const STimeout* timeout)
{
    char _id[32];

    if (sock->sock == SOCK_INVALID) {
        CORE_LOGF(eLOG_Error, ("%s[SOCK::Wait] "
                               " Invalid socket", s_ID(sock, _id)));
        assert(0);
        return eIO_Unknown;
    }

    /* check against already shutdown socket there */
    switch ( event ) {
    case eIO_Read:
        if (BUF_Size(sock->r_buf) != 0)
            return eIO_Success;
        if (sock->type == eSOCK_Datagram)
            return eIO_Closed;
        if (sock->r_status == eIO_Closed) {
            CORE_LOGF(eLOG_Warning, ("%s[SOCK::Wait(R)]  Socket %s",
                                     s_ID(sock, _id),
                                     sock->eof ? "closed" : "shut down"));
            return eIO_Closed;
        }
        if ( sock->eof )
            return sock->r_status;
        break;
    case eIO_Write:
        if (sock->type == eSOCK_Datagram)
            return eIO_Success;
        if (sock->w_status == eIO_Closed) {
            CORE_LOGF(eLOG_Warning, ("%s[SOCK::Wait(W)]  Socket shut down",
                                     s_ID(sock, _id)));
            return eIO_Closed;
        }
        break;
    case eIO_ReadWrite:
        if (sock->type == eSOCK_Datagram  ||  BUF_Size(sock->r_buf) != 0)
            return eIO_Success;
        if (sock->r_status == eIO_Closed  &&  sock->w_status == eIO_Closed) {
            CORE_LOGF(eLOG_Warning, ("%s[SOCK::Wait(RW)]  Socket shut down",
                                     s_ID(sock, _id)));
            return eIO_Closed;
        }
        if ( sock->eof )
            return sock->r_status;
        if (sock->r_status == eIO_Closed) {
            CORE_LOGF(eLOG_Note, ("%s[SOCK::Wait(RW)]  Socket %s",
                                  s_ID(sock, _id), sock->eof
                                  ? "closed" : "shut down for reading"));
            event = eIO_Write;
            break;
        }
        if (sock->w_status == eIO_Closed) {
            CORE_LOGF(eLOG_Note, ("%s[SOCK::Wait(RW)]  Socket shut down for "
                                  "writing", s_ID(sock, _id)));
            event = eIO_Read;
            break;
        }
        break;
    default:
        CORE_LOGF(eLOG_Error, ("%s[SOCK::Wait]  Invalid event %u",
                               s_ID(sock, _id), (unsigned int) event));
        return eIO_InvalidArg;
    }

    assert(sock->type != eSOCK_Datagram);
    /* do wait */
    {{
        struct timeval tv;
        SSOCK_Poll     poll;
        EIO_Status     status;
        const struct timeval* x_tv = s_to2tv(timeout, &tv);

        if ((status = s_WritePending(sock, x_tv, 0)) != eIO_Success) {
            if (event == eIO_Write  ||  sock->pending)
                return status;
        }
        poll.sock   = sock;
        poll.event  = event;
        poll.revent = eIO_Open;
        if ((status = s_SelectStallsafe(1, &poll, x_tv, 0)) != eIO_Success)
            return status;
        if (poll.revent == eIO_Close)
            return eIO_Unknown;
        assert(poll.event == poll.revent);
        return status/*success*/;
    }}
}


extern EIO_Status SOCK_Poll(size_t          n,
                            SSOCK_Poll      polls[],
                            const STimeout* timeout,
                            size_t*         n_ready)
{
    SSOCK_Poll    xx_polls[2];
    SSOCK_Poll*    x_polls;
    EIO_Status     status;
    size_t         x_n;
    struct timeval tv;

    if ((n == 0) != (polls == 0)) {
        if ( n_ready )
            *n_ready = 0;
        return eIO_InvalidArg;
    }

    for (x_n = 0; x_n < n; x_n++) {
        if (polls[x_n].sock                        &&
            polls[x_n].sock->sock != SOCK_INVALID  &&
            (polls[x_n].event == eIO_Read  ||
             polls[x_n].event == eIO_ReadWrite)    &&
            BUF_Size(polls[x_n].sock->r_buf) != 0) {
            polls[x_n].revent = eIO_Read;
            if (polls[x_n].event == eIO_ReadWrite  &&
                polls[x_n].sock->type == eSOCK_Datagram) {
                polls[x_n].revent = eIO_ReadWrite;
            }
        } else
            polls[x_n].revent = eIO_Open;
    }

    if (n == 1) {
        xx_polls[0] = polls[0];
        xx_polls[1].sock = 0;
        x_polls = xx_polls;
        x_n = 2;
    } else {
        x_polls = polls;
        x_n = n;
    }

    status = s_SelectStallsafe(x_n, x_polls, s_to2tv(timeout, &tv), n_ready);

    if (n == 1)
        polls[0].revent = xx_polls[0].revent;

    return status;
}


extern EIO_Status SOCK_SetTimeout(SOCK            sock,
                                  EIO_Event       event,
                                  const STimeout* timeout)
{
    char _id[32];

    switch ( event ) {
    case eIO_Read:
        sock->r_timeout = s_to2tv(timeout, &sock->r_tv);
        break;
    case eIO_Write:
        sock->w_timeout = s_to2tv(timeout, &sock->w_tv);
        break;
    case eIO_ReadWrite:
        sock->r_timeout = s_to2tv(timeout, &sock->r_tv);
        sock->w_timeout = s_to2tv(timeout, &sock->w_tv);
        break;
    case eIO_Close:
        sock->c_timeout = s_to2tv(timeout, &sock->c_tv);
        break;
    default:
        CORE_LOGF(eLOG_Error, ("%s[SOCK::SetTimeout]  Invalid event %u",
                               s_ID(sock, _id), (unsigned int) event));
        assert(0);
        return eIO_InvalidArg;
    }
    return eIO_Success;
}


extern const STimeout* SOCK_GetTimeout(SOCK      sock,
                                       EIO_Event event)
{
    char _id[32];

    switch ( event ) {
    case eIO_Read:
        return s_tv2to(sock->r_timeout, &sock->r_to);
    case eIO_Write:
        return s_tv2to(sock->w_timeout, &sock->w_to);
    case eIO_Close:
        return s_tv2to(sock->c_timeout, &sock->c_to);
    default:
        CORE_LOGF(eLOG_Error, ("%s[SOCK::GetTimeout]  Invalid event %u",
                               s_ID(sock, _id), (unsigned int) event));
        assert(0);
    }
    return 0;
}


extern EIO_Status SOCK_Read(SOCK           sock,
                            void*          buf,
                            size_t         size,
                            size_t*        n_read,
                            EIO_ReadMethod how)
{
    EIO_Status status;
    size_t     x_read;
    char       _id[32];

    if (sock->sock != SOCK_INVALID) {
        switch ( how ) {
        case eIO_ReadPlain:
            status = s_Read(sock, buf, size, &x_read, 0/*false, read*/);
            break;

        case eIO_ReadPeek:
            status = s_Read(sock, buf, size, &x_read, 1/*true, peek*/);
            break;

        case eIO_ReadPersist:
            x_read = 0;
            do {
                size_t xx_read;
                status = SOCK_Read(sock, (char*) buf + (buf? x_read : 0), size,
                                   &xx_read, eIO_ReadPlain);
                if (status != eIO_Success)
                    break;
                x_read += xx_read;
                size   -= xx_read;
            } while ( size );
            break;

        default:
            CORE_LOGF(eLOG_Error, ("%s[SOCK::Read]  Invalid read method %u",
                                   s_ID(sock, _id), (unsigned int) how));
            assert(0);
            x_read = 0;
            status = eIO_InvalidArg;
            break;
        }
    } else {
        CORE_LOGF(eLOG_Error, ("%s[SOCK::Read]  Invalid socket",
                               s_ID(sock, _id)));
        assert(0);
        x_read = 0;
        status = eIO_Unknown;
    }

    if ( n_read )
        *n_read = x_read;
    return status;
}


extern EIO_Status SOCK_PushBack(SOCK        sock,
                                const void* buf,
                                size_t      size)
{
    if (sock->sock == SOCK_INVALID) {
        char _id[32];
        CORE_LOGF(eLOG_Error, ("%s[SOCK::PushBack]  Invalid socket",
                               s_ID(sock, _id)));
        assert(0);
        return eIO_Unknown;
    }

    return BUF_PushBack(&sock->r_buf, buf, size) ? eIO_Success : eIO_Unknown;
}


extern EIO_Status SOCK_Write(SOCK            sock,
                             const void*     buf,
                             size_t          size,
                             size_t*         n_written,
                             EIO_WriteMethod how)
{
    EIO_Status status;
    char       _id[32];
    size_t     x_written;

    if (sock->sock != SOCK_INVALID) {
        switch ( how ) {
        case eIO_WritePlain:
            status = s_Write(sock, buf, size, &x_written);
            break;

        case eIO_WritePersist:
            x_written = 0;
            do {
                size_t xx_written;
                status = SOCK_Write(sock, (char*) buf + x_written, size,
                                    &xx_written, eIO_WritePlain);
                if (status != eIO_Success)
                    break;
                x_written += xx_written;
                size      -= xx_written;
            } while ( size );
            break;

        default:
            CORE_LOGF(eLOG_Error, ("%s[SOCK::Write]  Invalid write method %u",
                                   s_ID(sock, _id), (unsigned int) how));
            assert(0);
            x_written = 0;
            status = eIO_InvalidArg;
            break;
        }
    } else {
        CORE_LOGF(eLOG_Error, ("%s[SOCK::Write]  Invalid socket",
                               s_ID(sock, _id)));
        assert(0);
        x_written = 0;
        status = eIO_Unknown;
    }

    if ( n_written )
        *n_written = x_written;
    return status;
}


extern EIO_Status SOCK_Abort(SOCK sock)
{
    char _id[32];

    if (sock->sock == SOCK_INVALID) {
        CORE_LOGF(eLOG_Error, ("%s[SOCK::Abort] "
                               " Invalid socket", s_ID(sock, _id)));
        assert(0);
        return eIO_Unknown;
    }
    if (sock->type == eSOCK_Datagram) {
        CORE_LOGF(eLOG_Error, ("%s[SOCK::Abort]  Called for datagram socket",
                               s_ID(sock, _id)));
        assert(0);
        return eIO_InvalidArg;
    }

    sock->eof = 0;
    sock->w_len = 0;
    sock->r_status = sock->w_status = eIO_Closed;
    return eIO_Success;
}


extern EIO_Status SOCK_Status(SOCK      sock,
                              EIO_Event direction)
{
    if (direction != eIO_Read  &&  direction != eIO_Write)
        return eIO_InvalidArg;

    return (sock->sock == SOCK_INVALID ? eIO_Closed :
            sock->pending ? eIO_Timeout : s_Status(sock, direction));
}


extern void SOCK_GetPeerAddress(SOCK            sock,
                                unsigned int*   host,
                                unsigned short* port,
                                ENH_ByteOrder   byte_order)
{
    if ( host ) {
        *host = (unsigned int)
            (byte_order != eNH_HostByteOrder ? sock->host : ntohl(sock->host));
    }
    if ( port ) {
        *port = (unsigned short)
            (byte_order != eNH_HostByteOrder ? sock->port : ntohs(sock->port));
    }
}


extern char* SOCK_GetPeerAddressString(SOCK   sock,
                                       char*  buf,
                                       size_t buflen)
{
    if (!buf  ||  !buflen)
        return 0;
#ifdef NCBI_OS_UNIX
    if (sock->file[0])
        strncpy0(buf, sock->file, buflen - 1);
    else
#endif /*NCBI_OS_UNIX*/
        HostPortToString(sock->host, ntohs(sock->port), buf, buflen);
    return buf;
}


extern EIO_Status SOCK_GetOSHandle(SOCK   sock,
                                   void*  handle,
                                   size_t handle_size)
{
    if (!handle  ||  handle_size != sizeof(sock->sock)) {
        char _id[32];
        CORE_LOGF(eLOG_Error,("%s[SOCK::GetOSHandle]  Invalid handle %s%lu",
                              s_ID(sock, _id), handle ? "size " : "",
                              handle ? (unsigned long) handle_size : 0));
        assert(0);
        return eIO_InvalidArg;
    }

    memcpy(handle, &sock->sock, handle_size);
    return eIO_Success;
}


extern void SOCK_SetReadOnWriteAPI(ESwitch on_off)
{
    if (on_off == eDefault)
        on_off = eOff;
    s_ReadOnWrite = on_off;
}


extern void SOCK_SetReadOnWrite(SOCK sock, ESwitch on_off)
{
    if (sock->type != eSOCK_Datagram)
        sock->r_on_w = on_off;
}


extern void SOCK_SetInterruptOnSignalAPI(ESwitch on_off)
{
    if (on_off == eDefault)
        on_off = eOff;
    s_InterruptOnSignal = on_off;
}


extern void SOCK_SetInterruptOnSignal(SOCK sock, ESwitch on_off)
{
    sock->i_on_sig = on_off;
}


extern void SOCK_SetReuseAddressAPI(ESwitch on_off)
{
    s_ReuseAddress = on_off == eOn ? 1 : 0;
}


extern void SOCK_SetReuseAddress(SOCK sock, int/*bool*/ on_off)
{
    if (sock->sock != SOCK_INVALID && !s_SetReuseAddress(sock->sock, on_off)) {
        int x_errno = SOCK_ERRNO;
        char _id[32];
        CORE_LOGF_ERRNO_EX(eLOG_Warning, x_errno, SOCK_STRERROR(x_errno),
                           ("%s[SOCK::SetReuseAddress] "
                            " Failed setsockopt(%sREUSEADDR)",
                            s_ID(sock, _id), on_off ? "" : "!"));
    }
}


extern EIO_Status DSOCK_Create(SOCK* sock)
{
    return DSOCK_CreateEx(sock, eDefault);
}


extern EIO_Status DSOCK_CreateEx(SOCK* sock, ESwitch log)
{
    unsigned int x_id = ++s_ID_Counter * 1000;
    TSOCK_Handle x_sock;

    *sock = 0;
    /* initialize internals */
    verify(s_Initialized  ||  SOCK_InitializeAPI() == eIO_Success);

    /* create new datagram socket */
    if ((x_sock = socket(AF_INET, SOCK_DGRAM, 0)) == SOCK_INVALID) {
        int x_errno = SOCK_ERRNO;
        CORE_LOGF_ERRNO_EX(eLOG_Error, x_errno, SOCK_STRERROR(x_errno),
                           ("SOCK#%u[?]: [DSOCK::Create] "
                            " Cannot create datagram socket", x_id));
        return eIO_Unknown;
    }

    /* set to non-blocking mode */
    if ( !s_SetNonblock(x_sock, 1/*true*/) ) {
        CORE_LOGF(eLOG_Error, ("SOCK#%u[%u]: [DSOCK::Create] "
                               " Cannot set socket to non-blocking mode",
                               x_id, x_sock));
        SOCK_CLOSE(x_sock);
        return eIO_Unknown;
    }

    if ( !(*sock = (SOCK) calloc(1, sizeof(**sock))) ) {
        SOCK_CLOSE(x_sock);
        return eIO_Unknown;
    }

    /* success... */
    (*sock)->sock     = x_sock;
    (*sock)->id       = x_id;
    /* no host and port - not "connected" */
    (*sock)->log      = log;
    (*sock)->type     = eSOCK_Datagram;
    (*sock)->r_on_w   = eOff;
    (*sock)->i_on_sig = eDefault;
    (*sock)->r_status = eIO_Success;
    (*sock)->eof      = 0/*false*/;
    (*sock)->w_status = eIO_Success;
    /* all timeouts cleared - infinite */
    BUF_SetChunkSize(&(*sock)->r_buf, SOCK_BUF_CHUNK_SIZE);
    BUF_SetChunkSize(&(*sock)->w_buf, SOCK_BUF_CHUNK_SIZE);

    /* statistics & logging */
    if (log == eOn  ||  (log == eDefault  &&  s_Log == eOn))
        s_DoLog(*sock, eIO_Open, 0, 0, 0);

    return eIO_Success;
}


extern EIO_Status DSOCK_Bind(SOCK sock, unsigned short port)
{
    struct sockaddr_in addr;
    char _id[32];

    if (sock->type != eSOCK_Datagram) {
        CORE_LOGF(eLOG_Error, ("%s[DSOCK::Bind] "
                               " Not a datagram socket", s_ID(sock, _id)));
        assert(0);
        return eIO_InvalidArg;
    }
    assert(sock->sock != SOCK_INVALID);

    /* bind */
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port        = htons(port);
#ifdef HAVE_SIN_LEN
    addr.sin_len         = sizeof(addr);
#endif /*HAVE_SIN_LEN*/
    if (bind(sock->sock, (struct sockaddr*)&addr, sizeof(struct sockaddr))!=0){
        int x_errno = SOCK_ERRNO;
        CORE_LOGF_ERRNO_EX(eLOG_Error, x_errno, SOCK_STRERROR(x_errno),
                           ("%s[DSOCK::Bind]  Failed bind()", s_ID(sock,_id)));
        return eIO_Unknown;
    }

    /* statistics & logging */
    if (sock->log == eOn  ||  (sock->log == eDefault  &&  s_Log == eOn))
        s_DoLog(sock, eIO_Open, 0, 0, (struct sockaddr*) &addr);
    return eIO_Success;
}


extern EIO_Status DSOCK_Connect(SOCK sock,
                                const char* host, unsigned short port)
{
    struct sockaddr_in peer;
    char _id[32];

    if (sock->type != eSOCK_Datagram) {
        CORE_LOGF(eLOG_Error, ("%s[DSOCK::Connect] "
                               " Not a datagram socket", s_ID(sock, _id)));
        assert(0);
        return eIO_InvalidArg;
    }
    assert(sock->sock != SOCK_INVALID);

    /* drop all pending data */
    s_WipeRBuf(sock);
    s_WipeWBuf(sock);
    sock->id++;

    /* obtain host and port of the peer */
    if ( port )
        sock->port = htons(port);
    if (host  &&  *host) {
        if ((sock->host = SOCK_gethostbyname(host)) == 0) {
            CORE_LOGF(eLOG_Error, ("%s[DSOCK::Connect]  Failed "
                                   "SOCK_gethostbyname(%.64s)",
                                   s_ID(sock, _id), host));
            return eIO_Unknown;
        }
    }
    if (!sock->host || !sock->port) {
        CORE_LOGF(eLOG_Error, ("%s[DSOCK::Connect]  Cannot connect"
                               "to incomplete address", s_ID(sock, _id)));
        return eIO_InvalidArg;
    }

    /* connect */
    memset(&peer, 0, sizeof(peer));
    peer.sin_family      = AF_INET;
    peer.sin_addr.s_addr = sock->host;
    peer.sin_port        = sock->port;
#ifdef HAVE_SIN_LEN
    peer.sin_len         = sizeof(peer);
#endif /*HAVE_SIN_LEN*/
    if (connect(sock->sock, (struct sockaddr*) &peer, sizeof(peer)) != 0) {
        char addr[64];
        int x_errno = SOCK_ERRNO;
        if (SOCK_ntoa(sock->host, addr, sizeof(addr)) != 0)
            strcpy(addr, "???");
        CORE_LOGF_ERRNO_EX(eLOG_Error, x_errno, SOCK_STRERROR(x_errno),
                           ("%s[SOCK::Reconnect]  Cannot connect"
                            "datagram socket to %s:%hu", s_ID(sock, _id),
                            addr, ntohs(sock->port)));
        return eIO_Unknown;
    }

    /* statistics & logging */
    if (sock->log == eOn  ||  (sock->log == eDefault  &&  s_Log == eOn))
        s_DoLog(sock, eIO_Open, &peer, 0, (struct sockaddr*) &peer);
    return eIO_Success;
}


extern EIO_Status DSOCK_SendMsg(SOCK            sock,
                                const char*     host,
                                unsigned short  port,
                                const void*     data,
                                size_t          datalen)
{
    size_t             x_msgsize;
    char               w[1536];
    EIO_Status         status;
    unsigned short     x_port;
    unsigned int       x_host;
    void*              x_msg;
    struct sockaddr_in addr;

    if (sock->type != eSOCK_Datagram) {
        CORE_LOGF(eLOG_Error, ("%s[DSOCK::SendMsg] "
                               " Not a datagram socket", s_ID(sock, w)));
        assert(0);
        return eIO_InvalidArg;
    }
    assert(sock->sock != SOCK_INVALID);

    if ( datalen ) {
        s_Write(sock, data, datalen, &x_msgsize);
        verify(x_msgsize == datalen);
    }
    sock->eof = 1/*true - finalized message*/;

    x_port = port ? htons(port) : sock->port;
    if (host  &&  *host) {
        if ( !(x_host = SOCK_gethostbyname(host)) ) {
            CORE_LOGF(eLOG_Error, ("%s[DSOCK::SendMsg]  Failed "
                                   "SOCK_gethostbyname(%.64s)",
                                   s_ID(sock, w), host));
            return eIO_Unknown;
        }
    } else
        x_host = sock->host;

    if (!x_host  ||  !x_port) {
        CORE_LOGF(eLOG_Error, ("%s[DSOCK::SendMsg]  Cannot do default send "
                               "on unconnected socket", s_ID(sock, w)));
        return eIO_Unknown;
    }

    if ((x_msgsize = BUF_Size(sock->w_buf)) != 0) {
        if (x_msgsize <= sizeof(w))
            x_msg = w;
        else if ( !(x_msg = malloc(x_msgsize)) )
            return eIO_Unknown;
        verify(BUF_Peek(sock->w_buf, x_msg, x_msgsize) == x_msgsize);
    } else
        x_msg = 0;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = x_host;
    addr.sin_port        = x_port;
#ifdef HAVE_SIN_LEN
    addr.sin_len         = sizeof(addr);
#endif /*HAVE_SIN_LEN*/

    for (;;) { /* optionally auto-resume if interrupted by a signal */
        int  x_written;
        int  x_errno;
        if ((x_written = sendto(sock->sock, x_msg, x_msgsize, 0/*flags*/,
                                (struct sockaddr*) &addr, sizeof(addr))) >= 0){
            /* statistics & logging */
            if (sock->log == eOn  ||  (sock->log == eDefault && s_Log == eOn)){
                s_DoLog(sock, eIO_Write, x_msg, (size_t) x_written,
                        (struct sockaddr*) &addr);
            }
            sock->n_written += x_written;
            sock->n_out++;
            if ((size_t) x_written != x_msgsize) {
                sock->w_status = status = eIO_Closed;
                CORE_LOGF(eLOG_Error, ("%s[DSOCK::SendMsg] "
                                       " Partial datagram sent",s_ID(sock,w)));
                break;
            }
            sock->w_status = status = eIO_Success;
            break;
        }

        /* don't want to handle all possible errors... let them be "unknown" */
        sock->w_status = status = eIO_Unknown;
        x_errno = SOCK_ERRNO;

        /* blocked -- retry if unblocked before the timeout expires */
        /* (use stall protection if specified) */
        if (x_errno == SOCK_EWOULDBLOCK  ||  x_errno == SOCK_EAGAIN) {
            SSOCK_Poll poll;
            poll.sock   = sock;
            poll.event  = eIO_Write;
            poll.revent = eIO_Open;
            /* stall protection:  try pull incoming data from the socket */
            if ((status = s_Select(1, &poll, sock->w_timeout)) != eIO_Success)
                break;
            if (poll.revent == eIO_Close) {
                status = eIO_Unknown;
                break;
            }
            assert(poll.revent == eIO_Write);
            continue;
        }

        if (x_errno != SOCK_EINTR) {
            CORE_LOGF_ERRNO_EX(eLOG_Trace, x_errno, SOCK_STRERROR(x_errno),
                               ("%s[DSOCK::SendMsg] "
                                " Failed sendto()", s_ID(sock, w)));
            break;
        }

        if (sock->i_on_sig == eOn  ||
            (sock->i_on_sig == eDefault  &&  s_InterruptOnSignal == eOn)) {
            sock->w_status = status = eIO_Interrupt;
            break;
        }
    }

    if (x_msg  &&  x_msg != w)
        free(x_msg);
    if (status == eIO_Success)
        sock->w_status = s_WipeWBuf(sock);
    return status;
}


extern EIO_Status DSOCK_RecvMsg(SOCK            sock,
                                void*           buf,
                                size_t          buflen,
                                size_t          msgsize,
                                size_t*         msglen,
                                unsigned int*   sender_addr,
                                unsigned short* sender_port)
{
    size_t     x_msgsize;
    char       w[1536];
    EIO_Status status;
    void*      x_msg;

    if (sock->type != eSOCK_Datagram) {
        CORE_LOGF(eLOG_Error, ("%s[DSOCK::RecvMsg] "
                               " Not a datagram socket", s_ID(sock, w)));
        assert(0);
        return eIO_InvalidArg;
    }
    assert(sock->sock != SOCK_INVALID);

    s_WipeRBuf(sock);
    if ( msglen )
        *msglen = 0;
    if ( sender_addr )
        *sender_addr = 0;
    if ( sender_port )
        *sender_port = 0;

    x_msgsize = (msgsize  &&  msgsize < ((1 << 16) - 1))
        ? msgsize : ((1 << 16) - 1);

    if ( !(x_msg = (x_msgsize <= buflen
                    ? buf : (x_msgsize <= sizeof(w)
                             ? w : malloc(x_msgsize)))) ) {
        return eIO_Unknown;
    }

    for (;;) { /* auto-resume if either blocked or interrupted (optional) */
        int                x_errno;
        struct sockaddr_in addr;
#if defined(HAVE_SOCKLEN_T)
        typedef socklen_t  SOCK_socklen_t;
#elif defined(NCBI_OS_MAC)
        typedef UInt32     SOCK_socklen_t;
#else
        typedef int        SOCK_socklen_t;
#endif /*HAVE_SOCKLEN_T*/
        SOCK_socklen_t     addrlen = (SOCK_socklen_t) sizeof(addr);
        int                x_read = recvfrom(sock->sock, x_msg, x_msgsize, 0,
                                             (struct sockaddr*)&addr,&addrlen);
        if (x_read >= 0) {
            /* got a message */
            sock->r_status = status = eIO_Success;
            if ( x_read ) {
                if ( msglen )
                    *msglen = x_read;
                if ( sender_addr )
                    *sender_addr = addr.sin_addr.s_addr;
                if ( sender_port )
                    *sender_port = ntohs(addr.sin_port);
                if ((size_t) x_read > buflen  &&
                    !BUF_Write(&sock->r_buf,
                               (char*) x_msg  + buflen,
                               (size_t)x_read - buflen)) {
                    sock->r_status = eIO_Unknown;
                }
                if (buflen  &&  x_msgsize > buflen)
                    memcpy(buf, x_msg, buflen);
            }
            /* statistics & logging */
            if (sock->log == eOn  ||  (sock->log == eDefault && s_Log == eOn)){
                s_DoLog(sock, eIO_Read, x_msg, (size_t) x_read,
                        (struct sockaddr*) &addr);
            }
            sock->n_read += x_read;
            sock->n_in++;
            break;
        }

        x_errno = SOCK_ERRNO;
        sock->r_status = status = eIO_Unknown;
        if (x_errno != SOCK_EWOULDBLOCK  &&
            x_errno != SOCK_EAGAIN       &&
            x_errno != SOCK_EINTR) {
            /* catch unknown ERROR */
            CORE_LOGF_ERRNO_EX(eLOG_Trace, x_errno, SOCK_STRERROR(x_errno),
                               ("%s[DSOCK::RecvMsg] "
                                " Failed recvfrom()", s_ID(sock, w)));
            break;
        }

        if (x_errno == SOCK_EWOULDBLOCK  ||  x_errno == SOCK_EAGAIN) {
            SSOCK_Poll poll;
            poll.sock   = sock;
            poll.event  = eIO_Read;
            poll.revent = eIO_Open;
            if ((status = s_Select(1, &poll, sock->r_timeout)) != eIO_Success)
                break;
            if (poll.revent == eIO_Close) {
                status = eIO_Closed;
                break;
            }
            assert(poll.event == eIO_Read  &&  poll.revent == eIO_Read);
            continue;
        }

        if (x_errno != SOCK_EINTR)
            break;

        if (sock->i_on_sig == eOn  ||
            (sock->i_on_sig == eDefault  &&  s_InterruptOnSignal == eOn)) {
            status = eIO_Interrupt;
            break;
        }
    }

    if (x_msgsize > buflen && x_msg != w)
        free(x_msg);
    return status;
}


extern EIO_Status DSOCK_WaitMsg(SOCK sock, const STimeout* timeout)
{
    char           _id[32];
    EIO_Status     status;
    SSOCK_Poll     poll;
    struct timeval tv;

    if (sock->type != eSOCK_Datagram) {
        CORE_LOGF(eLOG_Error, ("%s[DSOCK::WaitMsg] "
                               " Not a datagram socket", s_ID(sock, _id)));
        assert(0);
        return eIO_InvalidArg;
    }
    assert(sock->sock != SOCK_INVALID);

    poll.sock   = sock;
    poll.event  = eIO_Read;
    poll.revent = eIO_Open;
    if ((status = s_Select(1, &poll, s_to2tv(timeout, &tv))) != eIO_Success ||
        poll.revent == eIO_Read) {
        return status;
    }
    assert(poll.revent == eIO_Close);
    return eIO_Closed;
}


extern EIO_Status DSOCK_WipeMsg(SOCK sock, EIO_Event direction)
{
    char _id[32];
    EIO_Status status;

    if (sock->type != eSOCK_Datagram) {
        CORE_LOGF(eLOG_Error, ("%s[DSOCK::WipeMsg] "
                               " Not a datagram socket", s_ID(sock, _id)));
        assert(0);
        return eIO_InvalidArg;
    }
    assert(sock->sock != SOCK_INVALID);

    switch (direction) {
    case eIO_Read:
        sock->r_status = status = s_WipeRBuf(sock);
        break;
    case eIO_Write:
        sock->w_status = status = s_WipeWBuf(sock);
        break;
    default:
        CORE_LOGF(eLOG_Error, ("%s[DSOCK::WipeMsg]  Invalid direction %u",
                               s_ID(sock, _id), (unsigned int) direction));
        assert(0);
        status = eIO_InvalidArg;
        break;
    }

    return status;
}


extern EIO_Status DSOCK_SetBroadcast(SOCK sock, int/*bool*/ broadcast)
{
    char _id[32];

    if (sock->type != eSOCK_Datagram) {
        CORE_LOGF(eLOG_Error, ("%s[DSOCK::SetBroadcast] "
                               " Not a datagram socket", s_ID(sock, _id)));
        assert(0);
        return eIO_InvalidArg;
    }
    assert(sock->sock != SOCK_INVALID);

#if defined(NCBI_OS_UNIX)  ||  defined(NCBI_OS_MSWIN)
    /* setsockopt() is not implemented for MAC (in MIT socket emulation lib) */
    {{
#  ifdef NCBI_OS_MSWIN
        BOOL bcast = !!broadcast;
#  else
        int  bcast = !!broadcast;
#  endif /*NCBI_OS_MSWIN*/
        if (setsockopt(sock->sock, SOL_SOCKET, SO_BROADCAST,
                       (const char*) &bcast, sizeof(bcast)) != 0) {
            int x_errno = SOCK_ERRNO;
            CORE_LOGF_ERRNO_EX(eLOG_Error, x_errno, SOCK_STRERROR(x_errno),
                               ("%s[DSOCK::SetBroadcast]  Failed "
                                "setsockopt(%sBROADCAST)",
                                s_ID(sock, _id), bcast ? "" : "NO"));
            return eIO_Unknown;
        }
    }}
#else
    return eIO_NotSupported;
#endif /*NCBI_OS_UNIX || NXBI_OS_MSWIN*/
    return eIO_Success;
}


extern int/*bool*/ SOCK_IsDatagram(SOCK sock)
{
    return sock->type == eSOCK_Datagram;
}


extern int/*bool*/ SOCK_IsClientSide(SOCK sock)
{
    return sock->type == eSOCK_ClientSide;
}


extern int/*bool*/ SOCK_IsServerSide(SOCK sock)
{
    return sock->type & eSOCK_ServerSide ? 1 : 0;
}


extern int SOCK_gethostname(char*  name,
                            size_t namelen)
{
    int error = 0;

    /* initialize internals */
    verify(s_Initialized  ||  SOCK_InitializeAPI() == eIO_Success);

    assert(name && namelen > 0);
    name[0] = name[namelen - 1] = '\0';
    if (gethostname(name, (int) namelen) != 0) {
        int x_errno = SOCK_ERRNO;
        CORE_LOG_ERRNO_EX(eLOG_Error, x_errno, SOCK_STRERROR(x_errno),
                          "[SOCK_gethostname]  Cannot get local hostname");
        error = 1;
    } else if ( name[namelen - 1] ) {
        CORE_LOG(eLOG_Error, "[SOCK_gethostname]  Buffer too small");
        error = 1;
    }

    if ( !error )
        return 0/*success*/;

    name[0] = '\0';
    return -1/*failed*/;
}


extern int SOCK_ntoa(unsigned int host,
                     char*        buf,
                     size_t       buflen)
{
    const unsigned char* b = (const unsigned char*) &host;
    char str[16];

    assert(buf && buflen > 0);
    verify(sprintf(str, "%u.%u.%u.%u",
                   (unsigned) b[0], (unsigned) b[1],
                   (unsigned) b[2], (unsigned) b[3]) > 0);
    assert(strlen(str) < sizeof(str));

    if (strlen(str) >= buflen) {
        buf[0] = '\0';
        return -1/*failed*/;
    }

    strcpy(buf, str);
    return 0/*success*/;
}


extern unsigned int SOCK_HostToNetLong(unsigned int value)
{
    return htonl(value);
}


extern unsigned short SOCK_HostToNetShort(unsigned short value)
{
    return htons(value);
}


extern unsigned int SOCK_gethostbyname(const char* hostname)
{
    unsigned int host;
    char buf[256];

    /* initialize internals */
    verify(s_Initialized  ||  SOCK_InitializeAPI() == eIO_Success);

    if ( !hostname ) {
        if (SOCK_gethostname(buf, sizeof(buf)) != 0)
            return 0;
        hostname = buf;
    }

    host = inet_addr(hostname);
    if (host == htonl(INADDR_NONE)) {
        int x_errno;
#if defined(HAVE_GETADDRINFO)
        struct addrinfo hints, *out = 0;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = PF_INET; /* currently, we only handle IPv4 */
        if ((x_errno = getaddrinfo(hostname, 0, &hints, &out)) == 0  &&  out) {
            struct sockaddr_in* addr = (struct sockaddr_in *) out->ai_addr;
            assert(addr->sin_family == AF_INET);
            host = addr->sin_addr.s_addr;
        } else {
            if (s_Log == eOn) {
                if (x_errno == EAI_SYSTEM)
                    x_errno = SOCK_ERRNO;
                else
                    x_errno += EAI_BASE;
                CORE_LOG_ERRNO_EX(eLOG_Warning, x_errno,SOCK_STRERROR(x_errno),
                                  "[SOCK_gethostbyname]  Failed "
                                  "getaddrinfo()");
            }
            host = 0;
        }
        if ( out ) {
            freeaddrinfo(out);
        }
#else /* use some variant of gethostbyname */
        struct hostent* he;
#  if defined(HAVE_GETHOSTBYNAME_R)
        struct hostent  x_he;
        char            x_buf[1024];

        x_errno = 0;
#    if (HAVE_GETHOSTBYNAME_R == 5)
        he = gethostbyname_r(hostname, &x_he, x_buf, sizeof(x_buf), &x_errno);
#    elif (HAVE_GETHOSTBYNAME_R == 6)
        if (gethostbyname_r(hostname, &x_he, x_buf, sizeof(x_buf),
                            &he, &x_errno) != 0) {
            assert(he == 0);
            he = 0;
        }
#    else
#      error "Unknown HAVE_GETHOSTBYNAME_R value"
#    endif /*HAVE_GETHOSTNBYNAME_R == N*/
#  else
        CORE_LOCK_WRITE;
        he = gethostbyname(hostname);
#    ifdef NCBI_OS_MAC
        x_errno = SOCK_ERRNO;
#    else
        x_errno = h_errno + DNS_BASE;
#    endif /*NCBI_OS_MAC*/
#  endif /*HAVE_GETHOSTBYNAME_R*/

        if (he && he->h_addrtype == AF_INET && he->h_length == sizeof(host)) {
            memcpy(&host, he->h_addr, sizeof(host));
        } else {
            host = 0;
            if ( he )
                x_errno = EINVAL;
        }

#  if !defined(HAVE_GETHOSTBYNAME_R)
        CORE_UNLOCK;
#  endif /*HAVE_GETHOSTBYNAME_R*/

        if (!host  &&  s_Log == eOn) {
#  ifdef NETDB_INTERNAL
            if (x_errno == NETDB_INTERNAL + DNS_BASE)
                x_errno = SOCK_ERRNO;
#  endif /*NETDB_INTERNAL*/
            CORE_LOG_ERRNO_EX(eLOG_Warning, x_errno, SOCK_STRERROR(x_errno),
                              "[SOCK_gethostbyname]  Failed "
                              "gethostbyname[_r]()");
        }

#endif /*HAVE_GETADDR_INFO*/
    }

    return host;
}


extern char* SOCK_gethostbyaddr(unsigned int host,
                                char*        name,
                                size_t       namelen)
{
    /* initialize internals */
    verify(s_Initialized  ||  SOCK_InitializeAPI() == eIO_Success);

    assert(name && namelen > 0);
    if ( !host ) {
        host = SOCK_gethostbyname(0);
    }

    if ( host ) {
        int x_errno;
#if defined(HAVE_GETNAMEINFO)
        struct sockaddr_in addr;

        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET; /* currently, we only handle IPv4 */
        addr.sin_addr.s_addr = host;
#  ifdef HAVE_SIN_LEN
        addr.sin_len = sizeof(addr);
#  endif /*HAVE_SIN_LEN*/
        if ((x_errno = getnameinfo((struct sockaddr*) &addr, sizeof(addr),
                                   name, namelen, 0, 0, 0)) == 0) {
            return name;
        } else {
            if (s_Log == eOn) {
                if (x_errno == EAI_SYSTEM)
                    x_errno = SOCK_ERRNO;
                else
                    x_errno += EAI_BASE;
                CORE_LOG_ERRNO_EX(eLOG_Warning, x_errno,SOCK_STRERROR(x_errno),
                                  "[SOCK_gethostbyaddr]  Failed "
                                  "getnameinfo()");
            }
            name[0] = '\0';
            return 0;
        }
#else /* use some variant of gethostbyaddr */
        struct hostent* he;
#  if defined(HAVE_GETHOSTBYADDR_R)
        struct hostent  x_he;
        char            x_buf[1024];

        x_errno = 0;
#    if (HAVE_GETHOSTBYADDR_R == 7)
        he = gethostbyaddr_r((char*) &host, sizeof(host), AF_INET, &x_he,
                             x_buf, sizeof(x_buf), &x_errno);
#    elif (HAVE_GETHOSTBYADDR_R == 8)
        if (gethostbyaddr_r((char*) &host, sizeof(host), AF_INET, &x_he,
                            x_buf, sizeof(x_buf), &he, &x_errno) != 0) {
            assert(he == 0);
            he = 0;
        }
#    else
#      error "Unknown HAVE_GETHOSTBYADDR_R value"
#    endif /*HAVE_GETHOSTBYADDR_R == N*/
#  else /*HAVE_GETHOSTBYADDR_R*/
        CORE_LOCK_WRITE;
        he = gethostbyaddr((char*) &host, sizeof(host), AF_INET);
#    ifdef NCBI_OS_MAC
        x_errno = SOCK_ERRNO;
#    else
        x_errno = h_errno + DNS_BASE;
#    endif /*NCBI_OS_MAC*/
#  endif /*HAVE_GETHOSTBYADDR_R*/

        if (!he  ||  strlen(he->h_name) >= namelen) {
            if (he  ||  SOCK_ntoa(host, name, namelen) != 0) {
                x_errno = ERANGE;
                name[0] = '\0';
                name = 0;
            }
        } else {
            strcpy(name, he->h_name);
        }

#  ifndef HAVE_GETHOSTBYADDR_R
        CORE_UNLOCK;
#  endif /*HAVE_GETHOSTBYADDR_R*/

        if (!name  &&  s_Log == eOn) {
#  ifdef NETDB_INTERNAL
            if (x_errno == NETDB_INTERNAL + DNS_BASE)
                x_errno = SOCK_ERRNO;
#  endif /*NETDB_INTERNAL*/
            CORE_LOG_ERRNO_EX(eLOG_Warning, x_errno, SOCK_STRERROR(x_errno),
                              "[SOCK_gethostbyaddr]  Failed "
                              "gethostbyaddr[_r]()");
        }

        return name;

#endif /*HAVE_GETNAMEINFO*/
    }

    name[0] = '\0';
    return 0;
}


/*
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 6.119  2003/07/18 20:04:49  lavr
 * Close all preprocessor conditional with #endif showing the condition
 *
 * Revision 6.118  2003/07/17 18:28:50  lavr
 * On I/O errors in connected socket: modify only direction affected, not both
 *
 * Revision 6.117  2003/07/15 18:09:07  lavr
 * Fix MS-Win compilation
 *
 * Revision 6.116  2003/07/15 16:50:20  lavr
 * Allow to build on-top SOCKs from UNIX socket fds (on UNIX only)
 * +SOCK_GetPeerAddressString()
 *
 * Revision 6.115  2003/06/09 19:47:52  lavr
 * Few changes to relocate some arg checks and asserts
 *
 * Revision 6.114  2003/06/04 20:59:14  lavr
 * s_Read() not to call s_Select() if read timeout is {0, 0}
 *
 * Revision 6.113  2003/05/31 05:17:32  lavr
 * Swap bitfields and enums in chain assignments
 *
 * Revision 6.112  2003/05/21 17:53:40  lavr
 * Add logs for broken connections; update both R and W status on them
 *
 * Revision 6.111  2003/05/21 04:00:12  lavr
 * Latch connection refusals in SOCK state properly
 *
 * Revision 6.110  2003/05/20 21:20:41  lavr
 * Special treatment of size==0 in SOCK_Write()
 *
 * Revision 6.109  2003/05/20 16:47:56  lavr
 * More accurate checks for pending connections/data
 *
 * Revision 6.108  2003/05/19 21:04:37  lavr
 * Fix omission to make listening sockets "connected"
 *
 * Revision 6.107  2003/05/19 18:47:42  lavr
 * Fix MSVC compilation errors
 *
 * Revision 6.106  2003/05/19 16:51:33  lavr
 * +SOCK_SetReuseAddress[API]() - both experimental!
 * Fix for bitfield signedness in some compilers (including MSVC).
 * More development in I/O tolerance to {0,0}-timeout.
 *
 * Revision 6.105  2003/05/14 15:48:54  lavr
 * Typo fix in s_Shutdown() for MSVC (x_how should be used instead of how)
 *
 * Revision 6.104  2003/05/14 14:51:27  lavr
 * BUGFIX: Connection stall in s_Connect()
 *
 * Revision 6.103  2003/05/14 13:19:18  lavr
 * Define SOCK_SHUTDOWN_RDWR for MSVC compilation
 *
 * Revision 6.102  2003/05/14 05:24:36  lavr
 * FIX: Yet another MSVC compilation fix
 *
 * Revision 6.101  2003/05/14 04:30:58  lavr
 * FIX: MSVC compilation of s_Shutdown()
 *
 * Revision 6.100  2003/05/14 04:28:01  lavr
 * BUGFIX: s_LogData -> s_Log
 *
 * Revision 6.99  2003/05/14 03:50:25  lavr
 * Revamped to allow pending connect and initial data output
 *
 * Revision 6.98  2003/05/05 20:24:13  lavr
 * Added EOF-on-read as a trace event to log when verbose mode is on
 *
 * Revision 6.97  2003/05/05 11:41:09  rsmith
 * added defines and declarations to allow cross compilation Mac->Win32
 * using Metrowerks Codewarrior.
 *
 * Revision 6.96  2003/04/30 17:00:18  lavr
 * Added on-stack buffers for small datagrams; few name collisions resolved
 *
 * Revision 6.95  2003/04/25 15:21:26  lavr
 * Explicit cast of calloc()'s result
 *
 * Revision 6.94  2003/04/14 15:14:20  lavr
 * Define SOCK_socklen_t for Mac's recvfrom() specially
 *
 * Revision 6.93  2003/04/11 20:59:06  lavr
 * Aux type SOCK_socklen_t defined centrally
 *
 * Revision 6.92  2003/04/04 21:00:37  lavr
 * +SOCK_CreateOnTop()
 *
 * Revision 6.91  2003/04/04 20:44:35  rsmith
 * do not include arpa/inet.h on CW with MSL.
 *
 * Revision 6.90  2003/04/03 14:16:18  rsmith
 * combine pp symbols NCBI_COMPILER_METROWERKS & _MSL_USING_MW_C_HEADERS into
 * NCBI_COMPILER_MW_MSL
 *
 * Revision 6.89  2003/04/02 16:21:34  rsmith
 * replace _MWERKS_ with NCBI_COMPILER_METROWERKS
 *
 * Revision 6.88  2003/04/02 13:26:07  rsmith
 * include ncbi_mslextras.h when compiling with MSL libs in Codewarrior.
 *
 * Revision 6.87  2003/03/25 22:18:06  lavr
 * shutdown(): Do not warn on ENOTCONN on SGI and OSF1 (in addition to Linux)
 *
 * Revision 6.86  2003/02/28 14:50:18  lavr
 * Add one more explicit cast to "unsigned" in s_DoLogData()
 *
 * Revision 6.85  2003/02/24 21:13:23  lavr
 * More comments added; fix for read-ahead on shut-down-for-write socket
 *
 * Revision 6.84  2003/02/20 17:52:30  lavr
 * Resolve dead-lock condition in s_SelectStallsafe()
 *
 * Revision 6.83  2003/02/04 22:03:54  lavr
 * Workaround for ENOTCONN in shutdown() on Linux; few more fixes
 *
 * Revision 6.82  2003/01/17 16:56:59  lavr
 * Always clear all pending data when reconnecting
 *
 * Revision 6.81  2003/01/17 15:56:05  lavr
 * Keep as much status as possible in failed pending connect
 *
 * Revision 6.80  2003/01/17 15:11:36  lavr
 * Update stat counters in s_Close() instead of s_Connect()
 *
 * Revision 6.79  2003/01/17 01:23:31  lavr
 * Better tracing and message counting
 *
 * Revision 6.78  2003/01/16 19:45:18  lavr
 * Minor patching and cleaning
 *
 * Revision 6.77  2003/01/16 16:32:34  lavr
 * Better logging; few minor patches in datagram socket API functions
 *
 * Revision 6.76  2003/01/15 19:52:47  lavr
 * Datagram sockets added
 *
 * Revision 6.75  2002/12/06 16:38:35  lavr
 * Fix for undefined h_errno on MacOS 9
 *
 * Revision 6.74  2002/12/06 15:06:54  lavr
 * Add missing x_errno definition in s_Connect() for non-Unix platforms
 *
 * Revision 6.73  2002/12/05 21:44:12  lavr
 * Implement SOCK_STRERROR() and do more accurate error logging
 *
 * Revision 6.72  2002/12/05 16:31:14  lavr
 * Define SOCK_Create() as a call
 *
 * Revision 6.71  2002/12/04 21:01:05  lavr
 * -CORE_LOG[F]_SYS_ERRNO()
 *
 * Revision 6.70  2002/12/04 16:55:02  lavr
 * Implement logging on connect and close
 *
 * Revision 6.69  2002/11/08 17:18:18  lavr
 * Minor change: spare -1 in >= by replacing it with >
 *
 * Revision 6.68  2002/11/01 20:12:55  lavr
 * Reimplement SOCK_gethostname() - was somewhat potentally buggy
 *
 * Revision 6.67  2002/10/29 22:20:52  lavr
 * Use proper indentation of preproc. macros; note post-accept() socket state
 *
 * Revision 6.66  2002/10/28 15:45:58  lavr
 * Use "ncbi_ansi_ext.h" privately and use strncpy0()
 *
 * Revision 6.65  2002/10/11 19:50:55  lavr
 * Few renames of internal functions (s_NCBI_Recv, s_Recv, etc)
 * Interface change: SOCK_gethostbyaddr() returns dotted IP address
 * when failed to convert given address into FQDN, it also now accepts
 * 0 to return the name (or dotted IP) of the local host
 *
 * Revision 6.64  2002/09/13 19:26:46  lavr
 * Few style-conforming changes
 *
 * Revision 6.63  2002/09/06 15:45:03  lavr
 * Return eIO_InvalidArg instead of generic eIO_Unknown where appropriate
 *
 * Revision 6.62  2002/09/04 15:11:00  lavr
 * Print ERRNO with failed connection attempt
 *
 * Revision 6.61  2002/08/28 15:59:24  lavr
 * Removed few redundant checks in s_SelectStallsafe()
 *
 * Revision 6.60  2002/08/27 03:16:15  lavr
 * Rename SOCK_htonl -> SOCK_HostToNetLong, SOCK_htons -> SOCK_HostToNetShort
 *
 * Revision 6.59  2002/08/15 18:46:52  lavr
 * s_Select(): do not return immediately if given all NULL sockets in "polls"
 *
 * Revision 6.58  2002/08/13 19:51:44  lavr
 * Fix letter case in s_SetNonblock() call
 *
 * Revision 6.57  2002/08/13 19:39:04  lavr
 * Set accepted socket in non-blocking mode after accept (not inherited)
 *
 * Revision 6.56  2002/08/13 19:29:30  lavr
 * Implement interrupted I/O
 *
 * Revision 6.55  2002/08/12 15:06:38  lavr
 * Implementation of plain and persistent SOCK_Write()
 *
 * Revision 6.54  2002/08/07 16:36:45  lavr
 * Added SOCK_SetInterruptOnSignal[API] calls and support placeholders
 * Renamed SOCK_GetAddress() -> SOCK_GetPeerAddress() and enum
 * ENH_ByteOrder employed in the latter call as the last arg
 * All-UNIX-specific kludge to check connected status on non-blocking socket
 * Added more checks against SOCK_INVALID and more error log messages
 *
 * Revision 6.53  2002/07/18 20:19:58  lebedev
 * NCBI_OS_MAC: unistd.h added
 *
 * Revision 6.52  2002/07/15 19:32:03  lavr
 * Do not intercept SIGPIPE in the case of an installed signal handler
 *
 * Revision 6.51  2002/07/01 20:52:23  lavr
 * Error (trace) printouts added in s_Select() and s_NCBI_Recv()
 *
 * Revision 6.50  2002/06/17 18:28:52  lavr
 * +BeOS specifics (by Vladimir Ivanov)
 *
 * Revision 6.49  2002/06/10 21:14:22  ucko
 * [SOCK_gethostbyaddr] When using getnameinfo, properly initialize the
 * sockaddr_in; in particular, set sin_len when present.
 * Also set sin_len in other functions that build a struct sockaddr_in.
 *
 * Revision 6.48  2002/06/10 19:52:45  lavr
 * Additional failsafe check whether the socket actually connected (Solaris)
 *
 * Revision 6.47  2002/05/13 19:49:22  ucko
 * Indent with spaces rather than tabs
 *
 * Revision 6.46  2002/05/13 19:08:11  ucko
 * Use get{addr,name}info in favor of gethostby{name,addr}(_r) when available
 *
 * Revision 6.45  2002/05/06 19:19:43  lavr
 * Remove unnecessary inits of fields returned from s_Select()
 *
 * Revision 6.44  2002/04/26 16:41:16  lavr
 * Redesign of waiting mechanism, and implementation of SOCK_Poll()
 *
 * Revision 6.43  2002/04/22 20:53:16  lavr
 * +SOCK_htons(); Set close timeout only when the socket was not yet shut down
 *
 * Revision 6.42  2002/04/17 20:05:05  lavr
 * Cosmetic changes
 *
 * Revision 6.41  2002/03/22 19:52:19  lavr
 * Do not include <stdio.h>: included from ncbi_util.h or ncbi_priv.h
 *
 * Revision 6.40  2002/02/11 20:36:44  lavr
 * Use "ncbi_config.h"
 *
 * Revision 6.39  2002/01/28 20:29:52  lavr
 * Distinguish between EOF and severe read error
 * Return eIO_Success if waiting for read in a stream with EOF already seen
 *
 * Revision 6.38  2001/12/03 21:35:32  vakatov
 * + SOCK_IsServerSide()
 * SOCK_Reconnect() - check against reconnect of server-side socket to its peer
 *
 * Revision 6.37  2001/11/07 19:00:11  vakatov
 * LSOCK_Accept() -- minor adjustments
 *
 * Revision 6.36  2001/08/31 16:00:58  vakatov
 * [MSWIN] "setsockopt()" -- Start using SO_REUSEADDR on MS-Win.
 * [MAC]   "setsockopt()" -- Do not use it on MAC whatsoever (as it is not
 *         implemented in the M.I.T. socket emulation lib).
 *
 * Revision 6.35  2001/08/29 17:32:56  juran
 * Define POSIX macros missing from Universal Interfaces 3.4
 * in terms of the 'proper' constants.
 * Complain about unsupported platforms at compile-time, not runtime.
 *
 * Revision 6.34  2001/07/11 16:16:39  vakatov
 * Fixed comments for HAVE_GETHOSTBYNAME_R, HAVE_GETHOSTBYADDR_R; other
 * minor (style and messages) fixes
 *
 * Revision 6.33  2001/07/11 00:54:35  vakatov
 * SOCK_gethostbyname() and SOCK_gethostbyaddr() -- now can work with
 * gethostbyname_r() with 6 args and gethostbyaddr_r() with 8 args
 * (in addition to those with 5 and 7 args, repectively).
 * [NCBI_OS_IRIX] s_Select() -- no final ASSERT() if built on IRIX.
 * SOCK_gethostbyaddr() -- added missing CORE_UNLOCK.
 *
 * Revision 6.32  2001/06/20 21:26:18  vakatov
 * As per A.Grichenko/A.Lavrentiev report:
 *   SOCK_Shutdown() -- typo fixed (use "how" rather than "x_how").
 *   SOCK_Shutdown(READ) -- do not call system shutdown if EOF was hit.
 *   Whenever shutdown on both READ and WRITE:  do the WRITE shutdown first.
 *
 * Revision 6.31  2001/06/04 21:03:08  vakatov
 * + HAVE_SOCKLEN_T
 *
 * Revision 6.30  2001/05/31 15:42:10  lavr
 * INADDR_* constants are all and always in host byte order -
 * this was mistakenly forgotten, and now fixed by use of htonl().
 *
 * Revision 6.29  2001/05/23 21:03:35  vakatov
 * s_SelectStallsafe() -- fix for the interpretation of "default" R-on-W mode
 * (by A.Lavrentiev)
 *
 * Revision 6.28  2001/05/21 15:10:32  ivanov
 * Added (with Denis Vakatov) automatic read on write data from the socket
 * (stall protection).
 * Added functions SOCK_SetReadOnWriteAPI(), SOCK_SetReadOnWrite()
 * and internal function s_SelectStallsafe().
 *
 * Revision 6.27  2001/04/25 19:16:01  juran
 * Set non-blocking mode on Mac OS. (from pjc)
 *
 * Revision 6.26  2001/04/24 21:03:42  vakatov
 * s_NCBI_Recv()   -- restore "r_status" to eIO_Success on success.
 * SOCK_Wait(READ) -- return eIO_Success if data pending in the buffer.
 * (w/A.Lavrentiev)
 *
 * Revision 6.25  2001/04/23 22:22:08  vakatov
 * SOCK_Read() -- special treatment for "buf" == NULL
 *
 * Revision 6.24  2001/04/04 14:58:59  vakatov
 * Cleaned up after R6.23 (and get rid of the C++ style comments)
 *
 * Revision 6.23  2001/04/03 20:30:15  juran
 * Changes to work with OT sockets.
 * Not all of pjc's changes are here -- I will test them shortly.
 *
 * Revision 6.22  2001/03/29 21:15:36  lavr
 * More accurate length calculation in 'SOCK_gethostbyaddr'
 *
 * Revision 6.21  2001/03/22 17:43:54  vakatov
 * Typo fixed in the SOCK_AllowSigPipeAPI() proto
 *
 * Revision 6.20  2001/03/22 17:40:36  vakatov
 * + SOCK_AllowSigPipeAPI()
 *
 * Revision 6.19  2001/03/06 23:54:20  lavr
 * Renamed: SOCK_gethostaddr -> SOCK_gethostbyname
 * Added:   SOCK_gethostbyaddr
 *
 * Revision 6.18  2001/03/02 20:10:29  lavr
 * Typos fixed
 *
 * Revision 6.17  2001/02/28 00:55:38  lavr
 * SOCK_gethostaddr: InitAPI added, SOCK_gethostname used instead of
 * gethostname
 *
 * Revision 6.16  2001/01/26 23:50:32  vakatov
 * s_NCBI_Recv() -- added check for ENOTCONN to catch EOF (mostly for Mac)
 *
 * Revision 6.15  2001/01/25 17:10:41  lavr
 * The following policy applied: on either read or write,
 * n_read and n_written returned to indicate actual number of passed
 * bytes, regardless of error status. eIO_Success means that the
 * operation went through smoothly, while any other status has to
 * be analyzed. Anyway, the number of passed bytes prior the error
 * occurred is returned in n_read and n_written respectively.
 *
 * Revision 6.14  2001/01/23 23:19:34  lavr
 * Typo fixed in comment
 *
 * Revision 6.13  2000/12/28 21:26:27  lavr
 * Cosmetic fix to get rid of "converting -1 to unsigned" warning
 *
 * Revision 6.12  2000/12/26 21:40:03  lavr
 * SOCK_Read modified to handle properly the case of 0 byte reading
 *
 * Revision 6.11  2000/12/05 23:27:09  lavr
 * Added SOCK_gethostaddr
 *
 * Revision 6.10  2000/12/04 17:34:19  beloslyu
 * the order of include files is important, especially on other Unixes!
 * Look the man on inet_ntoa
 *
 * Revision 6.9  2000/11/22 19:29:16  vakatov
 * SOCK_Create() -- pre-set the sock handle to SOCK_INVALID before connect
 *
 * Revision 6.8  2000/11/15 18:51:21  vakatov
 * Add SOCK_Shutdown() and SOCK_Status().  Remove SOCK_Eof().
 * Add more checking and diagnostics.
 * [NOTE: not tested on Mac]
 *
 * Revision 6.7  2000/06/23 19:34:44  vakatov
 * Added means to log binary data
 *
 * Revision 6.6  2000/05/30 23:31:44  vakatov
 * SOCK_host2inaddr() renamed to SOCK_ntoa(), the home-made out-of-scratch
 * implementation to work properly with GCC on IRIX64 platforms
 *
 * Revision 6.5  2000/03/24 23:12:08  vakatov
 * Starting the development quasi-branch to implement CONN API.
 * All development is performed in the NCBI C++ tree only, while
 * the NCBI C tree still contains "frozen" (see the last revision) code.
 *
 * Revision 6.4  2000/02/23 22:34:35  vakatov
 * Can work both "standalone" and as a part of NCBI C++ or C toolkits
 *
 * Revision 6.3  1999/10/19 22:21:48  vakatov
 * Put the call to "gethostbyname()" into a CRITICAL_SECTION
 *
 * Revision 6.2  1999/10/19 16:16:01  vakatov
 * Try the NCBI C and C++ headers only if NCBI_OS_{UNIX, MSWIN, MAC} is
 * not #define'd
 *
 * Revision 6.1  1999/10/18 15:36:38  vakatov
 * Initial revision (derived from the former "ncbisock.[ch]")
 *
 * ===========================================================================
 */
