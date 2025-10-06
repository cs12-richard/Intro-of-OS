#include <bits/stdc++.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/time.h>
#include <stdbool.h>
using namespace std;

struct Job {
    int type;
    int level;
    int pos;
};

//check if sort and merg
bool sorted[4][8];
bool merg[4][8];

vector<Job> job_q;
pthread_mutex_t global_mutex;
sem_t job_semaphore;
sem_t complete_semaphore;
bool done = false;
int* g_array;
int g_chunk_size;
int g_total_size;
pthread_t workers_array[8];

// bbsort
void bubble_sort(int* arr, int len) {
    for (int i = 0; i < len - 1; i++) {
        for (int j = 0; j < len - 1 - i; j++) {
            if (arr[j] > arr[j + 1]) {
                int temp = arr[j];
                arr[j] = arr[j + 1];
                arr[j + 1] = temp;
            }
        }
    }
}

void merge(int* arr, int start, int mid, int end) {
    int left_length = mid - start + 1;
    int right_length = end - mid;
    int* left_array = (int*)malloc(left_length * sizeof(int));
    int* right_array = (int*)malloc(right_length * sizeof(int));
    memcpy(left_array, arr + start, left_length * sizeof(int));
    memcpy(right_array, arr + mid + 1, right_length * sizeof(int));
    int left_index = 0;
    int right_index = 0;
    int merge_index = start;
    while (left_index < left_length && right_index < right_length) {
        if (left_array[left_index] <= right_array[right_index]) {
            arr[merge_index] = left_array[left_index];
            left_index++;
        } else {
            arr[merge_index] = right_array[right_index];
            right_index++;
        }
        merge_index++;
    }
    while (left_index < left_length) {
        arr[merge_index] = left_array[left_index];
        left_index++;
        merge_index++;
    }
    while (right_index < right_length) {
        arr[merge_index] = right_array[right_index];
        right_index++;
        merge_index++;
    }
    free(left_array);
    free(right_array);
}

void* worker_thread(void* arg) {
    int thread_id = *(int*)arg;
    while (1) {
        sem_wait(&job_semaphore);
        pthread_mutex_lock(&global_mutex);
        if (done) {
            pthread_mutex_unlock(&global_mutex);
            break;
        }
        Job current_job = job_q.front();
        job_q.erase(job_q.begin());
        pthread_mutex_unlock(&global_mutex);

        int start_index, end_index, mid_index, subarray_span, left_half_length;
        if (current_job.type == 0) {
            start_index = current_job.pos * g_chunk_size;
            int subarray_length;
            if (current_job.pos == 7 && g_total_size % 8 != 0) {
                subarray_length = g_chunk_size + (g_total_size % 8);
            } else {
                subarray_length = g_chunk_size;
            }
            bubble_sort(g_array + start_index, subarray_length);
        } 
        else {
            int power_for_span = 1;
            for (int k = 0; k < current_job.level; k++) {
                power_for_span = power_for_span * 2;
            }
            subarray_span = power_for_span * g_chunk_size;

            start_index = current_job.pos * subarray_span;
            end_index = start_index + subarray_span - 1;

            int total_subarrays_at_level = 1;
            for (int k = 0; k < (3 - current_job.level); k++) {
                total_subarrays_at_level = total_subarrays_at_level * 2;
            }
            int last_position_at_level = total_subarrays_at_level - 1;
            if (current_job.pos == last_position_at_level && g_total_size % 8 != 0) {
                end_index = g_total_size - 1;
            }

            int power_for_left = 1;
            for (int k = 0; k < (current_job.level - 1); k++) {
                power_for_left = power_for_left * 2;
            }
            left_half_length = power_for_left * g_chunk_size;
            mid_index = start_index + left_half_length - 1;

            merge(g_array, start_index, mid_index, end_index);
        }

        int current_level;
        if (current_job.type == 0) {
            current_level = 0;
        } else {
            current_level = current_job.level;
        }
        pthread_mutex_lock(&global_mutex);
        sorted[current_level][current_job.pos] = true;
        pthread_mutex_unlock(&global_mutex);
        sem_post(&complete_semaphore);
    }
    return NULL;
}

