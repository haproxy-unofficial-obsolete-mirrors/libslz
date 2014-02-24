#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct ref {
	unsigned short pos; /* pos of next byte after match, 0 = unassigned */
};

char *buffer;
int bufsize = 16384;
int buflen;

struct ref refs[65536];

static inline int memmatch(const char *a, const char *b, int max)
{
	int len;

	for (len = 0; len < max && a[len] == b[len]; len++);
	return len;
}

/* does 306 MB/s on non-compressible data, 230 MB/s on HTML.
 * gzip does 33 MB/s on non-compressible data.
 */
void encode(char *in, int ilen)
{
	int rem = ilen;
	int pos = 0;
	uint32_t word = 0;
	int bytes = 0;
	struct ref *ref;
	int mlen;

#define MINBYTES 2
	//printf("len = %d\n", ilen);
	while (rem >= MINBYTES) {
		word &= 0xFFFFFFFF >> ((5 - MINBYTES) * 8);
		word = (word << 8) + (unsigned char)in[pos++];
		ref = refs + word;
		__builtin_prefetch(ref);
		rem--;
		bytes++;
		if (bytes >= MINBYTES) {
			/* refs are indexed on the first byte *after* the key */
			if (ref->pos) {
				/* found a matching entry */
				mlen = memmatch(in + pos, in + ref->pos, rem);
				if (mlen < 1)
					goto tooshort;
				//printf("found [%d]:0x%06x == [%d] (+%d bytes)\n",
				//       pos - MINBYTES, word,
				//       ref->pos, mlen);
				ref->pos = pos;
				pos += mlen;
				rem -= mlen;
				bytes = 0;
			}
			else {
			tooshort:
				ref->pos = pos;
				//printf("litteral [%d]:%02x\n", pos - MINBYTES, word >> (8 * (MINBYTES - 1)));
				bytes--;
			}
		}
	}
}

int main(int argc, char **argv)
{
	int loops = 1;

	if (argc > 1)
		bufsize = atoi(argv[1]);

	if (argc > 2)
		loops = atoi(argv[2]);

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

	while (loops--) {
		memset(refs, 0, sizeof(refs));
		encode(buffer, buflen);
	}
	return 0;
}
