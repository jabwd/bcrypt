/*
 * bcrypt wrapper library
 *
 * Written in 2011, 2013, 2014 by Ricardo Garcia <public@rg3.name>
 *
 * To the extent possible under law, the author(s) have dedicated all copyright
 * and related and neighboring rights to this software to the public domain
 * worldwide. This software is distributed without any warranty.
 *
 * You should have received a copy of the CC0 Public Domain Dedication along
 * with this software. If not, see
 * <http://creativecommons.org/publicdomain/zero/1.0/>.
 */
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "bcrypt.h"
#include "crypt_blowfish/ow-crypt.h"

#define RANDBYTES (16)

static int try_close(int fd)
{
	int ret;
	for (;;) {
		errno = 0;
		ret = close(fd);
		if (ret == -1 && errno == EINTR)
			continue;
		break;
	}
	return ret;
}

static int try_read(int fd, char *out, size_t count)
{
	size_t total;
	ssize_t partial;

	total = 0;
	while (total < count)
	{
		for (;;) {
			errno = 0;
			partial = read(fd, out + total, count - total);
			if (partial == -1 && errno == EINTR)
				continue;
			break;
		}

		if (partial < 1)
			return -1;

		total += partial;
	}

	return 0;
}

/*
* Time taken only varies with the length of str1. Because user input is hashed
* with bcrypt_hashpw(passwd, hashed, outhash) beforehand and the length of
* hashed is constant the time taken to execute this function will be constant.
*/
static int timing_safe_strcmp(const char *str1, const char *str2)
{
	int i, str1len, str2len, rv = 0;
	str1len = strlen(str1);
	str2len = strlen(str2);
	for (i = 0; i < str1len; i++) {
		rv |= str1[i] ^ str2[i];
	}
	rv |= str1len ^ str2len; // lengths must match
	return rv;
}

int bcrypt_gensalt(int factor, char salt[BCRYPT_HASHSIZE])
{
	int fd;
	char input[RANDBYTES];
	int workf;
	char *aux;

	fd = open("/dev/urandom", O_RDONLY);
	if (fd == -1)
		return 1;

	if (try_read(fd, input, RANDBYTES) != 0) {
		if (try_close(fd) != 0)
			return 4;
		return 2;
	}

	if (try_close(fd) != 0)
		return 3;

	/* Generate salt. */
	workf = (factor < 4 || factor > 31)?12:factor;
	aux = crypt_gensalt_rn("$2a$", workf, input, RANDBYTES,
			       salt, BCRYPT_HASHSIZE);
	return (aux == NULL)?5:0;
}

int bcrypt_hashpw(const char *passwd, const char salt[BCRYPT_HASHSIZE], char hash[BCRYPT_HASHSIZE])
{
	char *aux;
	aux = crypt_rn(passwd, salt, hash, BCRYPT_HASHSIZE);
	return (aux == NULL)?1:0;
}

int bcrypt_checkpw(const char *passwd, const char hashed[BCRYPT_HASHSIZE])
{
	int ret;
	char outhash[BCRYPT_HASHSIZE];

	ret = bcrypt_hashpw(passwd, hashed, outhash);
	if (ret == 0 && timing_safe_strcmp(hashed, outhash) == 0)
	{
		return 1;
	}

	return 0;
}

#ifdef TEST_BCRYPT
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

int main()
{
	clock_t before;
	clock_t after;
	char salt[BCRYPT_HASHSIZE];
	char hash[BCRYPT_HASHSIZE];
	int ret;

	const char pass[] = "hi,mom";
	const char hash1[] = "$2a$10$VEVmGHy4F4XQMJ3eOZJAUeb.MedU0W10pTPCuf53eHdKJPiSE8sMK";
	const char hash2[] = "$2a$10$3F0BVk5t8/aoS.3ddaB3l.fxg5qvafQ9NybxcpXLzMeAt.nVWn.NO";

	ret = bcrypt_gensalt(12, salt);
	assert(ret == 0);
	printf("Generated salt: %s\n", salt);
	before = clock();
	ret = bcrypt_hashpw("testtesttest", salt, hash);
	assert(ret == 0);
	after = clock();
	printf("Hashed password: %s\n", hash);
	printf("Time taken: %f seconds\n",
	       (float)(after - before) / CLOCKS_PER_SEC);

	ret = bcrypt_hashpw(pass, hash1, hash);
	assert(ret == 0);
	printf("First hash check: %s\n", (strcmp(hash1, hash) == 0)?"OK":"FAIL");
	ret = bcrypt_hashpw(pass, hash2, hash);
	assert(ret == 0);
	printf("Second hash check: %s\n", (strcmp(hash2, hash) == 0)?"OK":"FAIL");

	before = clock();
	printf("First hash check w/ bcrypt_checkpw: %s\n", (bcrypt_checkpw(pass, hash1))?"OK":"FAIL");
	after = clock();
	printf("Time taken: %f seconds\n",
	       (float)(after - before) / CLOCKS_PER_SEC);

	before = clock();
	printf("Second hash check w/ bcrypt_checkpw: %s\n", (bcrypt_checkpw(pass, hash2))?"OK":"FAIL");
	after = clock();
	printf("Time taken: %f seconds\n",
	       (float)(after - before) / CLOCKS_PER_SEC);

	return 0;
}
#endif
