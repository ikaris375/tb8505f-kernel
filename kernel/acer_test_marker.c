#include <linux/init.h>
#include <linux/kernel.h>

static int __init acer_test_marker_init(void)
{
	pr_emerg("=== ACER NT35521 PSTORE DEBUG BUILD 2026-08-13 ===\n");
	return 0;
}

early_initcall(acer_test_marker_init);
