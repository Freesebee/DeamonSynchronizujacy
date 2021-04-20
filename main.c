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
    if (stat(path, &statbuffer) != 0)
        return 0;
    return S_ISDIR(statbuffer.st_mode);
}
int isRegularFile(const char *path) {
    struct stat statbuffer;
    if (stat(path, &statbuffer) != 0)
        return 0;
    return S_ISREG(statbuffer.st_mode);
}

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

            return 0;
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
/* tu należy wykonać czynności demona…  */




    return 0;
}

