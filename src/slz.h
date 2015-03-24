#ifndef _SLZ_H
#define _SLZ_H

#include <stdint.h>

/* We have two macros UNALIGNED_LE_OK and UNALIGNED_FASTER. The latter indicates
 * that using unaligned data is faster than a simple shift. On x86 32-bit at
 * least it is not the case as the per-byte access is 30% faster. A core2-duo on
 * x86_64 is 7% faster to read one byte + shifting by 8 than to read one word,
 * but a core i5 is 7% faster doing the unaligned read, so we privilege more
 * recent implementations here.
 */
#if defined(__x86_64__)
#define UNALIGNED_LE_OK
#define UNALIGNED_FASTER
#elif defined(__i386__) || defined(__i486__) || defined(__i586__) || defined(__i686__) || (defined(__ARMEL__) && defined(__ARM_ARCH_7A__))
#define UNALIGNED_LE_OK
//#define UNALIGNED_FASTER
#endif

/* Log2 of the size of the hash table used for the references table. */
#define HASH_BITS 13

enum slz_state {
	SLZ_ST_INIT,  /* stream initialized */
	SLZ_ST_EOB,   /* header or end of block already sent */
	SLZ_ST_FIXED, /* inside a fixed huffman sequence */
	SLZ_ST_LAST,  /* last block, BFINAL sent */
	SLZ_ST_DONE,  /* BFINAL+EOB sent BFINAL */
	SLZ_ST_END    /* end sent (BFINAL, EOB, CRC + len) */
};

struct slz_stream {
	uint32_t queue; /* last pending bits, LSB first */
	uint32_t qbits; /* number of bits in queue, < 8 */
	unsigned char *outbuf; /* set by encode() */
	enum slz_state state;
	uint32_t crc32;
	uint32_t ilen;
};

/* Functions specific to rfc1951 (deflate) */
void slz_prepare_dist_table();
long slz_rfc1951_encode(struct slz_stream *strm, unsigned char *out, const unsigned char *in, long ilen, int more);
void slz_rfc1951_init(struct slz_stream *strm, unsigned char *buf);
int slz_rfc1951_finish(struct slz_stream *strm, unsigned char *buf);

/* Functions specific to rfc1952 (gzip) */
void slz_make_crc_table(void);
uint32_t slz_crc32_by1(uint32_t crc, const unsigned char *buf, int len);
uint32_t slz_crc32_by4(uint32_t crc, const unsigned char *buf, int len);
long slz_rfc1952_encode(struct slz_stream *strm, unsigned char *out, const unsigned char *in, long ilen, int more);
int slz_rfc1952_send_header(struct slz_stream *strm, unsigned char *buf);
int slz_rfc1952_init(struct slz_stream *strm, unsigned char *buf);
int slz_rfc1952_finish(struct slz_stream *strm, unsigned char *buf);

#endif
