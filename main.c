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
#include <ctype.h>
#include <syslog.h> //biblioteka do logow
#include <stdbool.h>
#include <signal.h>

#define DEFAULT_SLEEP_TIME 300

int sleepTime; // czas snu Daemona
char* source, dest; // Ścieżki do plików/katalogów
bool allowRecursion; // Tryb umożliwiający rekurencyjną synchronizację

// otwiera logi, pierwszy argument jest dodawany na poczatek kazdego logu
// drugi to specjalne opcje, mozna wrzucic kilka za pomoca |
// trzeci wskazuje jaki typ programu loguje wiadomosci
//openlog("DAEMON:",LOG_PID, LOG_LOCAL1);

// wpisuje do logow, pierwszy argument okresla importance logu, drugi to wpisywany tekst
//syslog(LOG_NOTICE, "cokolwiek")

// zamykanie logu jest opcjonalne
//closelog();

// Sprawdza czy plik o podane sj  sciezce jest katalogiem
int isDirectory(const char *path) {
    struct stat statbuffer;
    if (stat(path, &statbuffer) != 0) //success = 0
        return 0;
    return S_ISDIR(statbuffer.st_mode); //0 jesli NIE jest katalogiem
}

// Sprawdza czy plik o podanej  sciezce jest plikiem
int isRegularFile(const char *path) {
    struct stat statbuffer;
    if (stat(path, &statbuffer) != 0) //success = 0
        return 0;
    return S_ISREG(statbuffer.st_mode); //0 jesli NIE jest katalogiem
}

// /bin/kill -s SIGUSR1 PID aby obudzić
void WakeUpDaemon(int signal)
{
    syslog(LOG_CONS, "WAKING UP DAEMON WITH SIGUSR1");
}

void Sleeping()
{
    signal(SIGUSR1, WakeUpDaemon);

    syslog(LOG_NOTICE, "GOING TO SLEEP\n");

    sleep(sleepTime);

    syslog(LOG_NOTICE, "DAEMON WAKES UP\n");

}

void CheckArguments(int argc, char** argv)
{
    switch(argc) // argv[0] to ścieżka do pliku .exe
    {
        case 3:
            sleepTime = DEFAULT_SLEEP_TIME;
            allowRecursion = false;
            break;

        case 4:
            sleepTime = atoi(argv[3]);

            if(argv[3] == "-R")
            {
                allowRecursion = true;
                sleepTime = DEFAULT_SLEEP_TIME;
            }
            else if (sleepTime == 0)
            {
                printf("Czas snu musi byc dodatnia liczba calkowita\n");
                exit(EXIT_FAILURE);
            }
            else
            {
                allowRecursion = false;
            }
            break;

        case 5:
            printf("Niezaimplementowane...\n");
            exit(EXIT_FAILURE);

            if(argv[3] == "-R")
            {
                allowRecursion = true;
            }
            else if (atoi(argv[3]))
            {
                printf("Czas snu musi byc liczba calkowita\n");
                exit(EXIT_FAILURE);
            }
            else
            {
                allowRecursion = false;
            }

            break;

        default:
            printf("Program przyjmuje 2, 3 lub 4 argumenty\n");
            exit(EXIT_FAILURE);
    }

    // Sprawdzanie czy sciezka zrodlowa to katalog
    if (isDirectory(argv[1]))
    {
        printf("Sciezka zrodlowa nie jest katalogiem\n");
        exit(EXIT_FAILURE);
    }
    else
    {
        source = argv[1];
    }

    // Sprawdzanie czy sciezka docelowa to katalog
    if (isDirectory(argv[2]))
    {
        printf("Sciezka docelowa nie jest katalogiem\n");
        exit(EXIT_FAILURE);
    }
    else
    {
        dest = argv[2];
    }
}

void InitializeDaemon()
{
    /* Stworzenie nowego procesu */
    pid_t pid = fork();
    switch (pid)
    {
        case -1:
            printf("Nie przypisano id procesu\n");
            exit (EXIT_FAILURE);

        case 0: // Instrukcje procesu potomnego
            break;

        default: // Instrukcje procesu macierzystego
            printf("SYNCHRONIZER PID: %d\n", (int)pid);
            exit (EXIT_SUCCESS);
            break;
    }

    /* Zmiana praw dostępu do plików za pomocą maski użytkownika */
    umask(0);

    /* Stworzenie nowej sesji i grupy procesów */
    if (setsid() < 0)
    {
        printf("Nie przypisano id sesji\n");
        exit(EXIT_FAILURE);
    }

    /* Ustaw katalog roboczy na katalog główny (root) */
    if (chdir ("/"))
    {
        printf("Nie udało się ustawić katalogu roboczego na katalog główny\n");
        exit (EXIT_FAILURE);
    }

    /* Zamknięcie standardowych deksryptorów plików */
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
}

void Synchronization(char* source, char* dest, bool allowRecursion)
{
    /* przeadresuj deskryptory plików 0, 1, 2 na /dev/null */
    open ("/dev/null", O_RDWR); /* stdin */
    dup (0); /* stdout */
    dup (0); /* stderror */

    /* Główny kod programu */
}

int main(int argc, char** argv)
{
//    ZMIENNNE GLOBALNE:
//    int sleepTime; // czas snu Daemona
//    char* source, dest; // Ścieżki do plików/katalogów
//    bool allowRecursion; // Tryb umożliwiający rekurencyjną synchronizację

    CheckArguments(argc, argv);

    InitializeDaemon();

    /* Otworzenie pliku z logami */
    /* cat /var/log/syslog | grep -i SYNCHRONIZER */
    openlog("SYNCHRONIZER",LOG_PID, LOG_DAEMON);
    syslog(LOG_NOTICE, "DAEMON SUMMONED\n");

    Sleeping();

    //Synchronization(source, dest, allowRecursion);

    syslog(LOG_NOTICE, "DAEMON EXORCISMED\n");
    closelog();

    exit(EXIT_SUCCESS);
}

