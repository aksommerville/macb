#include "macb.h"
#include <time.h>

/* Create.
 */
 
static int macb_main_create(struct macb_request *request) {
  int result=0,fd=-1;
  #define FAIL { result=-1; goto _done_; }
  
  // Set defaults.
  if (macb_request_infer_archive_path_if_missing(request)<0) return -1;
  
  // Acquire inputs.
  void *df=0,*rf=0,*fi=0;
  int dfc=0,rfc=0,fic=0;
  if (request->dfpathc) {
    if ((dfc=macb_file_read(&df,request->dfpath))<0) {
      fprintf(stderr,"%s: Failed to read data fork.\n",request->dfpath);
      FAIL
    }
  }
  if (request->rfpathc) {
    if ((rfc=macb_file_read(&rf,request->rfpath))<0) {
      fprintf(stderr,"%s: Failed to read resource fork.\n",request->rfpath);
      FAIL
    }
  }
  if (request->fipathc) {
    if ((fic=macb_file_read(&fi,request->fipath))<0) {
      fprintf(stderr,"%s: Failed to read finder info.\n",request->fipath);
      FAIL
    }
    if (fic!=128) {
      fprintf(stderr,"%s: Finder info must be exactly 128 bytes (have %d)\n",request->fipath,fic);
      FAIL
    }
  } else {
    if (!(fi=malloc(128))) FAIL
    macb_initialize_header(fi,request);
    fic=128;
  }
  
  // TODO Would it be helpful at this point to guess file types, if unspecified?
  
  // Add type, creator, lengths, and CRC to the header.
  if (macb_finish_header(fi,request,dfc,rfc)<0) FAIL
  
  // Write output.
  if ((fd=macb_file_openw(request->arpath))<0) {
    fprintf(stderr,"%s: Failed to open file for writing.\n",request->arpath);
    FAIL
  }
  if (macb_file_append(fd,fi,fic)<0) FAIL
  if (macb_file_append(fd,df,dfc)<0) FAIL
  if (dfc&127) {
    if (macb_file_append(fd,0,128-(dfc&127))<0) FAIL
  }
  if (macb_file_append(fd,rf,rfc)<0) FAIL
  if (rfc&127) {
    if (macb_file_append(fd,0,128-(rfc&127))<0) FAIL
  }
  
 _done_:
  if (df) free(df);
  if (rf) free(rf);
  free(fi);
  if (fd>=0) macb_file_close(fd);
  return result;
}

/* Extract.
 */
 
static int macb_extract_guess_outputs(struct macb_request *request,int dflen,int rflen) {

  // Issue a warning if both forks are empty -- that means we are (validly) not producing any output.
  if (!dflen&&!rflen) {
    fprintf(stderr,"%s:WARNING: Both forks empty. Not producing any output.\n",request->arpath);
    return 0;
  }

  // Output path prefix. Strip ".bin" if present, otherwise just the archive path.
  const char *pfx=request->arpath;
  int pfxc=request->arpathc;
  if ((pfxc>=4)&&!memcmp(pfx+pfxc-4,".bin",4)) pfxc-=4;
  
  if (dflen) {
    char *n=malloc(pfxc+6);
    if (!n) return -1;
    memcpy(n,pfx,pfxc);
    memcpy(n+pfxc,".data",6);
    if (request->dfpath) free(request->dfpath);
    request->dfpath=n;
    request->dfpathc=pfxc+5;
  }
  
  if (rflen) {
    char *n=malloc(pfxc+5);
    if (!n) return -1;
    memcpy(n,pfx,pfxc);
    memcpy(n+pfxc,".res",5);
    if (request->rfpath) free(request->rfpath);
    request->rfpath=n;
    request->rfpathc=pfxc+4;
  }
  
  return 0;
}
 
