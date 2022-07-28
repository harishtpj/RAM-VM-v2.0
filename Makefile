SRC_FILES = ram-vm.cpp
CFLAGS = -Wall
CC = g++
NAME = ram-vm
DST = bin

all: build

build:
	@$(CC) $(CFLAGS) $(SRC_FILES) -o $(DST)/$(NAME)
	@echo "---> Built RAM-VM v2.0(Kt)"

build-c:
	@$gcc $(CFLAGS) vm.c -o $(DST)/$(NAME)
	@echo "---> Built RAM-VM v2.0(Kt)"

clean:
	@rm $(DST)/$(NAME)
	@echo "---> Cleaned Working Directory"