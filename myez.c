#include <linux/module.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/buffer_head.h>
#include <linux/vfs.h>
#include <linux/writeback.h>
#include <linux/uio.h>
#include <linux/uaccess.h>

#include <linux/fs.h>
#include <linux/pagemap.h>
#include <linux/highmem.h>
#include <linux/time.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/backing-dev.h>
#include <linux/ramfs.h>
#include <linux/sched.h>
#include <linux/parser.h>
#include <linux/magic.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/fs_context.h>
#include <linux/fs_parser.h>

#include <linux/module.h>
#include <linux/printk.h>

#include "ezfs_ops.h"
#include "ezfs.h"


/* Block Size - 4096 */
#define MYEZ_BSIZE_BITS		12
#define MYEZ_BLOCK_SIZE		(1<<MYEZ_BSIZE_BITS)

#define MYEZ_MAGIC		0x1BADFACF

umode_t g_mode;
struct ezfs_inode *temp_e_ino;

// TODO: need to replace these values with find_first_zero_bit, as they will not be saved
// between reboots/module installation
uint64_t empty_sblock_no = 16;
struct mutex myezfs_lock;

struct myez_sb_info {
	unsigned long si_blocks;
	unsigned long si_freeb;
	unsigned long si_freei;
	unsigned long si_lf_eblk;
	unsigned long si_lasti;
	//DECLARE_BITMAP(si_imap, MYEZ_MAX_LASTI+1);
	struct mutex myez_lock;
};


struct myez_super_block {
	__le32 s_magic;
	__le32 s_start;
	__le32 s_end;
	__le32 s_from;
	__le32 s_to;
	__s32 s_bfrom;
	__s32 s_bto;
	char  s_fsname[6];
	char  s_volume[6];
	__u32 s_padding[118];
};

struct inode *myez_get_inode(struct super_block *sb, unsigned long ino);

static int myez_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode);
static int myez_rmdir(struct inode *dir, struct dentry *dentry);
//static int clear_dir_files(struct super_block *s, unsigned long ino);
static int clear_files(struct super_block *s, struct inode *inode);


static unsigned long find_contiguous_block(struct super_block *sb, unsigned long nfree_req, unsigned long indent)
{
	struct ezfs_sb_buffer_heads *fsi = sb->s_fs_info;
	unsigned long *free_data_blk;
	unsigned long empty_ino;

	int i = 0;

	free_data_blk = (unsigned long *)(((struct ezfs_super_block *)fsi->sb_bh->b_data)->free_data_blocks);
	//free_data_blk = free_data_blk >> indent;
	empty_ino = find_first_zero_bit(free_data_blk + indent, EZFS_MAX_DATA_BLKS - indent);

	if (empty_ino == EZFS_MAX_DATA_BLKS - indent)
		return EZFS_MAX_DATA_BLKS;

	for (i = 1; i < nfree_req; i++) {
		if (IS_SET((((struct ezfs_super_block *)(fsi->sb_bh->b_data))->free_data_blocks), empty_ino + i)) {
			empty_ino = find_contiguous_block(sb, nfree_req, empty_ino + 1);
			break;
		}
	}

	return empty_ino;
}

static int myez_move_block(unsigned long from, unsigned long to,
					struct super_block *sb)
{
	struct buffer_head *bh, *new;

	bh = sb_bread(sb, from);
	if (!bh)
		return -EIO;

	new = sb_getblk(sb, to);
	memcpy(new->b_data, bh->b_data, bh->b_size);
	mark_buffer_dirty(new);
	bforget(bh);
	brelse(new);
	return 0;
}

static int myez_get_block(struct inode *inode, sector_t block,
			struct buffer_head *bh_result, int create)
{
	int err;
	struct super_block *sb = inode->i_sb;
	struct ezfs_sb_buffer_heads *fsi = sb->s_fs_info;
	struct ezfs_inode *e_inode = inode->i_private;
	uint64_t block_start = e_inode->data_block_number;
	struct ezfs_dir_entry *de;

	uint64_t size = inode->i_size;
	uint64_t block_size = 4096;
	uint64_t n_blocks = (size/block_size);

	uint64_t phys = block + block_start;
	int i = 0;
	unsigned long test_b = 0;

	(void)err;
	(void)de;

	//printk(KERN_INFO "MYEZ GET BLOCK\n");
	if (size % block_size != 0)
		n_blocks += 1;

	if (!create) {
		if (block < (inode->i_blocks)/8)
			map_bh(bh_result, sb, phys);
		return 0;
	}

	if (block < (inode->i_blocks)/8)  {
		// printk(KERN_INFO "[MYEZ LS1] Make Sure Writing file %llu %llu %llu %lu\n", block, phys, empty_sblock_no, inode->i_ino);
		map_bh(bh_result, sb, phys);

		return 0;
	}

	if (phys >= EZFS_MAX_DATA_BLKS)
		return -ENOSPC;

