#ifndef __CLI_H__
#define __CLI_H__

#include <stddef.h>

struct cli
{
    enum
    {
        CLI_ADD,
        CLI_LIST,
        CLI_GET
    } cmd;
    union
    {
        struct
        {
            char *key;
            char *img_filename;
            char **entries;
            size_t entries_amount;
        } add;
        struct
        {
            char *img_filename;
        } list;
        struct
        {
            char *img_filename;
            char *key;
            char *out_filename;
            char *target_filename;
        } get;
    };
};

int parse_cli(int argc, char **argv, struct cli *cli);

#endif // __CLI_H__
