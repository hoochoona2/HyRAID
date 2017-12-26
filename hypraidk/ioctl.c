#include "hypraidk.h"

static struct block_device_operations g_hypraid_ioctl_op;
static struct block_device_operations *g_bdop;

extern int hypraid_block_replacement(int access_area, int blk_cnt);
extern int g_mode;

static int hypraid_ioctl(struct block_device *bdev, fmode_t mode, unsigned int cmd, unsigned long arg)
{
    int size = _IOC_SIZE(cmd);
    struct hypraid_ioctl hio;

    if(size)
    {
        if(_IOC_DIR(cmd) & _IOC_READ)
            if(access_ok(VERIFY_WRITE, (void*)arg, size) < 0)
                return -EINVAL;
        if(_IOC_DIR(cmd) & _IOC_WRITE)
            if(access_ok(VERIFY_READ, (void*)arg, size) < 0)
                return -EINVAL;
    }

    if(HYPRAID_IOCTL_MAGIC_KEY != _IOC_TYPE(cmd))
        return g_bdop->ioctl(bdev, mode, cmd, arg);

    if(HYPRAID_IOCTL_MAX <= _IOC_NR(cmd))
        return -EINVAL;

    switch(cmd)
    {
        case HYPRAID_GET_PRIORITY:
            copy_from_user((void*)&hio, (void*)arg, sizeof(hio));
            hypraid_get_block_area_priority(hio.uvalue[0], hio.uvalue[1], hio.kvalue);
            copy_to_user((void*)arg, (void*)&hio, sizeof(hio));
            break;

        case HYPRAID_BLOCK_REPLACEMENT:
            copy_from_user((void*)&hio, (void*)arg, sizeof(hio));
            hio.kvalue[0] = hypraid_block_replacement_test(hio.uvalue[0], hio.uvalue[1]);
            copy_to_user((void*)arg, (void*)&hio, sizeof(hio));
            break;

        case HYPRAID_BLOCK_REPLACEMENT_AREA:
            copy_from_user((void*)&hio, (void*)arg, sizeof(hio));
            hio.kvalue[0] = hypraid_block_replacement_area(hio.uvalue[0], hio.uvalue[1], hio.uvalue[2]);
            copy_to_user((void*)arg, (void*)&hio, sizeof(hio));
            break;

        case HYPRAID_PRINT_AREA:
            printk(KERN_INFO "print area ioctl in \n");
            hypraid_block_area_print();
            break;

        case HYPRAID_INSERT_AREA:
            copy_from_user((void*)&hio, (void*)arg, sizeof(hio));
            //hypraid_insert_block_area(g_access_area, hio.uvalue[0], hio.uvalue[1]);
            hypraid_insert_block_area_no_rearray(hio.uvalue[0], hio.uvalue[1]);
            break;

        case HYPRAID_DELETE_AREA:
            copy_from_user((void*)&hio, (void*)arg, sizeof(hio));
            hypraid_delete_block_area(hio.uvalue[0], hio.uvalue[1]);
            break;

        case HYPRAID_SET_MODE:
            copy_from_user((void*)&hio, (void*)arg, sizeof(hio));
            printk(KERN_INFO "old mode : %d -> new mode : %d\n", g_mode, hio.uvalue[0]);
            g_mode = hio.uvalue[0];
            break;

        case HYPRAID_REPLACEMENT_TEST:
            copy_from_user((void*)&hio, (void*)arg, sizeof(hio));
            hypraid_block_replacement(g_access_area, hio.uvalue[0]);
            break;

        case HYPRAID_TRANSFORM_BLOCK:
            {
                struct area_node *area;
                int access_area;

                copy_from_user((void*)&hio, (void*)arg, sizeof(hio));

                access_area = hypraid_get_bitmap_area(hio.uvalue[1]);

                if(0 == hio.uvalue[0])
                {
                    if(replacement_area_search_at(access_area, hio.uvalue[1], &area))
                    {
                        hio.kvalue[0] = area->rep_area.start;
                        hio.kvalue[1] = area->rep_area.end;
                    }
                    else
                        goto transform_error;

                }
                else if(1 == hio.uvalue[0])
                {
                    if(priority_area_search_at(access_area, hio.uvalue[1], &area))
                    {
                        hio.kvalue[0] = area->ori_area.start;
                        hio.kvalue[1] = area->ori_area.end;
                    }
                    else
                        goto transform_error;
                }
                else
                {
transform_error:
                    hio.kvalue[0] = -1;
                    hio.kvalue[1] = -1;
                }

                copy_to_user((void*)arg, (void*)&hio, sizeof(hio));
            }
            break;
    }

    return 0;
}

static void init_bdop(struct block_device_operations *set, const struct block_device_operations *data)
{
    *set = (struct block_device_operations){
        .open = data->open,
        .release = data->release,
        .rw_page = data->rw_page,
        .ioctl = hypraid_ioctl,
        .compat_ioctl = data->compat_ioctl,
        .direct_access = data->direct_access,
        .check_events = data->check_events,
        .media_changed = data->media_changed,
        .unlock_native_capacity = data->unlock_native_capacity,
        .revalidate_disk = data->revalidate_disk,
        .getgeo = data->getgeo,
        .swap_slot_free_notify = data->swap_slot_free_notify,
        .owner = THIS_MODULE
    };
}

void hypraid_open_ioctl(struct gendisk *disk)
{
    g_bdop = (struct block_device_operations*)disk->fops;
    init_bdop(&g_hypraid_ioctl_op, disk->fops);
    disk->fops = &g_hypraid_ioctl_op;
}

void hypraid_close_ioctl(struct gendisk *disk)
{
    disk->fops = g_bdop;
}

