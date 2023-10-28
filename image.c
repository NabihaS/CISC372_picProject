#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <string.h>
#include "image.h"
#include <pthread.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

/*** Version using Pthreads*****/

//An array of kernel matrices to be used for image convolution.  
//The indexes of these match the enumeration from the header file. ie. algorithms[BLUR] returns the kernel corresponding to a box blur.
Matrix algorithms[]={
    {{0,-1,0},{-1,4,-1},{0,-1,0}}, // EDGE
    {{0,-1,0},{-1,5,-1},{0,-1,0}}, // SHARPEN this works
    {{1/9.0,1/9.0,1/9.0},{1/9.0,1/9.0,1/9.0},{1/9.0,1/9.0,1/9.0}}, // BLUR
    {{1.0/16,1.0/8,1.0/16},{1.0/8,1.0/4,1.0/8},{1.0/16,1.0/8,1.0/16}}, // GAUSE_BLUR
    {{-2,-1,0},{-1,1,1},{0,1,2}}, // EMBOSS this works
    {{0,0,0},{0,1,0},{0,0,0}} // IDENTITY this works 
};


//getPixelValue - Computes the value of a specific pixel on a specific channel using the selected convolution kernel
//Paramters: srcImage:  An Image struct populated with the image being convoluted
//           x: The x coordinate of the pixel
//          y: The y coordinate of the pixel
//          bit: The color channel being manipulated
//          algorithm: The 3x3 kernel matrix to use for the convolution
//Returns: The new value for this x,y pixel and bit channel
uint8_t getPixelValue(Image* srcImage,int x,int y,int bit,Matrix algorithm){
    int px,mx,py,my,i,span;
    span=srcImage->width*srcImage->bpp;
    // for the edge pixes, just reuse the edge pixel
    px=x+1; py=y+1; mx=x-1; my=y-1;
    if (mx<0) mx=0;
    if (my<0) my=0;
    if (px>=srcImage->width) px=srcImage->width-1;
    if (py>=srcImage->height) py=srcImage->height-1;
    uint8_t result=
        algorithm[0][0]*srcImage->data[Index(mx,my,srcImage->width,bit,srcImage->bpp)]+
        algorithm[0][1]*srcImage->data[Index(x,my,srcImage->width,bit,srcImage->bpp)]+
        algorithm[0][2]*srcImage->data[Index(px,my,srcImage->width,bit,srcImage->bpp)]+
        algorithm[1][0]*srcImage->data[Index(mx,y,srcImage->width,bit,srcImage->bpp)]+
        algorithm[1][1]*srcImage->data[Index(x,y,srcImage->width,bit,srcImage->bpp)]+
        algorithm[1][2]*srcImage->data[Index(px,y,srcImage->width,bit,srcImage->bpp)]+
        algorithm[2][0]*srcImage->data[Index(mx,py,srcImage->width,bit,srcImage->bpp)]+
        algorithm[2][1]*srcImage->data[Index(x,py,srcImage->width,bit,srcImage->bpp)]+
        algorithm[2][2]*srcImage->data[Index(px,py,srcImage->width,bit,srcImage->bpp)];
    return result;
}

//convolute:  Applies a kernel matrix to an image
//Parameters: srcImage: The image being convoluted
//            destImage: A pointer to a  pre-allocated (including space for the pixel array) structure to receive the convoluted image.  It should be the same size as srcImage
//            algorithm: The kernel matrix to use for the convolution
//Returns: Nothing
void* convolute(void* arg){ // this new parameter is now the struct
    ThreadData* d = (ThreadData*)arg;
    int span=d->srcImage->bpp*d->srcImage->bpp;
    // row and pix need to be private, they are like i and j
    for (int row=d->my_start;row<d->my_end;row++){ // can parallelize this loop
        for (int pix=0;pix<d->srcImage->width;pix++){
            for (int bit=0;bit<d->srcImage->bpp;bit++){
                d->destImage->data[Index(pix,row,d->srcImage->width,bit,d->srcImage->bpp)]=getPixelValue(d->srcImage,pix,row,bit,d->algorithm);
            }
        }
    }
}
/*
 In image convolution, each channel (bit) is often processed separately, 
 and the final pixel value in the destination image is obtained by combining the results from all channels. 
*/
// Q: what is data vs bpp in Image type? BPP=bits per pix i think every image has a BPP per pixel, that needs to be iterated thru at each pix
// uint8 that get pixel returns just ensures a value btwn 0-255
// the inner most loop goes thru the color channel, i.e. bpp

