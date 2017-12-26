#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/blkdev.h>
#include <linux/hashtable.h>
#include "hypraidk.h"

#define HYPRAID_CONF_PATH "/etc/hypraid/dm.conf"

int hypraid_read_configure(struct hypraid_configure *hyconf)
{
    struct file *fp = NULL;
    int i;

    fp = open(HYPRAID_CONF_PATH, O_RDONLY, 0);
    if(NULL == fp)
    {
        printk(KERN_INFO "not found hypraid configure file\n");
        return 0;
    }

    for(i=0; i<3; i++)
    {
        int blocks;
        fscanf(fp, "%d", &blocks);
        hyconf->area_blocks[i] = blocks >> 3;
    }

    for(i=0; i<3; i++)
        printk(KERN_INFO "%d ", hyconf->area_blocks[i]);
    printk(KERN_INFO "\n");

    close(fp);

    return 1;
}
