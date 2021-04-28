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
    return S_ISREG(statbuffer.st_mode); //0 jesli NIE jest plikiem
}

char *AddFileNameToDirPath(char* DirPath,char* FileName)
{
    char *finalPath = malloc(sizeof(char) * (BUFFER_SIZE));
    if(finalPath==NULL)
    {
        syslog(LOG_ERR, "memory allocation error \n errno = %d", errno);
        exit(EXIT_FAILURE);
    }
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
    if (stat(path, &pathStat) != 0) //success = 0
        return 0;
    time_t time = pathStat.st_ctime;
    return time;
}

off_t FileSize(char *path) //jesli nie bedzie dzialalo to zmienic typ na _off_t
{
    struct stat pathStat;
    if (stat(path, &pathStat) != 0) //success = 0
        return 0;
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
    syslog(LOG_NOTICE, "Copying file %s and writing to file %s using read/write was a success",sourcePath,destinationPath);
    close(copyFromFile);
    close(copyToFile);
    free(buffer);

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
                    syslog(LOG_ERR, "Error while opening dir to delete: %s\nerrno = %d\n", relativePath, errno);
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
                    syslog(LOG_ERR, "Error while deleting dir: %s\nerrno = %d\n", relativePath, errno);
                    exit(EXIT_FAILURE);
                }
            }
            else
            {
                syslog(LOG_ERR, "Error while deleting dir: %s\nerrno = %d\n", relativePath, errno);
                exit(EXIT_FAILURE);
            }
        }

        syslog(LOG_NOTICE, "DELETED DIRECTORY: %s\n", relativePath);
    }
    else
    {
        if (unlink(relativePath))
        {
            syslog(LOG_ERR, "Error while deleting file: %s\nerrno = %d", relativePath, errno);
            exit(EXIT_FAILURE);
        }

        syslog(LOG_NOTICE, "DELETED FILE: %s\n", relativePath);
    }
}

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
        syslog(LOG_ERR, " Error reading file: %s or writing to file: %s",sourcePath,destinationPath);
        return(-1);
    }
    syslog(LOG_NOTICE, "Copying file %s and writing to file %s using mmap/write was a success",sourcePath,destinationPath);
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

void SyncCopy()
{
    DIR *dir_dest, *dir_source;
    struct dirent *entry_dest, *entry_source;
    char *sourcePath, *destPath;
    time_t srcModTime;
    time_t dstModTime;
    dir_source = opendir(source);

    if (dir_source == NULL)
    {
        syslog(LOG_ERR, "Source path error:%d\n", errno);
        exit(EXIT_FAILURE);
    }

    while ((entry_source = readdir(dir_source)) != NULL){
        if (strcmp(entry_source->d_name, ".") == 0 || strcmp(entry_source->d_name, "..") == 0)
            continue;

        dir_dest = opendir(dest);

        if (dir_dest == NULL) {
            syslog(LOG_ERR, "Destination path error:%d\n", errno);
            exit(EXIT_FAILURE);
        }

        while ((entry_dest = readdir(dir_dest)) != NULL){
            if (strcmp(entry_dest->d_name, ".") == 0 || strcmp(entry_dest->d_name, "..") == 0)
                continue;

            sourcePath = AddFileNameToDirPath(source, entry_source->d_name);
            destPath = AddFileNameToDirPath(dest, entry_source->d_name);

            syslog(LOG_NOTICE, "CHECKING: %s WITH %s\n",entry_source->d_name,entry_dest->d_name);
            srcModTime = ModificationTime(sourcePath);
            dstModTime = ModificationTime(destPath);
            if (srcModTime > dstModTime &&
                isRegularFile(sourcePath)) //zamienic na cos innego jesli rekurencja nie bedzie dzialala
            {
                //skopiuj plik z  source do dest
                if (FileSize(sourcePath) < fileSizeThreshold) {
                    syslog(LOG_NOTICE, "COPY NORMAL\n");
                    //CopyFileNormal(sourcePath, destPath);
                } else {
                    syslog(LOG_NOTICE, "COPY MMAP\n");
                    //CopyFileMmap(sourcePath, destPath);
                }
            }

            syslog(LOG_NOTICE, "1# CHECKING: %s WITH %s\n",entry_source->d_name,entry_dest->d_name);

            free(sourcePath);
            free(destPath);

            syslog(LOG_NOTICE, "2# CHECKING: %s WITH %s\n",entry_source->d_name,entry_dest->d_name);

            closedir(dir_dest);
        }
    }

    closedir(dir_source);
}

void SyncDelete()
{
    DIR *dir_dest, *dir_source;
    struct dirent *entry_dest, *entry_source;
    char *sourcePath, *destPath;

    bool allowDelete = true;

    dir_dest = opendir(dest);

    if (dir_dest == NULL) {
        syslog(LOG_ERR, "DELETION:OPENDIR(SOURCE) RETURNED WITH ERROR:%d\n", errno);
        exit(EXIT_FAILURE);
    }

    while ((entry_dest = readdir(dir_dest)) != NULL)
    {
        allowDelete = true;

        if (strcmp(entry_dest->d_name, ".") == 0 || strcmp(entry_dest->d_name, "..") == 0)
            continue;

        dir_source = opendir(source);

        if (dir_dest == NULL) {
            syslog(LOG_ERR, "DELETION:OPENDIR(DEST) RETURNED WITH ERROR:%d\n", errno);
            exit(EXIT_FAILURE);
        }

        while ((entry_source = readdir(dir_source)) != NULL)
        {
            if (strcmp(entry_source->d_name, ".") == 0 || strcmp(entry_source->d_name, "..") == 0)
                continue;

            sourcePath = AddFileNameToDirPath(source, entry_source->d_name);

            if (strcmp(entry_source->d_name, entry_dest->d_name) == 0) //TODO: Zabezpieczyć porównanie pliku z katalogiem
            {
                allowDelete = false;
                free(sourcePath);
                break;
            }

            free(sourcePath);
        }

        if(allowDelete)
        {
            DeleteEntry(AddFileNameToDirPath(dest, entry_dest->d_name));
        }

        closedir(dir_dest);
    }

    closedir(dir_source);
}

