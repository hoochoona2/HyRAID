#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/mman.h>
#include <string.h>

#include "../hyraidk/hypraid_ioctl.h"

int main(int argc, char *argv[])
{
    int fd;
    int i;
    struct hypraid_ioctl hio;

    if(argc < 2)
    {
        fprintf(stderr, "%s [command] {args}\n", argv[0]);

        return 1;
    }

    fd = open("/dev/mapper/hypraid", O_RDWR);
    if(-1 == fd)
    {
        fprintf(stderr, "open error\n");

        return 1;
    }

    if(!strcmp(argv[1], "priority"))
    {
        if(4 != argc)
        {
            fprintf(stderr, "%s priority [area num] [get block count]\n", argv[0]);

            return 1;
        }
        hio = (struct hypraid_ioctl){{atoi(argv[2]),atoi(argv[3]), 0, }, {0, }};
        ioctl(fd, HYPRAID_GET_PRIORITY, &hio);

        for(i=0; i<hio.uvalue[1]; i++)
            printf("%d\n", hio.kvalue[i]);
    }
    else if(!strcmp(argv[1], "replacement"))
    {
        if(4 != argc)
        {
            fprintf(stderr, "%s replacement [src block] [dst block]\n", argv[0]);

            return 1;
        }
        hio = (struct hypraid_ioctl){{atoi(argv[2]),atoi(argv[3]), 0, }, {0, }};
        ioctl(fd, HYPRAID_BLOCK_REPLACEMENT, &hio);
        ioctl(fd, HYPRAID_PRINT_AREA, &hio);
    }
    else if(!strcmp(argv[1], "replacement_area"))
    {
        if(5 != argc)
        {
            fprintf(stderr, "%s replacement [src block] [dst block] [count of block]\n", argv[0]);

            return 1;
        }
        hio = (struct hypraid_ioctl){{atoi(argv[2]),atoi(argv[3]), atoi(argv[4]), 0, }, {0, }};
        ioctl(fd, HYPRAID_BLOCK_REPLACEMENT_AREA, &hio);
    }
    else if(!strcmp(argv[1], "demo_insert"))
    {
        FILE *fp = NULL;
        int start, end;

        if(3 != argc)
        {
            fprintf(stderr, "%s insert [demo file]\n", argv[0]);

            return 1;
        }

        fp = fopen(argv[2], "r");
        if(NULL == fp)
        {
            fprintf(stderr, "fopen error\n");

            return 1;
        }

        while(-1 != fscanf(fp, "%d, %d\n", &start, &end))
        {
            hio = (struct hypraid_ioctl){{start, end, 0, }, {0, }};
            ioctl(fd, HYPRAID_INSERT_AREA, &hio);
        }
        ioctl(fd, HYPRAID_PRINT_AREA, &hio);
    }
    else if(!strcmp(argv[1], "insert"))
    {
        if(4 != argc)
        {
            fprintf(stderr, "%s insert [start block number] [end block number]\n", argv[0]);

            return 1;
        }
        hio = (struct hypraid_ioctl){{atoi(argv[2]),atoi(argv[3]), 0, }, {0, }};
        ioctl(fd, HYPRAID_INSERT_AREA, &hio);
        ioctl(fd, HYPRAID_PRINT_AREA, &hio);
    }
    else if(!strcmp(argv[1], "delete"))
    {
        if(4 != argc)
        {
            fprintf(stderr, "%s delete [start block number] [end block number]\n", argv[0]);

            return 1;
        }
        hio = (struct hypraid_ioctl){{atoi(argv[2]),atoi(argv[3]), 0, }, {0, }};
        ioctl(fd, HYPRAID_DELETE_AREA, &hio);
        ioctl(fd, HYPRAID_PRINT_AREA, &hio);
    }
    else if(!strcmp(argv[1], "mode"))
    {
        if(3 != argc)
        {
            fprintf(stderr, "%s mode [mode type]\n", argv[0]);

            return 1;
        }
        hio = (struct hypraid_ioctl){{atoi(argv[2]), 0}, {0, }};
        ioctl(fd, HYPRAID_SET_MODE, &hio);
    }
    else if(!strcmp(argv[1], "print"))
    {
        printf("print area\n");
        ioctl(fd, HYPRAID_PRINT_AREA);
    }
    else if(!strcmp(argv[1], "replacement_test"))
    {
        if(3 != argc)
        {
            fprintf(stderr, "%s replacement_test [block count]\n", argv[0]);

            return 1;
        }
        hio = (struct hypraid_ioctl){{atoi(argv[2]), 0}, {0, }};
        ioctl(fd, HYPRAID_REPLACEMENT_TEST, &hio);
    }
    else if(!strcmp(argv[1], "transform_block"))
    {
        if(4 != argc)
        {
            fprintf(stderr, "%s transform_block [type] [block]\n", argv[0]);
            fprintf(stderr, "[tyep]\n"
                            "0  :   VFS blocks\n"
                            "1  :   HyPRAID blocks\n");

            return 1;
        }
        hio = (struct hypraid_ioctl){{atoi(argv[2]), atoi(argv[3])}, {0, }};
        ioctl(fd, HYPRAID_TRANSFORM_BLOCK, &hio);

        printf("%s : [%d] => [%d, %d]\n", 0==hio.uvalue[0]?"VFS":"HyPRIAD",
                hio.uvalue[1], hio.kvalue[0], hio.kvalue[1]);
    }


    close(fd);

    return 0;
}
