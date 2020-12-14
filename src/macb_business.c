#include "macb.h"
#include <sys/time.h>

/*
https://code.google.com/archive/p/theunarchiver/wikis/MacBinarySpecs.wiki

00   1 version = 0
01   1 file name length 1..63
02  63 file name
41   4 type
45   4 creator
49   1 flags, see below
4a   1 pad = 0
4b   2 vert position = 0
4d   2 horz position = 0
4f   2 window or folder id = 0
51   1 protected (0|1) = 0
52   1 pad = 0
53   4 data len
57   4 res len
5b   4 create time
5f   4 modify time
63   2 comment len = 0
65   1 flags, see below
66  14 unused = 0
74   4 unpacked len = 0
78   2 addl hdr len = 0
7a   1 encoder version = 129
7b   1 required version = 129
7c   2 crc
7e   2 unused = 0
80

Flags at 0x49:
  80 locked
  40 invisible
  20 bundle
  10 system
  08 bozo
  04 busy
  02 changed
  01 inited
  
Flags at 0x65:
  80 hasNoInits
  40 isShared
  20 requiresSwitchLaunch
  10 ColorReserved
  08 color
  04 color
  02 color
  01 isOnDesk
*/

/* Read and write big-endian integers.
 */
 
void macb_wr32(uint8_t *dst,int p,uint32_t v) {
  dst[p+0]=v>>24;
  dst[p+1]=v>>16;
  dst[p+2]=v>>8;
  dst[p+3]=v;
}
 
void macb_wr16(uint8_t *dst,int p,uint16_t v) {
  dst[p+0]=v>>8;
  dst[p+1]=v;
}

uint32_t macb_rd32(const uint8_t *src,int p) {
  return (src[p]<<24)|(src[p+1]<<16)|(src[p+2]<<8)|src[p+3];
}

uint16_t macb_rd16(const uint8_t *src,int p) {
  return (src[p]<<8)|src[p+1];
}

/* Write file name to header, mangle as needed.
 */
 
static void macb_header_apply_file_name(unsigned char *hdr,const char *src,int srcc) {

  // Remove trailing slashes (the hell, user?) and trim to basename.
  while ((srcc>0)&&(src[srcc-1]=='/')) srcc--;
  int slashp=-1;
  int i=srcc;
  while (i-->0) {
    if (src[i]=='/') {
      slashp=i;
      break;
    }
  }
  if (slashp>=0) {
    src=src+slashp+1;
    srcc=srcc-slashp-1;
  }

  // If it ends with ".bin", strike that.
  if ((srcc>=4)&&!memcmp(src+srcc-4,".bin",4)) srcc-=4;
  
  // Trim leading and trailing space.
  while (srcc&&((unsigned char)src[0]<=0x20)) { srcc--; src++; }
  while (srcc&&((unsigned char)src[srcc-1]<=0x20)) srcc--;
  
  // Limit to 63 bytes by removing the end.
  if (srcc>63) srcc=63;
  
  // Copy to header.
  hdr[0x01]=srcc;
  memcpy(hdr+0x02,src,srcc);
  memset(hdr+0x02+srcc,0,63-srcc);
  
  // Replace colon and anything outside G0 with question marks.
  // We assume that this modern host system is not using MacRoman or whatever, high code points are probably a mistake.
  unsigned char *p=hdr+0x02;
  for (i=srcc;i-->0;p++) {
    if ((*p==':')||(*p<0x20)||(*p>0x7e)) *p='?';
  }
}

/* Initialize header.
 */
 
void macb_initialize_header(void *hdr,const struct macb_request *request) {
  unsigned char *HDR=hdr;

  memset(hdr,0,128);
  
  // File name from the archive path.
  macb_header_apply_file_name(hdr,request->arpath,request->arpathc);
  
  // Default type and creator. (if stipulated in (request), we'll pick that up later).
  memcpy(HDR+0x41,"File????",8);

  // Flags.  TODO Is this the appropriate default?
  HDR[0x49]=0x01; // [inited]
  HDR[0x65]=0x00;
  
  // MacBinary versions.
  HDR[0x7a]=0x81;
  HDR[0x7b]=0x81;
}

/* Guess a timestamp.
 * "Create" time is the older of the two inputs' ctimes, or Now.
 * "Modify" time is the younger of the two inputs' mtimes, or Now.
 */
 
static uint32_t macb_time_now() {
  // I'm not addressing time zone, leap seconds, any of that, because who cares.
  struct timeval tv={0};
  gettimeofday(&tv,0);
  return tv.tv_sec+UNIX_EPOCH_IN_MAC_TIME;
}
 
static uint32_t macb_guess_ctime(const struct macb_request *request) {
  uint32_t dtime=macb_stat_ctime(request->dfpath);
  uint32_t rtime=macb_stat_ctime(request->rfpath);
  if (!dtime&&!rtime) return macb_time_now();
  if (!dtime) return rtime;
  if (!rtime) return dtime;
  if (dtime<rtime) return dtime;
  return rtime;
}

static uint32_t macb_guess_mtime(const struct macb_request *request) {
  uint32_t dtime=macb_stat_mtime(request->dfpath);
  uint32_t rtime=macb_stat_mtime(request->rfpath);
  if (!dtime&&!rtime) return macb_time_now();
  if (!dtime) return rtime;
  if (!rtime) return dtime;
  if (dtime>rtime) return dtime;
  return rtime;
}

/* Finish header.
 */

int macb_finish_header(void *hdr,const struct macb_request *request,int dfc,int rfc) {

  // If type or creator was supplied, overwrite it.
  if (request->type) macb_wr32(hdr,0x41,request->type);
  if (request->creator) macb_wr32(hdr,0x45,request->creator);
  
  // Fork lengths.
  macb_wr32(hdr,0x53,dfc);
  macb_wr32(hdr,0x57,rfc);
  
  // Timestamps. Overwrite only if zero.
  if (!macb_rd32(hdr,0x5b)) macb_wr32(hdr,0x5b,macb_guess_ctime(request));
  if (!macb_rd32(hdr,0x5f)) macb_wr32(hdr,0x5f,macb_guess_mtime(request));
  
  // CRC.
  uint16_t crc=crc_macb(hdr,124,0);
  macb_wr16(hdr,0x7c,crc);

  return 0;
}
