#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* From RFC1952 about the GZIP file format :

A gzip file consists of a series of "members" ...

2.3. Member format

      Each member has the following structure:

         +---+---+---+---+---+---+---+---+---+---+
         |ID1|ID2|CM |FLG|     MTIME     |XFL|OS | (more-->)
         +---+---+---+---+---+---+---+---+---+---+

      (if FLG.FEXTRA set)

         +---+---+=================================+
         | XLEN  |...XLEN bytes of "extra field"...| (more-->)
         +---+---+=================================+

      (if FLG.FNAME set)

         +=========================================+
         |...original file name, zero-terminated...| (more-->)
         +=========================================+

      (if FLG.FCOMMENT set)

         +===================================+
         |...file comment, zero-terminated...| (more-->)
         +===================================+

      (if FLG.FHCRC set)

         +---+---+
         | CRC16 |
         +---+---+

         +=======================+
         |...compressed blocks...| (more-->)
         +=======================+

           0   1   2   3   4   5   6   7
         +---+---+---+---+---+---+---+---+
         |     CRC32     |     ISIZE     |
         +---+---+---+---+---+---+---+---+


2.3.1. Member header and trailer

         ID1 (IDentification 1)
         ID2 (IDentification 2)
            These have the fixed values ID1 = 31 (0x1f, \037), ID2 = 139
            (0x8b, \213), to identify the file as being in gzip format.

         CM (Compression Method)
            This identifies the compression method used in the file.  CM
            = 0-7 are reserved.  CM = 8 denotes the "deflate"
            compression method, which is the one customarily used by
            gzip and which is documented elsewhere.

         FLG (FLaGs)
            This flag byte is divided into individual bits as follows:

               bit 0   FTEXT
               bit 1   FHCRC
               bit 2   FEXTRA
               bit 3   FNAME
               bit 4   FCOMMENT
               bit 5   reserved
               bit 6   reserved
               bit 7   reserved

            Reserved FLG bits must be zero.

         MTIME (Modification TIME)
            This gives the most recent modification time of the original
            file being compressed.  The time is in Unix format, i.e.,
            seconds since 00:00:00 GMT, Jan.  1, 1970.  (Note that this
            may cause problems for MS-DOS and other systems that use
            local rather than Universal time.)  If the compressed data
            did not come from a file, MTIME is set to the time at which
            compression started.  MTIME = 0 means no time stamp is
            available.

         XFL (eXtra FLags)
            These flags are available for use by specific compression
            methods.  The "deflate" method (CM = 8) sets these flags as
            follows:

               XFL = 2 - compressor used maximum compression,
                         slowest algorithm
               XFL = 4 - compressor used fastest algorithm

         OS (Operating System)
            This identifies the type of file system on which compression
            took place.  This may be useful in determining end-of-line
            convention for text files.  The currently defined values are
            as follows:

                 0 - FAT filesystem (MS-DOS, OS/2, NT/Win32)
                 1 - Amiga
                 2 - VMS (or OpenVMS)
                 3 - Unix
                 4 - VM/CMS
                 5 - Atari TOS
                 6 - HPFS filesystem (OS/2, NT)
                 7 - Macintosh
                 8 - Z-System
                 9 - CP/M
                10 - TOPS-20
                11 - NTFS filesystem (NT)
                12 - QDOS
                13 - Acorn RISCOS
               255 - unknown

 ==> A file compressed using "gzip -1" on Unix-like systems can be :

        1F 8B 08 00  00 00 00 00  04 03
        <deflate-compressed stream>
        crc32 size32
*/