	mutex_lock(&myezfs_lock);
	if (!IS_SET((((struct ezfs_super_block *)(fsi->sb_bh->b_data))->free_data_blocks), phys)) {
		// printk(KERN_INFO "[MYEZ LS 2] Make Sure Writing file %llu %llu %llu %lu\n", block, phys, empty_sblock_no, inode->i_ino);
		map_bh(bh_result, sb, phys);
		inode->i_blocks += 8;
		SETBIT((((struct ezfs_super_block *)(fsi->sb_bh->b_data))->free_data_blocks), phys);
		mark_inode_dirty(inode);

		if (phys >= empty_sblock_no)
			empty_sblock_no = phys + 1;
	} else {
		test_b = find_contiguous_block(sb, ((inode->i_blocks)/8) + 1, 0);
		// printk(KERN_INFO "[MYEZ LS3] Make Sure Writing file %llu %llu %llu %lu %llu %lu\n", block, phys, empty_sblock_no, inode->i_ino, ((inode->i_blocks)/8) + 1, test_b);
		/*Need to do Stuff*/

		/*if ((empty_sblock_no + ((inode->i_blocks)/8)) >= EZFS_MAX_DATA_BLKS) {
		 *	mutex_unlock(&myezfs_lock);
		 *	return -ENOSPC;
		 *}
		 */
		if (test_b >= EZFS_MAX_DATA_BLKS) {
			mutex_unlock(&myezfs_lock);
			return -ENOSPC;
		}
		for (i = block_start; i < block_start + (inode->i_blocks)/8; i++) {
			if (myez_move_block(i, test_b + i, sb)) {
				mutex_unlock(&myezfs_lock);
				return -EIO;
			}
			SETBIT((((struct ezfs_super_block *)(fsi->sb_bh->b_data))->free_data_blocks), test_b + i);
			CLEARBIT((((struct ezfs_super_block *)(fsi->sb_bh->b_data))->free_data_blocks), i);
		}
		e_inode->data_block_number = test_b;

		if (test_b + ((inode->i_blocks)/8) + 1 >= empty_sblock_no)
			empty_sblock_no = test_b + ((inode->i_blocks)/8) + 1;
		phys = test_b + ((inode->i_blocks)/8);
		SETBIT((((struct ezfs_super_block *)(fsi->sb_bh->b_data))->free_data_blocks), phys);
		inode->i_blocks += 8;

		mark_inode_dirty(inode);
		map_bh(bh_result, sb, phys);
		//empty_sblock_no += 1;
		mutex_unlock(&myezfs_lock);
		return 0;
	}

	mutex_unlock(&myezfs_lock);
	return 0;
}
static int myez_readpage(struct file *file, struct page *page)
{
	return block_read_full_page(page, myez_get_block);
}

static int myez_writepage(struct page *page, struct writeback_control *wbc)
{
	//printk(KERN_INFO "[MYEZ W2] Make Sure Writing PAGE file\n");
	return block_write_full_page(page, myez_get_block, wbc);
}

static void myez_write_failed(struct address_space *mapping, loff_t to)
{
	struct inode *inode = mapping->host;

	//printk(KERN_INFO "[MYEZ W3] Make Sure Writing file FAILED\n");
	if (to > inode->i_size)
		truncate_pagecache(inode, inode->i_size);
}

static int myez_write_begin(struct file *file, struct address_space *mapping,
			loff_t pos, unsigned int len, unsigned int flags,
			struct page **pagep, void **fsdata)
{
	int ret;

	// printk(KERN_INFO "[MYEZ W4] Make Begin Writing file\n");
	ret = block_write_begin(mapping, pos, len, flags, pagep,
				myez_get_block);
	if (unlikely(ret))
		myez_write_failed(mapping, pos + len);

	return ret;
}

static sector_t myez_bmap(struct address_space *mapping, sector_t block)
{
	// printk(KERN_INFO "[MYEZ BMAP] Make Begin Writing file\n");
	return generic_block_bmap(mapping, block, myez_get_block);
}

static const struct address_space_operations myez_aops = {
	.readpage	= myez_readpage,
	.writepage	= myez_writepage,
	.write_begin	= myez_write_begin,
	.write_end	= generic_write_end,
	.bmap		= myez_bmap,
	//.set_page_dirty	= __set_page_dirty_no_writeback,
};

static const struct inode_operations myez_file_inode_operations = {
	//.setattr	= simple_setattr,
	//.getattr	= simple_getattr,
};

static const struct file_operations myez_file_operations = {
	.read_iter	= generic_file_read_iter,
	.write_iter	= generic_file_write_iter,
	.mmap		= generic_file_mmap,
	.fsync		= generic_file_fsync,
	.splice_read	= generic_file_splice_read,
	//.splice_write	= iter_file_splice_write,
	.llseek		= generic_file_llseek,
	//.get_unmapped_area	= ramfs_mmu_get_unmapped_area,
};

static int myez_readdir(struct file *f, struct dir_context *ctx)
{
	struct inode *dir = file_inode(f);
	struct ezfs_inode *e_inode = dir->i_private;
	struct buffer_head *bh;
	struct ezfs_dir_entry *de;
	int block;

	long long pos;

	//printk(KERN_INFO "Entered ReadDir [LS]  --- Loading module... Hello World 1332!: %lld\n", ctx->pos);
	if (ctx->pos > (4096-1)) {
		//printk(KERN_INFO "Returning ReadDir [LS]  --- Loading module... Hello World!: %lld\n", ctx->pos);
		return -EINVAL;
	}
	//printk(KERN_INFO "Coming here ReadDir [LS]  --- Loading module... Hello World!: %lld\n", ctx->pos);

	if (!dir_emit_dots(f, ctx))
		return 0;

	pos = ctx->pos - 2;

	//printk(KERN_INFO "Entered ReadDir [LS 2]  --- Loading module... Hello World!\n %lld", ctx->pos);
	//while (ctx->pos < dir->i_size) {

	block = e_inode->data_block_number;

	bh = sb_bread(dir->i_sb, block);
	if (!bh) {
		brelse(bh);
		return 0;
	}

	// printk(KERN_INFO "ezfs_dir-entry size: %lu\n", sizeof(struct ezfs_dir_entry));
	if (pos < 4096) {
		de = (struct ezfs_dir_entry *)(bh->b_data + pos);

		if (de->active == 1) {
			if (de->inode_no) {
				int size = strnlen(de->filename, EZFS_FILENAME_BUF_SIZE);
				// printk(KERN_INFO "Entered Read DirEmit 1  --- Loading module... Hello World!\n %lld %d DT_UNKNOWN: %d", ctx->pos, size, S_DT(dir->i_mode));
				if (!dir_emit(ctx, de->filename, size,
					      de->inode_no, S_DT(dir->i_mode))) {
					brelse(bh);
					return 0;
				}
			}
		}

		ctx->pos += sizeof(struct ezfs_dir_entry);
	}
	brelse(bh);

	return 0;
}

