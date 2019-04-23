CC = g++
LDFLAGS = -lreadline
CFLAGS = -std=c++11
DEPS = cmdline.h
OBJS = cmdline.o main.o
TARGET = sh

%.o: %.cpp $(DPES)
	$(CC) -g -c -o $@ $(CPPFLAGS) $<

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(CPPFLAGS) $(LDFLAGS)

clean:
	rm $(OBJS) $(TARGET)
