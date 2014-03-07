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

static inline int memmatch0(const char *a, const char *b, int max)
{
	int len;

	//for (len = 0; len < max && a[len] == b[len]; len++);
	//return len;

	//len = 0;
	//do {
	//	if (a[len] != b[len])
	//		break;
	//	len++;
	//} while (len < max);
	//return len;

	len = 0;
	do {
		if (*(uint32_t *)(a + len) != *(uint32_t *)(b + len))
			break;
		len += 4;
	} while (len <= max);

	while (len < max) {
		if (a[len] != b[len])
			break;
		len++;
	}

	return len;

	//const char *end = a + max;
	//
	//while (a <= end - 4) {
	//	if (*(uint32_t *)a != *(uint32_t *)b)
	//		break;
	//	b += 4;
	//	a += 4;
	//}
	//
	//while (a <= end - 1) {
	//	if (*a != *b)
	//		break;
	//	b++;
	//	a++;
	//}
	//return a - (end - max);

	//len = 0;
	//while (len <= max - 4 && *(uint32_t *)(a+len) == *(uint32_t *)(b+len))
	//	len += 4;
	////while (*(uint32_t *)(a+len) == *(uint32_t *)(b+len) && (len += 4) <= max - 4);
	//while (len < max && a[len] == b[len])
	//	len++;
	//return len;
}

int/*long*/ memmatch(const char *a, const char *b, int/*long*/ max)
{
	//long len;
	int len;

	//for (len = 0; len < max && a[len] == b[len]; len++);
	//return len;

	//len = 0;
	//do {
	//	if (a[len] != b[len])
	//		break;
	//	len++;
	//} while (len < max);
	//return len;

	// 2.832s
	//len = 0;
	//do {
	//	if (a[len] != b[len])
	//		break;
	//	len++;
	//} while (len <= max);
	//
	//return len;

	// 2.761s
	//const char *beg = a;
	//const char *end = a + max;
	//
	//do {
	//	if (*a != *b)
	//		break;
	//	b++;
	//	a++;
	//} while (a < end);
	//return a - beg;

	// 2.702s
	len = 0;
	do {
		if (a[len] != b[len])
			break;
		len++;
		max--;
	} while (max);
	return len;

	// 2.725s
	//const char *beg = a;
	//do {
	//	if (*a != *b)
	//		break;
	//	a++;
	//	b++;
	//	max--;
	//} while (max);
	//return a - beg;

	// 3.559s
	//asm(
	//    "repz cmpsb"
	//    : "=c" (len) : "c" (max), "D" (a), "S" (b) :);
	//return max - len - 1;

	// 2.738s
	//asm(
	//    "0:\n"
	//    "mov (%%rsi), %%al\n"
	//    "inc %%rsi\n"
	//    "cmp (%%rdi), %%al\n"
	//    "jne 1f\n"
	//    "inc %%rdi\n"
	//    "dec %%rcx\n"
	//    "jnz 0b\n"
	//    "1:\n"
	//    : "=c" (len) : "c" (max), "D" (a), "S" (b) : "%al");
	//return max - len;

	// 2.872s
	//asm(
	//    "lea (%%rdx, %%rdi), %%rcx\n"
	//    "0:\n"
	//    "mov (%%rsi), %%al\n"
	//    "inc %%rsi\n"
	//    "cmp (%%rdi), %%al\n"
	//    "jne 1f\n"
	//    "inc %%rdi\n"
	//    "cmp %%rcx, %%rdi\n"
	//    "jne 0b\n"
	//    "1:\n"
	//    "lea (%%rdi, %%rdx), %%rax\n"
	//    "sub %%rcx, %%rax\n"
	//    : "=a" (len) : "d" (max), "D" (a), "S" (b) : "%rcx");
	//return len;

	//const char *end = a + max;
	//
	//while (a <= end - 4) {
	//	if (*(uint32_t *)a != *(uint32_t *)b)
	//		break;
	//	b += 4;
	//	a += 4;
	//}
	//
	//while (a <= end - 1) {
	//	if (*a != *b)
	//		break;
	//	b++;
	//	a++;
	//}
	//return a - (end - max);

	//len = 0;
	//while (len <= max - 4 && *(uint32_t *)(a+len) == *(uint32_t *)(b+len))
	//	len += 4;
	////while (*(uint32_t *)(a+len) == *(uint32_t *)(b+len) && (len += 4) <= max - 4);
	//while (len < max && a[len] == b[len])
	//	len++;
	//return len;
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