//Usage: Prints usage information for the program
//Returns: -1
int Usage(){
    printf("Usage: image <filename> <type>\n\twhere type is one of (edge,sharpen,blur,gauss,emboss,identity)\n");
    return -1;
}

//GetKernelType: Converts the string name of a convolution into a value from the KernelTypes enumeration
//Parameters: type: A string representation of the type
//Returns: an appropriate entry from the KernelTypes enumeration, defaults to IDENTITY, which does nothing but copy the image.
enum KernelTypes GetKernelType(char* type){
    if (!strcmp(type,"edge")) return EDGE;
    else if (!strcmp(type,"sharpen")) return SHARPEN;
    else if (!strcmp(type,"blur")) return BLUR;
    else if (!strcmp(type,"gauss")) return GAUSE_BLUR;
    else if (!strcmp(type,"emboss")) return EMBOSS;
    else return IDENTITY;
}

//main:
//argv is expected to take 2 arguments.  First is the source file name (can be jpg, png, bmp, tga).  Second is the lower case name of the algorithm.
int main(int argc,char** argv){
    // argc is num arguments
    //argv is the input array
    // default e.g.= [./image pic1.jpg edge]
    // so to add an argument, the argc needs to check for more and thread_count needs to be argv[i] => [./image pic1.jpg edge 4]
    long t1,t2;
    t1=time(NULL);

    stbi_set_flip_vertically_on_load(0); 
    if (argc!=4) return Usage();
    char* fileName=argv[1];
    if (!strcmp(argv[1],"pic4.jpg")&&!strcmp(argv[2],"gauss")){ // its bc strcmp return 0 if its equal (which is not truthy) 
        printf("You have applied a gaussian filter to Gauss which has caused a tear in the time-space continum.\n");
    }
    // theres no error checking for if arg is not a valid filter?
    enum KernelTypes type=GetKernelType(argv[2]);

    Image srcImage,destImage,bwImage;   
    srcImage.data=stbi_load(fileName,&srcImage.width,&srcImage.height,&srcImage.bpp,0);
    if (!srcImage.data){
        printf("Error loading file %s.\n",fileName);
        return -1;
    }
    destImage.bpp=srcImage.bpp;
    destImage.height=srcImage.height;
    destImage.width=srcImage.width;
    destImage.data=malloc(sizeof(uint8_t)*destImage.width*destImage.bpp*destImage.height);

    /**** Creating threads ****/

    // last argument for thread count
    int thread_count=atoi(argv[3]);
    // array to keep track of threads. so one thread is thread_handles[i]
    pthread_t* thread_handles=(pthread_t*)malloc(thread_count*sizeof(pthread_t));
    
    // Allocate mem for all individual structs, and free later
    ThreadData* thread_datas=(ThreadData*)malloc(thread_count * sizeof(ThreadData)); 

    int local_n=srcImage.height/thread_count;
    // create threads. thread==i==rank
    for(int thread=0;thread<thread_count;thread++){ 
        thread_datas[thread].srcImage=&srcImage;
        thread_datas[thread].destImage=&destImage;
        // use a for loop to actually get the element you cant just pass a whole array to copy it into a struct value
        // algorithms[type]=algorithms[0]= {{0,-1,0},{-1,4,-1},{0,-1,0}}
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) {
                thread_datas[thread].algorithm[i][j] = algorithms[type][i][j];
            }
        }

        // define the start and end for each thread
        // FLAG. try printing these to make sure theyre actually updated
        thread_datas[thread].my_start=local_n*thread;
        thread_datas[thread].my_end=thread_datas[thread].my_start+local_n;
        // convolute will now be executed as 'main' for threads
        pthread_create(&thread_handles[thread],NULL,&convolute,&thread_datas[thread]); //so this struct is being passed in for each thread with corresponding data
        // how do i know each thread is properly writing to the DestImage??

        // FLAG account for the remainder rows if its not evenly divisible by thread count
        // FLAG right now your threads dont know their rank. add rank to the struct and allow convolute to have rank?

        // or do i free the struct here ????
        
    }
    // do the join 
    for(int thread=0;thread<thread_count;thread++){
        pthread_join(thread_handles[thread], NULL);
    }


    stbi_write_png("output.png",destImage.width,destImage.height,destImage.bpp,destImage.data,destImage.bpp*destImage.width);
    stbi_image_free(srcImage.data);
    free(thread_handles);
    free(thread_datas);
    free(destImage.data);
    t2=time(NULL);
    printf("Took %ld seconds\n",t2-t1);
   return 0;
}