# Makefile - Hastane Randevu Sistemi
# Kullanım: make         → derle
#           make clean   → temizle
#           make run     → derle ve çalıştır (port 8080)

CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -pthread -std=c99
TARGET  = hastane_server
SRCS    = main.c hash_table.c heap.c queue.c stack.c tree.c
OBJS    = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^
	@echo ""
	@echo "Derleme tamamlandi: ./$(TARGET)"
	@echo "Calistirmak icin: ./$(TARGET) 8080"

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

run: all
	./$(TARGET) 8080

clean:
	rm -f $(OBJS) $(TARGET)
	@echo "Temizlik tamamlandi."

# Bağımlılıklar
main.o:       main.c hash_table.h heap.h queue.h stack.h tree.h
hash_table.o: hash_table.c hash_table.h
heap.o:       heap.c heap.h
queue.o:      queue.c queue.h
stack.o:      stack.c stack.h
tree.o:       tree.c tree.h

.PHONY: all run clean
