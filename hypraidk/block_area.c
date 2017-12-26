#include "hypraidk.h"
#include <linux/list.h>
#include <linux/rbtree.h>
#include <linux/completion.h>

struct available_node;
struct block_areas;

static struct block_areas g_blk_areas[3];

static struct rb_root g_rep_area_tree = RB_ROOT;

static int hypraid_pop_priority_area(int access_area, int cnt, struct area_node **ret);

static void area_init(struct area_node *area, int start, int end)
{
    *area = (struct area_node)
    {
        .ori_area = BLOCK_AREA_INIT(start, end),
        .rep_area = BLOCK_AREA_INIT(start, end),
        .blk_cnt = end - start + 1,
        .parent = NULL,
    };
}

static struct area_node* area_alloc(int start, int end)
{
    struct area_node *new = vmalloc(sizeof(struct area_node));

    area_init(new, start, end);

    return new;
}

static void area_copy(struct area_node *src, struct area_node *dst)
{
    *dst = (struct area_node)
    {
        .ori_area = BLOCK_AREA_INIT(src->ori_area.start, src->ori_area.end),
        .rep_area = BLOCK_AREA_INIT(src->rep_area.start, src->rep_area.end),
        .blk_cnt = src->blk_cnt,
        .parent = NULL,
    };
}

static struct area_node* area_clone(struct area_node *area)
{
    struct area_node *clone = area_alloc(0, 0);

    area_copy(area, clone);

    return clone;
}

static struct available_node* available_alloc(int avail_value)
{
    struct available_node *new = vmalloc(sizeof(struct available_node*));

    INIT_LIST_HEAD(&new->areas);
    new->avail_value = avail_value;

    return new;
}

static int available_insert(int access_area, struct available_node *data)
{
    struct block_areas *blk_areas = BLOCK_AREAS(access_area);
    struct rb_root *root = &blk_areas->avail_tree;
    struct rb_node **new = &(root->rb_node), *parent = NULL;
    struct available_node *avail = NULL, *avail_tmp;

    while(*new)
    {
        struct available_node *this = container_of(*new, struct available_node, rb);

        parent = *new;

        if(data->avail_value < this->avail_value)
            new = &((*new)->rb_left);
        else if(this->avail_value < data->avail_value)
            new = &((*new)->rb_right);
        else if(this->avail_value == data->avail_value)
            return FALSE;
    }

    rb_link_node(&data->rb, parent, new);
    rb_insert_color(&data->rb, root);

    if(list_empty(&blk_areas->avails))
    {
        list_add(&data->list, &blk_areas->avails);

        return TRUE;
    }

    list_for_each_entry_safe(avail, avail_tmp, &blk_areas->avails, list)
    {
        if(data->avail_value < avail->avail_value)
        {
            list_add_tail(&data->list, &avail->list);

            return TRUE;
        }
    }

    list_add_tail(&data->list, &blk_areas->avails);

    return TRUE;
}

static int available_search(int access_area, int avail_value, struct available_node **ret)
{
    struct block_areas *blk_areas = BLOCK_AREAS(access_area);
    struct rb_root *root = &blk_areas->avail_tree;
    struct rb_node **new = &(root->rb_node), *parent = NULL;

    while(*new)
    {
        struct available_node *this = container_of(*new, struct available_node, rb);

        parent = *new;

        if(avail_value < this->avail_value)
            new = &((*new)->rb_left);
        else if(this->avail_value < avail_value)
            new = &((*new)->rb_right);
        else
        {
            *ret = this;
            return TRUE;
        }
    }

    return FALSE;
}

static void available_extract(int access_area, struct available_node *extract)
{
    struct block_areas *blk_areas = BLOCK_AREAS(access_area);
    struct rb_root *root = &blk_areas->avail_tree;

    list_del(&extract->list);
    rb_erase(&extract->rb, root);
}

static void available_free(struct available_node *avail)
{
    vfree(avail);
} 
static int area_insert(struct rb_root *root, struct block_area *data)
{
    struct rb_node **new = &(root->rb_node), *parent = NULL;

    while(*new)
    {
        struct block_area *this = container_of(*new, struct block_area, rb);

        parent = *new;
        if(data->end < this->start)
        {
            new = &((*new)->rb_left);
        }
        else if(this->end < data->start)
        {
            new = &((*new)->rb_right);
        }
        else if(this->start <= data->start && data->end <= this->end)
        {
            return FALSE;
        }
        else if(data->start <= this->start && this->end <= data->end)
        {
            return FALSE;
        }
        else
        {
            return FALSE;
        }
    }

    rb_link_node(&data->rb, parent, new);
    rb_insert_color(&data->rb, root);

    return TRUE;
}

