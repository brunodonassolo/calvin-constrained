#include "contiki.h"
#include <stdio.h>

#include "../../../../cc_api.h"
#include "../../../../runtime/north/cc_node.h"
#include "../../transport/socket/cc_transport_socket.h"
#include "../../../../runtime/north/cc_msgpack_helper.c"

PROCESS(calvin_c, "Calvin Constrained");
AUTOSTART_PROCESSES(&calvin_c);

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

void platform_write_node_state(struct node_t* node, char *buffer, size_t size)
{
}

result_t platform_read_node_state(struct node_t* node, char buffer[], size_t size)
{
	return CC_RESULT_SUCCESS;
}

#ifdef CC_DEEPSLEEP_ENABLED
void platform_deepsleep(node_t *node)
{
}
#endif

PROCESS_THREAD(calvin_c, ev, data)
{
	PROCESS_BEGIN();

	char *attr = NULL, *proxy_uris = NULL;
	node_t *node = NULL;

	if (api_runtime_init(&node, attr, proxy_uris, "./") != CC_RESULT_SUCCESS)
		return EXIT_FAILURE;

	if (api_runtime_start(node) != CC_RESULT_SUCCESS)
		return EXIT_FAILURE;

	return EXIT_SUCCESS;

	PROCESS_END();
}

