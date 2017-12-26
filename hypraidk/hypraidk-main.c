#include "hypraidk.h"

#define DRIVER_AUTHOR "hoochoona"
#define DRIVER_DESC "hypraid device module"

struct gendisk *g_disk = NULL;
static struct hypraid_configure g_conf;
struct block_device *g_bdev = NULL;
struct request_queue *g_queue = NULL;

static int __init blkdev_test_init(void)
{
    int part=0;

    g_disk = get_gendisk(MKDEV(252, 0), &part);

    if(!g_disk)
    {
        printk(KERN_INFO "get_gendisk error\n");

        return 1;
    }

    if(!hypraid_read_configure(&g_conf))
        return 0;

    g_bdev = bdget_disk(g_disk, 0);

    hypraid_open_block_area(g_conf.area_blocks);
    hypraid_open_bitmap(g_conf.area_blocks);
    hypraid_open_bio_collector(g_disk);
    hypraid_open_ioctl(g_disk);

    printk(KERN_INFO "hypraid load success\n");

    return 0;
}

static void __exit blkdev_test_exit(void)
{
    hypraid_close_priority();
    hypraid_close_bitmap();
    hypraid_close_bio_collector(g_disk);
    hypraid_close_ioctl(g_disk);
}

module_init(blkdev_test_init);
module_exit(blkdev_test_exit);

MODULE_LICENSE("GPL");

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);

MODULE_SUPPORTED_DEVICE("block devicce");
