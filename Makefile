CC = gcc
CFLAGS = -lcurl -lpthread  # Sadece gerekli bayraklar
TARGET = downloader
SRC = downloader.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(SRC) -o $(TARGET) $(CFLAGS)

clean:
	rm -f $(TARGET) part_* final_output*
