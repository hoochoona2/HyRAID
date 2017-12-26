#include "hypraidk.h"

struct file* open(const char *path, int flags, int rights)
{
    struct file *filp = NULL;
    mm_segment_t oldfs;
    int err = 0;

    oldfs = get_fs();
    set_fs(get_ds());
    filp = filp_open(path, flags, rights);
    set_fs(oldfs);
    if(IS_ERR(filp))
    {
        err = PTR_ERR(filp);
        return NULL;
    }

    return filp;
}

void close(struct file *file)
{
    filp_close(file, NULL);
}

int read(struct file *file, unsigned char *buf, unsigned int size)
{
    mm_segment_t oldfs;
    int ret;

    oldfs = get_fs();
    set_fs(get_ds());

    ret = vfs_read(file, buf, size, &file->f_pos);

    set_fs(oldfs);

    return ret;
}

int write(struct file *file, unsigned char *buf, unsigned int size)
{
    mm_segment_t oldfs;
    int ret;

    oldfs = get_fs();
    set_fs(get_ds());

    ret = vfs_write(file, buf, size, &file->f_pos);

    set_fs(oldfs);

    return ret;
}

int lseek(struct file *file, off_t offset, int whence)
{
    return vfs_llseek(file, offset, whence);
}

int fsync(struct file *file)
{
    return vfs_fsync(file, 0);
}

#define BUFSIZ 32
int _fscanf(struct file *file, int *len, const char *format, ...)
{
    va_list args;
    char buf[BUFSIZ] = {0, };
    ssize_t bytes;
    int ret = -1;
    loff_t pos = file->f_pos;

    if((bytes = read(file, buf, BUFSIZ)) <= 0)
        return -1;

    buf[bytes] = '\0';

    va_start(args, format);
    ret = vsscanf(buf, format, args);
    va_end(args);

    if(0 < ret)
    {
        file->f_pos = pos + *len;
        return *len;
    }

    return -1;
}
#undef BUFSIZ

int fprintf(struct file *file, const char *format, ...)
{
    va_list args;
    int buflen = 0;
    char *buf = NULL;
    int ret = 0;

    va_start(args, format);
    buflen = vsnprintf(NULL, 0, format, args);
    va_end(args);

    if(buflen <= 0)
        return -1;

    buf = (char*)vmalloc(buflen+1);
    memset(buf, 0x00, buflen+1);

    va_start(args, format);
    vsnprintf(buf, buflen+1, format, args);
    va_end(args);

    ret = write(file, buf, buflen);

    vfree(buf);

    return ret;
}

void bio_print(const char *label, struct bio *bio)
{
    printk(KERN_INFO "%s%s [%d, %d], size : %d, raw_sectors : %d raw_rw : %d\n", label, bio->bi_rw&WRITE?"Write":"Read", bio->bi_iter.bi_sector >> 3, (bio_end_sector(bio) >> 3) - 1, bio->bi_iter.bi_size, bio->bi_iter.bi_sector, bio->bi_rw);
}

void print_bio_page_hex_dump(char *label, struct bio *bio)
{
    struct bio_vec bvec;
    struct bvec_iter iter;

    bio_print(label, bio);
    bio_for_each_segment(bvec, bio, iter)
    {
        char *buffer = __bio_kmap_atomic(bio, iter);
        unsigned len = bvec.bv_len >> 9;

        print_hex_dump(KERN_INFO, label, DUMP_PREFIX_ADDRESS, 16, 1, buffer, len, false);

        __bio_kunmap_atomic(buffer);
    }
}
