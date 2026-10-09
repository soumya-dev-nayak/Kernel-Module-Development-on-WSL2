#include <linux/init.h>      /* __init / __exit macros */
#include <linux/module.h>    /* module_init, module_exit, MODULE_* macros */
#include <linux/kernel.h>    /* printk, KERN_* levels */

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Soumya Dev Nayak");
MODULE_DESCRIPTION("A simple Hello World kernel module for learning");
MODULE_VERSION("1.0");

static int __init hello_init(void)
{
    printk(KERN_INFO "hello: module loaded - Hello from the kernel!\n");
    return 0;   /* 0 = success. Non-zero = module load fails */
}

static void __exit hello_exit(void)
{
    printk(KERN_INFO "hello: module unloaded - Goodbye from the kernel!\n");
}

module_init(hello_init);
module_exit(hello_exit);