static int area_search(struct rb_root *root, int start, int end, struct block_area **ret)
{
    struct rb_node *node = root->rb_node;

    while(node)
    {
        struct block_area *this = container_of(node, struct block_area, rb);

        if(end < this->start)
        {
            node = node->rb_left;
        }
        else if(this->end < start)
        {
            node = node->rb_right;
        }
        else if(this->start <= start && end <= this->end)
        {
            *ret = this;
            return 1;
        }
        else if(start <= this->start && this->end <= end)
        {
            *ret = this;
            return 2;
        }
        else
        {
            *ret = this;
            return 3;
        }
    }

    *ret = NULL;

    return 0;
}

static int available_area_search(int access_area, int start, int end, struct area_node **ret)
{
    struct block_areas *blk_areas = BLOCK_AREAS(access_area);
    struct block_area *blk_area;
    int search = 0;

    search = area_search(&blk_areas->area_tree, start, end, &blk_area);
    if(search)
        *ret = container_of(blk_area, struct area_node, rep_area);
    else
        *ret = NULL;

    return search;
}

static int priority_area_insert(int access_area, struct area_node *data)
{
    struct block_areas *blk_areas = BLOCK_AREAS(access_area);

    if(!area_insert(&blk_areas->pri_area_tree, &data->rep_area))
        return FALSE;

    list_add(&data->list, &blk_areas->pri_areas);

    return TRUE;
}

static int priority_area_insert_low(int access_area, struct area_node *data)
{
    struct block_areas *blk_areas = BLOCK_AREAS(access_area);

    if(!area_insert(&blk_areas->pri_area_tree, &data->rep_area))
        return FALSE;

    list_add_tail(&data->list, &blk_areas->pri_areas);

    return TRUE;
}

static int replacement_area_insert(int access_area, struct area_node *data)
{
    struct block_areas *blk_areas = BLOCK_AREAS(access_area);

    return area_insert(blk_areas->rep_area_tree, &data->ori_area);
}

static int priority_area_search(int access_area, int start, int end, struct area_node **ret)
{
    struct block_areas *blk_areas = BLOCK_AREAS(access_area);
    struct block_area *blk_area;
    int search = 0;

    search = area_search(&blk_areas->pri_area_tree, start, end, &blk_area);
    if(search)
        *ret = container_of(blk_area, struct area_node, rep_area);
    else
        *ret = NULL;

    return search;
}

static int replacement_area_search(int access_area, int start, int end, struct area_node **ret)
{
    struct block_areas *blk_areas = BLOCK_AREAS(access_area);
    struct block_area *blk_area;
    int search = 0;

    search = area_search(blk_areas->rep_area_tree, start, end, &blk_area);
    if(search)
        *ret = container_of(blk_area, struct area_node, ori_area);
    else
        *ret = NULL;

    return search;
}

static int area_search_at(struct rb_root *root, int blk_num, struct block_area **ret)
{
    struct rb_node *node = root->rb_node;

    while(node)
    {
        struct block_area *this = container_of(node, struct block_area, rb);

        if(blk_num < this->start)
            node = node->rb_left;
        else if(this->end < blk_num)
            node = node->rb_right;
        else
        {
            *ret = this;
            return TRUE;
        }
    }

    *ret = NULL;

    return FALSE;
}

static int available_area_search_at(int access_area, int blk_num, struct area_node **ret)
{
    struct block_areas *blk_areas = BLOCK_AREAS(access_area);
    struct block_area *blk_area;
    int search = 0;

    search = area_search_at(&blk_areas->area_tree, blk_num, &blk_area);
    if(search)
        *ret = container_of(blk_area, struct area_node, rep_area);
    else
        *ret = NULL;

    return search;
}

int priority_area_search_at(int access_area, int blk_num, struct area_node **ret)
{
    struct block_areas *blk_areas = BLOCK_AREAS(access_area);
    struct block_area *blk_area;
    int search = 0;

    search = area_search_at(&blk_areas->pri_area_tree, blk_num, &blk_area);
    if(search)
        *ret = container_of(blk_area, struct area_node, rep_area);
    else
        *ret = NULL;

    return search;
}

int replacement_area_search_at(int access_area, int blk_num, struct area_node **ret)
{
    struct block_areas *blk_areas = BLOCK_AREAS(access_area);
    struct block_area *blk_area;
    int search = 0;

    search = area_search_at(blk_areas->rep_area_tree, blk_num, &blk_area);
    if(search)
        *ret = container_of(blk_area, struct area_node, ori_area);
    else
        *ret = NULL;

    return search;
}

static void area_extract(struct rb_root *root, struct block_area *extract)
{
    rb_erase(&extract->rb, root);
}

