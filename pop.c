#define _XOPEN_SOURCE 700

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <ftw.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <unistd.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

#define MAX_FILENAME_LEN 64

// join 2 path. returned pointer is for newly allocated memory and must be freed
char* join_paths(const char* path1, const char* path2)
{
    char* res;
    const int l1 = strlen(path1);
    if (path1[l1 - 1] == '/')
    {
        res = malloc(strlen(path1) + strlen(path2) + 1);
        if (!res)
            ERR("malloc");
        strcpy(res, path1);
    }
    else
    {
        res = malloc(strlen(path1) + strlen(path2) + 2);  // additional space for "/"
        if (!res)
            ERR("malloc");
        strcpy(res, path1);
        res[l1] = '/';
        res[l1 + 1] = 0;
    }
    return strcat(res, path2);
}

void usage(int argc, char** argv)
{
    (void)argc;
    fprintf(stderr, "USAGE: %s path\n", argv[0]);
    exit(EXIT_FAILURE);
}

typedef struct
{
    unsigned int size;
    char title[65];
} Book;

void parse_file(FILE* f, char** title, char** genre)
{
    char* line = NULL;
    size_t line_len = 0;
    while (getline(&line, &line_len, f) != -1)
    {
        size_t real_len = strlen(line);  // line_len is allocated size! it may be larger than real one
        if (line[real_len - 1] == '\n')
        {
            line[real_len - 1] = 0;  // remove trailing \n
            // it's wrong, but the last line may have missing \n, thus "if"
        }
        char* value = strchr(line, ':');
        if (value == NULL)
        {
            continue;
        }
        value[0] = 0;
        value++;  // move after ":"
        if (strcmp(line, "title") == 0)
        {
            *title = strdup(value);
        }
        else if (strcmp(line, "genre") == 0)
        {
            *genre = strdup(value);
        }
    }

    if (line)
        free(line);
}

int index_library(const char* name, const struct stat* s, int type, struct FTW* f)
{
    if (type == FTW_F)
    {
        puts(name);
        FILE* file = fopen(name, "r");
        if (!f)
            ERR("fopen");
        char* title = NULL;
        char* genre = NULL;
        parse_file(file, &title, &genre);
        if (title && strlen(title) > MAX_FILENAME_LEN)
            title[MAX_FILENAME_LEN] = 0;
        if (genre && strlen(genre) > MAX_FILENAME_LEN)
            genre[MAX_FILENAME_LEN] = 0;
        char* path = join_paths("../../", name);

        if (chdir("index/by-visible-title") != 0)
            ERR("chdir");
        if (symlink(path, &name[f->base]))
            ERR("symlink");
        chdir("../..");

        if (title)
        {
            if (chdir("index/by-title") != 0)
                ERR("chdir");
            if (symlink(path, title))
            {
                if (errno != EEXIST)
                    ERR("symlink");
            }
            chdir("../..");
        }

        if (title && genre)
        {
            free(path);
            path = join_paths("../../../", name);
            if (chdir("index/by-genre") != 0)
                ERR("chdir");
            if (mkdir(genre, 0755) != 0)
            {
                if (errno != EEXIST)
                    ERR("mkdir");
            }
            if (chdir(genre) != 0)
                ERR("chdir");
            if (symlink(path, title))
            {
                if (errno != EEXIST)
                    ERR("symlink");
            }
            chdir("../../..");
        }

        if (title)
            free(title);
        if (genre)
            free(genre);

        free(path);
        fclose(file);
    }
    return 0;
}

Book* read_database(char* path, int* N)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0)
        ERR("open");
    struct stat s;
    if (fstat(fd, &s) < 0)
        ERR("fstat");
    if (!S_ISREG(s.st_mode))
    {
        printf("%s is not a regular file, exiting\n", path);
        exit(EXIT_FAILURE);
    }
    const size_t size = s.st_size;
    *N = size / 68;
    struct iovec* buffers = (struct iovec*)malloc(*N * sizeof(struct iovec));
    Book* books = malloc(*N * sizeof(Book));
    if (buffers == NULL || books == NULL)
        ERR("malloc");

    for (int i = 0; i < *N; i++)
    {
        buffers[i].iov_base = &books[i];
        buffers[i].iov_len = 68;
        books[i].title[64] = 0;
    }
    if (readv(fd, buffers, *N) < size)
        ERR("readv");

    free(buffers);
    close(fd);
    return books;
}

int main(int argc, char** argv)
{
    struct stat s;
    if (stat("library", &s))
        ERR("stat");
    if (!S_ISDIR(s.st_mode))
    {
        printf("library is not a valid dir, exiting\n");
        return 1;
    }

    if (mkdir("index", 0755) != 0)
        ERR("mkdir");
    if (mkdir("index/by-visible-title", 0755) != 0)
        ERR("mkdir");
    if (mkdir("index/by-title", 0755) != 0)
        ERR("mkdir");
    if (mkdir("index/by-genre", 0755) != 0)
        ERR("mkdir");

    if (nftw("library", index_library, 100, FTW_PHYS))
    {
        ERR("nftw");
    }

    if (argc >= 2)
    {
        int N;
        Book* books = read_database(argv[1], &N);
        if (chdir("index/by-title") != 0)
            ERR("chdir");
        for (int i = 0; i < N; i++)
        {
            if (stat(books[i].title, &s))
            {
                printf("Book \"%s\" is missing!\n", books[i].title);
            }
            else if (s.st_size != books[i].size)
            {
                printf("Book \"%s\" has wrong size (%ld vs %d)!\n", books[i].title, s.st_size, books[i].size);
            }
        }
        free(books);
    }

    return 0;
}
