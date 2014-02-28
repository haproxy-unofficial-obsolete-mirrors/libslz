#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

char *buffer;
int bufsize = 16384;
int buflen;

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

static inline int memmatch(const char *a, const char *b, int max)
{
	int len;

	for (len = 0; len < max && a[len] == b[len]; len++);
	return len;
}

/* does 290 MB/s on non-compressible data, 330 MB/s on HTML. */
void encode(char *in, int ilen)
{
	int rem = ilen;
	int pos = 0;
	uint32_t word = 0;
	int mlen;
	uint32_t h, last;
	uint64_t ent;

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
		ent = refs[h];
		last = ent >> 32;
		refs[h] = (((uint64_t)(pos - 1)) << 32) + word;

		if (last < pos - 1 &&
		    (uint32_t)ent == word) {
			/* found a matching entry */
			mlen = memmatch(in + pos, in + last + 1, rem);
			//mlen = 4 + memmatch(in + pos + 4, in + last + 5, rem);
			//printf("found [%d]:0x%06x == [%d=@-%d] %d bytes\n",
			//       pos - 1, word,
			//       last, pos - 1 - last,
			//       mlen + 1);
			pos += mlen;
			rem -= mlen;
			ref += mlen;
		}
		else {
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
