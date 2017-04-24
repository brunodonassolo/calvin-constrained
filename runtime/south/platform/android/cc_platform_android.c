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
#include <time.h>
#include <unistd.h>
#include <stdarg.h>
#include <android/log.h>
#include <sys/socket.h>
#include <stdio.h>
#include "../../../../cc_api.h"
#include "cc_platform_android.h"
#include "../cc_platform.h"
#include "../../transport/socket/cc_transport_socket.h"
#include "../../../north/cc_proto.h"
#include "../../../../msgpuck/msgpuck.h"
#include "../../transport/fcm/cc_transport_fcm.h"
#include "calvinsys/accelerometer.h"
#include "../../../north/cc_msgpack_helper.h"
#include "../../../../calvinsys/cc_calvinsys.h"
#ifdef SLEEP_AT_UNACTIVITY
#include "time.h"
time_t last_activity;
#endif

static result_t external_calvinsys_init(calvinsys_t* calvinsys);
static result_t external_calvinsys_release(calvinsys_t* calvinsys);
static result_t external_calvinsys_input(calvinsys_t* calvinsys, char* command, char* data, size_t data_size);
static result_t command_transport_connected(node_t* node, char* payload_data, size_t size);
static result_t command_rt_stop(node_t* node, char* payload_data, size_t size);
static result_t command_calvin_msg(node_t* node, char* payload_data, size_t size);
static result_t platform_serialize_and_stop(node_t* node, char* payload_data, size_t size);
static result_t platform_trigger_reconnect(node_t* node, char* payload_data, size_t size);
static result_t platform_register_external_calvinsys(node_t* node, char* payload_data, size_t size);
static result_t platform_handle_external_calvinsys_data(node_t* node, char* payload_data, size_t size);

struct platform_command_handler_t platform_command_handlers[PLATFORM_ANDROID_NBR_OF_COMMANDS] = {
  {PLATFORM_ANDROID_CONNECT_REPLY, command_transport_connected},
  {PLATFORM_ANDROID_RUNTIME_STOP, command_rt_stop},
	{PLATFORM_ANDROID_RUNTIME_CALVIN_MSG, command_calvin_msg},
	{PLATFORM_ANDROID_RUNTIME_SERIALIZE_AND_STOP, platform_serialize_and_stop},
	{PLATFORM_ANDROID_RUNTIME_TRIGGER_RECONNECT, platform_trigger_reconnect},
	{PLATFORM_ANDROID_REGISTER_EXTERNAL_CALVINSYS, platform_register_external_calvinsys},
	{PLATFORM_ANDROID_EXTERNAL_CALVINSYS_PAYLOAD, platform_handle_external_calvinsys_data}
};

static int send_upstream_platform_message(const transport_client_t *transport_client, char *cmd, char *data, size_t data_size)
{
  int res = 0;
  transport_fcm_client_t *fcm_client = (transport_fcm_client_t *)transport_client->client_state;

  transport_set_length_prefix(data, data_size - TRANSPORT_LEN_PREFIX_SIZE);
  memcpy(data + TRANSPORT_LEN_PREFIX_SIZE, cmd, PLATFORM_ANDROID_COMMAND_SIZE); // set 2 byte command type
	res = write(((android_platform_t *)fcm_client->node->platform)->upstream_platform_fd[1], data, data_size);
	if (res == data_size) {
    log_debug("Wrote upstream message of size '%d'", res);
    return res;
  }

  log_error("Could not write upstream message of size '%d', res '%d'", data_size, res);

	return res;
}

static result_t send_downstream_platform_message(const node_t *node, char *data, size_t data_size)
{
  int res = 0;

  res = write(((android_platform_t*)node->platform)->downstream_platform_fd[1], data, data_size);
  if (res > 0) {
    log_debug("Wrote downstream message with size %d", data_size);
    return SUCCESS;
  }

  log_error("Could not write downstream message, result: %d", res);
  return FAIL;
}

