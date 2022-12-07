#include <linux/module.h>
#define INCLUDE_VERMAGIC
#include <linux/build-salt.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

BUILD_SALT;

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

#ifdef CONFIG_RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif

static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0xf1fbc9ab, "module_layout" },
	{ 0x474d721a, "simple_write_end" },
	{ 0xf7e1ec7e, "simple_write_begin" },
	{ 0xc7be62ae, "simple_readpage" },
	{ 0x831d168e, "simple_getattr" },
	{ 0x8e56aa58, "simple_setattr" },
	{ 0x706fcede, "generic_file_splice_read" },
	{ 0x4ddb1b90, "iter_file_splice_write" },
	{ 0x3ef0127a, "noop_fsync" },
	{ 0xc861e61e, "generic_file_mmap" },
	{ 0xb65f8f05, "generic_file_write_iter" },
	{ 0x4bc8c7d8, "generic_file_read_iter" },
	{ 0x5fb4af97, "generic_file_llseek" },
	{ 0x64d5ddb8, "unregister_filesystem" },
	{ 0x1f82d21a, "register_filesystem" },
	{ 0x96c377b8, "inc_nlink" },
	{ 0xa1fe1a9d, "simple_dir_operations" },
	{ 0xd8208950, "init_special_inode" },
	{ 0x4cc71d73, "current_time" },
	{ 0x3f6fc3ba, "inode_init_owner" },
	{ 0xe953b21f, "get_next_ino" },
	{ 0x98ec77a1, "new_inode" },
	{ 0xc5850110, "printk" },
	{ 0x5c599162, "kmem_cache_alloc_trace" },
	{ 0xd1ed3dd3, "kmalloc_caches" },
	{ 0xd74284fd, "kill_litter_super" },
	{ 0x1014c9ad, "__brelse" },
	{ 0x37a0cba, "kfree" },
	{ 0x5b8239ca, "__x86_return_thunk" },
	{ 0x12971fcb, "d_make_root" },
	{ 0xbbfade79, "set_nlink" },
	{ 0xe061d924, "make_kgid" },
	{ 0xa5ece68a, "make_kuid" },
	{ 0xe54d17ab, "iget_locked" },
	{ 0xab8a1e1b, "__bread_gfp" },
	{ 0xb5b8b809, "sb_set_blocksize" },
	{ 0x330c317d, "get_tree_bdev" },
	{ 0xbdfb6dbb, "__fentry__" },
};

MODULE_INFO(depends, "");