int main() {
    FILE* input_file = fopen("input.txt", "r");
    int total;
    fscanf(input_file, "%d", &total);
    int* ori_array = (int*)malloc(total * sizeof(int));
    for (int i = 0; i < total; i++) {
        fscanf(input_file, "%d", &ori_array[i]);
    }
    fclose(input_file);

    int chunk_size = total / 8;

    for (int i = 1; i <= 8; i++) {
        int* cur_array = (int*)malloc(total * sizeof(int));
        memcpy(cur_array, ori_array, total * sizeof(int));

        g_array = cur_array;
        g_chunk_size = chunk_size;
        g_total_size = total;

        memset(sorted, 0, sizeof(sorted));
        memset(merg, 0, sizeof(merg));
        job_q.clear();

        done = false;

        pthread_mutex_init(&global_mutex, NULL);
        sem_init(&job_semaphore, 0, 0);
        sem_init(&complete_semaphore, 0, 0);

        int thread_ids[8];
        for (int j = 0; j < i; j++) {
            thread_ids[j] = j;
            pthread_attr_t attr;
            pthread_attr_init(&attr);
            pthread_create(&workers_array[j], &attr, worker_thread, &thread_ids[j]);
            pthread_attr_destroy(&attr);
        }

        timeval start_time, end_time;
        gettimeofday(&start_time, NULL);

        pthread_mutex_lock(&global_mutex);
        for (int i = 0; i < 8; i++) {
            Job initial_job;
            initial_job.type = 0;
            initial_job.level = 0;
            initial_job.pos = i;
            job_q.push_back(initial_job);
        }
        pthread_mutex_unlock(&global_mutex);
        for (int i = 0; i < 8; i++) {
            sem_post(&job_semaphore);
        }

        int complete = 0;
        while (complete < 15) {
            sem_wait(&complete_semaphore);
            pthread_mutex_lock(&global_mutex);
            complete++;

            for (int current_level = 1; current_level <= 3; current_level++) {
                int total_subarrays_at_level = 1;
                for (int k = 0; k < (3 - current_level); k++) {
                    total_subarrays_at_level = total_subarrays_at_level * 2;
                }
                for (int current_pos = 0; current_pos < total_subarrays_at_level; current_pos++) {
                    int left_child_pos = current_pos * 2;
                    int right_child_pos = current_pos * 2 + 1;
                    bool left_child_sorted = sorted[current_level - 1][left_child_pos];
                    bool right_child_sorted = sorted[current_level - 1][right_child_pos];
                    bool already_issued = merg[current_level][current_pos];
                    if (left_child_sorted && right_child_sorted && !already_issued) {
                        Job merge_j;
                        merge_j.type = 1;
                        merge_j.level = current_level;
                        merge_j.pos = current_pos;
                        job_q.push_back(merge_j);
                        sem_post(&job_semaphore);
                        merg[current_level][current_pos] = true;
                    }
                }
            }
            pthread_mutex_unlock(&global_mutex);
        }

        gettimeofday(&end_time, NULL);
        double execution_time_ms = (end_time.tv_sec - start_time.tv_sec) * 1000.0 + (end_time.tv_usec - start_time.tv_usec) / 1000.0;
        cout << "worker thread #" << i << ", elapsed " << fixed << setprecision(6) << execution_time_ms << " ms" << endl;

        pthread_mutex_lock(&global_mutex);
        done = true;
        pthread_mutex_unlock(&global_mutex);
        for (int j = 0; j < i; j++) {
            sem_post(&job_semaphore);
        }
        for (int j = 0; j < i; j++) {
            pthread_join(workers_array[j], NULL);
        }

        pthread_mutex_destroy(&global_mutex);
        sem_destroy(&job_semaphore);
        sem_destroy(&complete_semaphore);

        char output_filename[20];
        sprintf(output_filename, "output_%d.txt", i);
        FILE* output_file = fopen(output_filename, "w");
        for (int j = 0; j < total; j++) {
            fprintf(output_file, "%d ", cur_array[j]);
        }
        fclose(output_file);
        free(cur_array);
    }
}