static result_t read_upstream(const node_t *node, char *buffer, size_t size)
{
	int bytes_read = 0, status = 0, fd = ((android_platform_t*)node->platform)->upstream_platform_fd[0];
	unsigned int datasize = 0;
	fd_set set;

	FD_ZERO(&set);
	FD_SET(fd, &set);

	status = select(fd + 1, &set, NULL, NULL, NULL);
	if (status > 0) {
		bytes_read = read(fd, buffer, TRANSPORT_LEN_PREFIX_SIZE);
    if (bytes_read < TRANSPORT_LEN_PREFIX_SIZE) {
      log_error("Failed to read message size '%d'", bytes_read);
      return FAIL;
    }

    datasize = transport_get_message_len(buffer);
		if (datasize > size - TRANSPORT_LEN_PREFIX_SIZE) {
			log_error("Error when reading upstream data, read buffer '%d' < message '%d'.", size, datasize);
      return FAIL;
    }

    bytes_read = read(fd, buffer + TRANSPORT_LEN_PREFIX_SIZE, datasize);
		if (bytes_read != datasize) {
			log_error("Error when reading from pipe, expected '%d', got '%d'", datasize, bytes_read);
			return FAIL;
		}
	} else if (status < 0) {
		log_error("Error on upstream select");
		return FAIL;
	}

	return SUCCESS;
}

static result_t platform_android_handle_fcm_data(node_t* node, char *buffer, size_t size)
{
	unsigned int i = 0;

	// handle command
	for (i = 0; i < PLATFORM_ANDROID_NBR_OF_COMMANDS; i++) {
		if (strncmp(platform_command_handlers[i].command, buffer, PLATFORM_ANDROID_COMMAND_SIZE) == 0) {
			log_debug("Will handle command '%.*s'", PLATFORM_ANDROID_COMMAND_SIZE, buffer);
      return platform_command_handlers[i].handler(node, buffer + PLATFORM_ANDROID_COMMAND_SIZE, size - PLATFORM_ANDROID_COMMAND_SIZE);
		}
	}

  log("Failed to handle '%.*s'", size, buffer);

	return FAIL;
}

static result_t platform_android_handle_data(node_t* node, transport_client_t *transport_client)
{
  return transport_handle_data(node, transport_client, platform_android_handle_fcm_data);
}

static result_t platform_android_handle_socket_data(node_t *node, transport_client_t *transport_client)
{
  return transport_handle_data(node, transport_client, node_handle_message);
}

static int platform_android_fcm_fd_handler(int fd, int events, void *data)
{
	node_t *node = (node_t *)data;

  if (platform_android_handle_data(node, node->transport_client) != SUCCESS)
		log_error("Error when handling data");

	return 1; // Always return 1, since 0  will unregister the FD in the looper
}

static int platform_android_socket_fd_handler(int fd, int events, void *data)
{
	node_t *node = (node_t *)data;

  if (platform_android_handle_socket_data(node, node->transport_client) != SUCCESS)
		log_error("Error when handling socket data");

	return 1; // Always return 1, since 0  will unregister the FD in the looper
}

