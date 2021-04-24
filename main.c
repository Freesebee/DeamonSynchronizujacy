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
#include <limits.h>
#include <string.h>

#define DEFAULT_SLEEP_TIME 300 // domyślny czas snu (5 min)

int sleepTime; // czas snu Daemona
char *source; // ścieżka do katalogu źródłowego
char *dest; // ścieżka do katalogu docelowego
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

// Sprawdza czy plik o podanej ścieżce jest dowiązaniem symbolicznym
int isSymbolicLink(const char *path)
{
    struct stat buf;
    int x = lstat (path, &buf);

    if (S_ISLNK(buf.st_mode))
        return 1; // Jest dowiązaniem symbolicznym
    else
        return 0;

/* TODO: Gdy zadziała wywalić poniższy kod */

//    x = stat ("junklink", &buf);
//    if (S_ISLNK(buf.st_mode)) printf (" stat says link\n");
//    if (S_ISREG(buf.st_mode)) printf (" stat says file\n");
//
//    x = lstat ("junklink", &buf);
//    if (S_ISLNK(buf.st_mode)) printf ("lstat says link\n");
//    if (S_ISREG(buf.st_mode)) printf ("lstat says file\n");
}

// Kopiowanie pliku z katalogu 1 do katalogu 2
void CopyFileNormal(char *sourcePath, char *destinationPath)
{
    int copyFromFile = open(sourcePath, O_RDONLY);
    int copyToFile = open(destinationPath, O_WRONLY | O_CREAT | O_TRUNC, EPERM);
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

void WakeUp(int signal)
{
    syslog(LOG_CONS, "WAKING UP DAEMON WITH SIGUSR1");
}

void GoToSleep()
{
    syslog(LOG_NOTICE, "GOING TO SLEEP\n");

    sleep(sleepTime);

    syslog(LOG_NOTICE, "DAEMON WAKES UP\n");

}

void CheckArguments(int argc, char **argv)
{
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
    if (isDirectory(argv[1]) == 0)
    {
        printf("Sciezka zrodlowa nie jest katalogiem\n");
        exit(EXIT_FAILURE);
    }
    else {
        source = realpath(argv[1], NULL);

        //TODO: Nie wiem czy powinniśmy przechowywać wskaźnik do argumentów programu czy
        //      zaalokować pamięć dla zmiennych
    }

    // Sprawdzanie czy sciezka docelowa to katalog
    if (isDirectory(argv[2]) == 0)
    {
        printf("Sciezka docelowa nie jest katalogiem\n");
        exit(EXIT_FAILURE);
    }
    else {
        dest = realpath(argv[2], NULL);
    }
}

void CheckPaths()
{
    // Sprawdzanie czy sciezka zrodlowa to katalog
    if (isDirectory(source) == 0)
    {
        syslog(LOG_ERR, "Ścieżka źródłowa nie istnieje/nie jest katalogiem\n");
        exit(EXIT_FAILURE);
    }

    // Sprawdzanie czy sciezka docelowa to katalog
    if (isDirectory(dest) == 0)
    {
        syslog(LOG_ERR, "Ścieżka docelowa nie istnieje/nie jest katalogiem\n");
        exit(EXIT_FAILURE);
    }
}

void InitializeDaemon()
{
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

    /* przeadresuj deskryptory plików 0, 1, 2 na /dev/null */
    open("/dev/null", O_RDWR); /* stdin */
    dup(0); /* stdout */
    dup(0); /* stderror */
    //TODO: @DevToxxy wytłumacz @FreeseBee do czego to służy
}

void Synchronization()
{
    DIR *dir_dest, *dir_source;
    struct dirent *entry_dest, *entry_source;

    dir_source = opendir(source);
    switch (errno) {
        case EACCES:
            syslog(LOG_ERR, "EACCES: %s",source);
            break;
        case EBADF  :
            syslog(LOG_ERR, "EBADF: %s",source);
            break;
        case EMFILE :
            syslog(LOG_ERR, "EMFILE: %s",source);
            break;
        case ENFILE   :
            syslog(LOG_ERR, "ENFILE: %s",source);
            break;
        case ENOENT :
            syslog(LOG_ERR, "ENOENT: %s",source);
            break;
        case ENOMEM :
            syslog(LOG_ERR, "ENOMEM: %s",source);
            break;
        case ENOTDIR:
            syslog(LOG_ERR, "ENOTDIR: %s",source);
            break;
    }

    if (dir_source)
    {
        syslog(LOG_NOTICE, "LOOKING INTO: %s", (char*)dir_source);

        while ((entry_source = readdir(dir_source)) != NULL)
        {
            //TODO: Naprawić sprawdzanie twardych dowiązań katalogu źródłowego
//            if(strcmp(entry_source->d_name, ".\0") == 0 || strcmp(entry_source->d_name, "..\0") == 0)
//                break;

            dir_dest = opendir(dest);

            switch (errno) {
                case EACCES:
                    syslog(LOG_ERR, "EACCES: %s",dest);
                    break;
                case EBADF  :
                    syslog(LOG_ERR, "EBADF: %s",dest);
                    break;
                case EMFILE :
                    syslog(LOG_ERR, "EMFILE: %s",dest);
                    break;
                case ENFILE   :
                    syslog(LOG_ERR, "ENFILE: %s",dest);
                    break;
                case ENOENT :
                    syslog(LOG_ERR, "ENOENT: %s",dest);
                    break;
                case ENOMEM :
                    syslog(LOG_ERR, "ENOMEM: %s",dest);
                    break;
                case ENOTDIR:
                    syslog(LOG_ERR, "ENOTDIR: %s",dest);
                    break;
            }

            if(dir_dest)
            {
                syslog(LOG_NOTICE, "COMPARING WITH: %s", (char*)dir_dest);

                while((entry_dest = readdir(dir_dest)) != NULL)
                {
                    //TODO: Naprawić sprawdzanie twardych dowiązań katalogi docelowego
//                    if(strcmp(entry_dest->d_name, ".\0") == 0 || strcmp(entry_dest->d_name, "..\0") == 0)
//                        break;

                    syslog(LOG_NOTICE, "CHECKING: %s WITH %s", entry_source->d_name, entry_dest->d_name);
                }

                closedir(dir_dest);
            }
        }

        closedir(dir_source);
    }
}

int main(int argc, char **argv) {

//    Sprawdzenie poprawności parametrów
//    oraz inicjalizacja zmiennych globalnych:
//    - int sleepTime; // czas snu Daemona
//    - char* source, dest; // Ścieżki do plików/katalogów źródłowego i docelowego
//    - bool allowRecursion; // Tryb umożliwiający rekurencyjną synchronizację
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

    Synchronization();

    syslog(LOG_NOTICE, "DAEMON EXORCUMCISED\n");
    closelog();

    exit(EXIT_SUCCESS);
}

