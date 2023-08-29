/* Based on pw-link */
#include "node_queue.h"
#include <stdio.h>
#include <stdbool.h>
#include <getopt.h>

#include <pipewire/pipewire.h>

int
main(int argc, char *argv[])
{
    bool is_file_given = false;
    const char *default_node_names[] = {"Chromium", "Firefox", "telegram-desktop"};
    const char *default_dst = "WEBRTC VoiceEngine";
    const char *node_names[1024];
    int node_names_count = sizeof(default_node_names) / sizeof(char *);
    memcpy(node_names, default_node_names, sizeof(default_node_names));

    const char *short_options = "hf:n:";
    const struct option long_options[] = {{"help", no_argument, NULL, 'h'},
                                          {"file", required_argument, NULL, 'f'},
                                          {"nodename", required_argument, NULL, 'n'},
                                          {NULL, 0, NULL, 0}};
    int res;
    int option_index;
    while ((res = getopt_long(argc, argv, short_options, long_options, &option_index)) != -1) {
        switch (res) {

        case 'f': {
            FILE *node_file = fopen(optarg, "r");
            if (node_file) {
                node_names_count = 0;
                char buf[256] = {};
                while (fscanf(node_file, "%255s", buf) == 1 && node_names_count < 1024) {
                    node_names[node_names_count++] = strdup(buf);
                }
                fclose(node_file);
                is_file_given = true;
            }
            break;
        }

        case 'n': {
            default_dst = optarg;
            break;
        }
        case 'h':
        default: {
            printf("Usage: ./pw_stereomix [OPTIONS]\n");
            printf("Example: ./pw_stereomix -f nodes.txt -n \"WEBRTC VoiceEngine\"\n\n");
            printf("--help, -h                                display this help text and exit\n");
            printf(
                "--file=<filename>, -f <filename>          gets the PipeWire nodes from the file stated in the argument\n");
            printf("--nodename=<node_name>, -n <node_name>    changes the connect destination node to the one that's "
                   "given in the argument\n");
            return 0;
        }
        }
    }
    int prog_res = stereomix_run(node_names, node_names_count, default_dst);
    
    if (is_file_given) {
        for (int i = 0; i < node_names_count; ++i) {
            free((void *) node_names[i]);
        }
    }
    if (prog_res) {
        fprintf(stderr, "An error occured during program execution!\n");
    }

    return prog_res;
}
