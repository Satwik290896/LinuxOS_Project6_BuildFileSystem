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
#include <ez_ops.h>


/* Block Size - 4096 */
#define MYEZ_BSIZE_BITS		12
#define MYEZ_BLOCK_SIZE		(1<<BFS_BSIZE_BITS)

#define MYEZ_MAGIC		0x1BADFACF


struct myez_sb_info {
	unsigned long si_blocks;
	unsigned long si_freeb;
	unsigned long si_freei;
	unsigned long si_lf_eblk;
	unsigned long si_lasti;
	DECLARE_BITMAP(si_imap, MYEZ_MAX_LASTI+1);
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
	.alloc_inode	= myez_alloc_inode,
	.free_inode	= myez_free_inode,
	.write_inode	= myez_write_inode,
	.evict_inode	= myez_evict_inode,
	.put_super	= myez_put_super,
	.statfs		= myez_statfs,
};

static const struct fs_context_operations myez_context_ops = {
	.free		= myez_free_fc,
	.parse_param	= myez_parse_param,
	.get_tree	= myez_get_tree,
};



static int myez_fill_super(struct super_block *s, void *data, int silent)
{
	struct buffer_head *bh, *sbh;
	struct myez_super_block *myez_sb;
	struct myez_sb_info *info;
	struct inode *inode;
	int ret = -EINVAL;

	info = kzalloc(sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;
	mutex_init(&info->myez_lock);
	s->s_fs_info = info;
	s->s_time_min = 0;
	s->s_time_max = U32_MAX;

	sb_set_blocksize(s, MYEZ_BLOCK_SIZE);

	sbh = sb_bread(s, 0);
	
	if (!sbh)
		goto out;
	myez_sb = (struct myez_super_block *)sbh->b_data;
	if (le32_to_cpu(bfs_sb->s_magic) != MYEZ_MAGIC) {
		goto out1;
	}


	s->s_magic = MYEZ_MAGIC;
	s->s_op = &myez_sops;
	
	brelse(sbh);
	mutex_destroy(&info->myez_lock);
	return 0;
	
out2:
	dput(s->s_root);
	s->s_root = NULL;
out1:
	brelse(sbh);
out:
	mutex_destroy(&info->myez_lock);
	kfree(info);
	s->s_fs_info = NULL;
	return ret;
	
}

static struct dentry *myez_mount(struct file_system_type *fs_type,
	int flags, const char *dev_name, void *data)
{
	return mount_bdev(fs_type, flags, dev_name, data, myez_fill_super);
}


void myez_kill_sb(struct super_block *sb)
{
	kfree(sb->s_fs_info);
	kill_litter_super(sb);
}


static struct file_system_type myez_fs_type = {
	.name		= "myez",
	.init_fs_context = myez_init_fs_context,
	.parameters	= myez_fs_parameters,
	.mount		= myez_mount,
	.kill_sb	= myez_kill_sb,
	.fs_flags	= FS_REQUIRES_DEV,
};






static int __init init_myez_fs(void)
{
	int err = init_inodecache();
	if (err)
		goto out1;
	err = register_filesystem(&myez_fs_type);
	if (err)
		goto out;
	return 0;
out:
	destroy_inodecache();
out1:
	return err;
}

static void __exit exit_myez_fs(void)
{
	unregister_filesystem(&myez_fs_type);
	destroy_inodecache();
}



module_init(init_myez_fs)
module_exit(exit_myez_fs)

