#include <stdio.h>
#include <io.h>
#include <fcntl.h>
#include "blast.h"
static unsigned read_input(void* file,unsigned char** data){
    static unsigned char bytes[16384];*data=bytes;return (unsigned)fread(bytes,1,sizeof(bytes),(FILE*)file);
}
static int write_output(void* file,unsigned char* data,unsigned size){
    return fwrite(data,1,size,(FILE*)file)!=size;
}
int main(void){
    _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
    return blast(read_input,stdin,write_output,stdout,NULL,NULL);
}
