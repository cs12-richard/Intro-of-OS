#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <string.h>
using namespace std;

int main(){
    int a;
    cout << "Input the matrix dimension: ";
    cin>>a;
    for (int p = 1; p <= 16; p++) {
        size_t total_size = 3 * n * n * sizeof(unsigned int);

        int fd = shm_open("/matrix_shm", O_CREAT | O_RDWR, 0600);
        ftruncate(fd, total_size);
        void *shm = mmap(NULL, total_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

        unsigned int *A = static_cast<unsigned int*>(shm);
        unsigned int *B = A + n * n;
        unsigned int *C = B + n * n;

        for (int i = 0; i < n * n; i++) {
            A[i] = i;
            B[i] = i; 
            C[i] = 0;
        }

        struct timeval start, end;
        gettimeofday(&start, NULL);

        int rpp = n / p;
        int remainder = n % p;
        int start_row = 0;
        pid_t pids[16];
        for (int i = 0; i < p; i++) {
            int rows = rpp + (i < remainder ? 1 : 0);
            pid_t pid = fork();
            if (pid == 0) {
                for (int row = start_row; row < start_row + rows; row++)
                    for (int col = 0; col < n; col++) {
                        unsigned int sum = 0;
                        for (int k = 0; k < n; k++) sum += A[row * n + k] * B[k * n + col];
                        C[row * n + col] = sum;
                    }
                exit(0);
            }
            pids[i] = pid;
            start_row += rows;
        }

        for (int i = 0; i < p; i++) {
            waitpid(pids[i], NULL, 0);
        }

        gettimeofday(&end, NULL);
        double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1000000.0;
        unsigned int checksum = 0;
        for (int i = 0; i < n * n; i++) {
            checksum += C[i];
        }

        cout << "Multiplying matrices using " << p << " process" << (p == 1 ? "" : "es") << endl;
        cout << "Elapsed time: " << fixed << setprecision(6) << elapsed << " sec, Checksum: " << checksum << endl;

        close(fd);
    }
}