void available_area_extract(int access_area, struct area_node *extract)
{
    struct block_areas* blk_areas = BLOCK_AREAS(access_area);
    struct available_node *parent = extract->parent;

    area_extract(&blk_areas->area_tree, &extract->rep_area);
    list_del(&extract->list);

    if(list_empty(&parent->areas))
    {
        available_extract(access_area, parent);
        available_free(parent);
    }

    extract->parent = NULL;
}

void priority_area_extract(int access_area, struct area_node *extract)
{
    struct block_areas *blk_areas = BLOCK_AREAS(access_area);

    area_extract(&blk_areas->pri_area_tree, &extract->rep_area);
    list_del(&extract->list);
}

void replacement_area_extract(int access_area, struct area_node *extract)
{
    struct block_areas *blk_areas = BLOCK_AREAS(access_area);

    area_extract(blk_areas->rep_area_tree, &extract->ori_area);
}

void _area_free(struct area_node *area)
{
    vfree(area);
}

void available_area_free(struct area_node *area)
{
    _area_free(area);
}

void priority_area_free(struct area_node *area)
{
    _area_free(area);
}

void replacement_area_free(struct area_node *area)
{
    //no work
}

static int available_area_insert(int access_area, struct area_node *area)
{
    struct block_areas *blk_areas = BLOCK_AREAS(access_area);
    struct available_node *parent = NULL;
    struct area_node *front_area = NULL, *rear_area = NULL, *check = NULL;

    //duplication check
    if(available_area_search(access_area, area->rep_area.start, area->rep_area.end, &check))
        return FALSE;

    //merge check
    available_area_search_at(access_area, area->rep_area.start - 1, &front_area);
    available_area_search_at(access_area, area->rep_area.end + 1, &rear_area);

    if(front_area)
    {
        available_area_extract(access_area, front_area);
        area->rep_area.start = front_area->rep_area.start;
        available_area_free(front_area);
    }

    if(rear_area)
    {
        available_area_extract(access_area, rear_area);
        area->rep_area.end = rear_area->rep_area.end;
        available_area_free(rear_area);
    }

    area->ori_area = area->rep_area;
    area->blk_cnt = area->rep_area.end - area->rep_area.start + 1;

    //available area insert
    area_insert(&blk_areas->area_tree, &area->rep_area);

    //mapping available parent
    if(available_search(access_area, area->blk_cnt, &parent))
    {
        area->parent = parent;
        list_add(&area->list, &parent->areas);
    }
    else
    {
        parent = available_alloc(area->blk_cnt);
        area->parent = parent;
        list_add(&area->list, &parent->areas);
        available_insert(access_area, parent);
    }

    return TRUE;
}

void hypraid_block_area_print(void)
{
    int i;

    for(i=0; i<3; i++)
    {
        struct available_node *avail;
        struct area_node *area;
        struct block_areas *blk_areas = &g_blk_areas[i];

        printk(KERN_INFO "--------device group %d----------\n", i);

        printk(KERN_INFO "[[priority areas]]\n");
        list_for_each_entry(area, &blk_areas->pri_areas, list)
        {
            printk(KERN_INFO "[%d, %d] -> [%d, %d]\n", area->rep_area.start, area->rep_area.end, area->ori_area.start, area->ori_area.end);
        }

        printk(KERN_INFO "[[available areas]]\n");
        list_for_each_entry(avail, &blk_areas->avails, list)
        {
            printk(KERN_INFO "[%d]\n", avail->avail_value);
            list_for_each_entry(area, &avail->areas, list)
            {
                printk(KERN_INFO "----[%d, %d]\n", area->rep_area.start, area->rep_area.end);
            }
        }
    }
}

int hypraid_delete_block_area(int start_blk, int end_blk)
{
    int blk = start_blk;
    int access_area = g_access_area;
    struct area_node *area = NULL;
    struct area_node *front_area = NULL, *rear_area = NULL;
    int i;

    if(end_blk < start_blk)
        return 0;

    printk(KERN_INFO "delete_function\n");
    if(!replacement_area_search_at(access_area, blk, &area))
    {
        printk(KERN_INFO "replacement area not found [%d, %d] -> [?, ?]\n", start_blk, end_blk);
        return -1;
    }

    printk(KERN_INFO "replacement area find [%d, %d] -> [%d, %d]\n", start_blk, end_blk, area->rep_area.start, area->rep_area.end);
    blk = area->ori_area.end;

    priority_area_extract(access_area, area);
    replacement_area_extract(access_area, area);

    for(i=0; i<area->blk_cnt; i++)
        hypraid_clear_bitmap(area->rep_area.start + i);

    if(available_area_search_at(access_area, area->rep_area.start - 1, &front_area))
        printk(KERN_INFO "delete find front area\n");
    if(available_area_search_at(access_area, area->rep_area.end + 1, &rear_area))
        printk(KERN_INFO "delete find rear area\n");
    if(front_area)
    {
        available_area_extract(access_area, front_area);
        area->rep_area.start = front_area->rep_area.start;
        available_area_free(front_area);
    }

    if(rear_area)
    {
        available_area_extract(access_area, rear_area);
        area->rep_area.end = rear_area->rep_area.end;
        available_area_free(rear_area);
    }

    area->ori_area = area->rep_area;
    area->blk_cnt = area->rep_area.end - area->rep_area.start + 1;

    available_area_insert(access_area, area);

    return hypraid_delete_block_area(blk+1, end_blk);
}

