/*
 * Copyright (c) 2016 Ericsson AB
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "cc_api.h"
#ifdef CC_GETOPT_ENABLED
#include <getopt.h>
#endif

#include "shell.h"
#include "msg.h"

#define MAIN_QUEUE_SIZE     (8)
static msg_t _main_msg_queue[MAIN_QUEUE_SIZE];


int calvin_c(int argc, char **argv)
{
	char *attr = NULL, *proxy_uris = NULL;
	//char *attr = "calvinip://localhost:5000", *proxy_uris = "{\"indexed_public\": {\"node_name\": {\"name\": \"constrained\"}}}";
	node_t *node = NULL;
#ifdef CC_GETOPT_ENABLED
	int c = 0;
	static struct option long_options[] = {
		{"attr", required_argument, NULL, 'a'},
		{"proxy_uris", required_argument, NULL, 'p'},
		{NULL, 0, NULL, 0}
	};

	while ((c = getopt_long(argc, argv, "a:p:", long_options, NULL)) != -1) {
		switch (c) {
		case 'a':
			attr = optarg;
			break;
		case 'p':
			proxy_uris = optarg;
			break;
		default:
			break;
		}
	}

#endif

	if (api_runtime_init(&node, attr, proxy_uris, "./") != CC_RESULT_SUCCESS)
		return EXIT_FAILURE;

	if (api_runtime_start(node) != CC_RESULT_SUCCESS)
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}

static const shell_command_t shell_commands[] = {
    { "calvin", "Start calving", calvin_c },
    { NULL, NULL, NULL }
};


int main(void)
{
    /* we need a message queue for the thread running the shell in order to
     * receive potentially fast incoming networking packets */
    msg_init_queue(_main_msg_queue, MAIN_QUEUE_SIZE);
    puts("RIOT calvin application");

    /* start shell */
    puts("All up, running the shell now");
    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);

    /* should be never reached */
    return 0;
}

