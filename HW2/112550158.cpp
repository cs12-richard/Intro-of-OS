#include <iostream>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <cstring>
#include <iomanip>

using namespace std;

int main() {
    int a;
    cout << "Input the matrix dimension: ";
    cin >> a;
    for (int p = 1; p <= 16; p++) {
        size_t total_size = 3 * a * a * sizeof(unsigned int);

        int fd = shm_open("/matrix_shm", O_CREAT | O_RDWR, 0600);
        if (fd == -1) { perror("shm_open"); exit(1); }
        if (ftruncate(fd, total_size) == -1) { perror("ftruncate"); exit(1); }
        void *shm = mmap(NULL, total_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (shm == MAP_FAILED) { perror("mmap"); exit(1); }

        unsigned int *A = static_cast<unsigned int*>(shm);
        unsigned int *B = A + a * a;
        unsigned int *C = B + a * a;

        for (int i = 0; i < a * a; i++) {
            A[i] = i;
            B[i] = i;
            C[i] = 0;
        }

        struct timeval start, end;
        gettimeofday(&start, NULL);

        int rpp = a / p;
        int remainder = a % p;
        int start_row = 0;
        pid_t pids[16];
        for (int i = 0; i < p; i++) {
            int rows = rpp + (i < remainder ? 1 : 0);
            pid_t pid = fork();
            if (pid == 0) {
                for (int row = start_row; row < start_row + rows; row++)
                    for (int col = 0; col < a; col++) {
                        unsigned int sum = 0;
                        for (int k = 0; k < a; k++) sum += A[row * a + k] * B[k * a + col];
                        C[row * a + col] = sum;
                    }
                exit(0);
            } else if (pid < 0) { perror("fork"); exit(1); }
            pids[i] = pid;
            start_row += rows;
        }

        for (int i = 0; i < p; i++) {
            waitpid(pids[i], NULL, 0);
        }

        gettimeofday(&end, NULL);
        double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1000000.0;
        unsigned int checksum = 0;
        for (int i = 0; i < a * a; i++) {
            checksum += C[i];
        }

        cout << "Multiplying matrices using " << p << " process" << (p == 1 ? "" : "es") << endl;
        cout << "Elapsed time: " << fixed << setprecision(6) << elapsed << " sec, Checksum: " << checksum << endl;

        if (munmap(shm, total_size) == -1) perror("munmap");
        close(fd);
        if (shm_unlink("/matrix_shm") == -1) perror("shm_unlink");
    }
    return 0;
}