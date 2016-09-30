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
#include "actor_identity.h"
#include "../port.h"
#include "../token.h"

result_t actor_identity(struct actor_t *actor)
{
	port_t *port = actor->inports;
	token_t *token = NULL;
	result_t result = SUCCESS;

	if (port->fifo != NULL) {
		while (result == SUCCESS && fifo_can_read(port->fifo)) {
			token = fifo_read(port->fifo);
			result = fifo_write(actor->outports->fifo, token);
			if (result != SUCCESS) {
				fifo_commit_read(port->fifo, false, false);
				result = FAIL;
			} else {
				fifo_commit_read(port->fifo, true, false);
			}
		}
	}

	return result;
}