static struct buffer_head *ezfs_find_entry(struct inode *dir,
					   const struct qstr *child,
					   struct ezfs_dir_entry **res_dir)
{
	unsigned long offset = 0;
	struct buffer_head *bh = NULL;
	struct ezfs_dir_entry *de;
	const unsigned char *name = child->name;
	int namelen = child->len;

	*res_dir = NULL;
	if (namelen > EZFS_FILENAME_BUF_SIZE)
		return NULL;

	bh = sb_bread(dir->i_sb, ((struct ezfs_inode *)dir->i_private)->data_block_number); //container_of(dir, struct ezfs_inode, vfs_inode)->sb_block);
	if (!bh) {
		brelse(bh);
		return NULL;
	}

	// do the lookup
	while (offset < dir->i_size) {
		de = (struct ezfs_dir_entry *)(bh->b_data + offset);
		offset += sizeof(struct ezfs_dir_entry);
		//printk(KERN_INFO "Entered ez_find_entry [LS 3]  --- Loading module... Hello World!: %s %d\n", de->filename, dir->i_size);
		if (de->active == 1) {
		//printk(KERN_INFO "Entered ez_find_entry [LS 4]  --- Loading module... Hello World!: %s\n", de->filename);
			if (!(memcmp(name, de->filename, namelen))) {
				// printk(KERN_INFO "Entered ez_find_entry [LS 5]  --- Loading module... Hello World!: %s\n", name);
				*res_dir = de;
				return bh;
			}
		}
	}
	brelse(bh);

	return NULL;
}

static struct dentry *myez_lookup(struct inode *dir, struct dentry *dentry,
				  unsigned int flags)
{
	struct inode *inode = NULL;
	struct buffer_head *bh;
	struct ezfs_dir_entry *de;
	//	struct ezfs_sb_buffer_heads *fsi = dir->i_sb->s_fs_info;

	//printk(KERN_INFO "Entered LOOKUP [LS 2]  --- Loading module... Hello World!\n");

	if (dentry->d_name.len > EZFS_FILENAME_BUF_SIZE)
		return ERR_PTR(-ENAMETOOLONG);

	//	mutex_lock(((struct ezfs_super_block *)fsi->sb_bh)->ezfs_lock);
	bh = ezfs_find_entry(dir, &dentry->d_name, &de);
	if (bh) {
		brelse(bh);
		// printk(KERN_INFO "Entered LOOKUP [LS 2000]  --- Loading module... %lld\n", de->inode_no);
		inode = myez_get_inode(dir->i_sb, de->inode_no);
	}
	//	mutex_unlock(((struct ezfs_super_block *)fsi->sb_bh)->ezfs_lock);

	return d_splice_alias(inode, dentry);
}

static int myez_add_entry(struct inode *dir, const struct qstr *child, int ino)
{
	const unsigned char *name = child->name;
	int namelen = child->len;
	struct buffer_head *bh;
	struct ezfs_dir_entry *de;
	int off, i;

	if (namelen <= 0)
		return -ENOENT;
	if (namelen > EZFS_MAX_FILENAME_LENGTH)
		return -ENAMETOOLONG;

	bh = sb_bread(dir->i_sb, ((struct ezfs_inode *)dir->i_private)->data_block_number);
	if (!bh)
		return -EIO;

	for (off = 0; off < EZFS_BLOCK_SIZE; off += sizeof(struct ezfs_dir_entry)) {
		de = (struct ezfs_dir_entry *)(bh->b_data + off);

		// once we find an empty direntry, use it
		if (!de->inode_no) {
			dir->i_mtime = current_time(dir);
			mark_inode_dirty(dir);
			de->inode_no = ino;
			de->active = 1;
			for (i = 0; i < EZFS_FILENAME_BUF_SIZE; i++)
				de->filename[i] = (i < namelen ? name[i] : 0);
			mark_buffer_dirty_inode(bh, dir);
			brelse(bh);
			return 0;
		}
	}

	return -ENOSPC;
}