void platform_evt_wait(node_t *node, struct timeval *timeout)
{
#ifdef SLEEP_AT_UNACTIVITY
	time_t current_time;
#endif
	android_platform_t *platform = (android_platform_t*)node->platform;
	int status = 0, timeout_trigger = 5000;
	int platform_trigger_id = 2, socket_transport_trigger_id = 3;

	if (node->transport_client == NULL || timeout != NULL) {
		// Sleep for some time before reconnect
		sleep(timeout->tv_sec);
		return;
	}

	// Add FD for FCM and platform communications
	if (ALooper_addFd(platform->looper, ((android_platform_t*) node->platform)->downstream_platform_fd[0], platform_trigger_id, ALOOPER_EVENT_INPUT, &platform_android_fcm_fd_handler, node) != 1) {
		log_error("Could not add fd to looper, looper: %p", platform->looper);
		sleep(5);
	}

	// Only att transport FD if a socket is used it not the same as
	if (node->transport_client->transport_type == TRANSPORT_SOCKET_TYPE) {
		if (ALooper_addFd(platform->looper, ((transport_socket_client_t *)node->transport_client->client_state)->fd, socket_transport_trigger_id, ALOOPER_EVENT_INPUT, &platform_android_socket_fd_handler, node) != 1) {
			log_error("Could not add socket fd");
			sleep(5);
		}
	}

	status = ALooper_pollOnce(timeout_trigger, NULL, NULL, NULL);

#ifdef SLEEP_AT_UNACTIVITY
	current_time = time(NULL);
	if (status == ALOOPER_POLL_TIMEOUT) {
		if (difftime(current_time, last_activity) >= ((double)PLATFORM_ANDROID_UNACTIVITY_TIMEOUT)) {
			log("platform timeout triggered");
			api_runtime_serialize_and_stop(node);
		}
	} else {
		last_activity = current_time;
	}
#endif
}

void platform_print(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	__android_log_vprint(ANDROID_LOG_VERBOSE, "Calvin Constrained Android", fmt, args);
	va_end(args);
}

static result_t command_calvin_msg(node_t *node, char *payload_data, size_t size)
{
  unsigned int len = transport_get_message_len(payload_data);

  return node_handle_message(node, payload_data + 4, len);
}

static result_t command_transport_connected(node_t *node, char *data, size_t size)
{
  node->transport_client->state = TRANSPORT_CONNECTED;
	return SUCCESS;
}

static result_t command_rt_stop(node_t *node, char *payload_data, size_t size)
{
  return api_runtime_stop(node);
}

static result_t platform_serialize_and_stop(node_t *node, char *data, size_t size)
{
  return api_runtime_serialize_and_stop(node);
}

static result_t platform_trigger_reconnect(node_t *node, char *data, size_t size)
{
  return api_reconnect(node);
}

static result_t node_setup_reply_handler(node_t *node, char *data, void *msg_data)
{
	// TODO: Return success or fail of the registration.
	return SUCCESS;
}

static result_t platform_register_external_calvinsys(node_t *node, char *data, size_t size)
{
	calvinsys_t *calvinsys = NULL;
	int name_len = 0;

	if (platform_mem_alloc((void **)&calvinsys, sizeof(calvinsys_t)) != SUCCESS) {
		log_error("Could not allocate memory for external calvinsys obj");
		return FAIL;
	}

	name_len = strlen(data);

	if (platform_mem_alloc((void **)&calvinsys->name, name_len + 1) != SUCCESS) {
		log_error("Could not allocate memory for name");
		platform_mem_free(calvinsys);
		return FAIL;
	}

	memset(calvinsys->name, 0, name_len + 1);
	memcpy(calvinsys->name, data, name_len);

	calvinsys->init = external_calvinsys_init;
	calvinsys->release = external_calvinsys_release;
	calvinsys->write = external_calvinsys_input;
	calvinsys->node = node;

	if (register_calvinsys(node, calvinsys) != SUCCESS) {
		log_error("Could not register calvinsys");
		platform_mem_free(calvinsys->name);
		platform_mem_free(calvinsys);
		return FAIL;
	}

	// Send new proxy config if successful
	return proto_send_node_setup(node, node_setup_reply_handler);
}

