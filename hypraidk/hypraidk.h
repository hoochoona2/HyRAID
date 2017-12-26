#ifndef _HYPRAIDK_H_
#define _HYPRAIDK_H_

#define SECTS_PER_BLK 8

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/blkdev.h>

#include <linux/syscalls.h>
#include <asm/uaccess.h>

#include "hypraid_ioctl.h"

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

#ifndef MAX
#define MAX(a, b) a<b?b:a
#define MIN(a, b) a<b?a:b
#endif

extern struct block_device *g_bdev;
extern struct gendisk *g_disk;
extern make_request_fn *g_make_request_fn;
extern int g_access_area;
extern struct request_queue *g_queue;

#define BLOCK_AREA_INIT(start, end) (struct block_area){{0, }, start, end}
#define BLOCK_AREAS_NUM(blk_areas) blk_areas->area_num
#define BLOCK_AREAS(area_num) &g_blk_areas[area_num]

void hypraid_open_ioctl(struct gendisk *disk);
void hypraid_close_ioctl(struct gendisk *disk);

void hypraid_open_bio_collector(struct gendisk *disk);
void hypraid_close_bio_collector(struct gendisk *disk);

struct file* open(const char *path, int flags, int rights);
void close(struct file *file);
int read(struct file *file, unsigned char *buf, unsigned int size);
int write(struct file *file, unsigned char *buf, unsigned int size);
int lseek(struct file *file, off_t offset, int whence);
int sync(struct file *file);

#define fscanf(file, format, ...) ({                            \
        int len = 0;                                            \
        _fscanf(file, &len, format "%n", __VA_ARGS__, &len);})

int _fscanf(struct file *file, int *len, const char *format, ...);
int fprintf(struct file *file, const char *format, ...);

struct hypraid_configure
{
    int area_blocks[3];
};

typedef int hypraidk_block_replacement_f(int access_area, int blk_cnt);

struct block_area
{
    struct rb_node rb;
    int start;
    int end;
};

struct area_node
{
    struct list_head list;
    struct available_node *parent;
    struct block_area ori_area;
    struct block_area rep_area;
    int blk_cnt;
};

struct available_node
{
    struct list_head list;
    struct rb_node rb;
    struct list_head areas;
    int avail_value;
};

struct block_areas
{
    int area_num;
    struct list_head avails;
    struct rb_root avail_tree;

    struct rb_root area_tree;

    struct list_head pri_areas;
    struct rb_root pri_area_tree;

    struct rb_root *rep_area_tree;

    hypraidk_block_replacement_f *up_rep_f;
    hypraidk_block_replacement_f *down_rep_f;
};

int hypraid_read_configure(struct hypraid_configure *hyconf);

int hypraid_open_bitmap(int area_blocks[3]);
void hypraid_close_bitmap(void);
int hypraid_get_bitmap_area(int blk);
int hypraid_get_bitmap_area_max(int area_num);
int hypraid_is_bitmap_free(int size);
int hypraid_is_area_bitmap_free(int area_num, int size);
int hypraid_get_bitmap(int blk);
int hypraid_set_bitmap(int blk);
int hypraid_clear_bitmap(int blk);

int hypraid_insert_block_area(int access_area, int start_blk, int end_blk);
int hypraid_delete_block_area(int start_blk, int end_blk);
//void hypraid_update_block_area(int start_blk, int end_blk);
int hypraid_get_block_area_priority(int area_num, int cnt, int *ret);
void hypraid_close_priority(void);
int hypraid_open_block_area(int area_blocks[3]);

int hypraid_block_replacement_test(int srcblk, int dstblk);
int hypraid_block_replacement_area(int srcblk, int dstblk, int cnt);
void hypraid_block_area_print(void);


int hypraid_insert_block_area_no_rearray(int start_blk, int end_blk);
int priority_area_search_at(int access_area, int blk_num, struct area_node **ret);
int replacement_area_search_at(int access_area, int blk_num, struct area_node **ret);

void print_bio_page_hex_dump(char *label, struct bio *bio);
void bio_print(const char *label, struct bio *bio);

#endif
