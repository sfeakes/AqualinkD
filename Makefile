#
# Options
# make DEBUG=true                             // Turn on timing statments
# make EXFLAGS="-D BETA_PDA_AUTOLABEL"        // Add compile flags
#
# define the C compiler to use
CC = gcc

#LIBS := -lpthread -lm
LIBS := -l pthread -l m
#LIBS := -lpthread -lwebsockets

# debug of not
#DBG = -g -O0 -fsanitize=address -static-libasan
#DBG = -g
DBG =

# USe below to remove unused functions and global variables.
#LFLAGS = -Wl,--gc-sections,--print-gc-sections
#GCCFLAGS = -Wall -ffunction-sections -fdata-sections

# define any compile-time flags
#GCCFLAGS = -Wall -O3 -Wextra
GCCFLAGS = -Wall -O3 $(EXFLAGS)
#GCCFLAGS = -Wall

#CFLAGS = -Wall -g $(LIBS)
#CFLAGS = -Wall -g $(LIBS) -std=gnu11
CFLAGS = $(GCCFLAGS) $(DBG) $(LIBS) -D MG_DISABLE_MD5 -D MG_DISABLE_HTTP_DIGEST_AUTH -D MG_DISABLE_MD5 -D MG_DISABLE_JSON_RPC


# Add inputs and outputs from these tool invocations to the build variables 

# define the C source files
SRCS = aqualinkd.c utils.c config.c aq_serial.c init_buttons.c aq_programmer.c net_services.c json_messages.c pda.c pda_menu.c pda_aq_programmer.c packetLogger.c pentair_messages.c mongoose.c
DBG_SRC = timespec_subtract.c

# If run with `make DEBUG=true` add debug files and pass parameter for compile
ifeq ($(DEBUG), true)
  SRCS := $(SRCS) $(DBG_SRC)
  CFLAGS := $(CFLAGS) -D AQ_DEBUG
endif

SL_SRC = serial_logger.c aq_serial.c utils.c packetLogger.c
LR_SRC = log_reader.c aq_serial.c utils.c packetLogger.c
PL_EXSRC = aq_serial.c
PL_EXOBJ = aq_serial_player.o
PL_SRC := $(filter-out aq_serial.c, $(SRCS))

OBJS = $(SRCS:.c=.o)

SL_OBJS = $(SL_SRC:.c=.o)
LR_OBJS = $(LR_SRC:.c=.o)
PL_OBJS = $(PL_SRC:.c=.o)

# define the executable file
MAIN = ./release/aqualinkd
SLOG = ./release/serial_logger
LOGR = ./release/log_reader
PLAY = ./release/aqualinkd-player

all:    $(MAIN) 
  @echo: $(MAIN) have been compiled

$(MAIN): $(OBJS) 
	$(CC) $(CFLAGS) $(INCLUDES) -o $(MAIN) $(OBJS) $(LFLAGS) $(LIBS)

slog:	$(SLOG)
  @echo: $(SLOG) have been compiled

$(SLOG): $(SL_OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $(SLOG) $(SL_OBJS)

logr:	$(LOGR)
  @echo: $(LOGR) have been compiled

$(LOGR): $(LR_OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $(LOGR) $(LR_OBJS)

player:	$(PLAY)
  @echo: $(PLAY) have been compiled

$(PL_EXOBJ): $(PL_EXSRC)
	$(CC) $(CFLAGS) -D PLAYBACK_MODE $(INCLUDES) -c $(PL_EXSRC) -o $(PL_EXOBJ)

$(PLAY): $(PL_OBJS) $(PL_EXOBJ)
	$(CC) $(CFLAGS) $(INCLUDES) -o $(PLAY) $(PL_OBJS) $(PL_EXOBJ)

.PHONY: git
git: clean $(MAIN) $(SLOG)
	./release/git_version.sh

	
# this is a suffix replacement rule for building .o's from .c's
# it uses automatic variables $<: the name of the prerequisite of
# the rule(a .c file) and $@: the name of the target of the rule (a .o file) 
# (see the gnu make manual section about automatic variables)
.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $<  -o $@

.PHONY: clean
clean:
	$(RM) *.o *~ $(MAIN) $(MAIN_U) $(PLAY) $(PL_EXOBJ)
	$(RM) $(wildcard *.o) $(wildcard *~) $(MAIN) $(MAIN_U) $(PLAY) $(PL_EXOBJ) $(LOGR) $(PLAY)

depend: $(SRCS)
	makedepend $(INCLUDES) $^

install: $(MAIN)
	./release/install.sh

