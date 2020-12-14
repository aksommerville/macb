#ifndef MACB_H
#define MACB_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>

#define UNIX_EPOCH_IN_MAC_TIME 2082844800

/* Request.
 **************************************************/
 
struct macb_request {
  // Strings of nonzero length are always NUL-terminated.
  char *arpath; int arpathc; // Archive, ie the MacBinary file.
  char *dfpath; int dfpathc; // Data fork
  char *rfpath; int rfpathc; // Resource fork
  char *fipath; int fipathc; // Finder info (MacBinary header)
  char command; // [cxth]
  uint32_t type,creator; // zero if unset, otherwise OSType; will write big-endianly
};

void macb_request_cleanup(struct macb_request *request);

/* Populate the request from command line.
 * Logs all errors to stderr.
 */
int macb_request_init(
  struct macb_request *request,
  int argc,char **argv
);

void macb_print_usage(const char *exename);

/* Quietly does nothing if the archive path is not empty.
 * Otherwise generates a sensible archive path based on the data or resource paths.
 * May fail and log the error if that's not possible.
 * Returns >0 if it changes something.
 */
int macb_request_infer_archive_path_if_missing(struct macb_request *request);

/* FS.
 *********************************************************/

/* Successful read always allocates its output buffer, even if the returned length is zero.
 * No arguments are checked, caller must sanitize.
 */
int macb_file_read_fd(void *dstpp,int fd);
int macb_file_read(void *dstpp,const char *path);
int macb_file_write(const char *path,const void *src,int srcc);

/* Read the first 128 bytes and return the total length.
 * If the file is not seekable, return zero instead -- caller should issue a warning then.
 */
int macb_file_read_header(void *dst_128b,const char *path);

int macb_file_openw(const char *path); // => fd
int macb_file_append(int fd,const void *src,int srcc); // (src) null to append zeroes.
int macb_file_close(int fd);

/* Read file timestamps and convert to Mac format.
 * Zero on any error; guaranteed safe if null, empty, etc.
 */
uint32_t macb_stat_ctime(const char *path);
uint32_t macb_stat_mtime(const char *path);

/* General MacBinary stuff.
 ********************************************************/

/* (hdr) must point to 128 bytes.
 * We wipe it completely and fill in whatever can be inferred from (request).
 * (request) is optional.
 */
void macb_initialize_header(void *hdr,const struct macb_request *request);

/* Selectively overwrite (hdr) with fork lengths, type, and creator.
 * Then calculate and write the CRC.
 */
int macb_finish_header(void *hdr,const struct macb_request *request,int dfc,int rfc);

void macb_wr32(uint8_t *dst,int p,uint32_t v);
void macb_wr16(uint8_t *dst,int p,uint16_t v);
uint32_t macb_rd32(const uint8_t *src,int p);
uint16_t macb_rd16(const uint8_t *src,int p);

/* BORROWED:
 * hfsutils - tools for reading and writing Macintosh HFS volumes
 * Copyright (C) 1996-1998 Robert Leslie
 */
unsigned short crc_binh(register const unsigned char *, register int,
			register unsigned short);
unsigned short crc_macb(register const unsigned char *, register int,
			register unsigned short);

#endif
