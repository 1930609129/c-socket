all:main
main:add.o max.o main.o
	gcc -o main main.o add.o max.o
add.o:add.c
	gcc -c add.c
max.o:max.c
	gcc -c max.c
main.o:main.c
	gcc -c main.c
clean:
	rm -rf *.o main
