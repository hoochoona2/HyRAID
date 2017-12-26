#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/blkdev.h>
#include <linux/hashtable.h>
#include "hypraidk.h"

make_request_fn *g_make_request_fn = NULL;
int g_access_area = 0;
int g_mode = 2;

struct area_node;

void re_request_bio_print(int access_area, int start_blk, int end_blk);
void re_request_bio(struct request_queue *q, struct bio *bio);
void bio_print(const char *label, struct bio *bio);

static void hypraid_bio_collect(struct request_queue *q, struct bio *bio)
{
    int access_area = g_access_area;
    int start_blk = bio->bi_iter.bi_sector >> 3;
    int end_blk = (bio_end_sector(bio) >> 3) - 1;
    char rw[30] = {0, };

    if(unlikely(!g_queue))
    {
        g_queue = q;
    }

    /*
    g_make_request_fn(q, bio);

    if(READ == bio_rw(bio))
        print_bio_page_hex_dump("READ bio: ", bio);
    else if(READA == bio_rw(bio))
        print_bio_page_hex_dump("READA bio: ", bio);

    return;
    */

    //re_request_bio_print(access_area, start_blk, end_blk);
    if(unlikely(REQ_DISCARD == (REQ_DISCARD & bio->bi_rw) || end_blk < start_blk))//!bio_has_data(bio)))// || end_blk < start_blk)
    {
        g_make_request_fn(q, bio);

        return;
    }

    //hypraid_insert_block_area_no_rearray(start_blk, end_blk);
    hypraid_insert_block_area(access_area, start_blk, end_blk);
    re_request_bio(q, bio);


    /*
    if(WRITE_FLUSH_FUA == (WRITE_FLUSH_FUA & bio->bi_rw))
        sprintf(rw, "WRITE_FLUSH_FUA");
    else if(WRITE_FUA == (WRITE_FUA & bio->bi_rw))
        sprintf(rw, "WRITE_FUA");
    else if(WRITE_FLUSH == (WRITE_FLUSH & bio->bi_rw))
        sprintf(rw, "WRITE_FLUSH");
    else if(WRITE_SYNC == (WRITE_SYNC & bio->bi_rw))
        sprintf(rw, "WRITE_SYNC");
    else if(WRITE_ODIRECT == (WRITE_ODIRECT & bio->bi_rw))
        sprintf(rw, "WRITE_ODIRECT");
    else if(WRITE == bio->bi_rw)
        sprintf(rw, "WRITE");
    else if(READ_SYNC == (READ_SYNC & bio->bi_rw))
        sprintf(rw, "READ_SYNC");
    else if(READA == (READA & bio->bi_rw))
        sprintf(rw, "READA");
    else if(READ == bio->bi_rw)
        sprintf(rw, "READ");
    else
        sprintf(rw, "UNKNOWN(%lu)", bio->bi_rw);

    printk(KERN_INFO "[type:%s][type value:%lu][start sector:%lu]"
            "[end sector:%lu][sector cnt:%u]\n",
            rw, bio->bi_rw,
            bio->bi_iter.bi_sector,
            bio_end_sector(bio),
            bio_sectors(bio));
            */
}

void hypraid_open_bio_collector(struct gendisk *disk)
{
    g_make_request_fn = disk->queue->make_request_fn;
    disk->queue->make_request_fn = hypraid_bio_collect;

}

void hypraid_close_bio_collector(struct gendisk *disk)
{
    disk->queue->make_request_fn = g_make_request_fn;
}
