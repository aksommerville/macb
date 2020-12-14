#include "macb.h"

/* Cleanup.
 */
 
void macb_request_cleanup(struct macb_request *request) {
  if (request->arpath) free(request->arpath);
  if (request->dfpath) free(request->dfpath);
  if (request->rfpath) free(request->rfpath);
  if (request->fipath) free(request->fipath);
  memset(request,0,sizeof(struct macb_request));
}

/* --help
 */
 
void macb_print_usage(const char *exename) {
  if (!exename||!exename[0]) exename="macb";
  fprintf(stderr,"\nUsage: %s OPTIONS\n",exename);
  fprintf(stderr,
    "\n"
    "OPTIONS:\n"
    "  -h,--help               Print this message.\n"
    "  -x FILE,--extract=FILE  Extract forks from this MacBinary file.\n"
    "  -c FILE,--create=FILE   Create this MacBinary file.\n"
    "  -t FILE,--tell=FILE     Show header of this MacBinary file.\n"
    "  -d FILE,--data=FILE     Data fork (input if -c, output if -x).\n"
    "  -r FILE,--res=FILE      Resource fork (input if -c, output if -x).\n"
    "  -f FILE,--finfo=FILE    Finder Info file (input if -c, output if -x).\n"
    "                          This is the 128-byte MacBinary header. Lengths and CRC are overwritten as needed.\n"
    "  -T STR,--type=STR       Set file type (-c only).\n"
    "  -C STR,--creator=STR    Set file creator (-c only).\n"
    "\n"
    "EXAMPLES:\n"
    "\n"
    "  Create a MacBinary file from existing data and resource forks:\n"
    "    $ macb -c MyNewFile.bin -d MyExistingData -r MyExistingResources -T \"Data\" -C \"Andy\"\n"
    "\n"
    "  Extract both forks:\n"
    "    $ macb -x MyExistingFile.bin -d MyNewData -r MyNewResources\n"
    "\n"
    "  Extract whatever is present and generate sensible file names:\n"
    "    $ macb -x MyExistingFile.bin\n"
    "    # May create 'MyExistingFile.data' and/or 'MyExistingFile.res'\n"
    "\n"
  );
}

/* Option key aliases.
 */
 
static int macb_canonicalize_option(const char *k,int kc) {
  if (kc==1) switch (k[0]) {
    case 'h': return 'h';
    case 'x': return 'x';
    case 'c': return 'c';
    case 't': return 't';
    case 'd': return 'd';
    case 'r': return 'r';
    case 'f': return 'f';
    case 'T': return 'T';
    case 'C': return 'C';
    default: return 0;
  }
  if ((kc==4)&&!memcmp(k,"help",4)) return 'h';
  if ((kc==7)&&!memcmp(k,"extract",7)) return 'x';
  if ((kc==6)&&!memcmp(k,"create",6)) return 'c';
  if ((kc==4)&&!memcmp(k,"tell",4)) return 't';
  if ((kc==4)&&!memcmp(k,"data",4)) return 'd';
  if ((kc==9)&&!memcmp(k,"data-fork",9)) return 'd';
  if ((kc==3)&&!memcmp(k,"res",3)) return 'r';
  if ((kc==8)&&!memcmp(k,"resource",8)) return 'r';
  if ((kc==8)&&!memcmp(k,"res-fork",8)) return 'r';
  if ((kc==13)&&!memcmp(k,"resource-fork",13)) return 'r';
  if ((kc==5)&&!memcmp(k,"finfo",5)) return 'f';
  if ((kc==4)&&!memcmp(k,"type",4)) return 'T';
  if ((kc==7)&&!memcmp(k,"creator",7)) return 'C';
  return 0;
}

/* Apply option.
 */
 
static int macb_set_command(
  struct macb_request *request,
  char command
) {
  if (request->command&&(request->command!=command)) {
    fprintf(stderr,"Conflicting commands '%c' and '%c'\n",request->command,command);
    return -1;
  }
  request->command=command;
  return 0;
}

static int macb_set_string(
  char **dst,int *dstc,
  const char *src,int srcc
) {
  if (*dst) {
    fprintf(stderr,"Conflicting paths '%.*s' and '%.*s'\n",*dstc,*dst,srcc,src);
    return -1;
  }
  if (!(*dst=malloc(srcc+1))) return -1;
  memcpy(*dst,src,srcc);
  (*dst)[srcc]=0;
  *dstc=srcc;
  return 0;
}

