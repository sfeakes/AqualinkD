
# define the C compiler to use
CC = gcc

LIBS := -lpthread -lm
#LIBS := -lpthread -lwebsockets

# debug of not
#$DBG = -g
$DBG =

# define any compile-time flags
#CFLAGS = -Wall -g -lpthread -lwiringPi -lm -I. 
#CFLAGS = -Wall -g $(LIBS) -I/usr/local/include/ -L/usr/local/lib/
#CFLAGS = -Wall -g $(LIBS) -std=gnu11 -I/nas/data/Development/Raspberry/aqualink/libwebsockets-2.0-stable/lib -L/nas/data/Development/Raspberry/aqualink/libwebsockets-2.0-stable/lib
#CFLAGS = -Wall -g $(LIBS)
#CFLAGS = -Wall -g $(LIBS) -std=gnu11
CFLAGS = -Wall $(DBG) $(LIBS) -D MG_DISABLE_MD5 -D MG_DISABLE_HTTP_DIGEST_AUTH -D MG_DISABLE_MD5 -D MG_DISABLE_JSON_RPC
#CFLAGS = -Wall $(DBG) $(LIBS) -D MG_DISABLE_MQTT -D MG_DISABLE_MD5 -D MG_DISABLE_HTTP_DIGEST_AUTH -D MG_DISABLE_MD5 -D MG_DISABLE_JSON_RPC

INCLUDES = -I/nas/data/Development/Raspberry/aqualink/aqualinkd

# Add inputs and outputs from these tool invocations to the build variables 

# define the C source files
SRCS = aqualinkd.c utils.c config.c aq_serial.c init_buttons.c aq_programmer.c net_services.c json_messages.c mongoose.c

SL_SRC = serial_logger.c aq_serial.c utils.c
AL_SRC = aquarite_logger.c aq_serial.c utils.c
AR_SRC = aquarite/aquarited.c aquarite/ar_net_services.c aquarite/ar_config.c aq_serial.c utils.c mongoose.c json_messages.c config.c

OBJS = $(SRCS:.c=.o)
SL_OBJS = $(SL_SRC:.c=.o)
AL_OBJS = $(AL_SRC:.c=.o)
AR_OBJS = $(AR_SRC:.c=.o)

# define the executable file
MAIN = ./release/aqualinkd
SLOG = ./release/serial_logger
AQUARITELOG = ./release/aquarite_logger
AQUARITED = ./release/aquarited

all:    $(MAIN) 
  @echo: $(MAIN) have been compiled

$(MAIN): $(OBJS) 
	$(CC) $(CFLAGS) $(INCLUDES) -o $(MAIN) $(OBJS) $(LFLAGS) $(LIBS)


slog:	$(SLOG)
  @echo: $(SLOG) have been compiled

$(SLOG): $(SL_OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $(SLOG) $(SL_OBJS)

aquaritelog:	$(AQUARITELOG)
  @echo: $(AQUARITELOG) have been compiled

$(AQUARITELOG): $(AL_OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $(AQUARITELOG) $(AL_OBJS)

aquarited:	$(AQUARITED)
  @echo: $(AQUARITED) have been compiled

$(AQUARITED): $(AR_OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $(AQUARITED) $(AR_OBJS)

# this is a suffix replacement rule for building .o's from .c's
# it uses automatic variables $<: the name of the prerequisite of
# the rule(a .c file) and $@: the name of the target of the rule (a .o file) 
# (see the gnu make manual section about automatic variables)
.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $<  -o $@

clean:
	$(RM) *.o *~ $(MAIN) $(MAIN_U)

depend: $(SRCS)
	makedepend $(INCLUDES) $^

install: $(MAIN)
	./release/install.sh
  

# All Target
#all: aqualinkd
#
# Tool invocations
#aqualinkd: $(OBJS) $(USER_OBJS)
#	@echo 'Building target: $@'
#	@echo 'Invoking: GCC C Linker'
#	gcc -L/home/perry/workspace/libwebsockets/Debug -pg -o"aqualinkd" $(OBJS) $(USER_OBJS) $(LIBS)
#	@echo 'Finished building target: $@'
#	@echo ' '
#
# Other Targets
#clean:
#	-$(RM) $(OBJS)$(C_DEPS)$(EXECUTABLES) aqualinkd
#	-@echo ' '
#
#.PHONY: all clean dependents
#.SECONDARY:
#
#-include ../makefile.targets
