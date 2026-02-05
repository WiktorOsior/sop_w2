# Library Indexer System Analysis

This document provides a detailed breakdown of the library indexing system. The program traverses a directory of text files, parses their metadata (Title, Genre), and builds a structured index using symbolic links and directory hierarchies.

## 1. Full Source Code

```c
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
```

---

## 2. Detailed Function Analysis

### `join_paths`

#### Code
```c
char* join_paths(const char* path1, const char* path2)
{
    char* res;
    const int l1 = strlen(path1);
    if (path1[l1 - 1] == '/')
    {
        res = malloc(strlen(path1) + strlen(path2) + 1);
        if (!res) ERR("malloc");
        strcpy(res, path1);
    }
    else
    {
        res = malloc(strlen(path1) + strlen(path2) + 2);  // additional space for "/"
        if (!res) ERR("malloc");
        strcpy(res, path1);
        res[l1] = '/';
        res[l1 + 1] = 0;
    }
    return strcat(res, path2);
}
```
#### Explanation
This utility function concatenates two file paths into a single string. It checks if the first path (`path1`) ends with a directory separator (`/`). If it does not, the function allocates extra memory to insert the separator, ensuring the resulting path is valid.

#### Key Filesystem Concepts
* **Path Construction:** In POSIX systems, creating valid paths manually requires careful handling of the slash (`/`) separator. Incorrect concatenation (e.g., `folderfile` instead of `folder/file`) causes filesystem calls to fail.
* **Dynamic Allocation:** Since path lengths are variable, memory is allocated on the heap using `malloc`. The caller is responsible for freeing this memory.

---

### `parse_file`

#### Code
```c
void parse_file(FILE* f, char** title, char** genre)
{
    char* line = NULL;
    size_t line_len = 0;
    while (getline(&line, &line_len, f) != -1)
    {
        size_t real_len = strlen(line);
        if (line[real_len - 1] == '\n') line[real_len - 1] = 0;
        
        char* value = strchr(line, ':');
        if (value == NULL) continue;
        
        value[0] = 0;
        value++;  // move after ":"
        
        if (strcmp(line, "title") == 0) *title = strdup(value);
        else if (strcmp(line, "genre") == 0) *genre = strdup(value);
    }
    if (line) free(line);
}
```
#### Explanation
This function reads a file stream line-by-line to extract metadata. It parses lines formatted as `key:value`. When it finds "title" or "genre", it duplicates the value string into the provided pointers.

#### Key Filesystem Concepts
* **Stream I/O (`getline`):** Uses standard library buffering to read text efficiently, which is generally preferred over raw `read()` calls for text parsing.
* **Data Extraction:** The function abstracts the file content logic away from the directory traversal logic.

---

### `index_library`

#### Code
```c
int index_library(const char* name, const struct stat* s, int type, struct FTW* f)
{
    if (type == FTW_F)
    {
        // ... (opens file, calls parse_file) ...

        // 1. Link by Visible Title (Filename)
        char* path = join_paths("../../", name);
        chdir("index/by-visible-title");
        symlink(path, &name[f->base]);
        chdir("../..");

        // 2. Link by Internal Title
        if (title) {
            chdir("index/by-title");
            symlink(path, title);
            chdir("../..");
        }

        // 3. Link by Genre
        if (title && genre) {
            path = join_paths("../../../", name); // Deeper nesting requires more "../"
            chdir("index/by-genre");
            mkdir(genre, 0755); // Ensure genre dir exists
            chdir(genre);
            symlink(path, title);
            chdir("../../..");
        }
        // ... (cleanup) ...
    }
    return 0;
}
```
#### Explanation
This is the callback function for `nftw`. For every regular file found:
1.  It reads the file to find the book's Title and Genre.
2.  It creates symbolic links in three different directory structures (`by-visible-title`, `by-title`, and `by-genre`).
3.  It handles directory switching (`chdir`) to place links in the correct locations.

#### Key Filesystem Concepts
* **`nftw` (File Tree Walk):** A powerful tool for traversing directory hierarchies. The `struct FTW* f` argument provides context, such as `f->base` (index of the filename within the full path).
* **`symlink` (Symbolic Links):** Creates a "shortcut" to a file. **Crucially**, the target path in a symlink is relative to the *link's location*, not the current working directory. This is why `join_paths` uses `../../`.
* **`mkdir` (0755):** Creates directories with specific permissions (User: Read/Write/Exec, Group/Others: Read/Exec).
* **`chdir`:** Changes the process's current working directory. While useful here, it must be carefully balanced (e.g., `chdir("../..")`) to return to the original state, otherwise subsequent operations will fail.

---

### `read_database`

#### Code
```c
Book* read_database(char* path, int* N)
{
    int fd = open(path, O_RDONLY);
    struct stat s;
    fstat(fd, &s);
    
    // ... validation ...

    *N = s.st_size / 68; // Calculate number of records
    struct iovec* buffers = malloc(*N * sizeof(struct iovec));
    Book* books = malloc(*N * sizeof(Book));

    for (int i = 0; i < *N; i++) {
        buffers[i].iov_base = &books[i];
        buffers[i].iov_len = 68; // Size of one record
    }
    readv(fd, buffers, *N);

    // ... cleanup ...
    return books;
}
```
#### Explanation
Reads a binary database file containing fixed-size book records (68 bytes each). It loads the entire database into an array of `Book` structures.

#### Key Filesystem Concepts
* **`fstat` vs `stat`:** Uses `fstat` because we already have the file descriptor (`fd`). This is more efficient than calling `stat` by path.
* **`readv` (Vectored I/O):** also known as Scatter/Gather I/O. It allows reading data from a single file descriptor into multiple non-contiguous memory buffers (the `books` array elements) in a single atomic system call. This avoids the overhead of calling `read()` inside a loop.
* **`S_ISREG`:** Macro to verify the file is a regular file (not a directory or device).

---

### `main`

#### Code
```c
int main(int argc, char** argv)
{
    // ... (stat check for library existence) ...
    // ... (mkdir calls for index structure) ...

    nftw("library", index_library, 100, FTW_PHYS);

    if (argc >= 2) {
        // ... (verification logic using read_database) ...
    }
    return 0;
}
```
#### Explanation
The entry point initializes the environment. It verifies the input directory, creates the output directory structure (`index/...`), triggers the file tree walk, and optionally verifies the results against a provided database file.

#### Key Filesystem Concepts
* **`nftw` flags (`FTW_PHYS`):** Instructs the walker to perform a **physical walk**, meaning it will *not* follow symbolic links. This prevents infinite loops if the directory structure contains circular links.
* **`stat`:** Used here to verify the existence and type of the `library` directory before starting operations.
