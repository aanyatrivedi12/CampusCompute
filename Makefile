.RECIPEPREFIX := >

CC = gcc
CPPFLAGS = -D_POSIX_C_SOURCE=200809L
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -pthread -MMD -MP
LDLIBS = -pthread -lrt

TARGET = campuscompute

SOURCES = main.c job.c scheduler.c banker.c logger.c ipc.c worker.c
OBJECTS = $(SOURCES:.c=.o)
DEPS = $(OBJECTS:.o=.d)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
>$(CC) $(LDFLAGS) -o $@ $(OBJECTS) $(LDLIBS)

%.o: %.c
>$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

clean:
>$(RM) $(OBJECTS) $(DEPS) $(TARGET)

-include $(DEPS)
