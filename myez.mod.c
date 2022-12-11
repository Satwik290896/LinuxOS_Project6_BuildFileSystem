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
	{ 0xc5cca43f, "generic_write_end" },
	{ 0x706fcede, "generic_file_splice_read" },
	{ 0xc861e61e, "generic_file_mmap" },
	{ 0xb65f8f05, "generic_file_write_iter" },
	{ 0x4bc8c7d8, "generic_file_read_iter" },
	{ 0xd7914529, "generic_file_fsync" },
	{ 0x211540f2, "generic_read_dir" },
	{ 0x5fb4af97, "generic_file_llseek" },
	{ 0x64d5ddb8, "unregister_filesystem" },
	{ 0x1f82d21a, "register_filesystem" },
	{ 0x12971fcb, "d_make_root" },
	{ 0xb5b8b809, "sb_set_blocksize" },
	{ 0x977f511b, "__mutex_init" },
	{ 0x648c07bc, "d_splice_alias" },
	{ 0x5ac4ad96, "iget_failed" },
	{ 0xe061d924, "make_kgid" },
	{ 0xa5ece68a, "make_kuid" },
	{ 0x9001d2de, "__bforget" },
	{ 0x69acdf38, "memcpy" },
	{ 0x9534ef77, "__getblk_gfp" },
	{ 0x887c42b, "sync_dirty_buffer" },
	{ 0xa0becaa9, "from_kgid" },
	{ 0xa5cd7345, "from_kuid" },
	{ 0xcbd4898c, "fortify_panic" },
	{ 0xa916b694, "strnlen" },
	{ 0x4c236f6f, "__x86_indirect_thunk_r15" },
	{ 0x83fc992e, "pv_ops" },
	{ 0xba8fbd64, "_raw_spin_lock" },
	{ 0x65487097, "__x86_indirect_thunk_rax" },
	{ 0xc833a065, "truncate_pagecache" },
	{ 0xf6007e82, "block_write_begin" },
	{ 0x542ff677, "iput" },
	{ 0x6239ab22, "d_instantiate" },
	{ 0x3f6fc3ba, "inode_init_owner" },
	{ 0xe54d17ab, "iget_locked" },
	{ 0x409bcb62, "mutex_unlock" },
	{ 0xbe3c88fc, "mark_buffer_dirty" },
	{ 0x2ab7989d, "mutex_lock" },
	{ 0x646830a5, "clear_inode" },
	{ 0xaef216cd, "truncate_inode_pages_final" },
	{ 0x5c599162, "kmem_cache_alloc_trace" },
	{ 0xd1ed3dd3, "kmalloc_caches" },
	{ 0x981b9b41, "kill_block_super" },
	{ 0x37a0cba, "kfree" },
	{ 0x330c317d, "get_tree_bdev" },
	{ 0xc959d152, "__stack_chk_fail" },
	{ 0x887a434, "drop_nlink" },
	{ 0x6f36a382, "__mark_inode_dirty" },
	{ 0x4cc71d73, "current_time" },
	{ 0x480ea7ae, "mark_buffer_dirty_inode" },
	{ 0xbbfade79, "set_nlink" },
	{ 0x1014c9ad, "__brelse" },
	{ 0x449ad0a7, "memcmp" },
	{ 0xab8a1e1b, "__bread_gfp" },
	{ 0xd5669e9d, "block_write_full_page" },
	{ 0xea6f53aa, "block_read_full_page" },
	{ 0xae6d286a, "generic_block_bmap" },
	{ 0xc5850110, "printk" },
	{ 0x5b8239ca, "__x86_return_thunk" },
	{ 0xbdfb6dbb, "__fentry__" },
};

MODULE_INFO(depends, "");

