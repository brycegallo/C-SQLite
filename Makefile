CC=gcc
db: db.c
	$(CC) db.c -o db -Wall -Wextra -pedantic -std=gnu99
