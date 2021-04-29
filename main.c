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
#include <syslog.h>
#include <stdbool.h>
#include <signal.h>
#include <limits.h>
#include <string.h>
#include <sys/mman.h>

#define DEFAULT_FILE_SIZE_THRESHOLD 1048576 // domyślny próg rozmiaru plików do kopiowania (1MB)
#define BUFFER_SIZE 2048 //domyślny rozmiar bufora(2 KB)
#define DEFAULT_SLEEP_TIME 300 // domyślny czas snu (5 min)

char *source; // ścieżka do katalogu źródłowego
char *dest; // ścieżka do katalogu docelowego

bool allowRecursion = false; //[Parametr: -R]
// Tryb umożliwiający rekurencyjną synchronizację

int fileSizeThreshold = DEFAULT_FILE_SIZE_THRESHOLD; //[Parametr: -fs]
// Próg dzielący pliki małe od dużych przy ich kopiowaniu

int sleepTime = DEFAULT_SLEEP_TIME; //[Parametr: -st]
// czas snu Daemona

// otwiera logi, pierwszy argument jest dodawany na poczatek kazdego logu
// drugi to specjalne opcje, mozna wrzucic kilka za pomoca |
// trzeci wskazuje jaki typ programu loguje wiadomosci
//openlog("DAEMON:",LOG_PID, LOG_LOCAL1);

// wpisuje do logow, pierwszy argument okresla importance logu, drugi to wpisywany tekst
//syslog(LOG_NOTICE, "cokolwiek")

// zamykanie logu jest opcjonalne
//closelog();

// Sprawdza czy plik o podanej sciezce jest katalogiem
int isDirectory(const char *path) {
    struct stat statbuffer;
    if (stat(path, &statbuffer) != 0) //poprawne zadzialanie f. = 0
        return 0;
    return S_ISDIR(statbuffer.st_mode); //0 jesli NIE jest katalogiem
}

// Sprawdza czy plik o podanej  sciezce jest plikiem
int isRegularFile(const char *path) {
    struct stat statbuffer;
    if (stat(path, &statbuffer) != 0) //poprawne zadzialanie f. = 0
        return 0;
    return S_ISREG(statbuffer.st_mode); //0 jesli NIE jest plikiem
}
//Dodaje nazwe pliku do sciezki
char *AddFileNameToDirPath(char* DirPath,char* FileName)
{
    char *finalPath = malloc(sizeof(char) * 4096 ); //max dlugosc sciezki w linuxie to 4096 znakow
    if(finalPath==NULL) //jesli malloc nie zadziala
    {
        syslog(LOG_ERR, "memory allocation error \n errno = %d", errno);
        exit(EXIT_FAILURE);
    }
    strcpy(finalPath,DirPath);
    strcat(finalPath, "/");
    strcat(finalPath, FileName);
    return finalPath;
}
//czas modyfikacji danego pliku
time_t ModificationTime(char *path)
{

    struct stat pathStat;
    if (stat(path, &pathStat) != 0) //poprawne zadzialanie f. = 0
        return 0;
    time_t time = pathStat.st_ctime;
    return time;
}
//rozmiar danego pliku
off_t FileSize(char *path)
{
    struct stat pathStat;
    if (stat(path, &pathStat) != 0) //poprawne zadzialanie f. = 0
        return 0;
    off_t size = pathStat.st_size;
    return size;
}
//Tryb danego katalogu np 0777
mode_t DirectoryMode(char *path)
{
    struct stat pathStat;
    if (stat(path, &pathStat) != 0) //poprawne zadzialanie f. = 0
        return 0;
    mode_t mode = pathStat.st_mode;
    return mode;
}