static int myez_create(struct inode *dir, struct dentry *dentry, umode_t mode,
		       bool excl)
{
	struct inode *inode;
	struct ezfs_inode *di;
	struct buffer_head *bh;
	struct super_block *sb = dir->i_sb;
	struct ezfs_sb_buffer_heads *fsi = sb->s_fs_info;
	int err, off;
	unsigned long ino;
	unsigned long empty_ino;
	(void) ino;

	empty_ino = find_first_zero_bit((const unsigned long *)(((struct ezfs_super_block *)fsi->sb_bh->b_data)->free_inodes), EZFS_MAX_INODES);

	if (empty_ino >= EZFS_MAX_INODES)
		return -ENOSPC;

	/* bh = sb_bread(sb, EZFS_INODE_STORE_DATABLOCK_NUMBER); */
	/* if (!bh) { */
	/*	//iget_failed(inode); */
	/*	brelse(bh); */
	/*	return -EIO; */
	/* } */
	bh = fsi->i_store_bh;

	inode = iget_locked(sb, empty_ino);

	if (!inode)
		return -ENOMEM;

	mutex_lock(&myezfs_lock);

	/*test_b = find_contiguous_block(sb, 1, 0);
	 *
	 *if (test_b >= EZFS_MAX_DATA_BLKS) {
	 *	mutex_unlock(&myezfs_lock);
	 *	return -ENOSPC;
	 *}
	 */

	off = empty_ino - EZFS_ROOT_INODE_NUMBER;
	//	di = (struct ezfs_inode *)bh->b_data + off;//(off * sizeof(struct ezfs_inode));
	di = (struct ezfs_inode *)fsi->i_store_bh->b_data + off;

	inode_init_owner(inode, dir, mode);
	inode->i_mode = mode;
	inode->i_mode |= S_IFREG;
	inode->i_mtime = inode->i_atime = inode->i_ctime = current_time(inode);
	inode->i_blocks = 0;
	inode->i_size = 0;
	inode->i_op = &myez_file_inode_operations;
	inode->i_fop = &myez_file_operations;
	inode->i_ino = empty_ino;
	inode->i_mapping->a_ops = &myez_aops;
	inode->i_private = di;
	set_nlink(inode, 1);


	di->mode = inode->i_mode;
	di->uid = i_uid_read(inode);
	di->gid = i_gid_read(inode);
	di->nlink = 1;
	di->i_atime = inode->i_atime;
	di->i_mtime = inode->i_mtime;
	di->i_ctime = inode->i_ctime;
	di->file_size = inode->i_size;
	di->nblocks = (inode->i_blocks)/8;

	SETBIT((((struct ezfs_super_block *)(fsi->sb_bh->b_data))->free_inodes), empty_ino);
	//SETBIT((((struct ezfs_super_block *)(fsi->sb_bh->b_data))->free_data_blocks), test_b);

	// printk(KERN_INFO "[MYEZ LS4 Create] Make Sure Writing file %llu %lu\n", empty_sblock_no, inode->i_ino);
	/*if (test_b >= empty_sblock_no)
	 *	empty_sblock_no = test_b + 1;
	 */
	//sempty_sblock_no += 1;
	//insert_inode_hash(inode);

	mark_inode_dirty(inode);
	mark_inode_dirty(dir);
	mark_buffer_dirty(bh);
	mark_buffer_dirty(fsi->sb_bh);

	//	mutex_unlock(&myezfs_lock);
	//printk(KERN_INFO "IN EZFS_CREATE : MARKED!\n");

	err = myez_add_entry(dir, &dentry->d_name, inode->i_ino);
	if (err) {
		drop_nlink(inode);
		mark_inode_dirty(inode);
		mutex_unlock(&myezfs_lock);
		//printk(KERN_INFO "error in __func__ from add_entry : %d\n", err);
		iput(inode);
		return err;
	}

	mutex_unlock(&myezfs_lock);
	d_instantiate(dentry, inode);
	return 0;
}

static int myez_unlink(struct inode *dir, struct dentry *dentry)
{
	struct buffer_head *bh;
	struct ezfs_dir_entry *de;
	struct inode *inode = d_inode(dentry);

	//printk(KERN_INFO "[MYEZ_UNLINK DEL]  --- Loading module... Hello World!:\n");
	//mutex_lock(&myezfs_lock);
	bh = ezfs_find_entry(dir, &dentry->d_name, &de);

	if (!bh)
		return 0;

	if (!inode->i_nlink)
		set_nlink(inode, 1);

	de->inode_no = 0;
	de->active = 0;
	mark_buffer_dirty_inode(bh, dir);
	dir->i_ctime = dir->i_mtime = current_time(dir);
	mark_inode_dirty(dir);
	inode->i_ctime = dir->i_ctime;

	inode_dec_link_count(inode);
	//set_nlink(inode, inode->i_nlink - 1);
	//mutex_unlock(&myezfs_lock);
	brelse(bh);

	return 0;
}

static int myez_rename(struct inode *old_dir, struct dentry *old_dentry,
		      struct inode *new_dir, struct dentry *new_dentry,
		      unsigned int flags)
{
	struct inode *old_inode, *new_inode;
	struct buffer_head *old_bh, *new_bh;
	struct ezfs_dir_entry *old_de, *new_de;
	struct super_block *sb;
	struct myez_sb_info *info;
	int error = -ENOENT;

	//printk(KERN_INFO "[MYEZ_RENAME]  --- Loading module... Hello World!\n");
	if (flags & ~RENAME_NOREPLACE)
		return -EINVAL;

	old_bh = new_bh = NULL;
	old_inode = d_inode(old_dentry);
	if (S_ISDIR(old_inode->i_mode))
		return -EINVAL;
	sb = old_inode->i_sb;
	info = sb->s_fs_info;

	//mutex_lock(&myezfs_lock);
	old_bh = ezfs_find_entry(old_dir, &old_dentry->d_name, &old_de);

	if (!old_bh)
		goto end_rename;

	error = -EPERM;
	new_inode = d_inode(new_dentry);
	new_bh = ezfs_find_entry(new_dir, &new_dentry->d_name, &new_de);

	if (new_bh && !new_inode) {
		brelse(new_bh);
		new_bh = NULL;
	}
	if (!new_bh) {
		error = myez_add_entry(new_dir, &new_dentry->d_name,
					old_inode->i_ino);
		if (error)
			goto end_rename;
	}
	old_de->inode_no = 0;
	old_dir->i_ctime = old_dir->i_mtime = current_time(old_dir);
	mark_inode_dirty(old_dir);
	if (new_inode) {
		new_inode->i_ctime = current_time(new_inode);
		inode_dec_link_count(new_inode);
	}
	mark_buffer_dirty_inode(old_bh, old_dir);
	error = 0;

end_rename:
	//mutex_unlock(&myezfs_lock);
	brelse(old_bh);
	brelse(new_bh);
	return error;
}

