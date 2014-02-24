#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <eb32tree.h>

struct ref {
	struct eb32_node node;
	unsigned int pos; /* pos of next byte after match */
};

char *buffer;
int bufsize = 16384;
int buflen;

struct ref *refs;
struct eb_root root = EB_ROOT_UNIQUE;

static inline int memmatch(const char *a, const char *b, int max)
{
	int len;

	for (len = 0; len < max && a[len] == b[len]; len++);
	return len;
}

/* does 10.9 MB/s on non-compressible data, 35 MB/s on HTML.
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

#define MINBYTES 3
	//printf("len = %d\n", ilen);
	while (rem >= MINBYTES) {
		word &= 0xFFFFFFFF >> ((5 - MINBYTES) * 8);
		word = (word << 8) + in[pos++];
		rem--;
		bytes++;
		if (bytes >= MINBYTES) {
			/*refs are indexed on the first byte *after* the key */
			refs[pos].node.key = word;
			refs[pos].pos = pos;
			//eb32_delete(&refs[pos].node);
			ref = (void *)eb32_insert(&root, &refs[pos].node);
			if (ref != &refs[pos]) {
				/* found a matching entry */
				mlen = memmatch(in + pos, in + ref->pos, rem);
				printf("found [%d]:0x%06x == [%d]:0x%06x (+%d bytes)\n",
				       pos - MINBYTES, word,
				       ref->pos, ref->node.key,
				       mlen);
				ref->pos = pos;
				pos += mlen;
				rem -= mlen;
				bytes = 0;
			}
			else {
				printf("litteral [%d]:%02x\n", pos - MINBYTES, word >> (8 * (MINBYTES - 1)));
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

	/* one ref per input byte in the worst case */
	refs = calloc(sizeof(*refs), bufsize);

	buflen = read(0, buffer, bufsize);
	if (buflen < 0) {
		perror("read");
		exit(2);
	}

	while (loops--) {
		encode(buffer, buflen);
		memset(refs, 0, bufsize * sizeof(*refs));
		root = EB_ROOT_UNIQUE;
	}
	return 0;
}
