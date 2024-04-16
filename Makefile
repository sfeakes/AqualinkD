#
# Options
#
# make          // standard build aqualinkd and serial_logger
# make debug    // Compule standard aqualinkd binary just with debugging
# make aqdebug  // Compile with extra aqualink debug information like timings
# make slog     // Serial logger
# make <other>  // not documenting
#

# Valid flags for AQ_FLAGS
AQ_RS16 = true
AQ_PDA  = true
AQ_ONETOUCH = true
AQ_IAQTOUCH = true
AQ_MANAGER =true
AQ_RS_EXTRA_OPTS = false
#AQ_CONTAINER = false // this is for compiling for containers
#AQ_MEMCMP = true // Not implimented correctly yet.

# Turn off threadded net services
AQ_NO_THREAD_NETSERVICE = false

# define the C compiler to use
CC = gcc

#LIBS := -lpthread -lm
#LIBS := -l pthread -l m
#LIBS := -l pthread -l m -static # Take out -static, just for dev
LIBS := -lpthread -lm

# Standard compile flags
GCCFLAGS = -Wall -O3
#GCCFLAGS = -O3
#GCCFLAGS = -Wall -O3 -Wextra
#GCCFLAGS = -Wl,--gc-sections,--print-gc-sections
#GCCFLAGS = -Wall -O3 -ffunction-sections -fdata-sections

# Standard debug flags
DGCCFLAGS = -Wall -O0 -g

# Aqualink Debug flags
#DBGFLAGS = -g -O0 -Wall -fsanitize=address -D AQ_DEBUG -D AQ_TM_DEBUG
DBGFLAGS = -g -O0 -Wall -D AQ_DEBUG -D AQ_TM_DEBUG

# Mongoose flags
#MGFLAGS = -D MG_DISABLE_MD5 -D MG_DISABLE_HTTP_DIGEST_AUTH -D MG_DISABLE_MD5 -D MG_DISABLE_JSON_RPC
# Mongoose 6.18 flags
MGFLAGS = -D MG_ENABLE_HTTP_SSI=0 -D MG_ENABLE_DIRECTORY_LISTING=0 -D MG_ENABLE_HTTP_CGI=0
#MGFLAGS =

# Detect OS and set some specifics
ifeq ($(OS),Windows_NT)
   # Windows Make.
   MKDIR = mkdir
   FixPath = $(subst /,\,$1)
