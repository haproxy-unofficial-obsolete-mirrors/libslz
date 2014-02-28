#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

char *buffer;
int bufsize = 16384;
int buflen;

#define HASH_BITS 16

/* format:
 * bit0..23 = last position in buffer of similar content
 */
uint32_t refs[1 << HASH_BITS];

static inline uint32_t hash(uint32_t a)
{
	return ((a << 19) + (a << 6) - a) >> (32 - HASH_BITS);
}

static inline int memmatch(const char *a, const char *b, int max)
{
	int len;

	for (len = 0; len < max && a[len] == b[len]; len++);
	return len;
}

unsigned int /*totin = 0, */lit = 0, ref = 0;
/* does 206 MB/s on non-compressible data, 286 MB/s on HTML. */
void encode(char *in, int ilen)
{
	int rem = ilen;
	int pos = 0;
	uint32_t word = 0;
	int mlen;
	uint32_t h, last;

	//printf("len = %d\n", ilen);
	while (rem >= 2) {
		//word = (word << 8) + (unsigned char)in[pos];
		//word = ((unsigned char)in[pos] << 24) + (word >> 8);
		word = *(uint32_t *)&in[pos];
		//word &= 0x00FFFFFF;
		//printf("%d %08x\n", pos, word);
		h = hash(word);
		__builtin_prefetch(refs + h);
		pos++;
		rem--;
		last = refs[h];
		refs[h] = pos - 1;

		if (last < pos - 1 && in[last] == (char)word) {
			/* found a matching entry */
			mlen = memmatch(in + pos, in + last + 1, rem);
			if (mlen < 3)
				goto tooshort;

			//printf("found [%d]:0x%06x == [%d=@-%d] %d bytes\n",
			//       pos - 1, word,
			//       last, pos - 1 - last,
			//       mlen + 1);
			pos += mlen;
			rem -= mlen;
			//word = 0;
			ref += mlen;
		}
		else {
		tooshort:
			//printf("litteral [%d]:%08x\n", pos - 1, word);
			lit++;
		}
	}
}

int main(int argc, char **argv)
{
	int loops = 1;
	int totin = 0;

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

	totin = buflen * loops;
	while (loops--) {
		//memset(refs, 0, sizeof(refs));
		encode(buffer, buflen);
		//totin += buflen;
	}
	printf("totin=%d lit=%d ref=%d\n", totin, lit, ref);
	return 0;
}