/* RFC1951 - deflate stream format


             * Data elements are packed into bytes in order of
               increasing bit number within the byte, i.e., starting
               with the least-significant bit of the byte.
             * Data elements other than Huffman codes are packed
               starting with the least-significant bit of the data
               element.
             * Huffman codes are packed starting with the most-
               significant bit of the code.

      3.2.3. Details of block format

         Each block of compressed data begins with 3 header bits
         containing the following data:

            first bit       BFINAL
            next 2 bits     BTYPE

         Note that the header bits do not necessarily begin on a byte
         boundary, since a block does not necessarily occupy an integral
         number of bytes.

         BFINAL is set if and only if this is the last block of the data
         set.

         BTYPE specifies how the data are compressed, as follows:

            00 - no compression
            01 - compressed with fixed Huffman codes
            10 - compressed with dynamic Huffman codes
            11 - reserved (error)

      3.2.4. Non-compressed blocks (BTYPE=00)

         Any bits of input up to the next byte boundary are ignored.
         The rest of the block consists of the following information:

              0   1   2   3   4...
            +---+---+---+---+================================+
            |  LEN  | NLEN  |... LEN bytes of literal data...|
            +---+---+---+---+================================+

         LEN is the number of data bytes in the block.  NLEN is the
         one's complement of LEN.

      3.2.5. Compressed blocks (length and distance codes)

         As noted above, encoded data blocks in the "deflate" format
         consist of sequences of symbols drawn from three conceptually
         distinct alphabets: either literal bytes, from the alphabet of
         byte values (0..255), or <length, backward distance> pairs,
         where the length is drawn from (3..258) and the distance is
         drawn from (1..32,768).  In fact, the literal and length
         alphabets are merged into a single alphabet (0..285), where
         values 0..255 represent literal bytes, the value 256 indicates
         end-of-block, and values 257..285 represent length codes
         (possibly in conjunction with extra bits following the symbol
         code) as follows:

Length encoding :
                Extra               Extra               Extra
            Code Bits Length(s) Code Bits Lengths   Code Bits Length(s)
            ---- ---- ------     ---- ---- -------   ---- ---- -------
             257   0     3       267   1   15,16     277   4   67-82
             258   0     4       268   1   17,18     278   4   83-98
             259   0     5       269   2   19-22     279   4   99-114
             260   0     6       270   2   23-26     280   4  115-130
             261   0     7       271   2   27-30     281   5  131-162
             262   0     8       272   2   31-34     282   5  163-194
             263   0     9       273   3   35-42     283   5  195-226
             264   0    10       274   3   43-50     284   5  227-257
             265   1  11,12      275   3   51-58     285   0    258
             266   1  13,14      276   3   59-66

Distance encoding :
                  Extra           Extra               Extra
             Code Bits Dist  Code Bits   Dist     Code Bits Distance
             ---- ---- ----  ---- ----  ------    ---- ---- --------
               0   0    1     10   4     33-48    20    9   1025-1536
               1   0    2     11   4     49-64    21    9   1537-2048
               2   0    3     12   5     65-96    22   10   2049-3072
               3   0    4     13   5     97-128   23   10   3073-4096
               4   1   5,6    14   6    129-192   24   11   4097-6144
               5   1   7,8    15   6    193-256   25   11   6145-8192
               6   2   9-12   16   7    257-384   26   12  8193-12288
               7   2  13-16   17   7    385-512   27   12 12289-16384
               8   3  17-24   18   8    513-768   28   13 16385-24576
               9   3  25-32   19   8   769-1024   29   13 24577-32768

      3.2.6. Compression with fixed Huffman codes (BTYPE=01)

         The Huffman codes for the two alphabets are fixed, and are not
         represented explicitly in the data.  The Huffman code lengths
         for the literal/length alphabet are:

                   Lit Value    Bits        Codes
                   ---------    ----        -----
                     0 - 143     8          00110000 through
                                            10111111
                   144 - 255     9          110010000 through
                                            111111111
                   256 - 279     7          0000000 through
                                            0010111
                   280 - 287     8          11000000 through
                                            11000111

*/



const unsigned char gzip_hdr[] = { 0x1F, 0x8B,   // ID1, ID2
				   0x08, 0x00,   // Deflate, flags (none)
				   0x00, 0x00, 0x00, 0x00, // mtime: none
				   0x04, 0x03 }; // fastest comp, OS=Unix

