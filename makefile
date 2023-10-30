
test: test.c ompimage.h
	gcc -g test.c -o image -lm
image:image.c image.h
	gcc -g image.c -o image -lm -lpthread
ompimage: ompimage.c ompimage.h
	gcc -g -Wall -fopenmp -o image ompimage.c -lm 
clean:
	rm -f image output.png