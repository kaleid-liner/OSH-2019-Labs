CC = g++
LDFLAGS = -lreadline
CPPFLAGS = -std=c++14
OBJS = cmdline.o shstat.o main.o
DEPS = cmdline.h shstat.h
SRCDIR = ./src
TARGET = sh
SRCDEPS = $(addprefix $(SRCDIR)/,$(DEPS))

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(CPPFLAGS) $(LDFLAGS)

$(OBJS): %.o: $(SRCDIR)/%.cpp $(SRCDEPS)
	$(CC) -g -c -o $@ $(CPPFLAGS) $<

.PHONY: clean
clean:
	rm -rf $(OBJS) $(TARGET)