char *buffer;
int bufsize = 32768;
int buflen;

/* CRC32 code adapted from RFC1952 */

/* Table of CRCs of all 8-bit messages. Filled with zeroes at the beginning,
 * indicating that it must be computed.
 */
static uint32_t crc32Lookup[4][256];
static const uint32_t *crc_table = crc32Lookup[0];

/* Make the table for a fast CRC. */
void make_crc_table(void)
{
	uint32_t c;
	int n, k;

	for (n = 0; n < 256; n++) {
		c = (uint32_t) n;
		for (k = 0; k < 8; k++) {
			if (c & 1) {
				c = 0xedb88320 ^ (c >> 1);
			} else {
				c = c >> 1;
			}
		}
		crc32Lookup[0][n] = c;
	}

	// for Slicing-by-4 and Slicing-by-8
	for (n = 0; n < 256; n++) {
		crc32Lookup[1][n] = (crc32Lookup[0][n] >> 8) ^ crc32Lookup[0][crc32Lookup[0][n] & 0xFF];
		crc32Lookup[2][n] = (crc32Lookup[1][n] >> 8) ^ crc32Lookup[0][crc32Lookup[1][n] & 0xFF];
		crc32Lookup[3][n] = (crc32Lookup[2][n] >> 8) ^ crc32Lookup[0][crc32Lookup[2][n] & 0xFF];
		// only Slicing-by-8
		//crc32Lookup[4][n] = (crc32Lookup[3][n] >> 8) ^ crc32Lookup[0][crc32Lookup[3][n] & 0xFF];
		//crc32Lookup[5][n] = (crc32Lookup[4][n] >> 8) ^ crc32Lookup[0][crc32Lookup[4][n] & 0xFF];
		//crc32Lookup[6][n] = (crc32Lookup[5][n] >> 8) ^ crc32Lookup[0][crc32Lookup[5][n] & 0xFF];
		//crc32Lookup[7][n] = (crc32Lookup[6][n] >> 8) ^ crc32Lookup[0][crc32Lookup[6][n] & 0xFF];
	}
}

// from RFC1952 : about 480 MB/s
uint32_t rfc1952_crc(uint32_t crc, const unsigned char *buf, int len)
{
	uint32_t c = ~crc;
	int n;

	for (n = 0; n < len; n++)
		c = crc_table[(c & 0xff) ^ buf[n]] ^ (c >> 8);

	return ~c;
}

// slice-by-4 : about 980 MB/s, little-endian only.
// see intel's zlib patches : https://github.com/jtkukunas/zlib/commits/master
// see also Linux's CRC32 based on PCLMULQDQ, claiming about 2100 MB/s
uint32_t crc32_4bytes(uint32_t previousCrc32, const void* data, size_t length)
{
	uint32_t* current = (uint32_t*) data;
	uint32_t crc = ~previousCrc32;
	// process four bytes at once
	while (length >= 4) {
		crc ^= *current++;
		crc = crc32Lookup[3][ crc & 0xFF] ^
		      crc32Lookup[2][(crc>>8 ) & 0xFF] ^
		      crc32Lookup[1][(crc>>16) & 0xFF] ^
		      crc32Lookup[0][ crc>>24 ];
		length -= 4;
	}
	const unsigned char* currentChar = (unsigned char*) current;
	// remaining 1 to 3 bytes
	while (length--)
		crc = (crc >> 8) ^ crc32Lookup[0][(crc & 0xFF) ^ *currentChar++];
	return ~crc;
}

uint32_t update_crc(uint32_t crc, const void *buf, int len)
{
	return rfc1952_crc(crc, buf, len);
	//return crc32_4bytes(crc, buf, len);
}

/* Return the CRC of the bytes buf[0..len-1]. */
uint32_t do_crc(void *buf, int len)
{
	return update_crc(0, buf, len);
}