int hypraid_block_replacement(int access_area, int blk_cnt)
{
    struct block_areas *blk_areas = BLOCK_AREAS(access_area);

    if(blk_areas->up_rep_f)
        blk_areas->up_rep_f(access_area, blk_cnt);

    if(blk_areas->down_rep_f)
        blk_areas->down_rep_f(access_area, blk_cnt);

    return 0;
}

static void hypraid_end_bio_wait(struct bio *bio, int error)
{
    struct completion *event = bio->bi_private;

    printk("%s success\n", bio->bi_rw&WRITE? "Write": "Read");

    complete(event);
}

static void hypraid_end_bio(struct bio *bio, int error)
{
    bio_put(bio);
}

static int hypraid_submit_bio_wait(int rw, struct bio *bio)
{
    struct completion event;

    rw |= REQ_SYNC;
    init_completion(&event);
    bio->bi_rw = rw;
    bio->bi_private = &event;
    bio->bi_end_io = hypraid_end_bio_wait;
    printk("%s request\n", bio->bi_rw&WRITE? "Write": "Read");
    g_make_request_fn(g_queue, bio); // not define queue
    wait_for_completion(&event);

    return 0;
}

static int hypraid_submit_bio(int rw, struct bio *bio)
{
    rw |= REQ_SYNC;
    bio->bi_rw = rw;
    bio->bi_private = bio;
    bio->bi_end_io = hypraid_end_bio;
    printk("%s request\n", bio->bi_rw&WRITE? "Write": "Read");
    g_make_request_fn(g_queue, bio); // not define queue

    return 0;
}

static int hypraid_block_operation_wait(struct page** pages, int rw, int blk, int cnt)
{
    struct bio *bio;
    int i;
    char label[30] = {0, };

    bio = bio_alloc(GFP_NOFS, cnt);
    bio->bi_iter.bi_sector = blk << 3;
    bio->bi_bdev = g_bdev;
    for(i=0; i<cnt; i++)
        bio_add_page(bio, pages[i], PAGE_SIZE, 0);

    bio_get(bio);
    hypraid_submit_bio_wait(rw, bio);
    //g_make_request_fn(g_queue, bio); // not define queue
    //submit_bio_wait(rw, bio);

    sprintf(label, "replacement(%s): ", rw&WRITE?"write":"read");
    print_bio_page_hex_dump(label, bio);

    bio_put(bio);

    return 0;
}

static int hypraid_block_operation(struct page** pages, int rw, int blk, int cnt)
{
    struct bio *bio;
    int i;
    //
    struct bio_vec bvec;
    struct bvec_iter iter;
    char label[30] = {0, };
    //

    bio = bio_alloc(GFP_NOFS, cnt);
    bio->bi_iter.bi_sector = blk << 3;
    bio->bi_bdev = g_bdev;
    for(i=0; i<cnt; i++)
        bio_add_page(bio, pages[i], PAGE_SIZE, 0);

    bio_get(bio);
    hypraid_submit_bio(rw, bio);

    //
    sprintf(label, "replacement(%s): ", rw&WRITE?"write":"read");
    bio_for_each_segment(bvec, bio, iter)
    {
        char *buffer = __bio_kmap_atomic(bio, iter);
        unsigned len = bvec.bv_len >> 9;

        print_hex_dump(KERN_INFO, label, DUMP_PREFIX_ADDRESS, 16, 1, buffer, len, false);

        __bio_kunmap_atomic(buffer);
    }

    return 0;
}

