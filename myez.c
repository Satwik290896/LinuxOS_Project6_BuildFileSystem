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

uint64_t empty_sblock_no = 16;

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
	int i=0;
	
	if (size % block_size != 0)
		n_blocks += 1;
	
	if (!create) {
		if (block < (inode->i_blocks)/8) {
			map_bh(bh_result, sb, phys);
		
		}
		return 0;
	}
	
	
	if (block < (inode->i_blocks)/8)  {
		printk(KERN_INFO "[MYEZ] Make Sure Writing file %d %d\n", block, phys);
		map_bh(bh_result, sb, phys);

		return 0;
	}
	
	if (!IS_SET((((struct ezfs_super_block *)(fsi->sb_bh->b_data))->free_data_blocks), phys)) {
		printk(KERN_INFO "[MYEZ LS 2] Make Sure Writing file %d %d \n", block, phys);
		map_bh(bh_result, sb, phys);
		inode->i_blocks += 8;
		SETBIT((((struct ezfs_super_block *)(fsi->sb_bh->b_data))->free_data_blocks), phys);
		mark_inode_dirty(inode);
		
		if (phys >= empty_sblock_no) {
			empty_sblock_no = phys + 1;
		}
	}
	else {
		printk(KERN_INFO "[MYEZ LS3] Make Sure Writing file %d %d\n", block,phys);
		/*Need to do Stuff*/
		
		for (i = 0; i < (inode->i_blocks)/8; i++) {
			if (myez_move_block(i, empty_sblock_no + i, sb))
				return -EIO;
			SETBIT((((struct ezfs_super_block *)(fsi->sb_bh->b_data))->free_data_blocks), empty_sblock_no + i);
		}
		e_inode->data_block_number = empty_sblock_no;
		empty_sblock_no += (inode->i_blocks)/8;
		phys = empty_sblock_no;
		SETBIT((((struct ezfs_super_block *)(fsi->sb_bh->b_data))->free_data_blocks), phys);
		inode->i_blocks += 8;

		mark_inode_dirty(inode);
		map_bh(bh_result, sb, phys);
		empty_sblock_no += 1;
		return 0;
	}
	
	return 0;
	
	//de = (struct ezfs_dir_entry *)(bh->b_data);
	
	
}
static int myez_readpage(struct file *file, struct page *page)
{
	return block_read_full_page(page, myez_get_block);
}

static int myez_writepage(struct page *page, struct writeback_control *wbc)
{
	printk(KERN_INFO "[MYEZ W2] Make Sure Writing PAGE file \n");
	return block_write_full_page(page, myez_get_block, wbc);
}

static void myez_write_failed(struct address_space *mapping, loff_t to)
{
	struct inode *inode = mapping->host;

	printk(KERN_INFO "[MYEZ W3] Make Sure Writing file FAILED\n");
	if (to > inode->i_size)
		truncate_pagecache(inode, inode->i_size);
}

static int myez_write_begin(struct file *file, struct address_space *mapping,
			loff_t pos, unsigned len, unsigned flags,
			struct page **pagep, void **fsdata)
{
	int ret;

	printk(KERN_INFO "[MYEZ W4] Make Begin Writing file \n");
	ret = block_write_begin(mapping, pos, len, flags, pagep,
				myez_get_block);
	if (unlikely(ret))
		myez_write_failed(mapping, pos + len);
	
	return ret;
		
}

