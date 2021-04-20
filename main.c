#include <stdio.h>
#include <dirent.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/fs.h>
#include <syslog.h> //biblioteka do logow
pid_t fork();
int setsid();
//otwiera logi, pierwszy argument jest dodawany na poczatek kazdego logu
//drugi to specjalne opcje, mozna wrzucic kilka za pomoca |
//trzeci wskazuje jaki typ programu loguje wiadomosci
//openlog("WyjebaloNamDemona",LOG_PID, LOG_LOCAL1)


//wpisuje do logow, pierwszy argument okresla importance logu, drugi to wpisywany tekst
//syslog(LOG_NOTICE, "cokolwiek")

//zamykanie logu jest opcjonalne
//closelog();

//Sprawdza czy plik o podanej  sciezce jest katalogiem
int isDirectory(const char *path) {
    struct stat statbuffer;
    if (stat(path, &statbuffer) != 0) //success = 0
        return 0;
    return S_ISDIR(statbuffer.st_mode); //0 jesli NIE jest katalogiem
}

//Sprawdza czy plik o podanej  sciezce jest plikiem
int isRegularFile(const char *path) {
    struct stat statbuffer;
    if (stat(path, &statbuffer) != 0) //success = 0
        return 0;
    return S_ISREG(statbuffer.st_mode); //0 jesli NIE jest katalogiem
}

int checkArguments(int argc, char** argv)
{
    //argv[0] to nazwa ścieżka do .exe
    if(argc < 3)
    {
        printf("Program przyjmuje co najmniej 2 argumenty\n");
        return -1;
    }

    //Sprawdzanie czy sciezka zrodlowa to katalog
    if (isDirectory(argv[1]) != 0)
    {
        printf("Sciezka zrodlowa nie jest katalogiem\n");
        return -1;
    }
//    else if (ENOENT == errno)
//    {
//        printf("Katalog zrodlowy nie istnieje\n");
//        return -1;
//    }

    //Sprawdzanie czy sciezka docelowa to katalog
    if (isDirectory(argv[2]) != 0)
    {
        printf("Sciezka docelowa nie jest katalogiem\n");
        return -1;
    }
//    else if (ENOENT == errno)
//    {
//        printf("Katalog docelowy nie istnieje\n");
//        return -1;
//    }

    return 0;
}

int main(int argc, char** argv)
{
    if(checkArguments(argc, argv) != 0){
        return -1;
    }

    pid_t pid;
    int i;
    /* stwórz nowy proces */
    pid = fork();
    if (pid == -1)
        return -1;
    else if (pid != 0)
        exit (EXIT_SUCCESS);
    /* stwórz nową sesję i grupę procesów */
    if (setsid() == -1)
        return -1;
    /* ustaw katalog roboczy na katalog główny */
    if (chdir ("/") == -1)
        return -1;
    /* zamknij wszystkie pliki otwarte - użycie opcji NR_OPEN to przesada, lecz działa */
    for (i = 0; i < NR_OPEN; i++)
        close (i);
    /* przeadresuj deskryptory plików 0, 1, 2 na /dev/null */
    open ("/dev/null", O_RDWR); /* stdin */
    dup (0); /* stdout */
    dup (0); /* stderror */

/* tu należy wykonać czynności demona…  */




    return 0;
}