#define DM_HYRAID_TARGET_SIZE 16
/*
static int __hypraid_block_replacement_operation(int srcblk, int dstblk, int cnt)
{
    struct page *pages[HYPRAID_MAX_PAGES];
    int i;

    printk(KERN_INFO "replacement area [%d, %d] -> [%d, %d]\n",
            srcblk, srcblk + cnt - 1, dstblk, dstblk + cnt - 1);

    for(i=0; i<cnt; i++)
        pages[i] = alloc_page(GFP_NOFS);

    for(i=0; i<cnt; i++)
        get_page(pages[i]);

    hypraid_block_operation(pages, READ_SYNC, srcblk, cnt);
    hypraid_block_operation(pages, WRITE_SYNC, dstblk, cnt);

    for(i=0; i<cnt; i++)
        put_page(pages[i]);

    return 1;
}
*/
static void __hypraid_block_replacement_operation_wait(struct page **pages, int rw, int blk, int cnt)
{
    int T = DM_HYRAID_TARGET_SIZE;
    int S = blk;
    int E = blk + cnt;
    int ST = T - (S % T);
    int ET = E % T;
    int nT = (cnt - (ST + ET)) / T;
    int i = 0;
    int page_index = 0;

    if(S/T == E/T)
    {
        ST -= (T - ET);
        ET = 0;
        nT = 0;
    }

    hypraid_block_operation_wait(&pages[page_index], rw, blk, ST);
    blk += ST;
    page_index += ST;

    for(i=0; i<nT; i++)
    {
        hypraid_block_operation_wait(&pages[page_index], rw, blk, T);
        blk += T;
        page_index += T;
    }

    if(ET)
    {
        hypraid_block_operation_wait(&pages[page_index], rw, blk, ET);
        blk += ET;
        page_index += ET;
    }
}

static void __hypraid_block_replacement_operation(struct page **pages, int rw, int blk, int cnt)
{
    int T = DM_HYRAID_TARGET_SIZE;
    int S = blk;
    int E = blk + cnt;
    int ST = T - (S % T);
    int ET = E % T;
    int nT = (cnt - (ST + ET)) / T;
    int i = 0;
    int page_index = 0;

    if(S/T == E/T)
    {
        ST -= (T - ET);
        ET = 0;
        nT = 0;
    }

    hypraid_block_operation(&pages[page_index], rw, blk, ST);
    blk += ST;
    page_index += ST;

    for(i=0; i<nT; i++)
    {
        hypraid_block_operation(&pages[page_index], rw, blk, T);
        blk += T;
        page_index += T;
    }

    if(ET)
    {
        hypraid_block_operation(&pages[page_index], rw, blk, ET);
        blk += ET;
        page_index += ET;
    }
}

#define HYPRAID_MAX_PAGES 16

static int _hypraid_block_replacement_operation(int srcblk, int dstblk, int cnt)
{
    struct page *pages[HYPRAID_MAX_PAGES];
    int i, done;

    while(1)
    {
        int page_cnt = MIN(HYPRAID_MAX_PAGES, cnt);

        if(0 == page_cnt)
            break;

        for(i=0; i<page_cnt; i++)
            pages[i] = alloc_page(GFP_NOFS);

        for(i=0; i<page_cnt; i++)
            get_page(pages[i]);

        __hypraid_block_replacement_operation_wait(pages, READ_SYNC, srcblk, page_cnt);
        __hypraid_block_replacement_operation_wait(pages, WRITE_SYNC, dstblk, page_cnt);

        for(i=0; i<page_cnt; i++)
            put_page(pages[i]);

        cnt -= page_cnt;
    }

    return 0;
}


/*****
 * ioctl test function
 */
int hypraid_block_replacement_area(int srcblk, int dstblk, int cnt)
{
    return _hypraid_block_replacement_operation(srcblk, dstblk, cnt);
}

int hypraid_block_replacement_test(int srcblk, int dstblk)
{
    struct page *page;
    struct bio *bio;

    page = alloc_page(GFP_NOFS);
    bio = bio_alloc(GFP_NOFS, 1);

    bio->bi_iter.bi_sector = srcblk << 3;
    bio->bi_bdev = g_bdev;
    bio_add_page(bio, page, PAGE_SIZE, 0);

    get_page(page);
    bio_get(bio);
    submit_bio_wait(READ, bio);
    bio_put(bio);

    bio = bio_alloc(GFP_NOFS, 1);
    bio->bi_iter.bi_sector = dstblk << 3;
    bio->bi_bdev = g_bdev;
    bio_add_page(bio, page, PAGE_SIZE, 0);

    bio_get(bio);
    submit_bio_wait(WRITE, bio);
    bio_put(bio);
    put_page(page);

    return 0;
}
/*****
 * ioctl test function end
 */

