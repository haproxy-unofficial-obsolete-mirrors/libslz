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

/* Length table for lengths 3 to 258, generated by mklen.sh
 * The entries contain :
 *   code - 257 = 0..28 in bits 0..4
 *   bits       = 0..5  in bits 5..7
 *   value      = 0..31 in bits 8..12
 */
static const uint16_t len_code[259] = {
	0x0000, 0x0000, 0x0000, 0x0000, 0x0001, 0x0002, 0x0003, 0x0004, //   0
	0x0005, 0x0006, 0x0007, 0x0028, 0x0128, 0x0029, 0x0129, 0x002a, //   8
	0x012a, 0x002b, 0x012b, 0x004c, 0x014c, 0x024c, 0x034c, 0x004d, //  16
	0x014d, 0x024d, 0x034d, 0x004e, 0x014e, 0x024e, 0x034e, 0x004f, //  24
	0x014f, 0x024f, 0x034f, 0x0070, 0x0170, 0x0270, 0x0370, 0x0470, //  32
	0x0570, 0x0670, 0x0770, 0x0071, 0x0171, 0x0271, 0x0371, 0x0471, //  40
	0x0571, 0x0671, 0x0771, 0x0072, 0x0172, 0x0272, 0x0372, 0x0472, //  48
	0x0572, 0x0672, 0x0772, 0x0073, 0x0173, 0x0273, 0x0373, 0x0473, //  56
	0x0573, 0x0673, 0x0773, 0x0094, 0x0194, 0x0294, 0x0394, 0x0494, //  64
	0x0594, 0x0694, 0x0794, 0x0894, 0x0994, 0x0a94, 0x0b94, 0x0c94, //  72
	0x0d94, 0x0e94, 0x0f94, 0x0095, 0x0195, 0x0295, 0x0395, 0x0495, //  80
	0x0595, 0x0695, 0x0795, 0x0895, 0x0995, 0x0a95, 0x0b95, 0x0c95, //  88
	0x0d95, 0x0e95, 0x0f95, 0x0096, 0x0196, 0x0296, 0x0396, 0x0496, //  96
	0x0596, 0x0696, 0x0796, 0x0896, 0x0996, 0x0a96, 0x0b96, 0x0c96, // 104
	0x0d96, 0x0e96, 0x0f96, 0x0097, 0x0197, 0x0297, 0x0397, 0x0497, // 112
	0x0597, 0x0697, 0x0797, 0x0897, 0x0997, 0x0a97, 0x0b97, 0x0c97, // 120
	0x0d97, 0x0e97, 0x0f97, 0x00b8, 0x01b8, 0x02b8, 0x03b8, 0x04b8, // 128
	0x05b8, 0x06b8, 0x07b8, 0x08b8, 0x09b8, 0x0ab8, 0x0bb8, 0x0cb8, // 136
	0x0db8, 0x0eb8, 0x0fb8, 0x10b8, 0x11b8, 0x12b8, 0x13b8, 0x14b8, // 144
	0x15b8, 0x16b8, 0x17b8, 0x18b8, 0x19b8, 0x1ab8, 0x1bb8, 0x1cb8, // 152
	0x1db8, 0x1eb8, 0x1fb8, 0x00b9, 0x01b9, 0x02b9, 0x03b9, 0x04b9, // 160
	0x05b9, 0x06b9, 0x07b9, 0x08b9, 0x09b9, 0x0ab9, 0x0bb9, 0x0cb9, // 168
	0x0db9, 0x0eb9, 0x0fb9, 0x10b9, 0x11b9, 0x12b9, 0x13b9, 0x14b9, // 176
	0x15b9, 0x16b9, 0x17b9, 0x18b9, 0x19b9, 0x1ab9, 0x1bb9, 0x1cb9, // 184
	0x1db9, 0x1eb9, 0x1fb9, 0x00ba, 0x01ba, 0x02ba, 0x03ba, 0x04ba, // 192
	0x05ba, 0x06ba, 0x07ba, 0x08ba, 0x09ba, 0x0aba, 0x0bba, 0x0cba, // 200
	0x0dba, 0x0eba, 0x0fba, 0x10ba, 0x11ba, 0x12ba, 0x13ba, 0x14ba, // 208
	0x15ba, 0x16ba, 0x17ba, 0x18ba, 0x19ba, 0x1aba, 0x1bba, 0x1cba, // 216
	0x1dba, 0x1eba, 0x1fba, 0x00bb, 0x01bb, 0x02bb, 0x03bb, 0x04bb, // 224
	0x05bb, 0x06bb, 0x07bb, 0x08bb, 0x09bb, 0x0abb, 0x0bbb, 0x0cbb, // 232
	0x0dbb, 0x0ebb, 0x0fbb, 0x10bb, 0x11bb, 0x12bb, 0x13bb, 0x14bb, // 240
	0x15bb, 0x16bb, 0x17bb, 0x18bb, 0x19bb, 0x1abb, 0x1bbb, 0x1cbb, // 248
	0x1dbb, 0x1ebb, 0x001c					        // 256
};


