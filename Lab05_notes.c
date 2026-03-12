#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <signal.h>
#include <limits.h>

// --- 2. Sygnały i procesy ---

// Podpinanie sygnałów
int sethandler(void (*f)(int), int sigNo) {
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = f;
    if (-1 == sigaction(sigNo, &act, NULL)) return -1;
    return 0;
}

// Sprzątanie procesów zombie (SIGCHLD)
void sigchld_handler(int sig) {
    pid_t pid;
    for (;;) {
        pid = waitpid(0, NULL, WNOHANG);
        if (0 == pid || (0 >= pid && ECHILD == errno)) return;
        if (0 >= pid) ERR("waitpid");
    }
}

// Oczekiwanie na wszystkie dzieci
void wait_for_all_children() {
    while (wait(NULL) > 0);
}


// --- 3. Obsługa plików (Wysoki poziom) ---

// Czytanie tekstowe linia po linii
void read_file_high_level(const char* filepath) {
    FILE *file = fopen(filepath, "r");
    if (!file) ERR("fopen r");

    char line[256];
    const char *delimiters = " ,\n";

    while (fgets(line, sizeof(line), file) != NULL) {
        char *token = strtok(line, delimiters);
        while (token != NULL) {
            printf("Odczytano: %s\n", token);
            token = strtok(NULL, delimiters);
        }
    }
    fclose(file);
}

// Zapis tekstowy
void write_file_high_level(const char* filepath, int val1, int val2) {
    FILE *file = fopen(filepath, "w"); 
    if (!file) ERR("fopen w");

    if (fprintf(file, "%d,%d\n", val1, val2) < 0) ERR("fprintf");
    fclose(file);
}


// --- 4. Obsługa plików (Niski poziom) ---

// Czytanie bajtowe
void read_file_low_level(const char* filepath) {
    int fd = open(filepath, O_RDONLY);
    if (fd < 0) ERR("open r");

    char buf[4096];
    ssize_t bytes;

    while ((bytes = TEMP_FAILURE_RETRY(read(fd, buf, sizeof(buf) - 1))) > 0) {
        buf[bytes] = '\0';
        char *ptr = buf;
        int v1, v2, offset;

        while (sscanf(ptr, "%d,%d%n", &v1, &v2, &offset) == 2) {
            printf("Odczytano: %d, %d\n", v1, v2);
            ptr += offset;
            while (*ptr == ' ' || *ptr == '\n') ptr++;
        }
    }
    if (bytes < 0) ERR("read low level");
    close(fd);
}

// Zapis bajtowy
void write_file_low_level(const char* filepath, int val1, int val2) {
    int fd = open(filepath, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) ERR("open w");

    char buf[128];
    int len = snprintf(buf, sizeof(buf), "%d,%d\n", val1, val2);
    
    if (TEMP_FAILURE_RETRY(write(fd, buf, len)) < 0) ERR("write low level");
    close(fd);
}


// --- 5. Narzędzia PIPE / FIFO ---

// Wczytywanie z potoku/FIFO do końca
void read_from_ipc(int fd) {
    char buf[512];
    ssize_t bytes;
    
    while ((bytes = TEMP_FAILURE_RETRY(read(fd, buf, sizeof(buf)))) > 0) {
        if (write(STDOUT_FILENO, buf, bytes) < 0) ERR("write stdout");
    }
    if (bytes < 0) ERR("read ipc");
}

// Pisanie tekstu do potoku/FIFO
void write_to_ipc(int fd, const char* msg) {
    size_t len = strlen(msg);
    if (TEMP_FAILURE_RETRY(write(fd, msg, len)) < 0) ERR("write ipc text");
}

// Pisanie danych binarnych (struktur/intów) do potoku/FIFO
void write_struct_to_ipc(int fd, int val1, int val2) {
    int data[2] = {val1, val2};
    if (TEMP_FAILURE_RETRY(write(fd, data, sizeof(data))) < 0) ERR("write ipc data");
}

// Tworzenie pliku FIFO
void make_fifo(const char* path) {
    if (mkfifo(path, 0666) == -1 && errno != EEXIST) ERR("mkfifo");
}


// --- 6. Szablony komunikacji IPC ---