static int hypraid_block_replacement_operation(int src_area_num, int dst_area_num, int blk_cnt)
{
    struct block_areas *dst_blk_areas = BLOCK_AREAS(dst_area_num);

    while(0 < blk_cnt)
    {
        struct area_node *area = NULL, *src_area = NULL, *dst_area = NULL;
        struct available_node *avail = NULL;
        int rep_blk_cnt = 0, i;

        blk_cnt -= hypraid_pop_priority_area(src_area_num, blk_cnt, &area);
        src_area = area_clone(area);

        if((rep_blk_cnt = hypraid_is_area_bitmap_free(dst_area_num, src_area->blk_cnt)) < 0)
        {
            hypraid_block_replacement(dst_area_num, -rep_blk_cnt);
        }

replacement_start:
        if(0 == src_area->blk_cnt)
        {
            available_area_insert(src_area_num, area);
            _area_free(src_area);

            continue;
        }

        if(!available_search(dst_area_num, src_area->blk_cnt, &avail))
        {
            avail = list_first_entry_or_null(&dst_blk_areas->avails, struct available_node, list);
            dst_area = list_first_entry_or_null(&avail->areas, struct area_node, list);

            available_area_extract(dst_area_num, dst_area);

            for(i=0; src_area->blk_cnt && i<dst_area->blk_cnt; i++, src_area->blk_cnt--)
            {
                hypraid_set_bitmap(dst_area->rep_area.start + i);
                hypraid_clear_bitmap(src_area->rep_area.start + i);
            }

            if(i != dst_area->blk_cnt)
            {
                struct area_node *avail_area = area_alloc(dst_area->rep_area.start + i, dst_area->rep_area.end);

                dst_area->rep_area.end = dst_area->rep_area.start + i - 1;
                dst_area->blk_cnt = i;

                available_area_insert(dst_area_num, avail_area);
            }

            dst_area->ori_area.start = src_area->ori_area.start;
            dst_area->ori_area.end = src_area->ori_area.start + i - 1;

            priority_area_insert(dst_area_num, dst_area);
            replacement_area_insert(dst_area_num, dst_area);

            _hypraid_block_replacement_operation(src_area->rep_area.start, dst_area->rep_area.start, i);

            src_area->rep_area.start += i;
            src_area->ori_area.start += i;

            goto replacement_start;
        }
        else
        {
            dst_area = list_first_entry_or_null(&avail->areas, struct area_node, list);
            available_area_extract(dst_area_num, dst_area);

            for(i=0; i<dst_area->blk_cnt; i++)
            {
                hypraid_set_bitmap(dst_area->rep_area.start + i);
                hypraid_clear_bitmap(src_area->rep_area.start + i);
            }

            dst_area->ori_area.start = src_area->ori_area.start;
            dst_area->ori_area.end = src_area->ori_area.end;

            priority_area_insert(dst_area_num, dst_area);
            replacement_area_insert(dst_area_num, dst_area);

            _hypraid_block_replacement_operation(src_area->rep_area.start, dst_area->rep_area.start, i);
        }

        available_area_insert(src_area_num, area);
        _area_free(src_area);
    }

    return 0;
}

static int hypraid_block_replacement_up(int access_area, int blk_cnt)
{
    return hypraid_block_replacement_operation(access_area, access_area-1, blk_cnt);
}

static int hypraid_block_replacement_down(int access_area, int blk_cnt)
{
    return hypraid_block_replacement_operation(access_area, access_area+1, blk_cnt);
}

int hypraid_insert_block_area_no_rearray(int start_blk, int end_blk)
{
    struct area_node *area = NULL;
    int access_area, i;

    if(end_blk < start_blk)
        return -1;

    access_area = hypraid_get_bitmap_area(start_blk);

    if(available_area_search_at(access_area, start_blk, &area))
    {
        struct area_node *split = NULL;

        available_area_extract(access_area, area);

        if(area->rep_area.start != start_blk)
        {
            split = area_clone(area);

            area_init(split, area->rep_area.start, start_blk - 1);

            area->ori_area.start = area->rep_area.start = start_blk;

            available_area_insert(access_area, split);
        }

        if(area->rep_area.end != end_blk)
        {
            split = area_clone(area);

            area_init(split, end_blk + 1, area->rep_area.end);

            area->ori_area.end = area->rep_area.end = end_blk;

            available_area_insert(access_area, split);
        }

        for(i=0; i<area->blk_cnt; i++)
            hypraid_set_bitmap(area->rep_area.start + i);

        priority_area_insert(access_area, area);
        replacement_area_insert(access_area, area);

        //printk(KERN_INFO "[%d, %d] -> [%d, %d] priority list insert\n", start_blk, end_blk, area->rep_area.start, area->rep_area.end);

        return 0;
    }

    replacement_area_search_at(access_area, start_blk, &area);

    priority_area_extract(access_area, area);
    priority_area_insert(access_area, area);

    //printk(KERN_INFO "[%d, %d] is priority update\n", start_blk, end_blk);

    return 1;
}

