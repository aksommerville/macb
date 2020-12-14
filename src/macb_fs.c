#include "macb.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

/* Read file in one shot.
 */
 
// Artificial read size limit (eg if they ask us to read from an infinite pipe or something).
#define MACB_READ_FILE_LIMIT (128*1024*1024)
 
int macb_file_read_fd(void *dstpp,int fd) {
  int dstc=0,dsta=1024;
  char *dst=malloc(dsta);
  if (!dst) return -1;
  while (1) {
    
    if (dstc>=dsta) {
      dsta<<=1;
      if (dsta>MACB_READ_FILE_LIMIT) {
        free(dst);
        return -1;
      }
      void *nv=realloc(dst,dsta);
      if (!nv) {
        free(dst);
        return -1;
      }
      dst=nv;
    }
    
    int err=read(fd,dst+dstc,dsta-dstc);
    if (err<0) {
      free(dst);
      return -1;
    }
    if (err<dsta-dstc) {
      dstc+=err;
      *(void**)dstpp=dst;
      return dstc;
    }
    dstc+=err;
  }
}
 
int macb_file_read(void *dstpp,const char *path) {
  int fd=open(path,O_RDONLY);
  if (fd<0) return -1;
  int dstc=macb_file_read_fd(dstpp,fd);
  close(fd);
  return dstc;
}

/* Write file in one shot.
 */
 
int macb_file_write(const char *path,const void *src,int srcc) {
  int fd=open(path,O_WRONLY|O_CREAT|O_TRUNC,0666);
  if (fd<0) return -1;
  int srcp=0;
  while (srcp<srcc) {
    int err=write(fd,(char*)src+srcp,srcc-srcp);
    if (err<=0) {
      close(fd);
      unlink(path);
      return -1;
    }
    srcp+=err;
  }
  close(fd);
  return 0;
}

/* Read header and report total length.
 */
 
int macb_file_read_header(void *dst_128b,const char *path) {
  int fd=open(path,O_RDONLY);
  if (!fd) return -1;
  if (read(fd,dst_128b,128)!=128) {
    close(fd);
    return -1;
  }
  off_t flen=lseek(fd,0,SEEK_END);
  close(fd);
  if ((flen<0)||(flen>INT_MAX)) return 0; // Not seekable or giant file. Indicate "got header but not length".
  return flen;
}

/* Open file for piecewise writing.
 */
 
int macb_file_openw(const char *path) {
  return open(path,O_WRONLY|O_CREAT|O_TRUNC,0666);
}

int macb_file_append(int fd,const void *src,int srcc) {
  if ((fd<0)||(srcc<0)) return -1;
  void *freeme=0;
  if (!src) {
    if (!(freeme=calloc(1,srcc))) return -1;
    src=freeme;
  }
  int srcp=0;
  while (srcp<srcc) {
    int err=write(fd,(char*)src+srcp,srcc-srcp);
    if (err<=0) {
      if (freeme) free(freeme);
      return -1;
    }
    srcp+=err;
  }
  if (freeme) free(freeme);
  return srcc;
}

int macb_file_close(int fd) {
  if (fd>=0) return close(fd);
  return 0;
}

/* Stat.
 */
 
uint32_t macb_stat_ctime(const char *path) {
  if (!path||!path[0]) return 0;
  struct stat st={0};
  stat(path,&st);
  if (!st.st_ctime) return 0;
  return st.st_ctime+UNIX_EPOCH_IN_MAC_TIME;
}

uint32_t macb_stat_mtime(const char *path) {
  if (!path||!path[0]) return 0;
  struct stat st={0};
  stat(path,&st);
  if (!st.st_mtime) return 0;
  return st.st_mtime+UNIX_EPOCH_IN_MAC_TIME;
}
