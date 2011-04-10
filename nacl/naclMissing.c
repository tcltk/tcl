/*
 *
 *   naclMissing: "implement" all missing syscalls, library funcs and vars.
 *
 */


#include <stdio.h>
#include <errno.h>
#include <string.h>

int timezone;

/* emulated */

char * getwd (char *buf)
{
  printf("*BADSYSCALL:getwd\n");
  strcpy(buf,"/");
  return "/";
}

/* traced */

int access(const char *name, int mode)
{
  printf("*BADSYSCALL:access(\"%s\",%d)\n",name,mode);
  // return 0;
  errno=ENOENT;return -1;
}

/* plugged */

int _execve (){printf("BADSYSCALL:_execve\n");errno=EINVAL;return -1;}
int accept (){printf("BADSYSCALL:accept\n");errno=EINVAL;return -1;}
//int access (){printf("BADSYSCALL:access\n");errno=EINVAL;return -1;}
int bind (){printf("BADSYSCALL:bind\n");errno=EINVAL;return -1;}
int chdir (){printf("BADSYSCALL:chdir\n");errno=EINVAL;return -1;}
int chmod (){printf("BADSYSCALL:chmod\n");errno=EINVAL;return -1;}
int chown (){printf("BADSYSCALL:chown\n");errno=EINVAL;return -1;}
int connect (){printf("BADSYSCALL:connect\n");errno=EINVAL;return -1;}
int dlclose (){printf("BADSYSCALL:dlclose\n");errno=EINVAL;return -1;}
int dlerror (){printf("BADSYSCALL:dlerror\n");errno=EINVAL;return -1;}
int dlopen (){printf("BADSYSCALL:dlopen\n");errno=EINVAL;return -1;}
int dlsym (){printf("BADSYSCALL:dlsym\n");errno=EINVAL;return -1;}
int dup2 (){printf("BADSYSCALL:\n");errno=EINVAL;return -1;}
int fcntl (){printf("BADSYSCALL:fcntl\n");errno=EINVAL;return -1;}
int fork (){printf("BADSYSCALL:fork\n");errno=EINVAL;return -1;}
int freeaddrinfo (){printf("BADSYSCALL:freeaddrinfo\n");errno=EINVAL;return -1;}
int ftruncate (){printf("BADSYSCALL:ftruncate\n");errno=EINVAL;return -1;}
char * gai_strerror (){printf("BADSYSCALL:gai_strerror\n");errno=EINVAL;return NULL;}
int getaddrinfo (){printf("BADSYSCALL:getaddrinfo\n");errno=EINVAL;return -1;}
int geteuid (){printf("BADSYSCALL:geteuid\n");errno=EINVAL;return -1;}
int getgrgid (){printf("BADSYSCALL:getgrgid\n");errno=EINVAL;return -1;}
void * getgrnam (){printf("BADSYSCALL:getgrnam\n");errno=EINVAL;return NULL;}
void * gethostbyaddr (){printf("BADSYSCALL:gethostbyaddr\n");errno=EINVAL;return NULL;}
void * gethostbyname (){printf("BADSYSCALL:gethostbyname\n");errno=EINVAL;return NULL;}
int gethostname (){printf("BADSYSCALL:gethostname\n");errno=EINVAL;return -1;}
int getnameinfo (){printf("BADSYSCALL:getnameinfo\n");errno=EINVAL;return -1;}
int getpeername (){printf("BADSYSCALL:getpeername\n");errno=EINVAL;return -1;}
void * getpwnam (){printf("BADSYSCALL:getpwnam\n");errno=EINVAL;return NULL;}
void * getpwuid (){printf("BADSYSCALL:getpwuid\n");errno=EINVAL;return NULL;}
void * getservbyname (){printf("BADSYSCALL:getservbyname\n");errno=EINVAL;return NULL;}
int getsockname (){printf("BADSYSCALL:getsockname\n");errno=EINVAL;return -1;}
int getsockopt (){printf("BADSYSCALL:getsockopt\n");errno=EINVAL;return -1;}
int getuid (){printf("BADSYSCALL:getuid\n");errno=EINVAL;return -1;}
//void * getwd (){printf("BADSYSCALL:getwd\n");errno=EINVAL;return NULL;}
void * inet_ntoa (){printf("BADSYSCALL:inet_ntoa\n");errno=EINVAL;return NULL;}
int kill (){printf("BADSYSCALL:kill\n");errno=EINVAL;return -1;}
int link (){printf("BADSYSCALL:link\n");errno=EINVAL;return -1;}
int listen (){printf("BADSYSCALL:listen\n");errno=EINVAL;return -1;}
int lstat (){printf("BADSYSCALL:lstat\n");errno=EINVAL;return -1;}
int mkdir (){printf("BADSYSCALL:mkdir\n");errno=EINVAL;return -1;}
int mkfifo (){printf("BADSYSCALL:mkfifo\n");errno=EINVAL;return -1;}
int mknod (){printf("BADSYSCALL:mknod\n");errno=EINVAL;return -1;}
int pipe (){printf("BADSYSCALL:pipe\n");errno=EINVAL;return -1;}
int readlink (){printf("BADSYSCALL:readlink\n");errno=EINVAL;return -1;}
int recv (){printf("BADSYSCALL:recv\n");errno=EINVAL;return -1;}
int rmdir (){printf("BADSYSCALL:rmdir\n");errno=EINVAL;return -1;}
int select (){printf("BADSYSCALL:select\n");errno=EINVAL;return -1;}
int send (){printf("BADSYSCALL:send\n");errno=EINVAL;return -1;}
int setsockopt (){printf("BADSYSCALL:setsockopt\n");errno=EINVAL;return -1;}
int shutdown (){printf("BADSYSCALL:shutdown\n");errno=EINVAL;return -1;}
int socket (){printf("BADSYSCALL:socket\n");errno=EINVAL;return -1;}
int symlink (){printf("BADSYSCALL:symlink\n");errno=EINVAL;return -1;}
int umask (){printf("BADSYSCALL:umask\n");errno=EINVAL;return -1;}
int unlink (){printf("BADSYSCALL:unlink\n");errno=EINVAL;return -1;}
int utime (){printf("BADSYSCALL:utime\n");errno=EINVAL;return -1;}
int waitpid (){printf("BADSYSCALL:waitpid\n");errno=EINVAL;return -1;}

