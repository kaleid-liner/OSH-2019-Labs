CC = g++
LDFLAGS = -lreadline
CFLAGS = 
DEPS = cmdline.h
OBJS = cmdline.o main.o
SRCS = cmdline.cpp main.cpp
TARGET = sh

$(OBJS): $(SRCS) $(DPES)
	$(CC) -c $(CPPFLAGS) $(LDFLAGS) $< -o $@

$(TARGET): $(OBJS)
	$(CC) $(CPPFLAGS) $^ -o $@

clean:
	rm $(OBJS) $(TARGET)