static int macb_extract_inner(struct macb_request *request,const uint8_t *src,int srcc) {

  // Get fork lengths and positions and validate aggressively.
  int dflen=macb_rd32(src,0x53);
  int rflen=macb_rd32(src,0x57);
  int addlhdrlen=macb_rd16(src,0x78);
  int dfp=0,rfp=0;
  if (addlhdrlen) {
    fprintf(stderr,
      "%s:WARNING: Additional header length %d. macb's author is not sure how to handle this, corruption may ensue.\n",
      request->arpath,addlhdrlen
    );
    addlhdrlen=(addlhdrlen+127)&~127;
  }
  dfp=128+addlhdrlen;
  rfp=dfp+((dflen+127)&~127);
  if ((dfp<128)||(dflen<0)||(dfp>srcc-dflen)) {
    fprintf(stderr,
      "%s:ERROR: Header indicates data fork %d bytes at %d -- impossible with archive length %d.\n",
      request->arpath,dflen,dfp,srcc
    );
    return -1;
  }
  if ((rfp<128)||(rflen<0)||(rfp>srcc-rflen)) {
    fprintf(stderr,
      "%s:ERROR: Header indicates resource fork %d bytes at %d -- impossible with archive length %d.\n",
      request->arpath,rflen,rfp,srcc
    );
    return -1;
  }
  if ((dfp<rfp+rflen)&&(rfp<dfp+dflen)) {
    fprintf(stderr,
      "%s:ERROR: Data fork (%d@%d) and resource fork (%d@%d) somehow overlap. "
      "This must be a problem with macb, not necessarily with your archive.\n",
      request->arpath,dflen,dfp,rflen,rfp
    );
    return -1;
  }
  
  // If no output arguments were provided, guess.
  if (!request->dfpathc&&!request->rfpathc&&!request->fipathc) {
    if (macb_extract_guess_outputs(request,dflen,rflen)<0) return -1;
  }
  
  // Write all files for which we have an output path.
  if (request->dfpathc) {
    if (macb_file_write(request->dfpath,src+dfp,dflen)<0) {
      fprintf(stderr,"%s: Failed to write %d-byte data fork.\n",request->dfpath,dflen);
      return -1;
    } else {
      printf("%s: Extracted data fork, %d bytes.\n",request->dfpath,dflen);
    }
  }
  if (request->rfpathc) {
    if (macb_file_write(request->rfpath,src+rfp,rflen)<0) {
      fprintf(stderr,"%s: Failed to write %d-byte resource fork.\n",request->rfpath,rflen);
      return -1;
    } else {
      printf("%s: Extracted resource fork, %d bytes.\n",request->rfpath,rflen);
    }
  }
  if (request->fipathc) {
    if (macb_file_write(request->fipath,src,128)<0) {
      fprintf(stderr,"%s: Failed to write 128-byte header.\n",request->fipath);
      return -1;
    } else {
      printf("%s: Extracted header.\n",request->fipath);
    }
  }

  return 0;
}
 
static int macb_main_extract(struct macb_request *request) {

  if (!request->arpathc) {
    fprintf(stderr,"Archive path required with '-x'\n");
    return -1;
  }
  
  uint8_t *src=0;
  int srcc=macb_file_read(&src,request->arpath);
  if (srcc<0) {
    fprintf(stderr,"%s: Failed to read archive file.\n",request->arpath);
    return -1;
  }
  if (srcc<128) {
    fprintf(stderr,"%s: Length %d less than 128, this can't be MacBinary.\n",request->arpath,srcc);
    free(src);
    return -1;
  }
  
  int err=macb_extract_inner(request,src,srcc);
  free(src);
  return err;
}

/* Tell.
 */
 
static void macb_report_ostype(const uint8_t *src,const char *what,const char *path) {
  int kosher=1,i=4;
  while (i-->0) {
    if ((src[i]<0x20)||(src[i]>0x7e)) kosher=0;
  }
  if (kosher) {
    printf("%s:INFO: File %s '%.4s'\n",path,what,src);
  } else {
    uint32_t be=(src[0]<<24)|(src[1]<<16)|(src[2]<<8)|src[3];
    printf("%s:WARNING: Unprintable file %s: 0x%08x\n",path,what,be);
  }
}

static void macb_report_time(uint32_t v,const char *which,const char *path) {
  // I'm not going to worry about the details of timezone, daylight savings, leap seconds, yadda yadda
  time_t unixtime=v-(int64_t)UNIX_EPOCH_IN_MAC_TIME;
  struct tm local={0};
  if (localtime_r(&unixtime,&local)==&local) {
    printf(
      "%s:INFO: %s time %04d-%02d-%02dT%02d:%02d:%02d\n",
      path,which,
      local.tm_year+1900,local.tm_mon+1,local.tm_mday,local.tm_hour,local.tm_min,local.tm_sec
    );
  } else {
    printf("%s:ERROR: Unable to format %s time 0x%08x.\n",path,which,v);
  }
}
 
