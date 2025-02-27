#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <linux/fs.h>
#include <unistd.h>
#include <fcntl.h>

#define MINIX_HEADER 32
#define GCC_HEADER 1024

#define SYS_SIZE 0x2000

#define DEFAULT_MAJOR_ROOT 3
#define DEFAULT_MINOR_ROOT 6

#define SETUP_SECTS 4

#define STRINGIFY(x) #x

#define ARG_LEN 3

void die(char * str)
{
	fprintf(stderr, "%s\n", str);
	exit(1);
}

void usage(void)
{
	die("Usage: build bootsect setup system [rootdev] [> image]");
}

int main(int argc, char **argv)
{
	int i, c, id;
	char buf[1024];
	char major_root, minor_root;
	struct stat sb;

	if ((argc != ARG_LEN) && (argc != ARG_LEN + 1))
		usage();

	if (argc == ARG_LEN + 1)
	{
		if (strcmp(argv[ARG_LEN], "FLOPPY"))
		{
			if (stat(argv[ARG_LEN], &sb))
			{
				perror(argv[ARG_LEN]);
				die("Couldn't stat root device");
			}
			major_root = sb.st_rdev & 0xff00;
			minor_root = sb.st_rdev & 0x00ff;
		}
		else
		{
			major_root = 0;
			minor_root = 0;
		}
	}
	else
	{
		major_root = DEFAULT_MAJOR_ROOT;
		minor_root = DEFAULT_MINOR_ROOT;
	}
	fprintf(stderr, "Root device is (%d, %d)\n", major_root, minor_root);
	if ((major_root != 2) && (major_root != 3) && (major_root != 0))
	{
		fprintf(stderr, "Illegal root device (major = %d)\n", major_root);
		die("Bad root device --- major #");
	}

	for (i = 0; i < sizeof(buf); i++)
		buf[i] = 0;

	if ((id = open(argv[1], O_RDONLY, 0)) < 0)
		die("Unable to open 'boot'");

	i = read(id, buf, sizeof(buf));
	fprintf(stderr, "Boot sector %d bytes.\n", i);
	if (i != 512)
		die("Boot block must be exactly 512 bytes");

	buf[508] = (char) minor_root;
	buf[509] = (char) major_root;
	i = write(1, buf, 512);
	if (i != 512)
		die("Write call failed");
	close(id);	

	if ((id = open(argv[2], O_RDONLY, 0)) < 0)
		die("Unable to open 'setup'");

	for (i = 0; (c = read(id, buf, sizeof(buf))) > 0; i += c)
		if (write(1, buf, c) != c)
			die("Write call failed");

	close(id);

	if (i > SETUP_SECTS * 512)
		die("Setup exceeds" STRINGIFY(SETUP_SECTS) " sectors - rewrite build/boot/setup");
	fprintf(stderr, "Setup is %d bytes.\n", i);
	for (c = 0; c < sizeof(buf); c++)
		buf[c] ='\0';
	while (i < SETUP_SECTS * 512)
	{
		c = SETUP_SECTS * 512 - i;
		if (c > sizeof(buf))
			c = sizeof(buf);
		
		if (write(1, buf, c) != c)
			die("Write call failed");
		i+=c;
	}
	
	//目前还不需要处理内核（system），所以这段代码要注释掉。
	//后续system被添加进来后，这段代码需要放开。
    /*
	if ((id = open(argv[3], O_RDONLY, 0)) < 0)
		die("Unable to open 'system'");
	for (i = 0; (c = read(id, buf, sizeof(buf))) > 0; i += c)
		if (write(1, buf, c) != c)
			die("Write call failed");
	close(id);
	fprintf(stderr, "System is %d bytes.\n", i);
	if (i > SYS_SIZE * 16)
		die("System is too big");
    */

	return 0;
}