/* Fixed Huffman table as per RFC1951.
 *
 *       Lit Value    Bits        Codes
 *       ---------    ----        -----
 *         0 - 143     8          00110000 through  10111111
 *       144 - 255     9         110010000 through 111111111
 *       256 - 279     7           0000000 through   0010111
 *       280 - 287     8          11000000 through  11000111
 *
 * The codes are encoded in reverse, the high bit of the code appears encoded
 * as bit 0. The table is built by mkhuff.sh. The 16 bits are encoded this way :
 *  - bits 0..3  : bits
 *  - bits 4..12 : code
 */
static const uint16_t fixed_huff[288] = {
	0x00c8, 0x08c8, 0x04c8, 0x0cc8, 0x02c8, 0x0ac8, 0x06c8, 0x0ec8, //   0
	0x01c8, 0x09c8, 0x05c8, 0x0dc8, 0x03c8, 0x0bc8, 0x07c8, 0x0fc8, //   8
	0x0028, 0x0828, 0x0428, 0x0c28, 0x0228, 0x0a28, 0x0628, 0x0e28, //  16
	0x0128, 0x0928, 0x0528, 0x0d28, 0x0328, 0x0b28, 0x0728, 0x0f28, //  24
	0x00a8, 0x08a8, 0x04a8, 0x0ca8, 0x02a8, 0x0aa8, 0x06a8, 0x0ea8, //  32
	0x01a8, 0x09a8, 0x05a8, 0x0da8, 0x03a8, 0x0ba8, 0x07a8, 0x0fa8, //  40
	0x0068, 0x0868, 0x0468, 0x0c68, 0x0268, 0x0a68, 0x0668, 0x0e68, //  48
	0x0168, 0x0968, 0x0568, 0x0d68, 0x0368, 0x0b68, 0x0768, 0x0f68, //  56
	0x00e8, 0x08e8, 0x04e8, 0x0ce8, 0x02e8, 0x0ae8, 0x06e8, 0x0ee8, //  64
	0x01e8, 0x09e8, 0x05e8, 0x0de8, 0x03e8, 0x0be8, 0x07e8, 0x0fe8, //  72
	0x0018, 0x0818, 0x0418, 0x0c18, 0x0218, 0x0a18, 0x0618, 0x0e18, //  80
	0x0118, 0x0918, 0x0518, 0x0d18, 0x0318, 0x0b18, 0x0718, 0x0f18, //  88
	0x0098, 0x0898, 0x0498, 0x0c98, 0x0298, 0x0a98, 0x0698, 0x0e98, //  96
	0x0198, 0x0998, 0x0598, 0x0d98, 0x0398, 0x0b98, 0x0798, 0x0f98, // 104
	0x0058, 0x0858, 0x0458, 0x0c58, 0x0258, 0x0a58, 0x0658, 0x0e58, // 112
	0x0158, 0x0958, 0x0558, 0x0d58, 0x0358, 0x0b58, 0x0758, 0x0f58, // 120
	0x00d8, 0x08d8, 0x04d8, 0x0cd8, 0x02d8, 0x0ad8, 0x06d8, 0x0ed8, // 128
	0x01d8, 0x09d8, 0x05d8, 0x0dd8, 0x03d8, 0x0bd8, 0x07d8, 0x0fd8, // 136
	0x0139, 0x1139, 0x0939, 0x1939, 0x0539, 0x1539, 0x0d39, 0x1d39, // 144
	0x0339, 0x1339, 0x0b39, 0x1b39, 0x0739, 0x1739, 0x0f39, 0x1f39, // 152
	0x00b9, 0x10b9, 0x08b9, 0x18b9, 0x04b9, 0x14b9, 0x0cb9, 0x1cb9, // 160
	0x02b9, 0x12b9, 0x0ab9, 0x1ab9, 0x06b9, 0x16b9, 0x0eb9, 0x1eb9, // 168
	0x01b9, 0x11b9, 0x09b9, 0x19b9, 0x05b9, 0x15b9, 0x0db9, 0x1db9, // 176
	0x03b9, 0x13b9, 0x0bb9, 0x1bb9, 0x07b9, 0x17b9, 0x0fb9, 0x1fb9, // 184
	0x0079, 0x1079, 0x0879, 0x1879, 0x0479, 0x1479, 0x0c79, 0x1c79, // 192
	0x0279, 0x1279, 0x0a79, 0x1a79, 0x0679, 0x1679, 0x0e79, 0x1e79, // 200
	0x0179, 0x1179, 0x0979, 0x1979, 0x0579, 0x1579, 0x0d79, 0x1d79, // 208
	0x0379, 0x1379, 0x0b79, 0x1b79, 0x0779, 0x1779, 0x0f79, 0x1f79, // 216
	0x00f9, 0x10f9, 0x08f9, 0x18f9, 0x04f9, 0x14f9, 0x0cf9, 0x1cf9, // 224
	0x02f9, 0x12f9, 0x0af9, 0x1af9, 0x06f9, 0x16f9, 0x0ef9, 0x1ef9, // 232
	0x01f9, 0x11f9, 0x09f9, 0x19f9, 0x05f9, 0x15f9, 0x0df9, 0x1df9, // 240
	0x03f9, 0x13f9, 0x0bf9, 0x1bf9, 0x07f9, 0x17f9, 0x0ff9, 0x1ff9, // 248
	0x0007, 0x0407, 0x0207, 0x0607, 0x0107, 0x0507, 0x0307, 0x0707, // 256
	0x0087, 0x0487, 0x0287, 0x0687, 0x0187, 0x0587, 0x0387, 0x0787, // 264
	0x0047, 0x0447, 0x0247, 0x0647, 0x0147, 0x0547, 0x0347, 0x0747, // 272
	0x0038, 0x0838, 0x0438, 0x0c38, 0x0238, 0x0a38, 0x0638, 0x0e38  // 280
};