else
   UNAME_S := $(shell uname -s)
   # Linux
   ifeq ($(UNAME_S),Linux)
	  MKDIR = mkdir -p
      FixPath = $1
	  # Get some system information
      PI_OS_VERSION = $(shell cat /etc/os-release | grep VERSION= | cut -d\" -f2)
      $(info OS: $(PI_OS_VERSION) )
      GLIBC_VERSION = $(shell ldd --version | grep ldd)
      $(info GLIBC build with: $(GLIBC_VERSION) )
      $(info GLIBC Prefered  : 2.24-11+deb9u1 2.24 )
   endif
   # OSX
   ifeq ($(UNAME_S),Darwin)
   endif
endif


# Main source files
SRCS = aqualinkd.c utils.c config.c aq_serial.c aq_panel.c aq_programmer.c net_services.c json_messages.c rs_msg_utils.c\
       devices_jandy.c packetLogger.c devices_pentair.c color_lights.c serialadapter.c aq_timer.c aq_scheduler.c web_config.c\
       serial_logger.c mongoose.c timespec_subtract.c


AQ_FLAGS =
# Add source and flags depending on protocols to support.
ifeq ($(AQ_PDA), true)
  SRCS := $(SRCS) pda.c pda_menu.c pda_aq_programmer.c
  AQ_FLAGS := $(AQ_FLAGS) -D AQ_PDA
endif

ifeq ($(AQ_ONETOUCH), true)
  SRCS := $(SRCS) onetouch.c onetouch_aq_programmer.c
  AQ_FLAGS := $(AQ_FLAGS) -D AQ_ONETOUCH
endif

ifeq ($(AQ_IAQTOUCH), true)
  SRCS := $(SRCS) iaqtouch.c iaqtouch_aq_programmer.c
  AQ_FLAGS := $(AQ_FLAGS) -D AQ_IAQTOUCH
endif

ifeq ($(AQ_RS16), true)
  AQ_FLAGS := $(AQ_FLAGS) -D AQ_RS16
endif

ifeq ($(AQ_MEMCMP), true)
  AQ_FLAGS := $(AQ_FLAGS) -D AQ_MEMCMP
endif


ifeq ($(AQ_MANAGER), true)
  AQ_FLAGS := $(AQ_FLAGS) -D AQ_MANAGER
  LIBS := $(LIBS) -lsystemd
  # aq_manager requires threads, so make sure that's turned on.
  ifeq ($(AQ_NO_THREAD_NETSERVICE), true)
    # Show error
    $(warning AQ_MANAGER requires threads, ignoring AQ_NO_THREAD_NETSERVICE)
  endif
else
  # No need for serial_logger without aq_manager
  SRCS := $(filter-out serial_logger.c, $(SRCS))
  # no threadded net service only valid without aq manager.
  ifeq ($(AQ_NO_THREAD_NETSERVICE), true)
    AQ_FLAGS := $(AQ_FLAGS) -D AQ_NO_THREAD_NETSERVICE
  endif
endif


# Put all flags together.
CFLAGS = $(GCCFLAGS) $(AQ_FLAGS) $(MGFLAGS)
DFLAGS = $(DGCCFLAGS) $(AQ_FLAGS) $(MGFLAGS)
DBG_CFLAGS = $(DBGFLAGS) $(AQ_FLAGS) $(MGFLAGS)

# Other sources.
DBG_SRC = $(SRCS) debug_timer.c
SL_SRC = serial_logger.c aq_serial.c utils.c packetLogger.c rs_msg_utils.c timespec_subtract.c

# Build durectories
OBJ_DIR := ./build
DBG_OBJ_DIR := $(OBJ_DIR)/debug
SL_OBJ_DIR := $(OBJ_DIR)/slog

# Object files
OBJ_FILES := $(patsubst %.c,$(OBJ_DIR)/%.o,$(SRCS))
DBG_OBJ_FILES := $(patsubst %.c,$(DBG_OBJ_DIR)/%.o,$(DBG_SRC))
SL_OBJ_FILES := $(patsubst %.c,$(SL_OBJ_DIR)/%.o,$(SL_SRC))


# define the executable file
MAIN = ./release/aqualinkd
SLOG = ./release/serial_logger
DEBG = ./release/aqualinkd-debug
#LOGR = ./release/log_reader
#PLAY = ./release/aqualinkd-player


# Rules to pass to make.
all:    $(MAIN) $(SLOG)
	$(info $(MAIN) has been compiled)
	$(info $(SLOG) has been compiled)

slog:	$(SLOG)
	$(info $(SLOG) has been compiled)

aqdebug: $(DEBG)
	$(info $(DEBG) has been compiled)

# Container, add container flag and compile
container: CFLAGS := $(CFLAGS) -D AQ_CONTAINER
container: $(MAIN) $(SLOG)
	$(info $(MAIN) has been compiled (** For Container use **))
	$(info $(SLOG) has been compiled (** For Container use **))

#debug, Just change compile flags and call MAIN
debug: CFLAGS = $(DFLAGS)
debug: $(MAIN) $(SLOG)
	$(info $(MAIN) has been compiled (** DEBUG **))
	$(info $(SLOG) has been compiled (** DEBUG **))

install: $(MAIN)
	./release/install.sh


# Rules to compile
$(OBJ_DIR)/%.o: %.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

$(DBG_OBJ_DIR)/%.o: %.c | $(DBG_OBJ_DIR)
	$(CC) $(DBG_CFLAGS) $(INCLUDES) -c -o $@ $<

$(SL_OBJ_DIR)/%.o: %.c | $(SL_OBJ_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

# Rules to link
$(MAIN): $(OBJ_FILES)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^ $(LIBS)

$(DEBG): $(DBG_OBJ_FILES)
	$(CC) $(DBG_CFLAGS) $(INCLUDES) -o $@ $^ $(LIBS)

$(SLOG): CFLAGS := $(CFLAGS) -D SERIAL_LOGGER
$(SLOG): $(SL_OBJ_FILES)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^ $(LIBS)

# Rules to make object directories.
$(OBJ_DIR):
	$(MKDIR) $(call FixPath,$@)

$(SL_OBJ_DIR):
	$(MKDIR) $(call FixPath,$@)

$(DBG_OBJ_DIR):
	$(MKDIR) $(call FixPath,$@)


# Clean rules
.PHONY: clean
clean:
	$(RM) *.o *~ $(MAIN) $(MAIN_U) $(PLAY) $(PL_EXOBJ) $(DEBG)
	$(RM) $(wildcard *.o) $(wildcard *~) $(OBJ_FILES) $(DBG_OBJ_FILES) $(SL_OBJ_FILES) $(MAIN) $(MAIN_U) $(PLAY) $(PL_EXOBJ) $(LOGR) $(PLAY) $(DEBG)





define DO_NOT_USE

# OLD MAKEFILE, STILL NEED TO MOVE THE BELOW OVER TO NEW Makefile
LOGR = ./release/log_reader
PLAY = ./release/aqualinkd-player



#
# Options
#
# make          // standard everything
# make debug    // Give standard binary just with debugging
# make aqdebug  // Compile with extra aqualink debug information like timings
# make slog     // Serial logger
# make <other>  // not documenting
#

# Valid flags for AQ_FLAGS
AQ_RS16 = true
AQ_PDA  = true
AQ_ONETOUCH = true
AQ_IAQTOUCH = true
#AQ_MEMCMP = true // Not implimented correctly yet.

# Turn off threadded net services
AQ_NO_THREAD_NETSERVICE = false

# Get some system information
PI_OS_VERSION = $(shell cat /etc/os-release | grep VERSION= | cut -d\" -f2)
$(info OS: $(PI_OS_VERSION) )
GLIBC_VERSION = $(shell ldd --version | grep ldd)
$(info GLIBC build with: $(GLIBC_VERSION) )
$(info GLIBC Prefered  : 2.24-11+deb9u1 2.24 )


# define the C compiler to use
CC = gcc

#LIBS := -lpthread -lm
LIBS := -l pthread -l m
#LIBS := -l pthread -l m -static # Take out -static, just for dev

# Standard compile flags
GCCFLAGS = -Wall -O3
#GCCFLAGS = -O3
#GCCFLAGS = -Wall -O3 -Wextra
#GCCFLAGS = -Wl,--gc-sections,--print-gc-sections
#GCCFLAGS = -Wall -O3 -ffunction-sections -fdata-sections

# Standard debug flags
DGCCFLAGS = -Wall -O0 -g

# Aqualink Debug flags
#DBGFLAGS = -g -O0 -Wall -fsanitize=address -D AQ_DEBUG -D AQ_TM_DEBUG
DBGFLAGS = -g -O0 -Wall -D AQ_DEBUG -D AQ_TM_DEBUG

# Mongoose flags
#MGFLAGS = -D MG_DISABLE_MD5 -D MG_DISABLE_HTTP_DIGEST_AUTH -D MG_DISABLE_MD5 -D MG_DISABLE_JSON_RPC
# Mongoose 6.18 flags
MGFLAGS = -D MG_ENABLE_HTTP_SSI=0 -D MG_ENABLE_DIRECTORY_LISTING=0 -D MG_ENABLE_HTTP_CGI=0
#MGFLAGS = 

# define the C source files
#SRCS = aqualinkd.c utils.c config.c aq_serial.c init_buttons.c aq_programmer.c net_services.c json_messages.c pda.c pda_menu.c \
#       pda_aq_programmer.c devices_jandy.c onetouch.c onetouch_aq_programmer.c packetLogger.c devices_pentair.c color_lights.c mongoose.c

SRCS = aqualinkd.c utils.c config.c aq_serial.c aq_panel.c aq_programmer.c net_services.c json_messages.c rs_msg_utils.c\
       devices_jandy.c packetLogger.c devices_pentair.c color_lights.c serialadapter.c aq_timer.c aq_scheduler.c web_config.c\
	   mongoose.c 


AQ_FLAGS = 
# Add source and flags depending on protocols to support.
ifeq ($(AQ_PDA), true)
  SRCS := $(SRCS) pda.c pda_menu.c pda_aq_programmer.c
  AQ_FLAGS := $(AQ_FLAGS) -D AQ_PDA
endif

ifeq ($(AQ_ONETOUCH), true)
  SRCS := $(SRCS) onetouch.c onetouch_aq_programmer.c
  AQ_FLAGS := $(AQ_FLAGS) -D AQ_ONETOUCH
endif

ifeq ($(AQ_IAQTOUCH), true)
  SRCS := $(SRCS) iaqtouch.c iaqtouch_aq_programmer.c
  AQ_FLAGS := $(AQ_FLAGS) -D AQ_IAQTOUCH
endif

ifeq ($(AQ_RS16), true)
  AQ_FLAGS := $(AQ_FLAGS) -D AQ_RS16
endif

ifeq ($(AQ_MEMCMP), true)
  AQ_FLAGS := $(AQ_FLAGS) -D AQ_MEMCMP
endif

ifeq ($(AQ_NO_THREAD_NETSERVICE), true)
  AQ_FLAGS := $(AQ_FLAGS) -D AQ_NO_THREAD_NETSERVICE
endif


# Put all flags together.
CFLAGS = $(GCCFLAGS) $(AQ_FLAGS) $(MGFLAGS)
DFLAGS = $(DGCCFLAGS) $(AQ_FLAGS) $(MGFLAGS)
DBG_CFLAGS = $(DBGFLAGS) $(AQ_FLAGS) $(MGFLAGS)

# Other sources.
#DBG_SRC = timespec_subtract.c debug_timer.c
DBG_SRC = debug_timer.c
SL_SRC = serial_logger.c aq_serial.c utils.c packetLogger.c rs_msg_utils.c
LR_SRC = log_reader.c aq_serial.c utils.c packetLogger.c
PL_EXSRC = aq_serial.c
PL_EXOBJ = aq_serial_player.o
PL_SRC := $(filter-out aq_serial.c, $(SRCS))

OBJS = $(SRCS:.c=.o)
DBG_OBJS = $(DBG_SRC:.c=.o)

SL_OBJS = $(SL_SRC:.c=.o)
LR_OBJS = $(LR_SRC:.c=.o)
PL_OBJS = $(PL_SRC:.c=.o)


# define the executable file
MAIN = ./release/aqualinkd
SLOG = ./release/serial_logger
LOGR = ./release/log_reader
PLAY = ./release/aqualinkd-player
DEBG = ./release/aqualinkd-debug


all:    $(MAIN) $(SLOG)
	$(info $(MAIN) has been compiled)
	$(info $(SLOG) has been compiled)

# debug, Just change compile flags and call MAIN
debug: CFLAGS = $(DFLAGS)
debug: $(MAIN)
	$(info $(MAIN) has been compiled)

$(MAIN): $(OBJS) 
	$(CC) $(CFLAGS) $(INCLUDES) -o $(MAIN) $(OBJS) $(LIBS)
	$(info $(MAIN) has been compiled)

slog:	$(SLOG)
	$(info $(SLOG) has been compiled)

$(SLOG): CFLAGS := $(CFLAGS) -D SERIAL_LOGGER
$(SLOG): $(SL_OBJS)
#	$(CC) $(CFLAGS) $(INCLUDES) -o $(SLOG) $(SL_OBJS) 
	$(CC) $(INCLUDES) -o $(SLOG) $(SL_OBJS) 


#.PHONY: clean_slog_o
#clean_slog_o:
#	$(RM) $(SL_OBJS)
#
#.PHONY: test
#test:	$(SLOG)
#test:	clean_slog_o
#test:	$(MAIN)

# Shouldn't need to use any of these options unless you're developing.

aqdebug: $(DEBG)
	$(info $(DEBG) has been compiled)

$(DEBG): CFLAGS = $(DBG_CFLAGS)
$(DEBG): $(OBJS) $(DBG_OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $(DEBG) $(OBJS) $(DBG_OBJS) $(DBGFLAGS) $(LIBS)

logr:	$(LOGR)
	$(info $(LOGR) has been compiled)

$(LOGR): $(LR_OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $(LOGR) $(LR_OBJS)

player:	$(PLAY)
	$(info $(PLAY) has been compiled)

$(PL_EXOBJ): $(PL_EXSRC)
	$(CC) $(CFLAGS) -D PLAYBACK_MODE $(INCLUDES) -c $(PL_EXSRC) -o $(PL_EXOBJ)

$(PLAY): $(PL_OBJS) $(PL_EXOBJ)
	$(CC) $(CFLAGS) $(INCLUDES) -o $(PLAY) $(PL_OBJS) $(PL_EXOBJ)

# Fof github publishing
.PHONY: git
git: clean $(MAIN) $(SLOG)
	./release/git_version.sh


# this is a suffix replacement rule for building .o's from .c's
# it uses automatic variables $<: the name of the prerequisite of
# the rule(a .c file) and $@: the name of the target of the rule (a .o file) 
# (see the gnu make manual section about automatic variables)
.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@


.PHONY: clean
clean:
	$(RM) *.o *~ $(MAIN) $(MAIN_U) $(PLAY) $(PL_EXOBJ) $(DEBG)
	$(RM) $(wildcard *.o) $(wildcard *~) $(MAIN) $(MAIN_U) $(PLAY) $(PL_EXOBJ) $(LOGR) $(PLAY) $(DEBG)

depend: $(SRCS)
	makedepend $(INCLUDES) $^

install: $(MAIN)
	./release/install.sh

endef

