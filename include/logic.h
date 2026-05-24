#ifndef __LOGIC_H__
#define __LOGIC_H__

#include "cli.h"

struct app_config
{
    size_t sequential_limit;
    size_t max_display_filename_len;
    size_t io_buf_size;
};

int cmd_add(struct cli *cli, struct app_config cfg);

int cmd_list(struct cli *cli, struct app_config cfg);

int cmd_get(struct cli *cli, struct app_config cfg);

#endif // __LOGIC_H__
