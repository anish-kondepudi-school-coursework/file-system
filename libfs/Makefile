# Target library
lib := libfs.a

all: $(lib)

$(lib): fs.o
	ar rcs $@ $^

fs.o: fs.c fs.h
	gcc -Werror -Wextra -c $<
