#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "lwip/sockets.h"

#include "../../../../cc_api.h"
#include "../../../../runtime/north/cc_node.h"
#include "../../transport/socket/cc_transport_socket.h"
#include "../../../../runtime/north/cc_msgpack_helper.h"

#include "../cc_platform.h"
#include "../../../north/cc_transport.h"
#include "../../../../calvinsys/cc_calvinsys.h"

void platform_init(void)
{
}

void platform_print(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vfprintf(stdout, fmt, args);
	fprintf(stdout, "\n");
	va_end(args);
}

result_t platform_create(struct node_t* node)
{
	node->platform = NULL;
	return CC_RESULT_SUCCESS;
}

result_t platform_create_calvinsys(calvinsys_t **calvinsys)
{
	return CC_RESULT_SUCCESS;
}

result_t platform_mem_alloc(void **buffer, uint32_t size)
{
	*buffer = malloc(size);
	if (*buffer == NULL) {
		cc_log_error("Failed to allocate '%ld' memory", (unsigned long)size);
		return CC_RESULT_FAIL;
	}

	return CC_RESULT_SUCCESS;
}

void *platform_mem_calloc(size_t nitems, size_t size)
{
	void *ptr = NULL;

	if (platform_mem_alloc(&ptr, nitems * size) != CC_RESULT_SUCCESS)
		return NULL;
	memset(ptr, 0, nitems * size);

	return ptr;
}

void platform_mem_free(void *buffer)
{
	free(buffer);
}


bool platform_evt_wait(struct node_t *node, uint32_t timeout_seconds)
{
	fd_set fds;
	int fd = 0;
	struct timeval tv, *tv_ref = NULL;

	if (timeout_seconds > 0) {
		tv.tv_sec = timeout_seconds;
		tv.tv_usec = 0;
		tv_ref = &tv;
	}

	FD_ZERO(&fds);

	if (node->transport_client != NULL && (node->transport_client->state == TRANSPORT_PENDING || node->transport_client->state == TRANSPORT_ENABLED)) {
		FD_SET(((transport_socket_client_t *)node->transport_client->client_state)->fd, &fds);
		fd = ((transport_socket_client_t *)node->transport_client->client_state)->fd;

		select(fd + 1, &fds, NULL, NULL, tv_ref);

		if (FD_ISSET(fd, &fds)) {
			if (transport_handle_data(node, node->transport_client, node_handle_message) != CC_RESULT_SUCCESS) {
				cc_log_error("Failed to read data from transport");
				node->transport_client->state = TRANSPORT_DISCONNECTED;
			}
			return true;
		}
	} else
		select(0, NULL, NULL, NULL, tv_ref);

	return false;
}

result_t platform_stop(struct node_t* node)
{
	return CC_RESULT_SUCCESS;
}

result_t platform_node_started(struct node_t* node)
{
	return CC_RESULT_SUCCESS;
}

#define CC_CONFIG_FILE "calvinconstrained.config"


#ifdef CC_DEEPSLEEP_ENABLED
void platform_deepsleep(node_t *node)
{
        cc_log("Going to deepsleep state, runtime will stop!");
}
#endif

#ifdef CC_STORAGE_ENABLED
void platform_write_node_state(node_t *node, char *buffer, size_t size)
{
        FILE *fp = NULL;
        int len = 0;

        fp = fopen(CC_CONFIG_FILE, "w+");
        if (fp != NULL) {
                len = fwrite(buffer, 1, size, fp);
                if (len != size)
                        cc_log_error("Failed to write node config");
                else
                        cc_log_debug("Wrote runtime state '%d' bytes", len);
                fclose(fp);
        } else
                cc_log("Failed to open %s for writing", CC_CONFIG_FILE);
}

result_t platform_read_node_state(node_t *node, char buffer[], size_t size)
{
        FILE *fp = NULL;

        fp = fopen(CC_CONFIG_FILE, "r+");
        if (fp != NULL) {
                fread(buffer, 1, size, fp);
                fclose(fp);
                return CC_RESULT_SUCCESS;
        }

        return CC_RESULT_FAIL;
}
#endif