static result_t platform_handle_external_calvinsys_data(node_t *node, char *data, size_t size)
{
	char *calvinsys_name;
	char *payload;
	char *command;
	int name_len, payload_len, command_len;
	calvinsys_t *sys = NULL;

	if (decode_string_from_map(data, "command", &command, &command_len) != SUCCESS) {
		log_error("Could not parse command key");
		return FAIL;
	}

	if (decode_string_from_map(data, "calvinsys", &calvinsys_name, &name_len) != SUCCESS) {
		log_error("Could not parse calvinsys key.");
		return FAIL;
	}

	if (decode_bin_from_map(data, "payload", &payload, &payload_len) != SUCCESS) {
		log_error("Could not parse paylaod key");
		return FAIL;
	}

	sys = (calvinsys_t *)list_get(node->calvinsys, calvinsys_name);
	if (sys == NULL) {
		log_error("Could not find calvinsys %s", calvinsys_name);
		return FAIL;
	}

	sys->command = command;
	sys->data = payload;
	sys->data_size = size;
	sys->new_data = 1;

	return SUCCESS;
}

static result_t external_calvinsys_init(calvinsys_t *calvinsys)
{
	char buffer[100];
	int len = strlen(calvinsys->name) + 6;

  strcpy(buffer + 6, calvinsys->name);
	if (send_upstream_platform_message(calvinsys->node->transport_client, PLATFORM_ANDROID_INIT_EXTERNAL_CALVINSYS, buffer, len) > 0)
    return SUCCESS;

  return FAIL;
}

static result_t external_calvinsys_release(calvinsys_t *calvinsys)
{
	char buffer[100];
	int len = strlen(calvinsys->name) + 6;

  strcpy(buffer + 6, calvinsys->name);
	if (send_upstream_platform_message(calvinsys->node->transport_client, PLATFORM_ANDROID_DESTROY_EXTERNAL_CALVINSYS, buffer, len) > 0)
    return SUCCESS;

  return FAIL;
}

static result_t external_calvinsys_input(calvinsys_t *calvinsys, char *command, char *data, size_t data_size)
{
	char buffer[500], *buf = buffer + 6;

	buf = mp_encode_map(buf, 3);
	buf = encode_str(&buf, "calvinsys", calvinsys->name, strlen(calvinsys->name) + 1);
	buf = encode_str(&buf, "command", command, strlen(command) + 1);
	buf = encode_bin(&buf, "payload", data, data_size);

	if (send_upstream_platform_message(calvinsys->node->transport_client, PLATFORM_ANDROID_DESTROY_EXTERNAL_CALVINSYS, buffer, buf - buffer) > 0)
    return SUCCESS;

  return FAIL;
}

result_t platform_create(node_t *node)
{
	//TODO: Create looper here
	android_platform_t *platform = NULL;
	node_attributes_t *attr = NULL;

	if (platform_mem_alloc((void **)&platform, sizeof(android_platform_t)) != SUCCESS) {
		log_error("Could not allocate memory for platform object.");
		return FAIL;
	}

	platform->send_downstream_platform_message = send_downstream_platform_message;
	platform->send_upstream_platform_message = send_upstream_platform_message;
	platform->read_upstream = read_upstream;
	node->platform = platform;

	if (pipe(((android_platform_t *)node->platform)->upstream_platform_fd) < 0 ||
			pipe(((android_platform_t *)node->platform)->downstream_platform_fd) < 0) {
		log_error("Could not open pipes for transport");
		platform_mem_free(platform);
		return FAIL;
	}

	log("platform fd up; %d %d, down: %d %d", ((android_platform_t*) node->platform)->upstream_platform_fd[0],
	    ((android_platform_t *)node->platform)->upstream_platform_fd[1],
	    ((android_platform_t *)node->platform)->downstream_platform_fd[0],
	    ((android_platform_t *)node->platform)->downstream_platform_fd[1]);

	if (platform_mem_alloc((void **)&attr, sizeof(node_attributes_t)) != SUCCESS) {
		log_error("Could not allocate memory for attributes");
		platform_mem_free(platform);
		return FAIL;
	}

	attr->indexed_public_owner = NULL;
	attr->indexed_public_node_name = NULL;
	attr->indexed_public_address = NULL;
	node->attributes = attr;

	return SUCCESS;
}