/* CRC32 code adapted from RFC1952 */

/* Table of CRCs of all 8-bit messages. Filled with zeroes at the beginning,
 * indicating that it must be computed.
 */
static uint32_t crc32Lookup[4][256];
static const uint32_t *crc_table = crc32Lookup[0];

/* data are queued by LSB first */
static uint32_t queue;
static uint32_t qbits; /* number of bits in queue, < 8 */

static char outbuf[65536 * 2];
static int  outsize = 65536;
static int  outlen;


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

/* Returns code for lengths 1 to 32768. The bit size for the next value can be
 * found this way :
 *
 *	bits = code >> 1;
 *	if (bits)
 *		bits--;
 *
 */
static uint32_t dist_to_code(uint32_t l)
{
	uint32_t code;

	code = 0;
	switch (l) {
	case 24577 ... 32768: code++;
	case 16385 ... 24576: code++;
	case 12289 ... 16384: code++;
	case 8193 ... 12288: code++;
	case 6145 ... 8192: code++;
	case 4097 ... 6144: code++;
	case 3073 ... 4096: code++;
	case 2049 ... 3072: code++;
	case 1537 ... 2048: code++;
	case 1025 ... 1536: code++;
	case 769 ... 1024: code++;
	case 513 ... 768: code++;
	case 385 ... 512: code++;
	case 257 ... 384: code++;
	case 193 ... 256: code++;
	case 129 ... 192: code++;
	case 97 ... 128: code++;
	case 65 ... 96: code++;
	case 49 ... 64: code++;
	case 33 ... 48: code++;
	case 25 ... 32: code++;
	case 17 ... 24: code++;
	case 13 ... 16: code++;
	case 9 ... 12: code++;
	case 7 ... 8: code++;
	case 5 ... 6: code++;
	case 4: code++;
	case 3: code++;
	case 2: code++;
	}

	return code;

	//bits = code >> 1;
	//if (bits)
	//	bits--;
	//
	//printf("l=%d code=%d bits=%d mask=%d value=%d\n",
	//       l, code, bits, (1 << bits) - 1, (l - 1) & ((1 << bits) - 1));
}

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


static void send_eob()
{
	uint32_t code, bits;

	/* end of block = 256 */
	code = fixed_huff[256];
	bits = code & 15;
	code >>= 4;
	enqueue(code, bits);
}

/* copies at most <len> litterals from <buf>, returns the amount of data
 * copied. <more> indicates that there are data past buf + <len>.
 */