int hypraid_insert_block_area(int access_area, int start_blk, int end_blk)
{
    int blk = start_blk;
    int blk_cnt = end_blk - start_blk + 1;
    int rep_blk_cnt = blk_cnt;
    struct block_areas *blk_areas = &g_blk_areas[access_area];
    struct available_node *avail = NULL;
    struct area_node *area = NULL;
    int i;

    if(end_blk < start_blk)
        return 0;

    if(replacement_area_search_at(access_area, blk, &area))
    {
        blk = area->ori_area.end;

        priority_area_extract(access_area, area);
        priority_area_insert(access_area, area);

        return hypraid_insert_block_area(access_area, blk+1, end_blk);
    }

    rep_blk_cnt = hypraid_is_area_bitmap_free(access_area, blk_cnt);
    if((rep_blk_cnt = hypraid_is_area_bitmap_free(access_area, blk_cnt)) < 0)
    {
        printk(KERN_INFO "replacement!\n");
        hypraid_block_replacement(access_area, -rep_blk_cnt);
    }

    if(!available_search(access_area, blk_cnt, &avail))
    {
        avail = list_first_entry_or_null(&blk_areas->avails, struct available_node, list);
        area = list_first_entry_or_null(&avail->areas, struct area_node, list);

        available_area_extract(access_area, area);

        for(i=0; blk_cnt && i<area->blk_cnt; i++, blk_cnt--)
            hypraid_set_bitmap(area->rep_area.start + i);

        blk += i;

        if(area->rep_area.end == area->rep_area.start + i - 1)
        {
            area->ori_area.start = start_blk;
            area->ori_area.end = blk - 1;

            priority_area_insert(access_area, area);
            replacement_area_insert(access_area, area);

            return hypraid_insert_block_area(access_area, blk, end_blk);
        }
        else
        {
            struct area_node *use_area = area_alloc(area->rep_area.start, area->rep_area.start + i - 1);

            area->rep_area.start += i;
            area->blk_cnt -= i;

            use_area->ori_area.start = start_blk;
            use_area->ori_area.end = blk - 1;

            available_area_insert(access_area, area);
            priority_area_insert(access_area, use_area);
            replacement_area_insert(access_area, use_area);
        }
    }
    else
    {
        area = list_first_entry_or_null(&avail->areas, struct area_node, list);
        available_area_extract(access_area, area);

        for(i=0; i<area->blk_cnt; i++)
            hypraid_set_bitmap(area->rep_area.start + i);

        area->ori_area.start = start_blk;
        area->ori_area.end = end_blk;

        priority_area_insert(access_area, area);
        replacement_area_insert(access_area, area);
    }

    return 0;
}

int hypraid_get_block_area_priority(int area_num, int cnt, int *ret)
{
    struct area_node *v = NULL;

    v = list_last_entry(&g_blk_areas[area_num].pri_areas, struct area_node, list);

    while(1)
    {
        int blk;

        if(&g_blk_areas[area_num].pri_areas == &v->list)
            break;

        for(blk=v->rep_area.start; blk <= v->rep_area.end && cnt; cnt--, blk++)
            *(ret++) = blk;

        if(0 < cnt)
            v = list_prev_entry(v, list);
        else
            break;
    }

    for(;cnt;cnt--)
        *(ret++) = -1;

    return 0;
}

static int hypraid_pop_priority_area(int access_area, int cnt, struct area_node **ret)
{
    struct block_areas *blk_areas = BLOCK_AREAS(access_area);
    struct area_node *area = NULL;
    int blk;

    if(cnt <= 0)
        return -1;

    if(list_empty(&blk_areas->pri_areas))
        return -1;

    area = list_last_entry(&blk_areas->pri_areas, struct area_node, list);
    priority_area_extract(access_area, area);
    replacement_area_extract(access_area, area);

    blk = MIN(cnt, area->blk_cnt);

    if(blk != area->blk_cnt)
    {
        struct area_node *avail_area = area_alloc(area->rep_area.start, area->rep_area.start + blk - 1);

        avail_area->ori_area.start = area->ori_area.start;
        avail_area->ori_area.end = avail_area->ori_area.start + blk - 1;
        area->rep_area.start += blk;
        area->ori_area.start += blk;
        area->blk_cnt = area->rep_area.end - area->rep_area.start + 1;

        priority_area_insert_low(access_area, area);
        replacement_area_insert(access_area, area);

        *ret = avail_area;
    }
    else
    {
        *ret = area;
    }

    return blk;
}

