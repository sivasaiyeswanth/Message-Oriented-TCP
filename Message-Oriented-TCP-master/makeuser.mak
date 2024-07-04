user1: user1.c
	gcc -o user1 user1.c -L. -lmsocket
	./user1

user2: user2.c
	gcc -o user2 user2.c -L. -lmsocket
	./user2