static const struct inode_operations myez_dir_inops = {
	.create		= myez_create,
	.lookup		= myez_lookup,
	//.link		= simple_link,
	.unlink		= myez_unlink,
	//.symlink	= ramfs_symlink,
	.mkdir		= myez_mkdir,
	.rmdir		= myez_rmdir,
	//.mknod		= ramfs_mknod,
	.rename		= myez_rename,
};

const struct file_operations myez_dir_operations = {
	.read		= generic_read_dir,
	.iterate_shared	= myez_readdir,
	.fsync		= generic_file_fsync,
	.llseek		= generic_file_llseek,
};

static struct ezfs_inode *find_inode(struct super_block *sb, uint64_t ino,
				     struct buffer_head **p)
{
	struct ezfs_sb_buffer_heads *fsi = sb->s_fs_info;

	ino -= EZFS_ROOT_INODE_NUMBER;

	//	*p = sb_bread(sb, EZFS_INODE_STORE_DATABLOCK_NUMBER);
	*p = fsi->i_store_bh;
	if (!*p)
		return ERR_PTR(-EIO);

	return (struct ezfs_inode *)(*p)->b_data + ino;//(off * sizeof(struct ezfs_inode));
	//return (struct ezfs_inode *)((*p)->b_data + ino);//(ino * sizeof(struct ezfs_inode)));
	//return (struct ezfs_inode *)((*p)->b_data + (ino * sizeof(struct ezfs_inode)));
}

static int ezfs_write_inode(struct inode *inode, struct writeback_control *wbc)
{
	struct ezfs_inode *di;
	struct buffer_head *bh;
	struct super_block *sb = inode->i_sb;
	struct ezfs_sb_buffer_heads *fsi = sb->s_fs_info;
	struct ezfs_inode *e_inode = inode->i_private;
	int err = 0;

	//printk(KERN_INFO "IN EZFS_WRITE_INODE : start\n");

	di = find_inode(sb, inode->i_ino, &bh);
	if (IS_ERR(di))
		return PTR_ERR(di);

	//printk(KERN_INFO "IN EZFS_WRITE_INODE : found inode\n");

	// lock
	//printk(KERN_INFO "[MYEZ GET INODE] From Buffer %lu %d\n", inode->i_ino, inode->i_mode);
	di->mode = inode->i_mode;
	di->uid = i_uid_read(inode);
	di->gid = i_gid_read(inode);
	di->nlink = inode->i_nlink;
	di->i_atime = inode->i_atime;
	di->i_mtime = inode->i_mtime;
	di->i_ctime = inode->i_ctime;
	di->file_size = inode->i_size;
	di->nblocks = (inode->i_blocks)/8;
	di->data_block_number = e_inode->data_block_number;

	mark_buffer_dirty(bh);
	mark_buffer_dirty(fsi->i_store_bh);

	//printk(KERN_INFO "IN EZFS_WRITE_INODE : wrote inode\n");

	/* if (wbc->sync_mode == WB_SYNC_ALL) { */
	/*	sync_dirty_buffer(bh); */
	/*	if (buffer_req(bh) && !buffer_uptodate(bh)) */
	/*		err = -EIO; */
	/* } */
	//brelse(bh);
	// unlock
	return err;

	// OLD IMPLEMENTATION BELOW

	//	struct ezfs_inode *e_inode = inode->i_private;
	//printk(KERN_INFO "[EZFS INODE Disk writeup]  --- Loading module... Hello World!\n");

	//	mark_inode_dirty(inode);
	/*c->nblocks = (inode->i_blocks)/8;
	 *e_inode->i_atime = inode->i_atime;
	 *e_inode->i_mtime = inode->i_mtime;
	 *e_inode->i_ctime = inode->i_ctime;
	 */

	return 0;
}


static void myez_put_super(struct super_block *s)
{
	//printk(KERN_INFO "MYEZ PUT SUPER\n");
	mutex_destroy(&myezfs_lock);
	//s->s_fs_info = NULL;
}

static void myez_evict_inode(struct inode *inode)
{
	/* unsigned long ino = inode->i_ino; */
	/* struct ezfs_inode *di; */
	/* struct buffer_head *bh; */
	/* struct super_block *s = inode->i_sb; */
	/* struct ezfs_sb_buffer_heads *fsi = s->s_fs_info; */
	/* struct ezfs_inode *e_inode = inode->i_private; */
	/* uint64_t block_start = e_inode->data_block_number; */
	/* unsigned long n_blocks = (inode->i_blocks)/8; */
	/* int i = 0; */
	int nlink = inode->i_nlink;

	//printk(KERN_INFO "[MYEZ Evict inode]  --- Loading module... Hello World!\n");
	truncate_inode_pages_final(&inode->i_data);

	if (nlink)
		return;

	/*Clearing everything here itself*/
	clear_files(inode->i_sb, inode);
	invalidate_inode_buffers(inode);
	clear_inode(inode);

	/*for (i = 0; i < n_blocks; i++)
	 *	CLEARBIT((((struct ezfs_super_block *)(fsi->sb_bh->b_data))->free_data_blocks), block_start+i);
	 */
	//mutex_unlock(&myezfs_lock);
}

static const struct super_operations myez_sops = {
	//.alloc_inode	= myez_alloc_inode,
	//.free_inode	= myez_free_inode,
	.write_inode	= ezfs_write_inode,
	.evict_inode	= myez_evict_inode,
	.put_super	= myez_put_super,
	//.statfs		= myez_statfs,
};