// Kopiowanie pliku z katalogu 1 do katalogu 2 za pomoca read/write
int CopyFileNormal(char *sourcePath, char *destinationPath)
{
    int copyFromFile = open(sourcePath, O_RDONLY);
    int copyToFile = open(destinationPath, O_WRONLY | O_CREAT | O_TRUNC, EPERM);
    char *buffer = malloc(sizeof(char) * BUFFER_SIZE);

    if (buffer == NULL) { //jesli malloc nie zadziala
        syslog(LOG_ERR, "COPY[read/write]:MEMORY ALLOCATION ERROR");
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
            syslog(LOG_ERR, "ERROR READING FILE: %s OR WRITING TO: %s",sourcePath,destinationPath);
            return(-1);
        }
    }
    syslog(LOG_NOTICE, "COPYING [read/write] %s TO %s",sourcePath,destinationPath);
    close(copyFromFile);
    close(copyToFile);
    free(buffer);

}
// Kopiowanie pliku z katalogu 1 do katalogu 2 za pomoca mmap/write
int CopyFileMmap(char *sourcePath, char *destinationPath)
{
    int copyFromFile = open(sourcePath, O_RDONLY);
    int copyToFile = open(destinationPath, O_WRONLY | O_CREAT | O_TRUNC, EPERM);
    struct stat pathStat;
    stat(sourcePath,&pathStat);
    char *mappedFile = mmap(NULL,pathStat.st_size, PROT_READ, MAP_SHARED,copyFromFile,0);
    if (mappedFile == NULL)
    {
        return(-1);
    }
    ssize_t bytesWritten = write(copyToFile, mappedFile, pathStat.st_size);
    if (bytesWritten != pathStat.st_size)
    {
        syslog(LOG_ERR, "ERROR READING FILE: %s OR WRITING TO: %s",sourcePath,destinationPath);
        return(-1);
    }
    syslog(LOG_NOTICE, "COPYING [mmap] %s TO %s",sourcePath,destinationPath);
    close(copyFromFile);
    close(copyToFile);

}
// Usuwanie pliku / katalogu
void DeleteEntry(char *relativePath)
{
    if (isDirectory(relativePath))
    {
        if (unlinkat(AT_FDCWD, relativePath, AT_REMOVEDIR))
        {
            if(errno == ENOTEMPTY)
            {
                DIR* dir;
                struct dirent *entry;

                dir = opendir(relativePath);

                if(dir == NULL)
                {
                    syslog(LOG_ERR, "ERROR WHILE OPENING DIR TO DELETE: %s\nerrno = %d\n", relativePath, errno);
                    exit(EXIT_FAILURE);
                }

                while((entry = readdir(dir)) != NULL)
                {
                    if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                        continue;

                    DeleteEntry(AddFileNameToDirPath(relativePath, entry->d_name));
                }

                if (unlinkat(AT_FDCWD, relativePath, AT_REMOVEDIR))
                {
                    syslog(LOG_ERR, "Error while deleting dir: %s\n errno = %d\n", relativePath, errno);
                    exit(EXIT_FAILURE);
                }
            }
            else
            {
                syslog(LOG_ERR, "Error while deleting dir: %s\n errno = %d\n", relativePath, errno);
                exit(EXIT_FAILURE);
            }
        }

        syslog(LOG_NOTICE, "DELETED DIRECTORY: %s\n", relativePath);
    }
    else
    {
        if (unlink(relativePath))
        {
            syslog(LOG_ERR, "Error while deleting file: %s\n errno = %d", relativePath, errno);
            exit(EXIT_FAILURE);
        }

        syslog(LOG_NOTICE, "DELETED FILE: %s\n", relativePath);
    }
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

void InitializeParameters(int argc, char **argv)
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
        syslog(LOG_ERR, "Source path is not a directory\n");
        exit(EXIT_FAILURE);
    }

    // Sprawdzanie czy sciezka docelowa to katalog
    if (isDirectory(dest) == 0)
    {
        syslog(LOG_ERR, "Destination path is not a directory\n");
        exit(EXIT_FAILURE);
    }
}

void InitializeDaemon()
{
    //Stworzenie nowego procesu
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

    //Zmiana praw dostępu do plików za pomocą maski użytkownika
    umask(0);

    //Stworzenie nowej sesji i grupy procesów
    if (setsid() < 0) {
        printf("Nie przypisano id sesji\n");
        exit(EXIT_FAILURE);
    }

    //Ustaw katalog roboczy na katalog główny (root)
    if (chdir("/")) {
        printf("Nie udało się ustawić katalogu roboczego na katalog główny\n");
        exit(EXIT_FAILURE);
    }

    //Zamknięcie standardowych deksryptorów plików
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    //przeadresuj deskryptory plików 0, 1, 2 na /dev/null
    open("/dev/null", O_RDWR); // stdin
    dup(0);  //stdout
    dup(0); //stderror
}

