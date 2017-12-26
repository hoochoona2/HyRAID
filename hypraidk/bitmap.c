#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/blkdev.h>
#include <linux/hashtable.h>
#include "hypraidk.h"

struct bitmap
{
    unsigned char *map;
    int sz_map;
    unsigned int max_bit;
    unsigned int free;
    unsigned int bit_cnt;
};

static int bitmap_open(struct bitmap *bitmap, const unsigned int max_bit)
{
    *bitmap = (struct bitmap)
    {
        .sz_map = max_bit >> 3,
        .map = (unsigned char*)vzalloc(max_bit >> 3),
        .free = max_bit,
        .max_bit = max_bit,
        .bit_cnt = 0,
    };

    if(NULL == bitmap->map)
        return 0;

    return 1;
}

static void bitmap_close(struct bitmap *bitmap)
{
    vfree(bitmap->map);
}

static int is_bitmap_free(struct bitmap *bitmap, const int size)
{
    return bitmap->free - size;
}

static int get_bitmap(struct bitmap *bitmap, const unsigned b)
{
    int index = b >> 3;
    int offset = 1 << (b & 7);

    return bitmap->map[index] & offset;
}

static int set_bitmap(struct bitmap *bitmap, const unsigned b)
{
    int index = b >> 3;
    int offset = 1 << (b & 7);

    if(bitmap->free <= 0)
        return 0;

    if(bitmap->map[index] & offset)
        return 0;

    bitmap->map[index] = bitmap->map[index] | offset;
    bitmap->free--;
    bitmap->bit_cnt++;

    return 1;
}

static int clear_bitmap(struct bitmap *bitmap, const unsigned b)
{
    int index = b >> 3;
    int offset = 1 << (b & 7);

    if(!(bitmap->map[index] & offset))
        return 0;

    bitmap->map[index] = bitmap->map[index] & ~offset;
    bitmap->free++;
    bitmap->bit_cnt--;

    return 1;
}

struct hypraid_bitmap
{
    struct bitmap bitmap;
    struct bitmap area[3];
};

static struct hypraid_bitmap g_bitmap;

int hypraid_open_bitmap(int area_blocks[3])
{
    int i;
    int total_blocks = 0;

    for(i=0; i<3; i++)
        total_blocks += area_blocks[i];

    if(!bitmap_open(&g_bitmap.bitmap, total_blocks))
        return 0;

    for(i=0; i<3; i++)
    {
        g_bitmap.area[i].sz_map = area_blocks[i] >> 3;
        g_bitmap.area[i].map = g_bitmap.bitmap.map;
        g_bitmap.area[i].free = area_blocks[i];
        g_bitmap.area[i].max_bit = area_blocks[i];
        g_bitmap.area[i].bit_cnt = 0;
    }

    return 1;
}

void hypraid_close_bitmap(void)
{
    bitmap_close(&g_bitmap.bitmap);
}

int hypraid_get_bitmap_area(int blk)
{
    int i;
    int bound = 0;

    if(blk < 0)
        return -1;

    for(i=0; i<3; i++)
    {
        bound += g_bitmap.area[i].max_bit;

        if(blk < bound)
            return i;
    }

    return -1;
}

int hypraid_get_bitmap_area_max(int area_num)
{
    if(0 < area_num || 2 < area_num)
        return 0;

    return g_bitmap.area[area_num].max_bit;
}

int hypraid_is_bitmap_free(int size)
{
    return is_bitmap_free(&g_bitmap.bitmap, size);
}

int hypraid_is_area_bitmap_free(int area_num, int size)
{
    return is_bitmap_free(&g_bitmap.area[area_num], size);
}

int hypraid_get_bitmap(int blk)
{
    return get_bitmap(&g_bitmap.bitmap, blk);
}

int hypraid_set_bitmap(int blk)
{
    if(set_bitmap(&g_bitmap.bitmap, blk))
    {
        int area_num = hypraid_get_bitmap_area(blk);

        g_bitmap.area[area_num].free--;
        g_bitmap.area[area_num].bit_cnt++;

        return 1;
    }

    return 0;
}

int hypraid_clear_bitmap(int blk)
{
    if(clear_bitmap(&g_bitmap.bitmap, blk))
    {
        int area_num = hypraid_get_bitmap_area(blk);

        g_bitmap.area[area_num].free++;
        g_bitmap.area[area_num].bit_cnt--;
    }

    return 0;
}