static sector_t myez_bmap(struct address_space *mapping, sector_t block)
{
	printk(KERN_INFO "[MYEZ BMAP] Make Begin Writing file \n");
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
	
	printk(KERN_INFO "Entered ReadDir [LS]  --- Loading module... Hello World 1332!: %lld \n", ctx->pos);
	if (ctx->pos > (4096-1)) {
		printk(KERN_INFO "Returning ReadDir [LS]  --- Loading module... Hello World!: %lld \n", ctx->pos);
		return -EINVAL;
	}
	
	printk(KERN_INFO "Coming here ReadDir [LS]  --- Loading module... Hello World!: %lld \n", ctx->pos);

	if (!dir_emit_dots(f, ctx))
		return 0;
		

	pos = ctx->pos - 2;
	
	printk(KERN_INFO "Entered ReadDir [LS 2]  --- Loading module... Hello World!\n %lld", ctx->pos);
	//while (ctx->pos < dir->i_size) {
		
	block = e_inode->data_block_number;
		
	bh = sb_bread(dir->i_sb, block);
	if (!bh) {
		brelse(bh);
		return 0;
	}

	printk(KERN_INFO "ezfs_dir-entry size: %lu\n", sizeof(struct ezfs_dir_entry));
	if (pos < 4096) {
		de = (struct ezfs_dir_entry *)(bh->b_data + pos);
		
		if (de->active == 1) {
		if(de->inode_no) {
			
			int size = strnlen(de->filename, EZFS_FILENAME_BUF_SIZE);
			printk(KERN_INFO "Entered Read DirEmit 1  --- Loading module... Hello World!\n %lld %d DT_UNKNOWN: %d", ctx->pos, size, S_DT(dir->i_mode));
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
		printk(KERN_INFO "Entered ez_find_entry [LS 3]  --- Loading module... Hello World!: %s\n", de->filename);
		if (de->active == 1) {
		printk(KERN_INFO "Entered ez_find_entry [LS 4]  --- Loading module... Hello World!: %s\n", de->filename);
		if (!(memcmp(name, de->filename, namelen))) {
			printk(KERN_INFO "Entered ez_find_entry [LS 5]  --- Loading module... Hello World!: %s\n", name);
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

	printk(KERN_INFO "Entered LOOKUP [LS 2]  --- Loading module... Hello World!\n");

	if (dentry->d_name.len > EZFS_FILENAME_BUF_SIZE)
		return ERR_PTR(-ENAMETOOLONG);

	//	mutex_lock(((struct ezfs_super_block *)fsi->sb_bh)->ezfs_lock);
	bh = ezfs_find_entry(dir, &dentry->d_name, &de);
	if (bh) {
		brelse(bh);
		inode = myez_get_inode(dir->i_sb, de->inode_no);
	}
	//	mutex_unlock(((struct ezfs_super_block *)fsi->sb_bh)->ezfs_lock);

	return d_splice_alias(inode, dentry);
}
static const struct inode_operations myez_dir_inops = {
	//.create		= ramfs_create,
	.lookup		= myez_lookup,
	//.link		= simple_link,
	//.unlink		= simple_unlink,
	//.symlink	= ramfs_symlink,
	//.mkdir		= ramfs_mkdir,
	//.rmdir		= simple_rmdir,
	//.mknod		= ramfs_mknod,
	//.rename		= simple_rename,
};

const struct file_operations myez_dir_operations = {
	.read		= generic_read_dir,
	.iterate_shared	= myez_readdir,
	.fsync		= generic_file_fsync,
	.llseek		= generic_file_llseek,
};

static const struct super_operations myez_sops = {
	//.alloc_inode	= myez_alloc_inode,
	//.free_inode	= myez_free_inode,
	//.write_inode	= myez_write_inode,
	//.evict_inode	= myez_evict_inode,
	//.put_super	= myez_put_super,
	//.statfs		= myez_statfs,
};

struct inode *myez_get_inode(struct super_block *sb,
			     unsigned long ino)
{
	struct inode *inode = iget_locked(sb, ino);
	struct ezfs_inode *di;
	struct buffer_head *bh;
	int off;

	if (!inode)
		return ERR_PTR(-ENOMEM);

	if (!(inode->i_state & I_NEW))
		return inode;
		
	
	bh = sb_bread(inode->i_sb, EZFS_INODE_STORE_DATABLOCK_NUMBER);
	if (!bh) {
		iget_failed(inode);
		return ERR_PTR(-EIO);
	}
	off = ino - EZFS_ROOT_INODE_NUMBER;
	di = (struct ezfs_inode *)bh->b_data + off;//(off * sizeof(struct ezfs_inode));

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
	brelse(bh);

	return inode;
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

	s->s_time_min = 0;
	s->s_time_max = U32_MAX;

	printk(KERN_INFO "Entered Super  --- Loading module... Hello World!\n");
	sb_set_blocksize(s, MYEZ_BLOCK_SIZE);

	sbh = sb_bread(s, 0);
	
	if (!sbh)
		goto out;

	fsi->sb_bh = sbh;	

	sbh2 = sb_bread(s, 1);
	fsi->i_store_bh = sbh2;

	printk(KERN_INFO "super sbh is done  --- Loading module... Hello World!\n");
	if (!sbh2)
		goto out;

	s->s_magic = EZFS_MAGIC_NUMBER;
	s->s_op = &myez_sops;
	
	inode = myez_get_inode(s, 1);

	printk(KERN_INFO "super sbh2 is done  --- Loading module... Hello World!\n");
	if (IS_ERR(inode))
		return PTR_ERR(inode);

	e_ino = (struct ezfs_inode *) sbh2->b_data;
	g_mode = e_ino->mode;
	temp_e_ino = e_ino;
	if (inode->i_state & I_NEW) {
		printk(KERN_INFO "inode NEW  --- Loading module... Hello World!\n");
		

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

	printk(KERN_INFO "inode is done  --- Loading module... Hello World!\n");
	s->s_root = d_make_root(inode);
	if (!s->s_root)
		return -ENOMEM;

	printk(KERN_INFO "Made Root Super completed --- Loading module... Hello World!\n");
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
	return ret;
	
}
static int myez_get_tree(struct fs_context *fc)
{
	int ret = -ENOPROTOOPT;

	ret = get_tree_bdev(fc, myez_fill_super);
	return ret;
}

static void myez_free_fc(struct fs_context *fc)
{
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
	
	printk(KERN_INFO "Init  --- Loading module... Hello World!\n");
	//sb_bread(, 0);
	fc->ops = &myez_context_ops;
	return 0;
}

void myez_kill_sb(struct super_block *sb)
{
	struct ezfs_sb_buffer_heads *fsi = sb->s_fs_info;

	brelse(fsi->sb_bh);
	brelse(fsi->i_store_bh);
	kfree(sb->s_fs_info);
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
	printk(KERN_INFO "Loading module... Hello World!\n");
	return register_filesystem(&myez_fs_type);
	
}

void exit_myez_fs(void)
{
	printk(KERN_INFO "Removing module... Goodbye World!\n");
	unregister_filesystem(&myez_fs_type);
}



module_init(init_myez_fs);
module_exit(exit_myez_fs);

MODULE_DESCRIPTION("A basic Hello World module");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Team27");
