.PHONY: all
CONTIKI_PROJECT = calvin

#CFLAGS += -DCC_TRANSPORT_SOCKET -DCC_TRANSPORT_SOCKET_SSDP_ENABLED
#CFLAGS += -DCC_STORAGE_ENABLED -DCC_RUNTIME_STATE_BUFFER_SIZE=10000
CFLAGS += -DCC_DEEPSLEEP_ENABLED -DCC_INACTIVITY_TIMEOUT=5 -DCC_SLEEP_TIME=60
CFLAGS += -DCC_ACTOR_IDENTITY -DCC_ACTOR_BUTTON -DCC_ACTOR_LIGHT -DCC_ACTOR_SOIL_MOISTURE -DCC_ACTOR_TEMPERATURE

BASE = $(HOME)/calvin-constrained
CONTIKIDIRS += $(BASE)
CONTIKIDIRS += $(BASE)/runtime/north
CONTIKIDIRS += $(BASE)/runtime/south/transport/socket
CONTIKIDIRS += $(BASE)/runtime/north/scheduler/np_scheduler
CONTIKIDIRS += $(BASE)/msgpuck
CONTIKIDIRS += $(BASE)/actors
CONTIKIDIRS += $(BASE)/calvinsys

PROJECT_SOURCEFILES += \
cc_api.c \
cc_common.c \
cc_scheduler.c \
cc_node.c \
cc_proto.c \
cc_tunnel.c \
cc_link.c \
cc_actor_store.c \
cc_actor.c \
cc_port.c \
cc_fifo.c \
cc_token.c \
msgpuck.c \
cc_calvinsys.c \
cc_actor_identity.c \
cc_actor_light.c \
cc_actor_button.c \
cc_actor_temperature.c \
cc_actor_soil_moisture.c
#cc_transport.c \
#cc_transport_socket.c \
#cc_msgpack_helper.c \

all: prepare $(CONTIKI_PROJECT)

prepare:
	find $(BASE) -type f -a \( -name "*.h" -o -name "*.c" \) -a -not -path "$(BASE)/micropython/*" -exec sed -i 's/list_add/list_calvin_add/g;s/list_remove/list_calvin_remove/g;s/list_t/list_calvin_t/g' {} +
	@sed -i -e 's/mp_decode_uint/mpk_decode_uint/' $(BASE)/msgpuck/msgpuck.h


CONTIKI = $(HOME)/iot-lab/parts/contiki
include $(CONTIKI)/Makefile.include
