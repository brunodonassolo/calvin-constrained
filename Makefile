APPLICATION = calvin

# This has to be the absolute path to the RIOT base directory:
RIOTBASE ?= $(HOME)/iot-lab/parts/RIOT

CFLAGS += -DCC_TRANSPORT_SOCKET -DCC_TRANSPORT_SOCKET_SSDP_ENABLED
#CFLAGS += -DCC_STORAGE_ENABLED -DCC_RUNTIME_STATE_BUFFER_SIZE=10000
CFLAGS += -DCC_DEEPSLEEP_ENABLED -DCC_INACTIVITY_TIMEOUT=5 -DCC_SLEEP_TIME=60
CFLAGS += -DCC_ACTOR_IDENTITY -DCC_ACTOR_BUTTON -DCC_ACTOR_LIGHT -DCC_ACTOR_SOIL_MOISTURE -DCC_ACTOR_TEMPERATURE
CFLAGS += -DCC_LWIP_SOCKET -DCC_GETOPT_ENABLED -DCC_DEBUG
CFLAGS += -Wno-address -Wno-error=format=

BASE = $(HOME)/calvin-constrained


DIRS += \
runtime/south/platform/riot/ \
runtime/north/ \
runtime/north/scheduler/np_scheduler/ \
runtime/south/transport/socket/ \
msgpuck \
calvinsys/ \
actors/

USEMODULE += lwip_ipv6_autoconfig lwip_ipv6
USEMODULE += lwip_sock_ip lwip_ipv4
USEMODULE += lwip_udp lwip_sock_udp
USEMODULE += lwip_tcp lwip_sock_tcp
USEMODULE += lwip_netdev

# shell modules
USEMODULE += shell
USEMODULE += shell_commands
USEMODULE += ps
USEMODULE += netstats_l2
USEMODULE += netstats_ipv6
USEMODULE += netstats_rpl
USEMODULE += auto_init_gnrc_netif
#USEMODULE += gnrc_netdev_default
USEMODULE += gnrc_pktdump
## Additional networking modules that can be dropped if not needed
USEMODULE += gnrc_icmpv6_echo


USEMODULE += actors calvinsys msgpuck socket np_scheduler north riot

# If no BOARD is found in the environment, use this default:
BOARD ?= native


# Change this to 0 to show compiler invocation lines by default:
QUIET ?= 0

CFLAGS += -DDEVELHELP

include $(RIOTBASE)/Makefile.include

prepare:
	find $(BASE) -type f -a \( -name "*.h" -o -name "*.c" \) -a -not -path "$(BASE)/micropython/*" -exec sed -i 's/list_add/list_calvin_add/g;s/list_remove/list_calvin_remove/g;s/list_t/list_calvin_t/g' {} +
#	@sed -i -e 's/mp_decode_uint/mpk_decode_uint/' $(BASE)/msgpuck/msgpuck.h