void re_request_bio_print(int access_area, int start_blk, int end_blk)
{
    struct area_node *area;
    int blk;

    printk(KERN_INFO "write : [%d, %d]\n", start_blk, end_blk);
    for(blk=start_blk; blk<=end_blk; blk += area->blk_cnt)
    {
        replacement_area_search_at(access_area, blk, &area);

        printk(KERN_INFO "write, re-request : [%d, %d] -> [%d, %d]\n", 
                blk, blk + area->blk_cnt - 1,
                area->rep_area.start, area->rep_area.end);
    }
}

struct bio_private
{
    bio_end_io_t *dm_bio_end_io;
    void *dm_bio_private;
};

static void end_bio(struct bio *bio, int error)
{
    struct bio_private *dm_private = bio->bi_private;

    bio_print("parent bio", bio);
    printk(KERN_INFO "parent bio error : %d\n", error);

    bio->bi_private = dm_private->dm_bio_private;
    dm_private->dm_bio_end_io(bio, error);

    vfree(dm_private);
}

static void end_chain_bio(struct bio *bio, int error)
{
    struct bio_private *dm_private = bio->bi_private;

    bio_print("chain bio", bio);
    printk(KERN_INFO "chain bio error : %d\n", error);

    bio->bi_private = dm_private->dm_bio_private;
    dm_private->dm_bio_end_io(bio, error);

    vfree(dm_private);
}

static void hypraid_make_request(struct request_queue *q, struct bio *bio)
{
    int T = DM_HYRAID_TARGET_SIZE;
    int S = bio->bi_iter.bi_sector >> 3;
    int E = (bio_end_sector(bio) >> 3) - 1;
    int cnt = E - S + 1;
    int ST = T - (S % T);
    int ET = E % T;
    int nT = (cnt - (ST + ET)) / T;
    int i = 0;

    if(S/T == E/T)
    {
        g_make_request_fn(q, bio);
        return;
    }

    for(i=0; i<nT; i++)
    {
        struct bio *split = bio_split(bio, T << 3, GFP_NOIO, fs_bio_set);
        bio_chain(split, bio);
        g_make_request_fn(q, split);
    }

    if(ET)
    {
        g_make_request_fn(q, bio);
    }
}

void re_request_bio(struct request_queue *q, struct bio *bio)
{
    struct bio *split = NULL;
    struct area_node *area = NULL;
    int start_blk = bio->bi_iter.bi_sector >> 3;
    int sector = bio->bi_iter.bi_sector & 7;
    int end_blk = (bio_end_sector(bio) >> 3) - 1;
    int i = start_blk, bindex = 0;

    bio_print("request bio", bio);

    for(i=start_blk; i<=end_blk; i+=(area->blk_cnt - (i - area->ori_area.start)))
    {
        replacement_area_search_at(0, i, &area);

        if(area)
        {
            if(area->ori_area.end < end_blk)
            {
                split = bio_split(bio, (area->blk_cnt << 3), GFP_NOIO, fs_bio_set);
                bio_chain(split, bio);
            }
            else
            {
                split = bio;
            }

            split->bi_iter.bi_sector = ((area->rep_area.start + (i - area->ori_area.start)) << 3) + sector;

            bio_print("replacement bio", split);
            hypraid_make_request(q, split);
            sector = 0;
        }
    }
}

int hypraid_open_block_area(int area_blocks[3])
{
    int i;
    int bound = 0;

    for(i=0; i<3; i++)
    {
        INIT_LIST_HEAD(&g_blk_areas[i].avails);
        INIT_LIST_HEAD(&g_blk_areas[i].pri_areas);

        g_blk_areas[i].area_num = i;
        g_blk_areas[i].area_tree = RB_ROOT;
        g_blk_areas[i].avail_tree = RB_ROOT;
        g_blk_areas[i].pri_area_tree = RB_ROOT;
        g_blk_areas[i].rep_area_tree = &g_rep_area_tree;
        g_blk_areas[i].up_rep_f = NULL; //(i==0)?NULL:hypraid_block_replacement_up;
        g_blk_areas[i].down_rep_f = (i==2)?NULL:hypraid_block_replacement_down;

        available_area_insert(i, area_alloc(bound, bound + area_blocks[i] - 1));
        bound += area_blocks[i];
    }

    return 1;
}

void hypraid_close_priority(void)
{
    /*
       struct block_area *v, *t;
       int i;

       for(i=0; i<2; i++)
       {
       list_for_each_entry_safe(v, t, &g_pl[i].h, list)
       {
       rb_erase(&v->rb, &g_pl[i].r);
       list_del(&v->list);
       vfree(v);
       }
       }
       */
}