void SyncCopy(char *sourceA, char *destA)
{
    bool allowCopy;
    DIR *dir_dest, *dir_source;
    struct dirent *entry_dest, *entry_source;
    char *sourcePath, *destPath;
    time_t srcModTime;
    time_t dstModTime;
    dir_source = opendir(sourceA);

    if (dir_source == NULL)
    {
        syslog(LOG_ERR, "SOURCE PATH ERROR:%d\n", errno);
        exit(EXIT_FAILURE);
    }

    while ((entry_source = readdir(dir_source)) != NULL)
    {
        sourcePath = AddFileNameToDirPath(sourceA, entry_source->d_name);
        destPath = AddFileNameToDirPath(destA, entry_source->d_name);

        //pomijanie twardych dowiązań do obecnego i nadrzędnego katalogu
        if ((strcmp(entry_source->d_name, ".") == 0 || strcmp(entry_source->d_name, "..") == 0)
            || (!allowRecursion && isDirectory(sourcePath)) || !(isRegularFile(sourcePath) || isDirectory(sourcePath)))
        {
            free(sourcePath);
            free(destPath);
            continue;
        }

        dir_dest = opendir(destA);

        if (dir_dest == NULL)
        {
            syslog(LOG_ERR, "DESTINATION PATH ERROR:%d\n", errno);
            exit(EXIT_FAILURE);
        }

        allowCopy = true;

        while ((entry_dest = readdir(dir_dest)) != NULL)
        {
            if (strcmp(entry_dest->d_name, ".") == 0 || strcmp(entry_dest->d_name, "..") == 0)
                continue;


            srcModTime = ModificationTime(sourcePath);
            dstModTime = ModificationTime(destPath);
            //sprawdzanie czy w katalogu docelowym jest taki sam plik/katalog jak w katalogu zrodlowym
            if (strcmp(entry_source->d_name,entry_dest->d_name) == 0 && srcModTime < dstModTime)
            {
                if (isRegularFile(sourcePath))
                {
                    allowCopy = false;
                    break;
                }
                else if (isDirectory(sourcePath) && allowRecursion)
                {
                   int createSuccess = mkdir(destPath, DirectoryMode(sourcePath));
                    if(createSuccess == 0)
                        syslog(LOG_NOTICE, "CREATED DIRECTORY: %s",destPath);
                    SyncCopy(sourcePath,destPath);
                }
            }
        }
        //jesli w katalogu docelowym nie ma takiego samego pliku/katalogu jak w katalogu zrodlowym
        //lub jest ale ma starszą datę modyfikacji
        if(allowCopy)
        {
            if (isRegularFile(sourcePath))
            {
                if (FileSize(sourcePath) < fileSizeThreshold) {
                    CopyFileNormal(sourcePath, destPath);
                } else {
                    CopyFileMmap(sourcePath, destPath);
                }
            }
            else if (isDirectory(sourcePath) && allowRecursion)
            {
                int createSuccess = mkdir(destPath, DirectoryMode(sourcePath));
                if(createSuccess == 0)
                    syslog(LOG_NOTICE, "CREATED DIRECTORY: %s",destPath);
                SyncCopy(sourcePath,destPath);
            }

        }

        closedir(dir_dest);
        free(sourcePath);
        free(destPath);
    }

    closedir(dir_source);
}

void SyncDelete(char *sourceDirPath, char *destDirPath)
{
    DIR *dir_dest, *dir_source;
    struct dirent *entryDest, *entrySource;
    char *sourceEntryPath, *destEntryPath;

    bool allowDelete;

    dir_dest = opendir(destDirPath);

    if (dir_dest == NULL) {
        syslog(LOG_ERR, "DELETE:OPENDIR(SOURCE) RETURNED WITH ERROR:%d\n", errno);
        exit(EXIT_FAILURE);
    }

    while ((entryDest = readdir(dir_dest)) != NULL)
    {
        allowDelete = true;

        destEntryPath = AddFileNameToDirPath(dest, entryDest->d_name);

        //pomijanie twardych dowiązań do obecnego i nadrzędnego katalogu
        if ((strcmp(entryDest->d_name, ".") == 0 || strcmp(entryDest->d_name, "..") == 0)
            || (allowRecursion == false && isRegularFile(destEntryPath) == 0)) //jeżeli nie jest plikiem
        {
            free(destEntryPath);
            continue;
        }

        dir_source = opendir(sourceDirPath);

        if (dir_source == NULL) {
            syslog(LOG_ERR, "DELETE:OPENDIR(DEST) RETURNED WITH ERROR:%d\n", errno);
            exit(EXIT_FAILURE);
        }

        while ((entrySource = readdir(dir_source)) != NULL)
        {
            if (strcmp(entrySource->d_name, ".") == 0 || strcmp(entrySource->d_name, "..") == 0)
                continue;

            sourceEntryPath = AddFileNameToDirPath(source, entrySource->d_name);

            if (strcmp(entrySource->d_name, entryDest->d_name) == 0)
            {
                if (isDirectory(destEntryPath) != 0 && isDirectory(sourceEntryPath) != 0) //teraz allowRecursion=true
                {
                    allowDelete = false;

                    SyncDelete(sourceEntryPath, destEntryPath); //wejscie do podkatalogu
                }
                else if(isRegularFile(destEntryPath) != 0 && isRegularFile(sourceEntryPath) != 0)
                {
                    allowDelete = false;
                }

                break;
            }
        }

        if(allowDelete)
        {
            DeleteEntry(destEntryPath);
        }

        free(sourceEntryPath);
        free(destEntryPath);

        closedir(dir_source);
    }

    closedir(dir_dest);
}


int main(int argc, char **argv)
{
    InitializeParameters(argc, argv);

    InitializeDaemon();

//    Umożliwienie budzenia daemona sygnałem SIGUSR1
//    /bin/kill -s SIGUSR1 PID aby obudzić
    signal(SIGUSR1, WakeUp);

//    Otworzenie pliku z logami
//    cat /var/log/syslog | grep -i SYNCHRONIZER
    openlog("SYNCHRONIZER", LOG_PID, LOG_DAEMON);
    syslog(LOG_NOTICE, ">>>>>>>>>>>>>>>>>>>> DAEMON SUMMONED\n");

    GoToSleep();

    CheckPaths();

    SyncDelete(source, dest);
    SyncCopy(source, dest);

    syslog(LOG_NOTICE, "<<<<<<<<<<<<<<<<<<<< DAEMON EXORCISED\n");
    closelog();

    exit(EXIT_SUCCESS);
}