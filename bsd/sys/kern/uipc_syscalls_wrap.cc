#include <stdint.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>

#include <bsd/uipc_syscalls.h>
#include <osv/debug.h>
#include "libc/af_local.h"

#include "libc/internal/libc.h"

//#define printf(...)		tprintf_d("socket-api", __VA_ARGS__);

extern "C"
int socketpair(int domain, int type, int protocol, int sv[2])
{
	int error;

	printf("socketpair(domain=%d, type=%d, protocol=%d)\n", domain, type,
		protocol);

	if (domain == AF_LOCAL)
		return socketpair_af_local(type, protocol, sv);

	error = linux_socketpair(domain, type, protocol, sv);
	if (error) {
		printf("socketpair() failed, errno=%d", error);
		errno = error;
		return -1;
	}

	return 0;
}

extern "C"
int getsockname(int sockfd, struct bsd_sockaddr *addr, socklen_t *addrlen)
{
	int error;

	printf("getsockname(sockfd=%d, ...)\n", sockfd);

	error = linux_getsockname(sockfd, addr, addrlen);
	if (error) {
		printf("getsockname() failed, errno=%d", error);
		errno = error;
		return -1;
	}

	return 0;
}

extern "C"
int getpeername(int sockfd, struct bsd_sockaddr *addr, socklen_t *addrlen)
{
	int error;

	printf("getpeername(sockfd=%d, ...)\n", sockfd);

	error = linux_getpeername(sockfd, addr, addrlen);
	if (error) {
		printf("getpeername() failed, errno=%d", error);
		errno = error;
		return -1;
	}

	return 0;
}

extern "C"
int accept4(int fd, struct bsd_sockaddr *__restrict addr, socklen_t *__restrict len, int flg)
{
	int fd2, error;

	printf("accept4(fd=%d, ..., flg=%d)\n", fd, flg);

	error = linux_accept4(fd, addr, len, &fd2, flg);
	if (error) {
		printf("accept4() failed, errno=%d", error);
		errno = error;
		return -1;
	}

	return fd2;
}

extern "C"
int accept(int fd, struct bsd_sockaddr *__restrict addr, socklen_t *__restrict len)
{
	int fd2, error;

	printf("accept(fd=%d, ...)\n", fd);

	error = linux_accept(fd, addr, len, &fd2);
	if (error) {
		printf("accept() failed, errno=%d\n", error);
		errno = error;
		return -1;
	}

	return fd2;
}

extern "C"
int bind(int fd, const struct bsd_sockaddr *addr, socklen_t len)
{
	int error;

	printf("bind(fd=%d, ...)\n", fd);

	error = linux_bind(fd, (void *)addr, len);
	if (error) {
		printf("bind() failed, errno=%d", error);
		errno = error;
		return -1;
	}

	return 0;
}

extern "C"
int connect(int fd, const struct bsd_sockaddr *addr, socklen_t len)
{
	int error;

	printf("connect(fd=%d, ...)\n", fd);

	error = linux_connect(fd, (void *)addr, len);
	if (error) {
		printf("connect() failed, errno=%d", error);
		errno = error;
		return -1;
	}

	return 0;
}

extern "C"
int listen(int fd, int backlog)
{
	int error;

	printf("listen(fd=%d, backlog=%d)\n", fd, backlog);

	error = linux_listen(fd, backlog);
	if (error) {
		printf("listen() failed, errno=%d", error);
		errno = error;
		return -1;
	}

	return 0;
}

extern "C"
ssize_t recvfrom(int fd, void *__restrict buf, size_t len, int flags,
		struct bsd_sockaddr *__restrict addr, socklen_t *__restrict alen)
{
	int error;
	ssize_t bytes;

	printf("recvfrom(fd=%d, buf=<uninit>, len=%d, flags=0x%x, ...)\n", fd,
		len, flags);

	error = linux_recvfrom(fd, (caddr_t)buf, len, flags, addr, alen, &bytes);
	if (error) {
		printf("recvfrom() failed, errno=%d", error);
		errno = error;
		return -1;
	}

	return bytes;
}