struct inode *myez_get_inode(struct super_block *sb,
			     unsigned long ino)
{
	struct inode *inode = iget_locked(sb, ino);
	struct ezfs_inode *di;
	struct buffer_head *bh;
	struct ezfs_sb_buffer_heads *fsi = sb->s_fs_info;
	int off;

	if (!inode)
		return ERR_PTR(-ENOMEM);

	if (!(inode->i_state & I_NEW))
		return inode;

	/* bh = sb_bread(inode->i_sb, EZFS_INODE_STORE_DATABLOCK_NUMBER); */
	/* if (!bh) { */
	/*	iget_failed(inode); */
	/*	brelse(bh); */
	/*	return ERR_PTR(-EIO); */
	/* } */

	bh = fsi->i_store_bh;
	off = ino - EZFS_ROOT_INODE_NUMBER;
	di = (struct ezfs_inode *)fsi->i_store_bh->b_data + off;//(off * sizeof(struct ezfs_inode));

	//printk(KERN_INFO "[MYEZ GET INODE] From Buffer %lu 0x%x %lld\n", ino, di->mode, di->data_block_number);
	inode->i_mode = di->mode;
	if (S_ISDIR(inode->i_mode)) {
		// dir
		inode->i_mode |= S_IFDIR;
		inode->i_op = &myez_dir_inops;
		inode->i_fop = &myez_dir_operations;
		inode->i_size = 4096;
	} else {
		// file
		inode->i_mode |= S_IFREG;
		inode->i_op = &myez_file_inode_operations;
		inode->i_fop = &myez_file_operations;
		inode->i_size = di->file_size;
	}
	i_uid_write(inode, di->uid);
	i_gid_write(inode, di->gid);
	set_nlink(inode, di->nlink);

	inode->i_mapping->a_ops = &myez_aops;
	inode->i_blocks = di->nblocks * 8;
	inode->i_atime = di->i_atime;
	inode->i_mtime = di->i_mtime;
	inode->i_ctime = di->i_ctime;

	inode->i_private = di;
	//mark_inode_dirty(inode);

	return inode;
}

static int myez_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode)
{
	struct inode *inode;
	struct ezfs_inode *di;
	struct buffer_head *bh;
	struct super_block *sb = dir->i_sb;
	struct ezfs_sb_buffer_heads *fsi = sb->s_fs_info;
	int err, off;
	unsigned long empty_ino;
	unsigned long test_b;

	/* bh = sb_bread(sb, EZFS_INODE_STORE_DATABLOCK_NUMBER); */
	/* if (!bh) { */
	/*	return -EIO; */
	/* } */
	bh = fsi->i_store_bh;

	mutex_lock(&myezfs_lock);

	// do the stuff
	empty_ino = find_first_zero_bit((const unsigned long *)(((struct ezfs_super_block *)fsi->sb_bh->b_data)->free_inodes), EZFS_MAX_INODES);

	if (empty_ino >= EZFS_MAX_INODES) {
		mutex_unlock(&myezfs_lock);
		return -ENOSPC;
	}

	inode = iget_locked(sb, empty_ino);
	if (!inode) {
		mutex_unlock(&myezfs_lock);
		return -ENOMEM;
	}

	test_b = find_contiguous_block(sb, 1, 0);

	if (test_b >= EZFS_MAX_DATA_BLKS) {
		mutex_unlock(&myezfs_lock);
		return -ENOSPC;
	}

	off = empty_ino - EZFS_ROOT_INODE_NUMBER;
	//	di = (struct ezfs_inode *)bh->b_data + off;
	di = (struct ezfs_inode *)fsi->i_store_bh->b_data + off;

	inode_init_owner(inode, dir, mode);
	inode->i_mode = mode;
	inode->i_mode |= S_IFDIR;
	inode->i_mtime = inode->i_atime = inode->i_ctime = current_time(inode);
	inode->i_blocks = 8;
	inode->i_size = EZFS_BLOCK_SIZE;
	inode->i_op = &myez_dir_inops;
	inode->i_fop = &myez_dir_operations;
	inode->i_ino = empty_ino;
	inode->i_mapping->a_ops = &myez_aops;
	inode->i_private = di;
	set_nlink(inode, 2);

	test_b = find_contiguous_block(sb, ((inode->i_blocks)/8) + 1, 0);
	di->data_block_number = test_b;
	di->mode = inode->i_mode;
	di->uid = i_uid_read(inode);
	di->gid = i_gid_read(inode);
	di->nlink = inode->i_nlink;
	di->i_atime = inode->i_atime;
	di->i_mtime = inode->i_mtime;
	di->i_ctime = inode->i_ctime;
	di->file_size = inode->i_size;
	di->nblocks = (inode->i_blocks)/8;

	//printk(KERN_INFO "[MYEZ myez_mkdir] SBlock %lld 0x%d\n", di->data_block_number, inode->i_mode);

	SETBIT((((struct ezfs_super_block *)(fsi->sb_bh->b_data))->free_inodes), empty_ino);
	SETBIT((((struct ezfs_super_block *)(fsi->sb_bh->b_data))->free_data_blocks), test_b);

	//printk(KERN_INFO "[MYEZ LS4 Create Dir] Make Sure Writing file %llu %lu %lu\n", empty_sblock_no, inode->i_ino, test_b);
	if (test_b >= empty_sblock_no)
		empty_sblock_no = test_b + 1;
	//empty_sblock_no += 1;

	set_nlink(dir, dir->i_nlink + 1);

	//printk(KERN_INFO "[MYEZ myez_mkdir] New Directory Created %s %lu\n", dentry->d_name.name, inode->i_ino);
	mark_inode_dirty(inode);
	mark_inode_dirty(dir);
	mark_buffer_dirty(bh);
	mark_buffer_dirty(fsi->sb_bh);
	err = myez_add_entry(dir, &dentry->d_name, inode->i_ino);
	if (err) {
		drop_nlink(inode);
		mark_inode_dirty(inode);
		mutex_unlock(&myezfs_lock);
		//printk(KERN_INFO "error in __func__ from add_entry : %d\n", err);
		iput(inode);
		return err;
	}

	mutex_unlock(&myezfs_lock);
	mark_buffer_dirty(bh);
	d_instantiate(dentry, inode);
	return 0;
}

