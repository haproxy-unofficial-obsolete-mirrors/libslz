#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/user.h>
#include <fcntl.h>
#include "slz.h"

/* some platforms do not provide PAGE_SIZE */
#ifndef PAGE_SIZE
#define PAGE_SIZE sysconf(_SC_PAGESIZE)
#endif

/* block size for experimentations */
#define BLK 32768

/* display the message and exit with the code */
__attribute__((noreturn)) void die(int code, const char *format, ...)
{
        va_list args;

        va_start(args, format);
        vfprintf(stderr, format, args);
        va_end(args);
        exit(code);
}

__attribute__((noreturn)) void usage(const char *name, int code)
{
	die(code,
	    "Usage: %s [option]* [file]\n"
	    "\n"
	    "The following arguments are supported :\n"
	    "  -0         disable compression, only uses format\n"
	    "  -1         enable compression [default]\n"
	    "  -b <size>  only use <size> bytes from the input file\n"
	    "  -c         send output to stdout [default]\n"
	    "  -f         force sending output to a terminal\n"
	    "  -h         display this help\n"
	    "  -l <loops> loop <loops> times over the same file\n"
	    "  -t         test mode: do not emit anything\n"
	    "  -v         increase verbosity\n"
	    "\n"
	    "  -D         use raw Deflate output format (RFC1951)\n"
	    "  -G         use Gzip output format (RFC1952) [default]\n"
	    "  -Z         use Zlib output format (RFC1950)\n"
	    "\n"
	    "If no file is specified, stdin will be used instead.\n"
	    "\n"
	    ,name);
}


int main(int argc, char **argv)
{
	const char *name = argv[0];
	struct stat instat;
	struct slz_stream strm;
	unsigned char *outbuf;
	unsigned char *buffer;
	int buflen;
	int totin = 0;
	int totout = 0;
	int ofs;
	int len;
	int loops = 1;
	int bufsize = 0;
	int console = 1;
	int level   = 1;
	int verbose = 0;
	int test    = 0;
	int format  = SLZ_FMT_GZIP;
	int force   = 0;
	int fd = 0;

	argv++;
	argc--;

	while (argc > 0) {
		if (**argv != '-')
			break;

		if (strcmp(argv[0], "-0") == 0)
			level = 0;

		else if (strcmp(argv[0], "-1") == 0)
			level = 1;

		else if (strcmp(argv[0], "-b") == 0) {
			if (argc < 2)
				usage(name, 1);
			bufsize = atoi(argv[1]);
			argv++;
			argc--;
		}

		else if (strcmp(argv[0], "-c") == 0)
			console = 1;

		else if (strcmp(argv[0], "-f") == 0)
			force = 1;

		else if (strcmp(argv[0], "-h") == 0)
			usage(name, 0);

		else if (strcmp(argv[0], "-l") == 0) {
			if (argc < 2)
				usage(name, 1);
			loops = atoi(argv[1]);
			argv++;
			argc--;
		}

		else if (strcmp(argv[0], "-t") == 0)
			test = 1;

		else if (strcmp(argv[0], "-v") == 0)
			verbose++;

		else if (strcmp(argv[0], "-D") == 0)
			format = SLZ_FMT_DEFLATE;

		else if (strcmp(argv[0], "-G") == 0)
			format = SLZ_FMT_GZIP;

		else if (strcmp(argv[0], "-Z") == 0)
			format = SLZ_FMT_ZLIB;

		else
			usage(name, 1);

		argv++;
		argc--;
	}

	if (argc > 0) {
		fd = open(argv[0], O_RDONLY);
		if (fd == -1) {
			perror("open()");
			exit(1);
		}
	}

	if (isatty(1) && !test && !force)
		die(1, "Use -f if you really want to send compressed data to a terminal, or -h for help.\n");

	slz_make_crc_table();
	slz_prepare_dist_table();

	buflen = bufsize;
	if (bufsize <= 0) {
		if (fstat(fd, &instat) == -1) {
			perror("fstat(fd)");
			exit(1);
		}
		/* size of zero means stat the file */
		bufsize = instat.st_size;
		if (bufsize <= 0)
			die(1, "Cannot determine input size, use -b to force it.\n");

		buflen = bufsize;
		bufsize = (bufsize + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
	}

	outbuf = calloc(1, BLK + 4096);
	if (!outbuf) {
		perror("calloc");
		exit(1);
	}

	buffer = mmap(NULL, bufsize, PROT_READ, MAP_PRIVATE, fd, 0);
	if (buffer == MAP_FAILED) {
		buffer = calloc(1, bufsize);
		if (!buffer) {
			perror("calloc");
			exit(1);
		}

		buflen = read(fd, buffer, bufsize);
		if (buflen < 0) {
			perror("read");
			exit(2);
		}
	}

	while (loops--) {
		slz_init(&strm, level, format);

		len = ofs = 0;
		do {
			len += slz_encode(&strm, outbuf + len, buffer + ofs, (buflen - ofs) > BLK ? BLK : buflen - ofs, (buflen - ofs) > BLK);
			if (buflen - ofs > BLK) {
				totout += len;
				ofs += BLK;
				if (console && !test)
					write(1, outbuf, len);
				len = 0;
			}
			else
				ofs = buflen;
		} while (ofs < buflen);
		len += slz_finish(&strm, outbuf + len);
		totin += ofs;
		totout += len;
		if (console && !test)
			write(1, outbuf, len);
	}
	if (verbose)
		fprintf(stderr, "totin=%d totout=%d ratio=%.2f%% crc32=%08x\n", totin, totout, totout * 100.0 / totin, strm.crc32);

	return 0;
}