static unsigned int copy_lit_huff(const unsigned char *buf, int len, int more)
{
	uint32_t code, bits;
	uint32_t pos;

	if (len + 5 > outsize - outlen) {
		len = outsize - outlen - 5;
		more = 1;
	}

	if (len <= 0)
		return 0;

	//fprintf(stderr, "len=%d more=%d\n", len, more);
	enqueue(2 + !more, 3); // BFINAL = !more ; BTYPE = 01

	pos = 0;
	while (pos < len) {
		code = fixed_huff[buf[pos++]];
		bits = code & 15;
		code >>= 4;
		enqueue(code, bits);
	}
	send_eob();
	return len;
}

/* dumps buffer to stdout */
static void dump_outbuf()
{
	write(1, outbuf, outlen);
	outlen = 0;
}

#define HASH_BITS 15

/* stats */
unsigned int lit = 0, ref = 0;

/* format:
 * bit0..31  = word
 * bit32..63 = last position in buffer of similar content
 */
uint64_t refs[1 << HASH_BITS];

/* This hash provides good average results on HTML contents, and is among the
 * few which provide almost optimal results on various different pages.
 */
static inline uint32_t hash(uint32_t a)
{
	return ((a << 19) + (a << 6) - a) >> (32 - HASH_BITS);
}

static inline long memmatch(const char *a, const char *b, long max)
{
	long len;

	len = 0;
	do {
		if (a[len] != b[len])
			break;
		len++;
	} while (len < max);
	return len;
}

/* does 290 MB/s on non-compressible data, 330 MB/s on HTML. */
void encode(const char *in, long ilen)
{
	long rem = ilen;
	long pos = 0;
	uint32_t word = 0;
	long mlen;
	uint32_t h, last;
	uint64_t ent;

	uint32_t crc = 0;
	uint32_t len;
	uint32_t plit = 0;

	//printf("len = %d\n", ilen);
	while (1) {
		//word = (word << 8) + (unsigned char)in[pos];
		//word = ((unsigned char)in[pos] << 24) + (word >> 8);
		word = *(uint32_t *)&in[pos];
		//word &= 0x00FFFFFF;
		//printf("%d %08x\n", pos, word);
		h = hash(word);
		__builtin_prefetch(refs + h);
		pos++;
		rem--;
		ent = refs[h];
		last = ent >> 32;
		refs[h] = (((uint64_t)(pos - 1)) << 32) + word;

		if (last < pos - 1 &&
		    last >= pos - 32768 &&
		    (uint32_t)ent == word && rem >= 2) {
			/* found a matching entry */

			/* first, copy pending literals */
			fprintf(stderr, "dumping %d literals from %ld\n", plit, pos - 1 - plit);
			while (plit) {
				//flush_bits();
				len = copy_lit_huff(in + pos - 1 - plit, plit, 1);
				crc = update_crc(crc, in + pos - 1 - plit, len);
				plit -= len;
				//if (outlen > 32768)
					dump_outbuf();
			}

			mlen = memmatch(in + pos, in + last + 1, rem);
			//mlen = 4 + memmatch(in + pos + 4, in + last + 5, rem);
			fprintf(stderr, "found [%ld]:0x%06x == [%d=@-%ld] %ld bytes, rem=%ld\n",
			       pos - 1, word,
			       last, pos - 1 - last,
			       mlen + 1, rem);

			ref += mlen; // for statistics
			rem -= mlen;
			//pos += mlen;

			/* for now we copy them as literals */
			mlen++;
			pos--;
			do {
				//flush_bits();
				len = copy_lit_huff(in + pos, mlen, 1);
				crc = update_crc(crc, in + pos, len);
				pos += len;
				mlen -= len;
				//if (outlen > 32768)
					dump_outbuf();
			} while (mlen);
		}
		else {
			if (rem < 0)
				break;
			fprintf(stderr, "litteral [%ld]:%08x\n", pos - 1, word);
			plit++;
			lit++; // for statistics
		}
	}

	/* now copy remaining literals or mark the end */
	if (!plit) {
		send_eob();
		//flush_bits();
		//len = copy_lit(NULL, 0, 0);
	}
	else while (plit) {
		//flush_bits();
		len = copy_lit_huff(in + pos - 1 - plit, plit, 0);
		crc = update_crc(crc, in + pos - 1 - plit, len);
		plit -= len;
		if (outlen > 32768)
			dump_outbuf();
	}

	flush_bits();
	copy_32b(crc);
	copy_32b(buflen);
	dump_outbuf();
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
		write(1, gzip_hdr, sizeof(gzip_hdr));
		memset(refs, 0, sizeof(refs));
		encode(buffer, buflen);
	}
	//fprintf(stderr, "totin=%d lit=%d ref=%d\n", totin, lit, ref);
	return 0;
}
