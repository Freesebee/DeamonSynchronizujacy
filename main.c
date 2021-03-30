#include <stdio.h>
#include <dirent.h>
#include <errno.h>
int main(int argc, char** argv)
{
    if(argc < 3) //argv[0] to nazwa ścieżka do .exe
    {
        printf("Program przyjmuje co najmniej 2 argumenty\n");
        return -1;
    }
    //Sprawdzanie czy sciezka zrodlowa to katalog
    if (opendir(argv[1]))
    {
        //Sprawdzanie czy sciezka docelowa to katalog
        if (opendir(argv[2]))
        {
            printf("Katalog docelowy istnieje, program -> demon\n");
        }
        else if (ENOENT == errno)
        {
            printf("Katalog docelowy nie istnieje\n");
            return -1;
        }
        else
        {
            printf("opendir() sie wyjebal na plecy z jakiegos innego powodu\n");
            return -1;
        }
    }
    else if (ENOENT == errno)
    {
        printf("Katalog zrodlowy nie istnieje\n");
        return -1;
    }
    else
    {
        printf("opendir() sie wyjebal na plecy z jakiegos innego powodu\n");
        return -1;
    }





    return 0;
}