result_t platform_node_started(struct node_t *node)
{
	// Send RT started, tc is not created here, so just write on the pipe
	char buffer[6];

	memset(buffer, 0, 6);
	memset(buffer + 3, 2 & 0xFF, 1);
	memcpy(buffer + 4, PLATFORM_ANDROID_RUNTIME_STARTED, 2);

	if (write(((android_platform_t*)node->platform)->upstream_platform_fd[1], buffer, 6) < 0) {
		log_error("Failed to write rt started command");
		return FAIL;
	}

	return SUCCESS;
}

result_t platform_create_calvinsys(struct node_t *node)
{
	android_platform_t* platform = (android_platform_t *)node->platform;

	platform->looper = ALooper_prepare(ALOOPER_PREPARE_ALLOW_NON_CALLBACKS);

	return SUCCESS;
}

void platform_init(void)
{
#ifdef SLEEP_AT_UNACTIVITY
	last_activity = time(NULL);
#endif
	srand(time(NULL));
}

result_t platform_stop(node_t *node)
{
	// Write node stop on pipe, tc will not exist here
	char buffer[6];

	memset(buffer, 0, 6);
	memset(buffer + 3, 2 & 0xFF, 1);
	memcpy(buffer + 4, PLATFORM_ANDROID_RUNTIME_STOP, 2);

	if (write(((android_platform_t *)node->platform)->upstream_platform_fd[1], buffer, 6) < 0) {
		log_error("Failed to write rt started command");
		return FAIL;
	}

	// TODO: Cleanup
	return SUCCESS;
}

result_t platform_mem_alloc(void **buffer, uint32_t size)
{
	*buffer = malloc(size);
	if (*buffer == NULL) {
		log_error("Failed to allocate '%ld' memory", (unsigned long)size);
		return FAIL;
	}

	log_debug("Allocated '%ld'", (unsigned long)size);

	return SUCCESS;
}

void *platform_mem_calloc(size_t nitems, size_t size)
{
  void *ptr = NULL;
  if (platform_mem_alloc(&ptr, nitems * size) != SUCCESS)
    return NULL;

  memset(ptr, 0, nitems * size);
  return ptr;
}

void platform_mem_free(void *buffer)
{
	free(buffer);
}

#ifdef USE_PERSISTENT_STORAGE
void platform_write_node_state(node_t *node, char *buffer, size_t size)
{
	FILE *fp = NULL;
	char *filename = "calvinconstrained.config";
	char abs_filepath[strlen(filename) + strlen(node->storage_dir) + 1];

	strcpy(abs_filepath, node->storage_dir);
	if (node->storage_dir[strlen(node->storage_dir)-1] != '/')
		strcat(abs_filepath, "/");
	strcat(abs_filepath, filename);

	fp = fopen(abs_filepath, "w+");
	if (fp != NULL) {
		if (fwrite(buffer, 1, size, fp) != size)
			log_error("Failed to write node config");
		else
			log("Wrote node state to disk");
		fclose(fp);
	} else {
		log_error("Failed to open calvinconstrained.config for writing");
		log_error("Errno: %d, error: %s", errno, strerror(errno));
	}
}

result_t platform_read_node_state(node_t *node, char buffer[], size_t size)
{
	result_t result = FAIL;
	FILE *fp = NULL;
	char *filename = "calvinconstrained.config";
	char abs_filepath[strlen(filename) + strlen(node->storage_dir) + 1];
	int len = 0;

	strcpy(abs_filepath, node->storage_dir);
	if (node->storage_dir[strlen(node->storage_dir)-1] != '/')
		strcat(abs_filepath, "/");
	strcat(abs_filepath, filename);

	fp = fopen(abs_filepath, "r+");
	if (fp != NULL) {
		len = fread(buffer, 1, size, fp);
		if (len > 0)
			result = SUCCESS;
		fclose(fp);
	}

	return result;
}
#endif