// V2
void SyncCopyV()
{
    DIR *dir_dest, *dir_source;
    struct dirent *entry_dest, *entry_source;
    char *sourcePath, *destPath;
    time_t srcModTime;
    time_t dstModTime;
    bool allowCopy = true;

    dir_source = opendir(source);

    if (dir_source == NULL) {
        syslog(LOG_ERR, "COPY:OPENDIR(SOURCE) RETURNED WITH ERROR:%d\n", errno);
        exit(EXIT_FAILURE);
    }

    while ((entry_source = readdir(dir_source)) != NULL)
    {
        allowCopy = true;

        if (strcmp(entry_source->d_name, ".") == 0 || strcmp(entry_source->d_name, "..") == 0)
            continue;

        dir_dest = opendir(dest);

        if (dir_dest == NULL) {
            syslog(LOG_ERR, "COPY:OPENDIR(DEST) RETURNED WITH ERROR:%d\n", errno);
            exit(EXIT_FAILURE);
        }
        sourcePath = AddFileNameToDirPath(source, entry_source->d_name);

        while ((entry_dest = readdir(dir_dest)) != NULL)
        {
            if (strcmp(entry_dest->d_name, ".") == 0 || strcmp(entry_dest->d_name, "..") == 0)
                continue;

            destPath = AddFileNameToDirPath(dest, entry_dest->d_name);

            if (strcmp(entry_source->d_name, entry_dest->d_name) == 0) //TODO: Zabezpieczyć porównanie pliku z katalogiem
            {
                allowCopy = false;

                if(isRegularFile(sourcePath))
                {
                    srcModTime = ModificationTime(sourcePath);
                    dstModTime = ModificationTime(destPath);
                    if(srcModTime>dstModTime)
                    {
                        if (FileSize(sourcePath) < fileSizeThreshold) {
                            CopyFileNormal(sourcePath, destPath);
                        } else {
                            CopyFileMmap(sourcePath, destPath);
                        }
                    }
                }
                free(destPath);
                break;
            }
            free(destPath);
        }
        free(entry_dest);

        destPath = AddFileNameToDirPath(dest, entry_source->d_name);
        if(allowCopy)
        {
            if(isRegularFile(sourcePath))
            {
                if (FileSize(sourcePath) < fileSizeThreshold) {
                    CopyFileNormal(sourcePath, destPath);
                } else {
                    CopyFileMmap(sourcePath, destPath);
                }
            }
        }
        closedir(dir_dest);
        free(sourcePath);
        free(destPath);
    }

    closedir(dir_source);
    free(entry_source);

}
bool SyncCopyXD()
{
    struct dirent *filesListing;
    DIR *srcDir = opendir(source);
    DIR *dstDir = opendir(dest);
    char *srcPath, *dstPath;

    if(srcDir == NULL)
    {
        syslog(LOG_ERR, "Error opening directory: %s", source);
        return false;
    }
    else if(dstDir == NULL)
    {
        syslog(LOG_ERR, "Error opening directory: %s", dest);
        return false;
    }

    while((filesListing = readdir(srcDir)) != NULL)
    {
        if(strcmp(filesListing->d_name, ".") == 0 || strcmp(filesListing->d_name, "..") == 0)
        {
            continue;
        }
        srcPath = AddFileNameToDirPath(source, filesListing->d_name);
        dstPath = AddFileNameToDirPath(dest, filesListing->d_name);
        time_t srcModTime;
        time_t dstModTime;

        if(isRegularFile(srcPath))
        {
            srcModTime = ModificationTime(srcPath);
            dstModTime = ModificationTime(dstPath);
            if(srcModTime>dstModTime)
            {
                if (FileSize(srcPath) < fileSizeThreshold) {
                    CopyFileNormal(srcPath, dstPath);
                } else {
                    CopyFileMmap(srcPath, dstPath);
                }
            }
        }
//        else if(isDirectory(srcPath) && recursive)
//        {
//            if(stat(dstPath, &dstFileStat) == -1) //if dir is not avalible
//            {
//                mkdir(dstPath, srcFileStat.st_mode);
//                syslog(LOG_INFO, "[ MKDIR] Folder: %s", dstPath);
//            }
//            syncFiles(srcPath, dstPath);
//        }
        free(srcPath);
        free(dstPath);
    }
    closedir(dstDir);
    closedir(srcDir);
    free(filesListing);
    return true;
}

int main(int argc, char **argv)
{

//    Sprawdzenie poprawności parametrów
//    oraz inicjalizacja zmiennych globalnych:
//    - int sleepTime; // czas snu Daemona
//    - char* source, dest; // Ścieżki do plików/katalogów źródłowego i docelowego
//    - bool allowRecursion; // Tryb umożliwiający rekurencyjną synchronizację
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

    //Sprawdź czy po pobudce katalogi istnieją
    CheckPaths();

    //SyncDelete();
    SyncCopy();

    syslog(LOG_NOTICE, "<<<<<<<<<<<<<<<< DAEMON EXORCUMCISED\n");
    closelog();

    exit(EXIT_SUCCESS);
}