static int macb_main_tell(struct macb_request *request) {

  if (!request->arpathc) {
    fprintf(stderr,"Archive path required with '-t'\n");
    return -1;
  }

  uint8_t hdr[128];
  int flen=macb_file_read_header(hdr,request->arpath);
  if (flen<0) {
    fprintf(stderr,"%s: Failed to read header.\n",request->arpath);
    return -1;
  }
  
  // Validate version numbers and whatnot.
  int sigok=1;
  if (hdr[0x00]) {
    printf(
      "%s:ERROR: Leading byte should be zero, found 0x%02x. This is probably not a MacBinary file.\n",
      request->arpath,hdr[0x00]
    );
    sigok=0;
  }
  if (hdr[0x4a]) {
    printf("%s:ERROR: Byte [0x4a] should be zero, found 0x%02x.\n",request->arpath,hdr[0x4a]);
    sigok=0;
  }
  if (hdr[0x52]) {
    printf("%s:ERROR: Byte [0x52] should be zero, found 0x%02x.\n",request->arpath,hdr[0x52]);
    sigok=0;
  }
  if (!memcmp(hdr+0x66,"mBIN",4)) {
    printf("%s:INFO: Detected MacBinary III signature.\n",request->arpath);
  } else if (memcmp(hdr+0x66,"\0\0\0\0\0\0\0\0\0\0\0\0\0\0",14)) {
    printf("%s:WARNING: Expected fourteen zero bytes at 0x66.\n",request->arpath);
    sigok=0;
  }
  if (hdr[0x7e]||hdr[0x7f]) {
    printf(
      "%s:WARNING: Expected two trailing zero bytes in header, found 0x%02x 0x%02x.\n",
      request->arpath,hdr[0x7e],hdr[0x7f]
    );
    sigok=0;
  }
  if (sigok) {
    printf("%s:INFO: Heuristic format check OK.\n",request->arpath);
  }
  
  // Validate lengths.
  int expectlen=128;
  int dflen=macb_rd32(hdr,0x53);
  int rflen=macb_rd32(hdr,0x57);
  int cmtlen=macb_rd16(hdr,0x63);
  int addlhdrlen=macb_rd16(hdr,0x78);
  if (dflen<0) {
    printf("%s:ERROR: Invalid data fork length 0x%08x.\n",request->arpath,dflen);
  } else {
    printf("%s:INFO: Data fork length %d.\n",request->arpath,dflen);
    expectlen+=dflen;
    if (dflen&127) expectlen+=128-(dflen&127);
  }
  if (rflen<0) {
    printf("%s:ERROR: Invalid resource fork length 0x%08x.\n",request->arpath,rflen);
  } else {
    printf("%s:INFO: Resource fork length %d.\n",request->arpath,rflen);
    expectlen+=rflen;
    if (rflen&127) expectlen+=128-(rflen&127);
  }
  if (cmtlen) {
    printf("%s:INFO: Comment length %d.\n",request->arpath,cmtlen);
    expectlen+=cmtlen;
    //TODO The spec doesn't say whether comments should pad to 128 bytes.
    // Since everything else does, I'll assume that this should too.
    if (cmtlen&127) expectlen+=128-(cmtlen&127);
  }
  if (addlhdrlen) {
    printf("%s:INFO: Additional header length %d.\n",request->arpath,addlhdrlen);
    expectlen+=addlhdrlen;
    if (addlhdrlen&127) expectlen+=128-(addlhdrlen&127);
  }
  
  // Validate total length.
  if (expectlen<0) {
    printf("%s:ERROR: Integer overflow calculating total length.\n",request->arpath);
  } else if (!flen) {
    printf(
      "%s:WARNING: Unable to determine archive length. Can't validate against expected length %d.\n",
      request->arpath,expectlen
    );
  } else if (expectlen>flen) {
    printf("%s:ERROR: Expected length %d but found %d.\n",request->arpath,expectlen,flen);
  } else if (expectlen<flen) {
    printf(
      "%s:WARNING: Extra unexpected data (%d - %d = %d extra bytes)\n",
      request->arpath,flen,expectlen,flen-expectlen
    );
  } else {
    printf("%s:INFO: Length %d matches expectation.\n",request->arpath,flen);
  }
  
  // Validate and report file name.
  unsigned char safename[64];
  memcpy(safename,hdr+0x02,63);
  safename[63]=0;
  int namelen=hdr[0x01];
  if (namelen>63) {
    printf("%s:ERROR: Name length %d exceeds buffer size!\n",request->arpath,namelen);
  } else if (!namelen) {
    printf("%s:ERROR: Name length zero.\n",request->arpath);
  } else {
    int loc=0,hic=0;
    int i=0; for (;i<namelen;i++) {
      if (safename[i]<0x20) {
        loc++;
        safename[i]='?';
      } else if (safename[i]>0x7e) {
        hic++;
        safename[i]='?';
      }
    }
    if (loc) {
      printf("%s:WARNING: File name contains %d bytes in 0x00..0x1f. This is probably an error.\n",request->arpath,loc);
    } else if (hic) {
      printf(
        "%s:WARNING: File name contains %d bytes in 0x7e..0xff. Not necessarily a problem, but we won't print them here.\n",
        request->arpath,hic
      );
    }
    printf("%s:INFO: File name '%.*s'\n",request->arpath,namelen,safename);
  }
  
  // Validate and report type, creator, flags, and timestamps.
  macb_report_ostype(hdr+0x41,"type",request->arpath);
  macb_report_ostype(hdr+0x45,"creator",request->arpath);
  
  // Report all other fields.
  printf("%s:INFO: Finder flags 0x%02x,0x%02x.\n",request->arpath,hdr[0x49],hdr[0x65]);//TODO anyone care about the bits' names?
  printf(
    "%s:INFO: Position in Finder window (%d,%d).\n",
    request->arpath,macb_rd16(hdr,0x4d),macb_rd16(hdr,0x4b)
  );
  printf("%s:INFO: Window/folder ID 0x%04x.\n",request->arpath,macb_rd16(hdr,0x4f));
  if (hdr[0x51]==0x01) printf("%s:INFO: Protected bit set.\n",request->arpath);
  else if (hdr[0x51]==0x00) printf("%s:INFO: No protected bit.\n",request->arpath);
  else printf("%s:WARNING: Unexpected value 0x%02x for protected bit.\n",request->arpath,hdr[0x51]);
  macb_report_time(macb_rd32(hdr,0x5b),"Create",request->arpath);
  macb_report_time(macb_rd32(hdr,0x5f),"Modify",request->arpath);
  uint32_t uplen=macb_rd32(hdr,0x74);
  if (uplen) printf("%s:INFO: Unpacked length %d.\n",request->arpath,uplen);
  printf("%s:INFO: MacBinary version source=0x%02x, minimum=0x%02x.\n",request->arpath,hdr[0x7a],hdr[0x7b]);
  
  // Validate CRC.
  uint16_t crcactual=crc_macb(hdr,124,0);
  uint16_t crcexpect=macb_rd16(hdr,124);
  if (crcactual==crcexpect) {
    printf("%s:INFO: CRC 0x%04x matches.\n",request->arpath,crcactual);
  } else {
    printf("%s:ERROR: CRC mismatch! Stated 0x%04x but calculated 0x%04x.\n",request->arpath,crcexpect,crcactual);
  }

  return 0;
}

/* Main entry point.
 */

int main(int argc,char **argv) {
  struct macb_request request={0};
  if (macb_request_init(&request,argc,argv)<0) return 1;
  
  int status=0;
  switch (request.command) {
    case 'h': macb_print_usage((argc>=1)?argv[0]:"macb"); break;
    case 'c': if (macb_main_create(&request)<0) status=1; break;
    case 'x': if (macb_main_extract(&request)<0) status=1; break;
    case 't': if (macb_main_tell(&request)<0) status=1; break;
    case 0: macb_print_usage((argc>=1)?argv[0]:"macb"); status=1; break;
    default: fprintf(stderr,"unknown command '%c'!\n",request.command); status=1; break;
  }
  
  macb_request_cleanup(&request);
  return 0;
}
