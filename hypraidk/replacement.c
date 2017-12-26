#include "hypraidk.h"

void write_end_bio(struct bio *bio, int error)
{
    bio_put(bio);

    printk(KERN_INFO "write_end_bio\n");
}

void read_end_bio(struct bio *bio, int error)
{
    //struct page *page = bio->bi_private;

    //if(!error)
    //    SetPageUptodate(page);

    //bio_put(bio);

    printk(KERN_INFO "read_end_bio\n");
    //unlock_page(page);
}


