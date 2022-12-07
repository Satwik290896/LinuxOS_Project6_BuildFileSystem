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


/* BFS superblock layout on disk */
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

static const struct super_operations myez_sops = {
	/*.alloc_inode	= myez_alloc_inode,
	.free_inode	= myez_free_inode,
	.write_inode	= myez_write_inode,
	.evict_inode	= myez_evict_inode,
	.put_super	= myez_put_super,
	.statfs		= myez_statfs,*/
};

static const struct address_space_operations myez_aops = {
	.readpage	= simple_readpage,
	.write_begin	= simple_write_begin,
	.write_end	= simple_write_end,
	//.set_page_dirty	= __set_page_dirty_no_writeback,
};

static const struct inode_operations myez_file_inode_operations = {
	.setattr	= simple_setattr,
	.getattr	= simple_getattr,
};


static const struct file_operations myez_file_operations = {
	.read_iter	= generic_file_read_iter,
	.write_iter	= generic_file_write_iter,
	.mmap		= generic_file_mmap,
	.fsync		= noop_fsync,
	.splice_read	= generic_file_splice_read,
	.splice_write	= iter_file_splice_write,
	.llseek		= generic_file_llseek,
	//.get_unmapped_area	= ramfs_mmu_get_unmapped_area,
};

static const struct inode_operations myez_dir_inode_operations = {
	/*.create		= ramfs_create,
	.lookup		= simple_lookup,
	.link		= simple_link,
	.unlink		= simple_unlink,
	.symlink	= ramfs_symlink,
	.mkdir		= ramfs_mkdir,
	.rmdir		= simple_rmdir,
	.mknod		= ramfs_mknod,
	.rename		= simple_rename,*/
};

struct inode *myez_get_inode(struct super_block *sb,
				const struct inode *dir, umode_t mode, dev_t dev)
{
	struct inode * inode = new_inode(sb);

	if (inode) {
		inode->i_ino = get_next_ino();
		inode_init_owner(inode, dir, mode);
		inode->i_mapping->a_ops = &myez_aops;
		mapping_set_gfp_mask(inode->i_mapping, GFP_HIGHUSER);
		mapping_set_unevictable(inode->i_mapping);
		inode->i_atime = inode->i_mtime = inode->i_ctime = current_time(inode);
		switch (mode & S_IFMT) {
		default:
			init_special_inode(inode, mode, dev);
			break;
		case S_IFREG:
			inode->i_op = &myez_file_inode_operations;
			inode->i_fop = &myez_file_operations;
			break;
		case S_IFDIR:
			inode->i_op = &myez_dir_inode_operations;
			inode->i_fop = &simple_dir_operations;

			/* directory inodes start off with i_nlink == 2 (for "." entry) */
			inc_nlink(inode);
			break;
		}
	}
	return inode;
}

static int myez_fill_super(struct super_block *s, struct fs_context *fc)
{
	//struct myez_fs_info *fsi = s->s_fs_info;
	struct buffer_head *bh, *sbh;
	struct myez_super_block *myez_sb;
	struct myez_sb_info *info;
	struct inode *inode;
	int ret = -EINVAL;

	/*info = kzalloc(sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;
	mutex_init(&info->myez_lock);
	*/
	//s->s_fs_info = info;
	s->s_time_min = 0;
	s->s_time_max = U32_MAX;

	sb_set_blocksize(s, MYEZ_BLOCK_SIZE);

	sbh = sb_bread(s, 0);
	
	if (!sbh)
		goto out;
	
	myez_sb = (struct myez_super_block *)sbh->b_data;
	if (le32_to_cpu(myez_sb->s_magic) != EZFS_MAGIC_NUMBER) {
		goto out1;
	}


	s->s_magic = EZFS_MAGIC_NUMBER;
	s->s_op = &myez_sops;
	
	inode = myez_get_inode(s, NULL, S_IFDIR, 0);
	s->s_root = d_make_root(inode);
	if (!s->s_root)
		return -ENOMEM;

	brelse(sbh);
	//mutex_destroy(&info->myez_lock);
	return 0;
	
out2:
	dput(s->s_root);
	s->s_root = NULL;
out1:
	brelse(sbh);
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


static const struct fs_context_operations myez_context_ops = {
	//.free		= myez_free_fc,
	//.parse_param	= myez_parse_param,
	.get_tree	= myez_get_tree,
};







int myez_init_fs_context(struct fs_context *fc)
{

	fc->ops = &myez_context_ops;
	return 0;
}

void myez_kill_sb(struct super_block *sb)
{
	kfree(sb->s_fs_info);
	kill_litter_super(sb);
}


static struct file_system_type myez_fs_type = {
	.name		= "myez",
	.init_fs_context = myez_init_fs_context,
	//.parameters	= myez_fs_parameters,
	/*.mount		= myez_mount,*/
	.kill_sb	= myez_kill_sb,
	.fs_flags	= FS_REQUIRES_DEV,
};






static int __init init_myez_fs(void)
{
	int err; //= init_inodecache();
	//if (err)
	//	goto out1;
	err = register_filesystem(&myez_fs_type);
	if (err)
		goto out;
	return 0;
out:
	//destroy_inodecache();
out1:
	return err;
}

static void __exit exit_myez_fs(void)
{
	unregister_filesystem(&myez_fs_type);
	//destroy_inodecache();
}



module_init(init_myez_fs)
module_exit(exit_myez_fs)
MODULE_LICENSE("GPL");