static int macb_set_ostype(
  uint32_t *dst,
  const char *src,int srcc
) {
  if (
    (srcc!=4)||
    (src[0]<0x20)||(src[0]>0x7e)||
    (src[1]<0x20)||(src[1]>0x7e)||
    (src[2]<0x20)||(src[2]>0x7e)||
    (src[3]<0x20)||(src[3]>0x7e)
  ) {
    // ASCII is I think not actually a requirement, but you'd be crazy to do otherwise.
    fprintf(stderr,"Type and creator must be 4 ASCII characters.\n");
    return -1;
  }
  uint32_t packed=(src[0]<<24)|(src[1]<<16)|(src[2]<<8)|src[3];
  if (*dst&&(*dst!=packed)) {
    fprintf(stderr,
      "Conflicting type or creator '%c%c%c%c' and '%.4s'\n",
      (*dst)>>24,((*dst)>>16)&0xff,((*dst)>>8)&0xff,(*dst)&0xff,src
    );
    return -1;
  }
  *dst=packed;
  return 0;
}
 
static int macb_apply_option(
  struct macb_request *request,
  char k,
  const char *v,int vc
) {
  switch (k) {
    case 'h': return macb_set_command(request,'h');
    case 'x':
    case 'c':
    case 't': {
        if (macb_set_command(request,k)<0) return -1;
        if (macb_set_string(&request->arpath,&request->arpathc,v,vc)<0) return -1;
      } return 0;
    case 'd': return macb_set_string(&request->dfpath,&request->dfpathc,v,vc);
    case 'r': return macb_set_string(&request->rfpath,&request->rfpathc,v,vc);
    case 'f': return macb_set_string(&request->fipath,&request->fipathc,v,vc);
    case 'T': return macb_set_ostype(&request->type,v,vc);
    case 'C': return macb_set_ostype(&request->creator,v,vc);
    default: {
        fprintf(stderr,"Unexpected option '%c'\n",k);
        return -1;
      }
  }
  return 0;
}

/* Init.
 */

int macb_request_init(
  struct macb_request *request,
  int argc,char **argv
) {
  int argp=1;
  while (argp<argc) {
    const char *arg=argv[argp++];
    
    // Empty argument is illegal.
    if (!arg||!arg[0]) goto _unexpected_;
    
    // No positional arguments.
    if (arg[0]!='-') goto _unexpected_;
    
    // No naked dash arguments, and the count of dashes doesn't matter.
    while (arg[0]=='-') arg++;
    if (!arg[0]) goto _unexpected_;
    
    // Split key and value.
    const char *argi=arg;
    const char *k=argi,*v=0;
    int kc=0,vc=0;
    while (*argi&&(*argi!='=')) { argi++; kc++; }
    if (*argi=='=') {
      argi++;
      v=argi;
      while (*argi) { argi++; vc++; }
    } else if ((argp<argc)&&(argv[argp][0]!='-')) {
      v=argv[argp++];
      while (v[vc]) vc++;
    }
    
    // Canonicalize key -- everything meaningful is a single character.
    char kk=macb_canonicalize_option(k,kc);
    if (!kk) goto _unexpected_;
    
    if (macb_apply_option(request,kk,v,vc)<0) return -1;
    continue;
    
   _unexpected_:
    fprintf(stderr,"%s: Unexpected argument '%s'\n",argv[0],arg);
    return -1;
  }
  return 0;
}

/* Infer archive path.
 */
 
static int macb_request_set_archive_path(
  struct macb_request *request,
  const char *pfx,int pfxc,
  const char *sfx,int sfxc
) {
  int dstc=pfxc+sfxc;
  if (!(request->arpath=malloc(dstc+1))) return -1;
  memcpy(request->arpath,pfx,pfxc);
  memcpy(request->arpath+pfxc,sfx,sfxc);
  request->arpath[dstc]=0;
  request->arpathc=dstc;
  return 1;
}
 
int macb_request_infer_archive_path_if_missing(struct macb_request *request) {
  if (request->arpathc) return 0;
  if (request->arpath) free(request->arpath);
  request->arpath=0;
  
  if ((request->dfpathc>=5)&&!memcmp(request->dfpath+request->dfpathc-5,".data",5)) {
    return macb_request_set_archive_path(request,request->dfpath,request->dfpathc-5,".bin",4);
  }
  
  if ((request->rfpathc>=4)&&!memcmp(request->rfpath+request->rfpathc-4,".res",4)) {
    return macb_request_set_archive_path(request,request->rfpath,request->rfpathc-4,".bin",4);
  }
  
  if ((request->rfpathc>=5)&&!memcmp(request->rfpath+request->rfpathc-5,".rsrc",5)) {
    return macb_request_set_archive_path(request,request->rfpath,request->rfpathc-5,".bin",4);
  }
  
  fprintf(stderr,"Unable to infer archive path.\n");
  return -1;
}
