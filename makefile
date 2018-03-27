# Makefile
TARGET = hgt2png

CC_BIN = g++
CC_FLG = -std=c++11 -Wall -O3

$(TARGET): 
	$(CC_BIN) $(CC_FLG) hgt2png.cpp -o $(TARGET) -lpng -lz

.PHONY: clean
clean:
	@rm -f $(TARGET)