/* data are queued by LSB first */
static uint32_t queue;
static uint32_t qbits; /* number of bits in queue, < 8 */

static char outbuf[65536 * 2];
static int  outsize = 65536;
static int  outlen;

/* enqueue code x of <xbits> bits (LSB aligned) and copy complete bytes into.
 * out buf. X must not contain non-zero bits above xbits.
 */
static void enqueue(uint32_t x, uint32_t xbits)
{
	queue += x << qbits;
	qbits += xbits;
	if (qbits < 8)
		return;

	if (qbits < 16) {
		/* usual case */
		outbuf[outlen] = queue;
		outlen += 1;
		queue >>= 8;
		qbits -= 8;
		return;
	}
	/* case where we queue large codes after small ones, eg: 7 then 9 */
	outbuf[outlen]     = queue;
	outbuf[outlen + 1] = queue >> 8;
	outlen += 2;
	queue >>= 16;
	qbits -= 16;
}

/* align to next byte */
static inline void flush_bits()
{
	if (qbits) {
		outbuf[outlen] = queue;
		outlen += 1;
		queue = 0;
		qbits = 0;
	}
}

/* only valid if buffer is already aligned */
static inline void copy_8b(uint32_t x)
{
	outbuf[outlen] = x;
	outlen += 1;
}

/* only valid if buffer is already aligned */
static inline void copy_16b(uint32_t x)
{
	outbuf[outlen] = x;
	outbuf[outlen + 1] = x >> 8;
	outlen += 2;
}

/* only valid if buffer is already aligned */
static inline void copy_32b(uint32_t x)
{
	outbuf[outlen] = x;
	outbuf[outlen + 1] = x >> 8;
	outbuf[outlen + 2] = x >> 16;
	outbuf[outlen + 3] = x >> 24;
	outlen += 4;
}

/* copies at most <len> litterals from <buf>, returns the amount of data
 * copied. <more> indicates that there are data past buf + <len>.
 */
static unsigned int copy_lit(const void *buf, int len, int more)
{
	if (len + 5 > outsize - outlen) {
		len = outsize - outlen - 5;
		more = 1;
	}

	if (len <= 0)
		return 0;

	if (len > 65535) {
		len = 65535;
		more = 1;
	}

	//fprintf(stderr, "len=%d more=%d\n", len, more);
	enqueue(!more, 3); // BFINAL = !more ; BTYPE = 00
	flush_bits();
	copy_16b(len);  // len
	copy_16b(~len); // nlen
	memcpy(outbuf + outlen, buf, len);
	outlen += len;
	return len;
}

/* dumps buffer to stdout */
static void dump_outbuf()
{
	write(1, outbuf, outlen);
	outlen = 0;
}

int main(int argc, char **argv)
{
	int loops = 1;
	int totin = 0;

	if (argc > 1)
		bufsize = atoi(argv[1]);

	if (argc > 2)
		loops = atoi(argv[2]);

	make_crc_table();

	buffer = calloc(1, bufsize);
	if (!buffer) {
		perror("calloc");
		exit(1);
	}

	buflen = read(0, buffer, bufsize);
	if (buflen < 0) {
		perror("read");
		exit(2);
	}

	totin = buflen * loops;
	while (loops--) {
		uint32_t crc;
		uint32_t len, rem;

		//memset(refs, 0, sizeof(refs));
		//encode(buffer, buflen);
		//totin += buflen;

		crc = 0;
		write(1, gzip_hdr, sizeof(gzip_hdr));
		rem = buflen;

		while (rem) {
			len = copy_lit(buffer + buflen - rem, rem, 0);
			crc = update_crc(crc, buffer + buflen - rem, len);
			dump_outbuf();
			rem -= len;
		}

		flush_bits();
		copy_32b(crc);
		copy_32b(buflen);
		dump_outbuf();
	}
	//fprintf(stderr, "totin=%d lit=%d ref=%d\n", totin, lit, ref);
	return 0;
}
