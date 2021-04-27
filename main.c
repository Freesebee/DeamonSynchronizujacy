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
#include <sys/mman.h>

#define DEFAULT_FILE_SIZE_THRESHOLD 1048576 // domyślny próg rozmiaru plików do kopiowania
#define BUFFER_SIZE 2048 //2 KB
#define DEFAULT_SLEEP_TIME 300 // domyślny czas snu (5 min)

char *source; // ścieżka do katalogu źródłowego
char *dest; // ścieżka do katalogu docelowego

bool allowRecursion; //[Parametr: -R]
// Tryb umożliwiający rekurencyjną synchronizację

int fileSizeThreshold; //[Parametr: -fs]
// Próg dzielący pliki małe od dużych przy ich kopiowaniu

int sleepTime; //[Parametr: -st]
// czas snu Daemona

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

char *AddFileNameToDirPath(char* DirPath,char* FileName)
{
    char *finalPath = malloc(sizeof(char) * (BUFFER_SIZE));
    strcpy(finalPath,DirPath);
    strcat(finalPath, "/");
    strcat(finalPath, FileName);
    return finalPath;
}

bool CompareFiles(char *sourcePath, char *destinationPath)
{
    if(destinationPath == sourcePath)
    {
        return true;
    }
    else return false;
   }

time_t ModificationTime(char *path)
{
    struct stat pathStat;
    stat(path,&pathStat);
    time_t time = pathStat.st_ctime;
    return time;
}

off_t FileSize(char *path) //jesli nie bedzie dzialalo to zmienic typ na _off_t
{
    struct stat pathStat;
    stat(path,&pathStat);
    off_t size = pathStat.st_size;
    return size;
}
// Kopiowanie pliku z katalogu 1 do katalogu 2
int CopyFileNormal(char *sourcePath, char *destinationPath)
{
    int copyFromFile = open(sourcePath, O_RDONLY);
    int copyToFile = open(destinationPath, O_WRONLY | O_CREAT | O_TRUNC, EPERM);
    char *buffer = malloc(sizeof(char) * BUFFER_SIZE);

    if (buffer == NULL) {
        syslog(LOG_ERR, "Memory allocation error");
        close(copyFromFile);
        close(copyToFile);
        return (-1);
    }

    for (;;) {
        ssize_t bytesRead = read(copyFromFile, buffer, BUFFER_SIZE);
        if (bytesRead <= 0) {
            break;
        }
        ssize_t bytesWritten = write(copyToFile, buffer, bytesRead);
        if (bytesWritten != bytesRead) {
            syslog(LOG_ERR, " Error reading file: %s or writing to file: %s",sourcePath,destinationPath);
            return(-1);
        }
    }
    syslog(LOG_NOTICE, "Copying file %s and writing to file %s was a success",sourcePath,destinationPath);
    close(copyFromFile);
    close(copyToFile);
    free(buffer);

}

int CopyFileMmap(char *sourcePath, char *destinationPath)
{
    int copyFromFile = open(sourcePath, O_RDONLY);
    int copyToFile = open(destinationPath, O_WRONLY | O_CREAT | O_TRUNC, EPERM);
    struct stat pathStat;
    stat(sourcePath,&pathStat);
    char *mappedFile = mmap(NULL,pathStat.st_size, PROT_READ, MAP_SHARED,copyFromFile,0);
    if (mappedFile <= 0)
    {
        return(-1);
    }
    ssize_t bytesWritten = write(copyToFile, mappedFile, pathStat.st_size);
    if (bytesWritten != pathStat.st_size)
    {
        syslog(LOG_ERR, " Error reading file: %s or writing to file: %s",sourcePath,destinationPath);
        return(-1);
    }
    syslog(LOG_NOTICE, "Copying file %s and writing to file %s was a success",sourcePath,destinationPath);
    close(copyFromFile);
    close(copyToFile);

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
    if(argc < 2 || argc > 8)
    {
        printf("Niepoprawna liczba argumentow\nSyntax: *source *dest [-R] [-st sleepTime] [-fs fileSizeThreshold]\n");
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

    //Domyślna inicjalizacja zmiennych globalnych
    sleepTime = DEFAULT_SLEEP_TIME;
    fileSizeThreshold = DEFAULT_FILE_SIZE_THRESHOLD;
    allowRecursion = false;

    int i;
    bool fs_set = false, st_set = false, R_set = false;

    for(i = 3; i < argc; i++)
    {
        if (strcmp(argv[i], "-R") == 0)
        {
            if(R_set)
            {
                printf("Parametr allowRecursion zosal juz zainicjalizowany\n");
                exit(EXIT_FAILURE);
            }

            allowRecursion = true;
            R_set = true;
        }
        else if (strcmp(argv[i], "-fs") == 0)
        {
            if(fs_set)
            {
                printf("Parametr fileSizeThreshold zosal juz zainicjalizowany\n");
                exit(EXIT_FAILURE);
            }

            if(++i > argc-1 || (fileSizeThreshold = atoi(argv[i])) <= 0)
            {
                printf("Parametr fileSizeThreshold musi byc liczba dodatnia\n");
                exit(EXIT_FAILURE);
            }

            fs_set = true;
        }
        else if (strcmp(argv[i], "-st") == 0)
        {
            if(st_set)
            {
                printf("Parametr sleepTime zostal juz zainicjalizowany\n");
                exit(EXIT_FAILURE);
            }

            if (++i > argc-1 || (sleepTime = atoi(argv[i])) <= 0)
            {
                printf("Parametr sleepTime musi byc liczba dodatnia\n");
                exit(EXIT_FAILURE);
            }

            st_set = true;
        }
        else
        {
            printf("Blad %d argumentu\nSyntax: *source *dest [-R] [-st sleepTime] [-fs fileSizeThreshold]\n",i);
            exit(EXIT_FAILURE);
        }
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
    char *sourcePath, *destPath;
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
            if(strcmp(entry_source->d_name, ".") == 0 || strcmp(entry_source->d_name, "..") == 0)
                continue;
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
                    if(strcmp(entry_dest->d_name, ".") == 0 || strcmp(entry_dest->d_name, "..") == 0)
                        continue;
                    sourcePath = AddFileNameToDirPath(source,entry_source->d_name);
                    destPath = AddFileNameToDirPath(dest, entry_dest->d_name);



                    if(!CompareFiles(sourcePath,destPath) ||
                    ModificationTime(sourcePath) > ModificationTime(destPath))
                    {
                        //skopiuj plik z  source do dest
                        if(FileSize(sourcePath)< fileSizeThreshold)
                        {
                            CopyFileNormal(sourcePath,destPath);
                        }
                        else
                        {
                            CopyFileMmap(sourcePath,destPath);
                        }
                    }
                    else
                    {

                        //jesli sa takie same przejdz do nast pliku sourceDir
                        break;
                    }
                    syslog(LOG_NOTICE, "CHECKING: %s WITH %s", entry_source->d_name, entry_dest->d_name);
                    free(sourcePath);
                    free(destPath);
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

    //Sprawdź czy po pobudce katalogi istnieją
    CheckPaths();

    //Synchronization();
    //sprawdzanie kopiowania (DEBUG)
//    char *src = "/home/student/przyklad1";
//    char *dest = "/home/student/Muzyka/przykladcopied";
//    CopyFileMmap(src,dest);
    syslog(LOG_NOTICE, "DAEMON EXORCUMCISED\n");
    closelog();

    exit(EXIT_SUCCESS);
}