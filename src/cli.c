#include "cli.h"
#include <getopt.h>
#include <string.h>
#include <libgen.h>
#include <stdio.h>

int parse_cli(int argc, char **argv, struct cli *cli)
{
    if (argc < 2) {
		printf("Usage: %s get|add|list [ARGS]\n", argv[0]);
        return -1;
    }
	char *arg_command_str = argv[1];
	argv++;
	argc--;
	if (strcmp(arg_command_str, "add") == 0) {
		cli->cmd = CLI_ADD;
		char *key = NULL;
		char *image = NULL;
		struct option longopts[] = {
			{ "key", required_argument, NULL, 'k' },
			{ "image", required_argument, NULL, 'i' },
			{ 0, 0, 0, 0 },
		};
		int opt;
		while ((opt = getopt_long(argc, argv, "k:i:", longopts, NULL)) != -1) {
			switch (opt) {
			case 'k':
				key = optarg;
				break;
			case 'i':
				image = optarg;
				break;
			default:
				printf(
					"add usage: %s add --key key_string --image disk.img entry1.txt [entry2.txt entry3/ ...]",
					argv[-1]);
				return -1;
			}
		}
		if (key == NULL || image == NULL || optind >= argc) {
			printf(
				"add usage: %s add --key key_string --image disk.img entry1.txt [entry2.txt entry3/ ...]",
				argv[-1]);
			return -1;
		}
        cli->add.img_filename = image;
		cli->add.key = key;
		cli->add.img_filename = image;
		cli->add.entries = &argv[optind];
		cli->add.entries_amount = argc - optind;
	} else if (strcmp(arg_command_str, "list") == 0) {
        cli->cmd = CLI_LIST;
		char *img_filename = NULL;
		struct option longopts[] = {
			{ "image", required_argument, NULL, 'i' },
			{ 0, 0, 0, 0 },
		};
		int opt;
		while ((opt = getopt_long(argc, argv, "i:", longopts, NULL)) != -1) {
			switch (opt) {
			case 'i':
				img_filename = optarg;
				break;
			default:
				printf("list usage: %s list --image disk.img", argv[-1]);
				return -1;
			}
		}
		if (img_filename == NULL) {
			printf("list usage: %s list --image disk.img", argv[-1]);
			return -1;
		}
		cli->list.img_filename = img_filename;
	} else if (strcmp(arg_command_str, "get") == 0) {
        cli->cmd = CLI_GET;
		char *img_filename = NULL;
		char *key = NULL;
		char *out_filename = NULL;
		struct option longopts[] = {
			{ "image", required_argument, NULL, 'i' },
			{ "key", required_argument, NULL, 'k' },
			{ "out", required_argument, NULL, 'o' },
			{ 0, 0, 0, 0 },
		};
		int opt;
		while ((opt = getopt_long(argc, argv, "i:k:o:", longopts, NULL)) !=
			   -1) {
			switch (opt) {
			case 'i':
				img_filename = optarg;
				break;
			case 'k':
				key = optarg;
				break;
			case 'o':
				out_filename = optarg;
				break;
			default:
				printf(
					"get usage: %s get --image disk.img --key key_string --out result_file target_filename",
					argv[-1]);
				return -1;
			}
		}
		if (img_filename == NULL || key == NULL || out_filename == NULL ||
			optind >= argc) {
			printf(
				"get usage: %s get --image disk.img --key key_string --out result_file target_filename",
				argv[-1]);
			return -1;
		}
		cli->get.img_filename = img_filename;
		cli->get.key = key;
		cli->get.out_filename = out_filename;
		cli->get.target_filename = argv[optind];
	} else {
		printf("Usage: %s get|add|list [ARGS]\n", argv[-1]);
		return -1;
	}
	return 0;
}
