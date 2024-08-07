/* SPDX-License-Identifier: BSD-2-Clause */
#pragma once
#include <lib/error.hpp>

#if __has_include_next(<errno.h>)
#include_next <errno.h>
#endif

namespace Gaia {

namespace Posix {
using Errno = int;

inline Errno error_to_errno(Error error) {
  switch (error) {
  case Error::NO_SUCH_FILE_OR_DIRECTORY:
    return ENOENT;
  case Error::IS_A_DIRECTORY:
    return EISDIR;
  case Error::NOT_A_DIRECTORY:
    return ENOTDIR;
  case Error::NOT_IMPLEMENTED:
    return ENOSYS;
  case Error::NOT_A_TTY:
    return ENOTTY;
  default:
    return -1;
  }
}
} // namespace Posix

/**
 * @brief Converts Errno to a string
 *
 * @param error The error to convert
 * @return The error converted to a string
 */
inline frg::string_view error_to_string(Gaia::Posix::Errno error) {
  switch (error) {
  case EAGAIN:
    return "Operation would block (EAGAIN)";
  case EACCES:
    return "Access denied (EACCESS)";
  case EBADF:
    return "Bad file descriptor (EBADF)";
  case EEXIST:
    return "File exists already (EEXIST)";
  case EFAULT:
    return "Access violation (EFAULT)";
  case EINTR:
    return "Operation interrupted (EINTR)";
  case EINVAL:
    return "Invalid argument (EINVAL)";
  case EIO:
    return "I/O error (EIO)";
  case EISDIR:
    return "Resource is directory (EISDIR)";
  case ENOENT:
    return "No such file or directory (ENOENT)";
  case ENOMEM:
    return "Out of memory (ENOMEM)";
  case ENOTDIR:
    return "Expected directory instead of file (ENOTDIR)";
  case ENOSYS:
    return "Operation not implemented (ENOSYS)";
  case EPERM:
    return "Operation not permitted (EPERM)";
  case EPIPE:
    return "Broken pipe (EPIPE)";
  case ESPIPE:
    return "Seek not possible (ESPIPE)";
  case ENXIO:
    return "No such device or address (ENXIO)";
  case ENOEXEC:
    return "Exec format error (ENOEXEC)";
  case ENOSPC:
    return "No space left on device (ENOSPC)";
  case ENOTSOCK:
    return "Socket operation on non-socket (ENOTSOCK)";
  case ENOTCONN:
    return "Transport endpoint is not connected (ENOTCONN)";
  case EDOM:
    return "Numerical argument out of domain (EDOM)";
  case EILSEQ:
    return "Invalid or incomplete multibyte or wide character (EILSEQ)";
  case ERANGE:
    return "Numerical result out of range (ERANGE)";
  case E2BIG:
    return "Argument list too long (E2BIG)";
  case EADDRINUSE:
    return "Address already in use (EADDRINUSE)";
  case EADDRNOTAVAIL:
    return "Cannot assign requested address (EADDRNOTAVAIL)";
  case EAFNOSUPPORT:
    return "Address family not supported by protocol (EAFNOSUPPORT)";
  case EALREADY:
    return "Operation already in progress (EALREADY)";
  case EBADMSG:
    return "Bad message (EBADMSG)";
  case EBUSY:
    return "Device or resource busy (EBUSY)";
  case ECANCELED:
    return "Operation canceled (ECANCELED)";
  case ECHILD:
    return "No child processes (ECHILD)";
  case ECONNABORTED:
    return "Software caused connection abort (ECONNABORTED)";
  case ECONNREFUSED:
    return "Connection refused (ECONNREFUSED)";
  case ECONNRESET:
    return "Connection reset by peer (ECONNRESET)";
  case EDEADLK:
    return "Resource deadlock avoided (EDEADLK)";
  case EDESTADDRREQ:
    return "Destination address required (EDESTADDRREQ)";
  case EDQUOT:
    return "Disk quota exceeded (EDQUOT)";
  case EFBIG:
    return "File too large (EFBIG)";
  case EHOSTUNREACH:
    return "No route to host (EHOSTUNREACH)";
  case EIDRM:
    return "Identifier removed (EIDRM)";
  case EINPROGRESS:
    return "Operation now in progress (EINPROGRESS)";
  case EISCONN:
    return "Transport endpoint is already connected (EISCONN)";
  case ELOOP:
    return "Too many levels of symbolic links (ELOOP)";
  case EMFILE:
    return "Too many open files (EMFILE)";
  case EMLINK:
    return "Too many links (EMLINK)";
  case EMSGSIZE:
    return "Message too long (EMSGSIZE)";
  case EMULTIHOP:
    return "Multihop attempted (EMULTIHOP)";
  case ENAMETOOLONG:
    return "File name too long (ENAMETOOLONG)";
  case ENETDOWN:
    return "Network is down (ENETDOWN)";
  case ENETRESET:
    return "Network dropped connection on reset (ENETRESET)";
  case ENETUNREACH:
    return "Network is unreachable (ENETUNREACH)";
  case ENFILE:
    return "Too many open files in system (ENFILE)";
  case ENOBUFS:
    return "No buffer space available (ENOBUFS)";
  case ENODEV:
    return "No such device (ENODEV)";
  case ENOLCK:
    return "No locks available (ENOLCK)";
  case ENOLINK:
    return "Link has been severed (ENOLINK)";
  case ENOMSG:
    return "No message of desired type (ENOMSG)";
  case ENOPROTOOPT:
    return "Protocol not available (ENOPROTOOPT)";
  case ENOTEMPTY:
    return "Directory not empty (ENOTEMPTY)";
  case ENOTRECOVERABLE:
    return "Sate not recoverable (ENOTRECOVERABLE)";
  case ENOTSUP:
    return "Operation not supported (ENOTSUP)";
  case ENOTTY:
    return "Inappropriate ioctl for device (ENOTTY)";
  case EOVERFLOW:
    return "Value too large for defined datatype (EOVERFLOW)";
  case EOWNERDEAD:
    return "Owner died (EOWNERDEAD)";
  case EPROTO:
    return "Protocol error (EPROTO)";
  case EPROTONOSUPPORT:
    return "Protocol not supported (EPROTONOSUPPORT)";
  case EPROTOTYPE:
    return "Protocol wrong type for socket (EPROTOTYPE)";
  case EROFS:
    return "Read-only file system (EROFS)";
  case ESRCH:
    return "No such process (ESRCH)";
  case ESTALE:
    return "Stale file handle (ESTALE)";
  case ETIMEDOUT:
    return "Connection timed out (ETIMEDOUT)";
  case ETXTBSY:
    return "Text file busy (ETXTBSY)";
  case EXDEV:
    return "Invalid cross-device link (EXDEV)";
  case ENODATA:
    return "No data available (ENODATA)";
  case ETIME:
    return "Timer expired (ETIME)";
  case ENOKEY:
    return "Required key not available (ENOKEY)";
  case ESHUTDOWN:
    return "Cannot send after transport endpoint shutdown (ESHUTDOWN)";
  case EHOSTDOWN:
    return "Host is down (EHOSTDOWN)";
  case EBADFD:
    return "File descriptor in bad state (EBADFD)";
  case ENOMEDIUM:
    return "No medium found (ENOMEDIUM)";
  case ENOTBLK:
    return "Block device required (ENOTBLK)";
  case ENONET:
    return "Machine is not on the network (ENONET)";
  case EPFNOSUPPORT:
    return "Protocol family not supported (EPFNOSUPPORT)";
  case ESOCKTNOSUPPORT:
    return "Socket type not supported (ESOCKTNOSUPPORT)";
  case ESTRPIPE:
    return "Streams pipe error (ESTRPIPE)";
  case EREMOTEIO:
    return "Remote I/O error (EREMOTEIO)";
  case ERFKILL:
    return "Operation not possible due to RF-kill (ERFKILL)";
  case EBADR:
    return "Invalid request descriptor (EBADR)";
  case EUNATCH:
    return "Protocol driver not attached (EUNATCH)";
  case EMEDIUMTYPE:
    return "Wrong medium type (EMEDIUMTYPE)";
  case EREMOTE:
    return "Object is remote (EREMOTE)";
  case EKEYREJECTED:
    return "Key was rejected by service (EKEYREJECTED)";
  case EUCLEAN:
    return "Structure needs cleaning (EUCLEAN)";
  case EBADSLT:
    return "Invalid slot (EBADSLT)";
  case ENOANO:
    return "No anode (ENOANO)";
  case ENOCSI:
    return "No CSI structure available (ENOCSI)";
  case ENOSTR:
    return "Device not a stream (ENOSTR)";
  case ETOOMANYREFS:
    return "Too many references: cannot splice (ETOOMANYREFS)";
  case ENOPKG:
    return "Package not installed (ENOPKG)";
  case EKEYREVOKED:
    return "Key has been revoked (EKEYREVOKED)";
  case EXFULL:
    return "Exchange full (EXFULL)";
  case ELNRNG:
    return "Link number out of range (ELNRNG)";
  case ENOTUNIQ:
    return "Name not unique on network (ENOTUNIQ)";
  case ERESTART:
    return "Interrupted system call should be restarted (ERESTART)";
  case EUSERS:
    return "Too many users (EUSERS)";
  default:
    return "Unknown error code (?)";
  }
}
} // namespace Gaia