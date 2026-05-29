#include "cli.h"
#include <string.h>
#include <stdio.h>

int parse_cli(int argc, char **argv, struct cli *cli)
{
    if (argc < 2) {
        printf("Usage: %s -add|-list|-get [ARGS]\n", argv[0]);
        return -1;
    }
    char *arg_command_str = argv[1];
    if (arg_command_str[0] != '-') {
        printf("Usage: %s -add|-list|-get [ARGS]\n", argv[0]);
        return -1;
    }
    arg_command_str++;
    if (strcmp(arg_command_str, "add") == 0) {
        cli->cmd = CLI_ADD;
        char *key = NULL;
        char *image = NULL;
        int i = 2;
        while (i < argc) {
            if (argv[i][0] == '-' && strlen(argv[i]) > 1) {
                if (strcmp(argv[i], "-key") == 0) {
                    if (i + 1 >= argc) {
                        printf("add usage: %s -add -key key_string -image disk.img entry1.txt [entry2.txt entry3/ ...]", argv[0]);
                        return -1;
                    }
                    key = argv[i + 1];
                    i += 2;
                } else if (strcmp(argv[i], "-image") == 0) {
                    if (i + 1 >= argc) {
                        printf("add usage: %s -add -key key_string -image disk.img entry1.txt [entry2.txt entry3/ ...]", argv[0]);
                        return -1;
                    }
                    image = argv[i + 1];
                    i += 2;
                } else {
                    printf("add usage: %s -add -key key_string -image disk.img entry1.txt [entry2.txt entry3/ ...]", argv[0]);
                    return -1;
                }
            } else {
                break;
            }
        }
        if (key == NULL || image == NULL || i >= argc) {
            printf("add usage: %s -add -key key_string -image disk.img entry1.txt [entry2.txt entry3/ ...]", argv[0]);
            return -1;
        }
        cli->add.img_filename = image;
        cli->add.key = key;
        cli->add.img_filename = image;
        cli->add.entries = &argv[i];
        cli->add.entries_amount = argc - i;
    } else if (strcmp(arg_command_str, "list") == 0) {
        cli->cmd = CLI_LIST;
        char *img_filename = NULL;
        int i = 2;
        while (i < argc) {
            if (argv[i][0] == '-' && strlen(argv[i]) > 1) {
                if (strcmp(argv[i], "-image") == 0) {
                    if (i + 1 >= argc) {
                        printf("list usage: %s -list -image disk.img", argv[0]);
                        return -1;
                    }
                    img_filename = argv[i + 1];
                    i += 2;
                } else {
                    printf("list usage: %s -list -image disk.img", argv[0]);
                    return -1;
                }
            } else {
                break;
            }
        }
        if (img_filename == NULL) {
            printf("list usage: %s -list -image disk.img", argv[0]);
            return -1;
        }
        cli->list.img_filename = img_filename;
    } else if (strcmp(arg_command_str, "get") == 0) {
        cli->cmd = CLI_GET;
        char *img_filename = NULL;
        char *key = NULL;
        char *out_filename = NULL;
        int i = 2;
        while (i < argc) {
            if (argv[i][0] == '-' && strlen(argv[i]) > 1) {
                if (strcmp(argv[i], "-image") == 0) {
                    if (i + 1 >= argc) {
                        printf("get usage: %s -get -image disk.img -key key_string -out result_file target_filename", argv[0]);
                        return -1;
                    }
                    img_filename = argv[i + 1];
                    i += 2;
                } else if (strcmp(argv[i], "-key") == 0) {
                    if (i + 1 >= argc) {
                        printf("get usage: %s -get -image disk.img -key key_string -out result_file target_filename", argv[0]);
                        return -1;
                    }
                    key = argv[i + 1];
                    i += 2;
                } else if (strcmp(argv[i], "-out") == 0) {
                    if (i + 1 >= argc) {
                        printf("get usage: %s -get -image disk.img -key key_string -out result_file target_filename", argv[0]);
                        return -1;
                    }
                    out_filename = argv[i + 1];
                    i += 2;
                } else {
                    printf("get usage: %s -get -image disk.img -key key_string -out result_file target_filename", argv[0]);
                    return -1;
                }
            } else {
                break;
            }
        }
        if (img_filename == NULL || key == NULL || out_filename == NULL ||
            i >= argc) {
            printf("get usage: %s -get -image disk.img -key key_string -out result_file target_filename", argv[0]);
            return -1;
        }
        cli->get.img_filename = img_filename;
        cli->get.key = key;
        cli->get.out_filename = out_filename;
        cli->get.target_filename = argv[i];
    } else {
        printf("Usage: %s -add|-list|-get [ARGS]\n", argv[0]);
        return -1;
    }
    return 0;
}
