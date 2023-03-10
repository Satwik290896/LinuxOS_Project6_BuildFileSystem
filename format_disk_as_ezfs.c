#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>

/* These are the same on a 64-bit architecture */
#define timespec64 timespec

#include "ezfs.h"


void passert(int condition, char *message)
{
	printf("[%s] %s\n", condition ? " OK " : "FAIL", message);
	if (!condition)
		exit(1);
}

void inode_reset(struct ezfs_inode *inode)
{
	struct timespec current_time;

	/* In case inode is uninitialized/previously used */
	memset(inode, 0, sizeof(*inode));
	memset(&current_time, 0, sizeof(current_time));

	/* These sample files will be owned by the first user and group on the system */
	inode->uid = 1000;
	inode->gid = 1000;

	/* Current time UTC */
	clock_gettime(CLOCK_REALTIME, &current_time);
	inode->i_atime = inode->i_mtime = inode->i_ctime = current_time;
}

void dentry_reset(struct ezfs_dir_entry *dentry)
{
	memset(dentry, 0, sizeof(*dentry));
}

int main(int argc, char *argv[])
{
	int status;
	int fd;
	ssize_t ret, len;
	struct ezfs_super_block sb;
	struct ezfs_inode inode;
	struct ezfs_dir_entry dentry;
	struct stat stat_buf;
	off_t big_img_size, big_txt_size;
	FILE *img_file, *txt_file;
	size_t img_file_len, txt_file_len;

	char *hello_contents = "Hello world!\n";
	char *names_contents = "Chun-Wei Shaw, Mohsin Rizvi, Sai Satwik Vaddi\n";
	char *img_contents, *txt_contents;
	char buf[EZFS_BLOCK_SIZE];
	const char zeroes[EZFS_BLOCK_SIZE] = { 0 };

	if (argc != 2) {
		printf("Usage: ./format_disk_as_ezfs DEVICE_NAME.\n");
		return -1;
	}

	/* Get size and contents of big_img.jpeg */
	status = stat("big_files/big_img.jpeg", &stat_buf);
	if (status < 0) {
		perror("Error stating big_img.jpeg");
		return -1;
	}
	big_img_size = stat_buf.st_size;

	img_contents = malloc(big_img_size * sizeof(char));

	img_file = fopen("big_files/big_img.jpeg", "r");
	if (!img_file) {
		perror("Error opening big_img.jpeg");
		free(img_contents);
		return -1;
	}
	img_file_len = fread(img_contents, sizeof(char), big_img_size, img_file);
	if (img_file_len != big_img_size) {
		perror("Bad image file");
		free(img_contents);
		return -1;
	}

	/* Get size and contents of big_txt.txt */
	status = stat("big_files/big_txt.txt", &stat_buf);
	if (status < 0) {
		perror("Error stating big_txt.txt");
		free(img_contents);
		return -1;
	}
	big_txt_size = stat_buf.st_size;

	txt_contents = malloc(big_txt_size * sizeof(char));

	txt_file = fopen("big_files/big_txt.txt", "r");
	if (!txt_file) {
		perror("Error opening big_txt.txt");
		free(img_contents);
		free(txt_contents);
		return -1;
	}
	txt_file_len = fread(txt_contents, sizeof(char), big_txt_size, txt_file);
	if (txt_file_len != big_txt_size) {
		perror("Bad txt file");
		free(img_contents);
		free(txt_contents);
		return -1;
	}

	fd = open(argv[1], O_RDWR);
	if (fd == -1) {
		perror("Error opening the device");
		free(img_contents);
		free(txt_contents);
		return -1;
	}
	memset(&sb, 0, sizeof(sb));

	sb.version = 1;
	sb.magic = EZFS_MAGIC_NUMBER;

	/* The first two inodes and datablocks are taken by the root and
	 * hello.txt file, respectively. Mark them as such.
	 */
	SETBIT(sb.free_inodes, 0);
	SETBIT(sb.free_inodes, 1);

	SETBIT(sb.free_data_blocks, 0);
	SETBIT(sb.free_data_blocks, 1);

	/* set bits for subdir, names.txt, big_img.jpeg, and big_txt.txt */
	SETBIT(sb.free_inodes, 2);
	SETBIT(sb.free_inodes, 3);
	SETBIT(sb.free_inodes, 4);
	SETBIT(sb.free_inodes, 5);
	SETBIT(sb.free_inodes, 6);
	SETBIT(sb.free_data_blocks, 2);
	SETBIT(sb.free_data_blocks, 3);
	SETBIT(sb.free_data_blocks, 4);
	SETBIT(sb.free_data_blocks, 5);
	SETBIT(sb.free_data_blocks, 6);
	SETBIT(sb.free_data_blocks, 7);
	SETBIT(sb.free_data_blocks, 8);
	SETBIT(sb.free_data_blocks, 9);
	SETBIT(sb.free_data_blocks, 10);
	SETBIT(sb.free_data_blocks, 11);
	SETBIT(sb.free_data_blocks, 12);
	SETBIT(sb.free_data_blocks, 13);
	SETBIT(sb.free_data_blocks, 14);
	SETBIT(sb.free_data_blocks, 15);

	/* Write the superblock to the first block of the filesystem. */
	ret = write(fd, (char *)&sb, sizeof(sb));
	passert(ret == EZFS_BLOCK_SIZE, "Write superblock");

	inode_reset(&inode);
	inode.mode = S_IFDIR | 0777;
	inode.nlink = 3; // add 1 to 2 because add another directory
	inode.data_block_number = EZFS_ROOT_DATABLOCK_NUMBER;
	inode.file_size = EZFS_BLOCK_SIZE;
	inode.nblocks = 1;

	/* Write the root inode starting in the second block. */
	ret = write(fd, (char *)&inode, sizeof(inode));
	passert(ret == sizeof(inode), "Write root inode");

	/* The hello.txt file will take inode num following root inode num. */
	inode_reset(&inode);
	inode.nlink = 1;
	inode.mode = S_IFREG | 0666;
	inode.data_block_number = EZFS_ROOT_DATABLOCK_NUMBER + 1;
	inode.file_size = strlen(hello_contents);
	inode.nblocks = 1;

	ret = write(fd, (char *) &inode, sizeof(inode));
	passert(ret == sizeof(inode), "Write hello.txt inode");

	/* Write inode for subdir */
	inode_reset(&inode);
	inode.mode = S_IFDIR | 0777;
	inode.nlink = 2; // "subdir", "."
	inode.data_block_number = EZFS_ROOT_DATABLOCK_NUMBER + 2;
	inode.file_size = EZFS_BLOCK_SIZE;
	inode.nblocks = 1;

	ret = write(fd, (char *) &inode, sizeof(inode));
	passert(ret == sizeof(inode), "Write subdir inode");

	/* Write inode for names.txt */
	inode_reset(&inode);
	inode.nlink = 1;
	inode.mode = S_IFREG | 0666;
	inode.data_block_number = EZFS_ROOT_DATABLOCK_NUMBER + 3;
	inode.file_size = strlen(names_contents);
	inode.nblocks = 1;

	ret = write(fd, (char *) &inode, sizeof(inode));
	passert(ret == sizeof(inode), "Write names.txt inode");

	/* Write inode for big_img.jpeg */
	inode_reset(&inode);
	inode.nlink = 1;
	inode.mode = S_IFREG | 0666;
	inode.data_block_number = EZFS_ROOT_DATABLOCK_NUMBER + 4;
	inode.file_size = big_img_size;
	inode.nblocks = 8;

	ret = write(fd, (char *) &inode, sizeof(inode));
	passert(ret == sizeof(inode), "Write big_img.jpeg inode");

	/* Write inode for big_txt.txt */
	inode_reset(&inode);
	inode.nlink = 1;
	inode.mode = S_IFREG | 0666;
	inode.data_block_number = EZFS_ROOT_DATABLOCK_NUMBER + 12;
	inode.file_size = big_txt_size;
	inode.nblocks = 2;

	ret = write(fd, (char *) &inode, sizeof(inode));
	passert(ret == sizeof(inode), "Write big_txt.txt inode");

	/* lseek to the next data block */
	ret = lseek(fd, EZFS_BLOCK_SIZE - 6 * sizeof(struct ezfs_inode),
		SEEK_CUR);
	passert(ret >= 0, "Seek past inode table");

	/* dentry for hello.txt */
	dentry_reset(&dentry);
	strncpy(dentry.filename, "hello.txt", sizeof(dentry.filename));
	dentry.active = 1;
	dentry.inode_no = EZFS_ROOT_INODE_NUMBER + 1;

	ret = write(fd, (char *) &dentry, sizeof(dentry));
	passert(ret == sizeof(dentry), "Write dentry for hello.txt");

	/* dentry for subdir */
	dentry_reset(&dentry);
	strncpy(dentry.filename, "subdir", sizeof(dentry.filename));
	dentry.active = 1;
	dentry.inode_no = EZFS_ROOT_INODE_NUMBER + 2;

	ret = write(fd, (char *) &dentry, sizeof(dentry));
	passert(ret == sizeof(dentry), "Write dentry for subdir");

	/* lseek to the next data block */
	len = EZFS_BLOCK_SIZE - 2 * sizeof(struct ezfs_dir_entry);
	ret = write(fd, zeroes, len);
	passert(ret == len, "Pad to end of root dentries");

	/* hello.txt contents */
	len = strlen(hello_contents);
	strncpy(buf, hello_contents, len);
	ret = write(fd, buf, len);
	passert(ret == len, "Write hello.txt contents");

	/* lseek to subdir data block */
	len = EZFS_BLOCK_SIZE - strlen(hello_contents);
	ret = write(fd, zeroes, len);
	passert(ret == len, "Seek to subdir block");

	/* subdir contents */

	/* dentry for names.txt */
	dentry_reset(&dentry);
	strncpy(dentry.filename, "names.txt", sizeof(dentry.filename));
	dentry.active = 1;
	dentry.inode_no = EZFS_ROOT_INODE_NUMBER + 3;

	ret = write(fd, (char *) &dentry, sizeof(dentry));
	passert(ret == sizeof(dentry), "Write dentry for names.txt");

	/* dentry for big_img.jpeg */
	dentry_reset(&dentry);
	strncpy(dentry.filename, "big_img.jpeg", sizeof(dentry.filename));
	dentry.active = 1;
	dentry.inode_no = EZFS_ROOT_INODE_NUMBER + 4;

	ret = write(fd, (char *) &dentry, sizeof(dentry));
	passert(ret == sizeof(dentry), "Write dentry for big_img.jpeg");

	/* dentry for big_txt.txt */
	dentry_reset(&dentry);
	strncpy(dentry.filename, "big_txt.txt", sizeof(dentry.filename));
	dentry.active = 1;
	dentry.inode_no = EZFS_ROOT_INODE_NUMBER + 5;

	ret = write(fd, (char *) &dentry, sizeof(dentry));
	passert(ret == sizeof(dentry), "Write dentry for big_txt.txt");

	/* pad to end of subdir contents */
	len = EZFS_BLOCK_SIZE - 3 * sizeof(struct ezfs_dir_entry);
	ret = write(fd, zeroes, len);
	passert(ret == len, "Seek to end of subdir contents");

	/* names.txt contents */
	len = strlen(names_contents);
	strncpy(buf, names_contents, len);
	ret = write(fd, buf, len);
	passert(ret == len, "Write names.txt contents");

	/* lseek to big_img data block */
	len = EZFS_BLOCK_SIZE - strlen(names_contents);
	ret = write(fd, zeroes, len);
	passert(ret == len, "Seek to img block");

	/* big_img.jpeg contents */
	ret = write(fd, img_contents, big_img_size);
	passert(ret == big_img_size, "Write big_img.jpeg contents");

	/* lseek to big_txt data block */
	int overflow = big_img_size % EZFS_BLOCK_SIZE;

	if (overflow != 0) {
		len = EZFS_BLOCK_SIZE - (big_img_size % EZFS_BLOCK_SIZE);
		ret = write(fd, zeroes, len);
		passert(ret == len, "Seek to txt block");
	}

	/* big_txt.txt contents */
	ret = write(fd, txt_contents, big_txt_size);
	passert(ret == big_txt_size, "Write big_txt.txt contents");

	ret = fsync(fd);
	passert(ret == 0, "Flush writes to disk");

	close(fd);
	printf("Device [%s] formatted successfully.\n", argv[1]);

	/* TODO: make sure we close images and free contents in every passert that may go wrong. */
	fclose(img_file);
	fclose(txt_file);
	free(img_contents);
	free(txt_contents);

	return 0;
}
