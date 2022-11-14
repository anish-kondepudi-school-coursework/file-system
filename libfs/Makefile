# Target library
lib := libfs.a

all: $(lib)

$(lib): fs.o disk.o
	ar rcs $@ $^

fs.o: fs.c fs.h
	gcc -Werror -Wextra -c $<

disk.o: disk.c disk.h
	gcc -Werror -Wextra -c $<

clean:
	rm -rf $(lib) *.o
