#include <stdio.h>

int main(int argc, char** argv)
{
    if(argc < 3) //argv[0] to nazwa ścieżka do .exe
    {
        printf("Program przyjmuje co najmniej 2 argumenty\n");
        return -1;
    }

    return 0;
}