static int has_children(struct super_block *sb, struct inode *dir)
{
	struct ezfs_inode *ez_inode = dir->i_private;
	struct buffer_head *bh;
	struct ezfs_dir_entry *de;
	int i = 0;

	bh = sb_bread(sb, ez_inode->data_block_number);
	if (!bh)
		return -EIO;

	for (i = 0; i < EZFS_BLOCK_SIZE; i += sizeof(struct ezfs_dir_entry)) {
		de = (struct ezfs_dir_entry *)(bh->b_data + i);
		if (de->inode_no && (de->active == 1)) {
			brelse(bh);
			return 1;
		}
	}

	brelse(bh);
	return 0;
}

static int clear_files(struct super_block *s, struct inode *inode)
{
	struct buffer_head *bh;
	unsigned long t_block = 0;
	struct ezfs_dir_entry *de;
	//unsigned long temp_inode_no;
	unsigned long s_block_no;
	unsigned long block_no;
	struct ezfs_sb_buffer_heads *fsi = s->s_fs_info;
	struct ezfs_inode *einode = ((struct ezfs_inode *)inode->i_private);

	s_block_no = ((struct ezfs_inode *)inode->i_private)->data_block_number;
	t_block = 0;

	while (t_block < (inode->i_blocks)/8) {
		block_no = s_block_no + t_block;
		bh = sb_bread(s, block_no);
		if (!bh) {
			brelse(bh);
			return 0;
		}
		de = (struct ezfs_dir_entry *)(bh->b_data);

		memset(de, 0, 4096);
		mark_buffer_dirty(bh);
		brelse(bh);
		mutex_lock(&myezfs_lock);
		CLEARBIT((((struct ezfs_super_block *)(fsi->sb_bh->b_data))->free_data_blocks), block_no);
		inode->i_blocks -= 8;
		mutex_unlock(&myezfs_lock);
		t_block += 1;
	}

	mutex_lock(&myezfs_lock);
	CLEARBIT((((struct ezfs_super_block *)(fsi->sb_bh->b_data))->free_inodes), inode->i_ino);
	memset(einode, 0, sizeof(struct ezfs_inode));

	//memset(inode, 0, sizeof(inode));

	mutex_unlock(&myezfs_lock);
	return 0;
}

static int clear_empty_directory(struct super_block *s, struct inode *inode)
{
	struct buffer_head *bh;
	unsigned long offset = 0;
	struct ezfs_dir_entry *de;
	//unsigned long temp_inode_no;
	unsigned long block_no;
	struct ezfs_sb_buffer_heads *fsi = s->s_fs_info;
	//struct ezfs_inode *einode = ((struct ezfs_inode *)inode->i_private);

	block_no = ((struct ezfs_inode *)inode->i_private)->data_block_number;
	bh = sb_bread(s, block_no);

	if (!bh) {
		brelse(bh);
		return 0;
	}

	while (offset < inode->i_size) {
		de = (struct ezfs_dir_entry *)(bh->b_data + offset);
		offset += sizeof(struct ezfs_dir_entry);

		if (de->active == 1)
			return 1;
	}

	mutex_lock(&myezfs_lock);
	CLEARBIT((((struct ezfs_super_block *)(fsi->sb_bh->b_data))->free_data_blocks), block_no);
	CLEARBIT((((struct ezfs_super_block *)(fsi->sb_bh->b_data))->free_inodes), inode->i_ino);
	//memset(einode, 0, sizeof(struct ezfs_inode));

	de = (struct ezfs_dir_entry *)(bh->b_data);
	memset(de, 0, 4096);

	//memset(inode, 0, sizeof(inode));

	mutex_unlock(&myezfs_lock);

	brelse(bh);
	return 0;
}

static int myez_rmdir(struct inode *dir, struct dentry *dentry)
{
	struct super_block *s = dir->i_sb;
	//struct ezfs_sb_buffer_heads *fsi = s->s_fs_info;
	struct buffer_head *bh;
	//struct buffer_head *bh2;

	struct ezfs_dir_entry *de;
	//struct ezfs_inode *einode;
	struct inode *inode = d_inode(dentry);
	bool ret_clear = 0;

	bh = ezfs_find_entry(dir, &dentry->d_name, &de);
	if (!bh)
		return 0;

	if (!S_ISDIR(inode->i_mode)) {
		brelse(bh);
		return -ENOTDIR;
	}

	//printk(KERN_INFO "[MYEZ clear dir ] Old Inode getting cleared: %lld", de->inode_no);
	//clear_dir_files(s, de->inode_no);

	ret_clear = clear_empty_directory(s, inode);

	// check number of children on inode
	if (!simple_empty(dentry) || (has_children(dir->i_sb, inode) == 1) || (ret_clear)) {
		brelse(bh);
		return -ENOTEMPTY;
	}

	if (!inode->i_nlink)
		drop_nlink(inode);

	simple_unlink(dir, dentry);
	drop_nlink(dir);

	de->inode_no = 0;
	de->active = 0;
	mark_buffer_dirty_inode(bh, dir);
	dir->i_ctime = dir->i_mtime = current_time(dir);
	mark_inode_dirty(dir);
	inode->i_ctime = dir->i_ctime;
	inode_dec_link_count(inode);

	brelse(bh);

	return 0;
}

