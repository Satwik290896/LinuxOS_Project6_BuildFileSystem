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
	{ 0xd7914529, "generic_file_fsync" },
	{ 0x211540f2, "generic_read_dir" },
	{ 0x5fb4af97, "generic_file_llseek" },
	{ 0x64d5ddb8, "unregister_filesystem" },
	{ 0x1f82d21a, "register_filesystem" },
	{ 0x31549b2a, "__x86_indirect_thunk_r10" },
	{ 0x84264ced, "fs_umode_to_ftype" },
	{ 0xcbd4898c, "fortify_panic" },
	{ 0xa916b694, "strnlen" },
	{ 0x4c236f6f, "__x86_indirect_thunk_r15" },
	{ 0x83fc992e, "pv_ops" },
	{ 0xba8fbd64, "_raw_spin_lock" },
	{ 0x65487097, "__x86_indirect_thunk_rax" },
	{ 0x5c599162, "kmem_cache_alloc_trace" },
	{ 0xd1ed3dd3, "kmalloc_caches" },
	{ 0x981b9b41, "kill_block_super" },
	{ 0x37a0cba, "kfree" },
	{ 0x12971fcb, "d_make_root" },
	{ 0xbbfade79, "set_nlink" },
	{ 0xe061d924, "make_kgid" },
	{ 0xa5ece68a, "make_kuid" },
	{ 0xb5b8b809, "sb_set_blocksize" },
	{ 0x5b8239ca, "__x86_return_thunk" },
	{ 0x648c07bc, "d_splice_alias" },
	{ 0xe54d17ab, "iget_locked" },
	{ 0x1014c9ad, "__brelse" },
	{ 0x449ad0a7, "memcmp" },
	{ 0xab8a1e1b, "__bread_gfp" },
	{ 0xc5850110, "printk" },
	{ 0x330c317d, "get_tree_bdev" },
	{ 0xbdfb6dbb, "__fentry__" },
};

MODULE_INFO(depends, "");