// Szablon: Rodzic ma osobny potok DO każdego dziecka (Rodzic pisze, dzieci czytają)
void template_N_pipes_parent_to_children(int N) {
    int fds_write[N]; // 1D tablica deskryptorów dla rodzica
    
    for (int i = 0; i < N; i++) {
        int tmpfd[2];
        if (pipe(tmpfd) == -1) ERR("pipe");

        if (fork() == 0) { // DZIECKO
            // Zamyka końcówki odziedziczone z poprzednich iteracji pętli
            for (int j = 0; j < i; j++) close(fds_write[j]);
            
            close(tmpfd[1]); // Dziecko nie pisze
            
            // Logika dziecka (czyta z tmpfd[0])
            read_from_ipc(tmpfd[0]);
            
            close(tmpfd[0]);
            exit(EXIT_SUCCESS);
        }
        
        // RODZIC
        close(tmpfd[0]); // Rodzic nie czyta
        fds_write[i] = tmpfd[1]; // Zapisuje końcówkę do pisania do dziecka
    }

    // Logika rodzica (pisze do każdego dziecka)
    for (int i = 0; i < N; i++) {
        write_to_ipc(fds_write[i], "Wiadomosc od rodzica\n");
        close(fds_write[i]); // Zamknięcie potoku po zakończeniu pisania
    }
    
    wait_for_all_children();
}

// Szablon: Każde dziecko ma własny potok DO rodzica (Dzieci piszą, rodzic czyta)
void template_N_pipes_children_to_parent(int N) {
    int fds_read[N]; // 1D tablica deskryptorów dla rodzica
    
    for (int i = 0; i < N; i++) {
        int tmpfd[2];
        if (pipe(tmpfd) == -1) ERR("pipe");

        if (fork() == 0) { // DZIECKO
            // Zamyka końcówki odziedziczone z poprzednich iteracji pętli
            for (int j = 0; j < i; j++) close(fds_read[j]);
            
            close(tmpfd[0]); // Dziecko nie czyta
            
            // Logika dziecka (pisze do tmpfd[1])
            write_to_ipc(tmpfd[1], "Wiadomosc od dziecka\n");
            
            close(tmpfd[1]);
            exit(EXIT_SUCCESS);
        }
        
        // RODZIC
        close(tmpfd[1]); // Rodzic nie pisze
        fds_read[i] = tmpfd[0]; // Zapisuje końcówkę do czytania od dziecka
    }

    // Logika rodzica (czyta od każdego dziecka)
    for (int i = 0; i < N; i++) {
        read_from_ipc(fds_read[i]);
        close(fds_read[i]); // Zamknięcie potoku po odczycie
    }
    
    wait_for_all_children();
}

// Szablon: Komunikacja niezależnych procesów po FIFO
void template_N_FIFOs(int N) {
    char name[64];

    // Tworzenie plików
    for (int i = 0; i < N; i++) {
        snprintf(name, sizeof(name), "/tmp/fifo_%d", i);
        make_fifo(name);
    }

    // Procesy potomne piszą do FIFO
    for (int i = 0; i < N; i++) {
        if (fork() == 0) {
            snprintf(name, sizeof(name), "/tmp/fifo_%d", i);
            int fd = open(name, O_WRONLY); // Blokuje do momentu otwarcia O_RDONLY
            if (fd < 0) ERR("open fifo child");

            write_to_ipc(fd, "Dane\n");

            close(fd);
            exit(EXIT_SUCCESS);
        }
    }

    // Proces główny czyta z FIFO
    int fds[N];
    for (int i = 0; i < N; i++) {
        snprintf(name, sizeof(name), "/tmp/fifo_%d", i);
        fds[i] = open(name, O_RDONLY); // Odblokowuje dzieci
        if (fds[i] < 0) ERR("open fifo parent");
    }

    for (int i = 0; i < N; i++) read_from_ipc(fds[i]);

    // Sprzątanie plików z dysku
    for (int i = 0; i < N; i++) {
        close(fds[i]);
        snprintf(name, sizeof(name), "/tmp/fifo_%d", i);
        unlink(name);
    }
    wait_for_all_children();
}


// --- 7. Main ---

void usage(char *name) {
    fprintf(stderr, "Użycie: %s <N>\n", name);
    exit(EXIT_FAILURE);
}

int main(int argc, char **argv) {
    if (argc != 2) usage(argv[0]);
    int N = atoi(argv[1]);
    if (N <= 0) usage(argv[0]);

    // sethandler(sigchld_handler, SIGCHLD);

    // Wywołaj potrzebny szablon
    // template_N_pipes_parent_to_children(N);

    return EXIT_SUCCESS;
}
