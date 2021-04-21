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

pid_t pid, sid; //ID procesu oraz sesji demona

//pid_t fork();

//int setsid();

// otwiera logi, pierwszy argument jest dodawany na poczatek kazdego logu
// drugi to specjalne opcje, mozna wrzucic kilka za pomoca |
// trzeci wskazuje jaki typ programu loguje wiadomosci
//openlog("DAEMON ERROR",LOG_PID, LOG_LOCAL1);

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

void CheckArguments(int argc, char** argv, int * sleepTimePointer)
{
    switch(argc) // argv[0] to ścieżka do pliku .exe
    {
        case 3:
            *sleepTimePointer = 300;
            break;
        case 5:
            // Dodatkowa opcja -R pozwalająca na rekurencyjną synchronizację katalogów
            // (teraz pozycje będące katalogami nie są ignorowane)

            // brak break, gdyż należy zainicjalozować sleepTimePointer
        case 4:
            *sleepTimePointer = (int) argv[3];
            break;
        default:
            printf("Program przyjmuje 2 lub 3 argumenty\n");
            exit(EXIT_FAILURE);
    }

    // Sprawdzanie czy sciezka zrodlowa to katalog
    if (isDirectory(argv[1]))
    {
        printf("Sciezka zrodlowa nie jest katalogiem\n");
        exit(EXIT_FAILURE);
    }

    // Sprawdzanie czy sciezka docelowa to katalog
    if (isDirectory(argv[2]))
    {
        printf("Sciezka docelowa nie jest katalogiem\n");
        exit(EXIT_FAILURE);
    }
}

void InitializeDaemon()
{
    /* Stworzenie nowego procesu */
    pid = fork();

    switch (pid)
    {
        case -1:
            printf("Nie przypisano id procesu\n");
            exit (EXIT_FAILURE);
        case 0:
            // Instrukcje procesu potomnego
            break;
        default:
            // Instrukcje procesu macierzystego
            // exit (EXIT_SUCCESS);
            break;
    }

    /* Zmiana praw dostępu do plików za pomocą maski użytkownika */
    umask(0);

    /* Otworzenie pliku z logami */
    //openlog();

    /* Stworzenie nowej sesji i grupy procesów */
    sid = setsid();

    switch (sid)
    {
        case -1:
            printf("Nie przypisano id sesji\n");
            exit (EXIT_FAILURE);
        default:
            break;
    }

    /* Ustaw katalog roboczy na katalog główny (root) */
    if (chdir ("/")) {
        printf("Nie udało się ustawić katalogu roboczego na katalog główny\n");
        exit (EXIT_FAILURE);
    }

    /* Zamknięcie standardowych deksryptorów plików */
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
}

void SleepingAnimation(sleepTime)
{
    printf("Z");
    for (int i = 0; i < sleepTime-1; ++i) {
        sleep(1);
        printf("z");
    }
}

void Synchronization(char* source, char* dest)
{
    /* przeadresuj deskryptory plików 0, 1, 2 na /dev/null */
    open ("/dev/null", O_RDWR); /* stdin */
    dup (0); /* stdout */
    dup (0); /* stderror */

    /* Główny kod programu */
}

int main(int argc, char** argv)
{
    int sleepTime;
    CheckArguments(argc, argv, &sleepTime);

    InitializeDaemon();

    SleepingAnimation(sleepTime);
    // zastąp przez: sleep(sleepTime);

    Synchronization(argv[1], argv[2]);

    return 0;
}

