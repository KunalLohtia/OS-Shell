all: simpleshell

sshell: simpleshell.c
	gcc -Wall -Wextra -Werror -o simpleshell simpleshell.c

clean:
	rm -f simpleshell
