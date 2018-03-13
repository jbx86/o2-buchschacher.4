all: oss user

oss: oss.c proj4.h
	gcc -o oss oss.c

user: user.c proj4.h
	gcc -o user user.c