static int myez_fill_super(struct super_block *s, struct fs_context *fc)
{
	struct ezfs_sb_buffer_heads *fsi = s->s_fs_info;
	struct buffer_head *bh, *sbh, *sbh2;
	struct myez_super_block *myez_sb;
	struct myez_sb_info *info;
	struct inode *inode;
	struct ezfs_inode *e_ino;
	int ret = -EINVAL;

	(void)info;
	(void)myez_sb;
	(void)bh;


	mutex_init(&myezfs_lock);
	s->s_time_min = 0;
	s->s_time_max = U32_MAX;

	//printk(KERN_INFO "Entered Super  --- Loading module... Hello World!\n");
	sb_set_blocksize(s, MYEZ_BLOCK_SIZE);

	sbh = sb_bread(s, EZFS_SUPERBLOCK_DATABLOCK_NUMBER);

	if (!sbh)
		goto out;

	fsi->sb_bh = sbh;

	sbh2 = sb_bread(s, EZFS_INODE_STORE_DATABLOCK_NUMBER);
	fsi->i_store_bh = sbh2;

	//printk(KERN_INFO "super sbh is done  --- Loading module... Hello World!\n");
	if (!sbh2)
		goto out;

	s->s_magic = EZFS_MAGIC_NUMBER;
	s->s_op = &myez_sops;

	inode = myez_get_inode(s, 1);

	//printk(KERN_INFO "super sbh2 is done  --- Loading module... Hello World!\n");
	if (IS_ERR(inode)) {
		mutex_destroy(&myezfs_lock);
		return PTR_ERR(inode);
	}

	e_ino = (struct ezfs_inode *) sbh2->b_data;
	g_mode = e_ino->mode;
	temp_e_ino = e_ino;
	if (inode->i_state & I_NEW) {
		//printk(KERN_INFO "inode NEW  --- Loading module... Hello World!\n");

		inode->i_mode = e_ino->mode;
		//g_mode =
		i_uid_write(inode, e_ino->uid);
		i_gid_write(inode, e_ino->gid);
		set_nlink(inode, e_ino->nlink);
		inode->i_size = 4096;
		inode->i_blocks = e_ino->nblocks * 8;
		inode->i_atime = e_ino->i_atime;
		inode->i_mtime = e_ino->i_mtime;
		inode->i_ctime = e_ino->i_ctime;
		inode->i_private = e_ino;

		inode->i_op = &myez_dir_inops;
		inode->i_fop = &myez_dir_operations;
		inode->i_mapping->a_ops = &myez_aops;
	}

	//printk(KERN_INFO "inode is done  --- Loading module... Hello World!\n");
	s->s_root = d_make_root(inode);
	if (!s->s_root) {
		mutex_destroy(&myezfs_lock);
		return -ENOMEM;
	}

	//printk(KERN_INFO "Made Root Super completed --- Loading module... Hello World!\n");
	return 0;

	//out2:
	//dput(s->s_root);
	//s->s_root = NULL;
	//out1:
	//brelse(sbh);
out:
	//mutex_destroy(&info->myez_lock);
	//kfree(info);
	s->s_fs_info = NULL;
	mutex_destroy(&myezfs_lock);
	return ret;
}
static int myez_get_tree(struct fs_context *fc)
{
	int ret = -ENOPROTOOPT;

	//printk(KERN_INFO "MYEZ GET TREE\n");
	ret = get_tree_bdev(fc, myez_fill_super);
	return ret;
}

static void myez_free_fc(struct fs_context *fc)
{
	//printk(KERN_INFO "MYEZ FREE FC\n");
	kfree(fc->s_fs_info);
}

static const struct fs_context_operations myez_context_ops = {
	.free		= myez_free_fc,
	//.parse_param	= myez_parse_param,
	.get_tree	= myez_get_tree,
};


int myez_init_fs_context(struct fs_context *fc)
{
	struct ezfs_sb_buffer_heads *fsi;

	fsi = kzalloc(sizeof(*fsi), GFP_KERNEL);
	if (!fsi)
		return -ENOMEM;

	//fsi->mount_opts.mode = RAMFS_DEFAULT_MODE;
	fc->s_fs_info = fsi;

	//printk(KERN_INFO "Init  --- Loading module... Hello World!\n");
	//sb_bread(, 0);
	fc->ops = &myez_context_ops;
	return 0;
}

void myez_kill_sb(struct super_block *sb)
{
	struct ezfs_sb_buffer_heads *fsi = sb->s_fs_info;

	//printk(KERN_INFO "myez_kill_sb\n");
	mark_buffer_dirty(fsi->sb_bh);
	mark_buffer_dirty(fsi->i_store_bh);
	brelse(fsi->sb_bh);
	brelse(fsi->i_store_bh);
	kfree(sb->s_fs_info);
	sb->s_fs_info = NULL;
	kill_block_super(sb);
}


static struct file_system_type myez_fs_type = {
	.name		= "myezfs",
	.init_fs_context = myez_init_fs_context,
	//.parameters	= myez_fs_parameters,
	//.mount		= myez_mount,
	.kill_sb	= myez_kill_sb,
	//.fs_flags	= FS_REQUIRES_DEV,
};

int init_myez_fs(void)
{
	//printk(KERN_INFO "Loading module... Hello World!\n");
	return register_filesystem(&myez_fs_type);
}

void exit_myez_fs(void)
{
	//printk(KERN_INFO "Removing module... Goodbye World!\n");
	unregister_filesystem(&myez_fs_type);
}



module_init(init_myez_fs);
module_exit(exit_myez_fs);

MODULE_DESCRIPTION("A basic Hello World module");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Team27");
