CC      = mpicc
CFLAGS  = -Wall -Wextra -O2 -march=native -fopenmp -Isrc -Ivendor -MMD -MP
LDFLAGS = -fopenmp -lm

TARGET = sauvola
SRCS   = src/main.c src/sauvola.c src/image.c
OBJS   = $(SRCS:.c=.o)
DEPS   = $(OBJS:.o=.d)

STB_IMG   = vendor/stb_image.h
STB_WRITE = vendor/stb_image_write.h
STB_URL   = https://raw.githubusercontent.com/nothings/stb/master

.PHONY: all clean deps

all: $(STB_IMG) $(STB_WRITE) $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) 

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(STB_IMG):
	mkdir -p vendor
	curl -fsSL $(STB_URL)/stb_image.h -o $@

$(STB_WRITE):
	mkdir -p vendor
	curl -fsSL $(STB_URL)/stb_image_write.h -o $@

deps: $(STB_IMG) $(STB_WRITE)

clean:
	rm -f $(OBJS) $(DEPS) $(TARGET)

-include $(DEPS)