extern "C"
ssize_t recv(int fd, void *buf, size_t len, int flags)
{
	int error;
	ssize_t bytes;

	printf("recv(fd=%d, buf=<uninit>, len=%d, flags=0x%x)\n", fd, len, flags);

	error = linux_recv(fd, (caddr_t)buf, len, flags, &bytes);
	if (error) {
		printf("recv() failed, errno=%d", error);
		errno = error;
		return -1;
	}

	return bytes;
}

extern "C"
ssize_t recvmsg(int fd, struct msghdr *msg, int flags)
{
	ssize_t bytes;
	int error;

	printf("recvmsg(fd=%d, msg=..., flags=0x%x)\n", fd, flags);

	error = linux_recvmsg(fd, msg, flags, &bytes);
	if (error) {
		printf("recvmsg() failed, errno=%d", error);
		errno = error;
		return -1;
	}

	return bytes;
}

extern "C"
ssize_t sendto(int fd, const void *buf, size_t len, int flags,
    const struct bsd_sockaddr *addr, socklen_t alen)
{
	int error;
	ssize_t bytes;

	printf("sendto(fd=%d, buf=..., len=%d, flags=0x%x, ...\n", fd, len, flags);

	error = linux_sendto(fd, (caddr_t)buf, len, flags, (caddr_t)addr,
			   alen, &bytes);
	if (error) {
		printf("sendto() failed, errno=%d", error);
		errno = error;
		return -1;
	}

	return bytes;
}

extern "C"
ssize_t send(int fd, const void *buf, size_t len, int flags)
{
	int error;
	ssize_t bytes;

	printf("send(fd=%d, buf=..., len=%d, flags=0x%x)\n", fd, len, flags);

	error = linux_send(fd, (caddr_t)buf, len, flags, &bytes);
	if (error) {
		printf("send() failed, errno=%d", error);
		errno = error;
		return -1;
	}

	return bytes;
}

extern "C"
ssize_t sendmsg(int fd, const struct msghdr *msg, int flags)
{
	ssize_t bytes;
	int error;

	printf("sendmsg(fd=%d, msg=..., flags=0x%x)\n", fd, flags);

	error = linux_sendmsg(fd, (struct msghdr *)msg, flags, &bytes);
	if (error) {
		printf("sendmsg() failed, errno=%d", error);
		errno = error;
		return -1;
	}

	return bytes;
}

extern "C"
int getsockopt(int fd, int level, int optname, void *__restrict optval,
		socklen_t *__restrict optlen)
{
	int error;

	printf("getsockopt(fd=%d, level=%d, optname=%d)\n", fd, level, optname);

	error = linux_getsockopt(fd, level, optname, optval, optlen);
	if (error) {
		printf("getsockopt() failed, errno=%d", error);
		errno = error;
		return -1;
	}

	return 0;
}

extern "C"
int setsockopt(int fd, int level, int optname, const void *optval, socklen_t optlen)
{
	int error;

	printf("setsockopt(fd=%d, level=%d, optname=%d, (*(int)optval)=%d, optlen=%d)\n",
		fd, level, optname, *(int *)optval, optlen);

	error = linux_setsockopt(fd, level, optname, (caddr_t)optval, optlen);
	if (error) {
		printf("setsockopt() failed, errno=%d", error);
		errno = error;
		return -1;
	}

	return 0;
}

extern "C"
int shutdown(int fd, int how)
{
	int error;

	printf("shutdown(fd=%d, how=%d)\n", fd, how);

	// Try first if it's a AF_LOCAL socket (af_local.cc), and if not
	// fall back to network sockets. TODO: do this more cleanly.
	error = shutdown_af_local(fd, how);
	if (error != ENOTSOCK) {
	    return error;
	}
	error = linux_shutdown(fd, how);
	if (error) {
		printf("shutdown() failed, errno=%d", error);
		errno = error;
		return -1;
	}

	return 0;
}

extern "C"
int socket(int domain, int type, int protocol)
{
	int s, error;

	printf("socket(domain=%d, type=%d, protocol=%d)\n", domain, type, protocol);

	error = linux_socket(domain, type, protocol, &s);
	if (error) {
		printf("socket() failed, errno=%d", error);
		errno = error;
		return -1;
	}

	return s;
}
