#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/user.h>
#include "slz.h"

/* block size for experimentations */
#define BLK 32768

int main(int argc, char **argv)
{
	int loops = 1;
	int totin = 0;
	int ofs;
	int totout = 0;
	int len;
	struct slz_stream strm;
	unsigned char *outbuf;
	unsigned char *buffer;
	int bufsize = 32768;
	int buflen;
	struct stat instat;

	if (argc > 1)
		bufsize = atoi(argv[1]);

	if (argc > 2)
		loops = atoi(argv[2]);

	slz_make_crc_table();
	slz_prepare_dist_table();

	buflen = bufsize;
	if (bufsize <= 0) {
		if (fstat(0, &instat) == -1) {
			perror("fstat(stdin)");
			exit(1);
		}
		/* size of zero means stat the file */
		bufsize = instat.st_size;
		if (bufsize <= 0) {
			printf("Cannot determine input size\n");
			exit(1);
		}

		buflen = bufsize;
		bufsize = (bufsize + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
	}

	outbuf = calloc(1, BLK + 4096);
	if (!outbuf) {
		perror("calloc");
		exit(1);
	}

	buffer = mmap(NULL, bufsize, PROT_READ, MAP_PRIVATE, 0, 0);
	if (buffer == MAP_FAILED) {
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
	}

	while (loops--) {
		len = slz_rfc1951_init(&strm, outbuf);

		ofs = 0;
		do {
			len += slz_rfc1951_encode(&strm, outbuf + len, buffer + ofs, (buflen - ofs) > BLK ? BLK : buflen - ofs, (buflen - ofs) > BLK);
			if (buflen - ofs > BLK) {
				totout += len;
				ofs += BLK;
				write(1, outbuf, len);
				len = 0;
			}
			else
				ofs = buflen;
		} while (ofs < buflen);
		len += slz_rfc1951_finish(&strm, outbuf + len);
		totin += ofs;
		totout += len;
		write(1, outbuf, len);
	}
	fprintf(stderr, "totin=%d totout=%d ratio=%.2f%% crc32=%08x\n", totin, totout, totout * 100.0 / totin, strm.crc32);
	return 0;
}
