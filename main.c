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
char *source, dest; // Ścieżki do plików/katalogów
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

// Kopiowanie pliku z katalogu 1 do katalogu 2
void CopyFileNormal(char *sourcePath, char *destinationPath) {
    int copyFromFile = open(sourcePath, O_RDONLY);
    int copyToFile = open(destinationPath, O_WRONLY | O_CREAT | O_TRUNC, PERM);
    int bufferSize = 4096;
    char *buffer = (char *) malloc(sizeof(char) * (bufferSize));

    if (buffer == NULL) {
        syslog(LOG_ERR, "Memory allocation error");
        close(copyFromFile);
        close(copyToFile);
        return (-1);
    }

    for (;;) {
        ssize_t bytesRead = read(copyFromFile, buffer, bufferSize);
        if (bytesRead <= 0) {
            break;
        }
        ssize_t bytesWritten = write(copyToFile, buffer, bufferSize);
        if (bytesWritten != bytesRead) {
            syslog(LOG_ERR, "Error reading file: %s or writing to file: %s",sourcePath,destinationPath);
            return(-1);
        }
    }
    close(copyFromFile);
    close(copyToFile);
    free(buffer);

}

void WakeUp(int signal) {
    syslog(LOG_CONS, "WAKING UP DAEMON WITH SIGUSR1");
}

void GoToSleep() {
    syslog(LOG_NOTICE, "GOING TO SLEEP\n");

    sleep(sleepTime);

    syslog(LOG_NOTICE, "DAEMON WAKES UP\n");

}

void CheckArguments(int argc, char **argv) {
    switch (argc) // argv[0] to ścieżka do pliku .exe
    {
        case 3:
            sleepTime = DEFAULT_SLEEP_TIME;
            allowRecursion = false;
            break;

        case 4:
            sleepTime = atoi(argv[3]);

            if (argv[3] == "-R") {
                allowRecursion = true;
                sleepTime = DEFAULT_SLEEP_TIME;
            }
            else if (sleepTime == 0) {
                printf("Czas snu musi byc dodatnia liczba calkowita\n");
                exit(EXIT_FAILURE);
            }
            else {
                allowRecursion = false;
            }
            break;

        case 5:
            printf("Niezaimplementowane...\n");
            exit(EXIT_FAILURE);

            if (argv[3] == "-R") {
                allowRecursion = true;
            } else if (atoi(argv[3])) {
                printf("Czas snu musi byc liczba calkowita\n");
                exit(EXIT_FAILURE);
            } else {
                allowRecursion = false;
            }

            break;

        default:
            printf("Program przyjmuje 2, 3 lub 4 argumenty\n");
            exit(EXIT_FAILURE);
    }

    // Sprawdzanie czy sciezka zrodlowa to katalog
    if (isDirectory(argv[1])) {
        printf("Sciezka zrodlowa nie jest katalogiem\n");
        exit(EXIT_FAILURE);
    } else {
        source = argv[1];
    }

    // Sprawdzanie czy sciezka docelowa to katalog
    if (isDirectory(argv[2])) {
        printf("Sciezka docelowa nie jest katalogiem\n");
        exit(EXIT_FAILURE);
    } else {
        dest = argv[2];
    }
}

void CheckPaths()
{
    // Sprawdzanie czy sciezka zrodlowa to katalog
    if (isDirectory(source)) {
        syslog(LOG_ERR, "Source catalog does not exists or isn't a catalog\n");
        exit(EXIT_FAILURE);
    }

    // Sprawdzanie czy sciezka docelowa to katalog
    if (isDirectory(argv[2])) {
        syslog(LOG_ERR, "Destination catalog does not exists or isn't a catalog\n");
        exit(EXIT_FAILURE);
    }
}

void InitializeDaemon() {
    /* Stworzenie nowego procesu */
    pid_t pid = fork();
    switch (pid) {
        case -1:
            printf("Nie przypisano id procesu\n");
            exit(EXIT_FAILURE);

        case 0: // Instrukcje procesu potomnego
            break;

        default: // Instrukcje procesu macierzystego
            printf("SYNCHRONIZER PID: %d\n", (int) pid);
            exit(EXIT_SUCCESS);
            break;
    }

    /* Zmiana praw dostępu do plików za pomocą maski użytkownika */
    umask(0);

    /* Stworzenie nowej sesji i grupy procesów */
    if (setsid() < 0) {
        printf("Nie przypisano id sesji\n");
        exit(EXIT_FAILURE);
    }

    /* Ustaw katalog roboczy na katalog główny (root) */
    if (chdir("/")) {
        printf("Nie udało się ustawić katalogu roboczego na katalog główny\n");
        exit(EXIT_FAILURE);
    }

    /* Zamknięcie standardowych deksryptorów plików */
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
}

void Synchronization(char *source, char *dest, bool allowRecursion) {
    /* przeadresuj deskryptory plików 0, 1, 2 na /dev/null */
    open("/dev/null", O_RDWR); /* stdin */
    dup(0); /* stdout */
    dup(0); /* stderror */

    /* Główny kod programu */
}

int main(int argc, char **argv) {
//    ZMIENNNE GLOBALNE:
//    int sleepTime; // czas snu Daemona
//    char* source, dest; // Ścieżki do plików/katalogów
//    bool allowRecursion; // Tryb umożliwiający rekurencyjną synchronizację

//    Sprawdzenie poprawności parametrów
//    oraz inicjalizacjia zmiennych globalnych
    CheckArguments(argc, argv);

    InitializeDaemon();

//    Umożliwienie budzenia daemona sygnałem SIGUSR1
//    /bin/kill -s SIGUSR1 PID aby obudzić
    signal(SIGUSR1, WakeUp);

//    Otworzenie pliku z logami
//    cat /var/log/syslog | grep -i SYNCHRONIZER
    openlog("SYNCHRONIZER", LOG_PID, LOG_DAEMON);
    syslog(LOG_NOTICE, "DAEMON SUMMONED\n");

    GoToSleep();

//    Sprawdź czy po pobudce katalogi istnieją
    CheckPaths();

    //Synchronization(source, dest, allowRecursion);

    syslog(LOG_NOTICE, "DAEMON EXORCUMCISED\n");
    closelog();

    exit(EXIT_SUCCESS);
}

