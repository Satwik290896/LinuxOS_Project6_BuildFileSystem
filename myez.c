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



static const struct address_space_operations myez_aops = {
	//.readpage	= simple_readpage,
	//.write_begin	= simple_write_begin,
	//.write_end	= simple_write_end,
	//.set_page_dirty	= __set_page_dirty_no_writeback,
};

static const struct inode_operations myez_file_inode_operations = {
	//.setattr	= simple_setattr,
	//.getattr	= simple_getattr,
	
};

static const struct file_operations myez_file_operations = {
	//.read_iter	= generic_file_read_iter,
	//.write_iter	= generic_file_write_iter,
	//.mmap		= generic_file_mmap,
	//.fsync		= noop_fsync,
	//.splice_read	= generic_file_splice_read,
	//.splice_write	= iter_file_splice_write,
	//.llseek		= generic_file_llseek,
	//.get_unmapped_area	= ramfs_mmu_get_unmapped_area,
};

static int myez_readdir(struct file *f, struct dir_context *ctx)
{
	struct inode *dir = file_inode(f);
	struct ezfs_inode *e_inode = dir->i_private;
	struct buffer_head *bh;
	struct ezfs_dir_entry *de;
	unsigned int offset;
	int block;

	long long pos;
	
	printk(KERN_INFO "Entered ReadDir [LS]  --- Loading module... Hello World!\n");
	if (ctx->pos & (4096 - 1)) {
		return -EINVAL;
	}
	
	if (!dir_emit_dots(f, ctx))
		return 0;
		

	pos = ctx->pos - 2;
	
	printk(KERN_INFO "Entered ReadDir [LS 2]  --- Loading module... Hello World!\n %lld", ctx->pos);
	//while (ctx->pos < dir->i_size) {
		
	block = e_inode->data_block_number;
		
	bh = sb_bread(dir->i_sb, block);
	if (!bh) {
		return 0;
	}

	offset = 0;
	while (pos < 4096 / sizeof(struct ezfs_dir_entry)) {
		de = (struct ezfs_dir_entry *)(bh->b_data + pos);
			
		if (de->inode_no) {
			printk(KERN_INFO "Entered Read DirEmit 1  --- Loading module... Hello World!\n %lld", ctx->pos);
			int size = strnlen(de->filename, EZFS_FILENAME_BUF_SIZE);
				
			if (!dir_emit(ctx, de->filename, size,
					de->inode_no, DT_UNKNOWN)) {
				brelse(bh);
				return 0;
			}
		}
	
		offset += 1;
		ctx->pos += 1;
		pos = ctx->pos - 2;
	}
	brelse(bh);
	
	return 0;
}

static struct dentry *myez_lookup(struct inode *dir, struct dentry *dentry,
						unsigned int flags)
{
	printk(KERN_INFO "Entered LOOKUP [LS 2]  --- Loading module... Hello World!\n");
	return NULL;
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
	struct inode * inode = iget_locked(sb, ino);

	if (!inode)
		return ERR_PTR(-ENOMEM);
	
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

	if (inode->i_state & I_NEW) {
		printk(KERN_INFO "inode NEW  --- Loading module... Hello World!\n");
		e_ino = (struct ezfs_inode *) sbh2->b_data;

		inode->i_mode = e_ino->mode;
		i_uid_write(inode, e_ino->uid);
		i_gid_write(inode, e_ino->gid);
		set_nlink(inode, e_ino->nlink);
		inode->i_size = 4096;
		inode->i_blocks = e_ino->nblocks;
		inode->i_atime = e_ino->i_atime;
		inode->i_mtime = e_ino->i_mtime;
		inode->i_ctime = e_ino->i_ctime;
		inode->i_private = e_ino;

		inode->i_op = &myez_dir_inops;
		inode->i_fop = &myez_dir_operations;
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
