#include <bits/stdc++.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/time.h>
#include <stdbool.h>
using namespace std;

struct Job {
    int type;  // 0: sort (排序), 1: merge (合併)
    int level; // 層級 (排序: 0, 合併: 1~3)
    int pos;   // 位置 (子陣列索引)
};

// 狀態陣列：2D 陣列，sorted_status[level][pos] = 是否已排序
// issued_status[level][pos] = 是否已產生合併工作 (僅 level 1~3 用)
bool sorted_status[4][8];
bool issued_status[4][8];

// Job 佇列：使用 std::vector 實現
vector<Job> job_queue;

// 氣泡排序 (僅用於底層子陣列)
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

// 合併兩個有序子陣列
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

pthread_mutex_t mutex;
sem_t job_semaphore;
sem_t complete_semaphore;
bool done = false;
int* global_array;
int global_chunk_size;
int global_total_size;
pthread_t workers_array[8];

void* worker_thread(void* arg) {
    int thread_id = *(int*)arg;
    while (1) {  // 無限迴圈，直到 done_flag
        sem_wait(&job_semaphore);  // 等待新工作信號
        pthread_mutex_lock(&mutex);
        if (done) {  // 檢查終止旗標
            pthread_mutex_unlock(&mutex);
            break;
        }
        Job current_job = job_queue.front();  // 取出前端
        job_queue.erase(job_queue.begin());  // 移除前端
        pthread_mutex_unlock(&mutex);

        int start_index, end_index, mid_index, subarray_span, left_half_length;
        if (current_job.type == 0) {  // 排序工作 (Bubble Sort)
            start_index = current_job.pos * global_chunk_size;
            int subarray_length;
            if (current_job.pos == 7 && global_total_size % 8 != 0) {  // 如果是最後子陣列且有餘數
                subarray_length = global_chunk_size + (global_total_size % 8);  // 標準大小 + 餘數
            } else {
                subarray_length = global_chunk_size;  // 標準大小
            }
            bubble_sort(global_array + start_index, subarray_length);
        } else {  // 合併工作 (Merge)
            // 計算 2^level (展開位移：用迴圈乘法)
            int power_for_span = 1;
            for (int k = 0; k < current_job.level; k++) {
                power_for_span = power_for_span * 2;
            }
            subarray_span = power_for_span * global_chunk_size;  // 子陣列總跨度

            start_index = current_job.pos * subarray_span;  // 起始索引
            end_index = start_index + subarray_span - 1;  // 預設結束索引

            // 計算該層總子陣列數 (2^(3 - level))
            int total_subarrays_at_level = 1;
            for (int k = 0; k < (3 - current_job.level); k++) {
                total_subarrays_at_level = total_subarrays_at_level * 2;
            }
            int last_position_at_level = total_subarrays_at_level - 1;
            if (current_job.pos == last_position_at_level && global_total_size % 8 != 0) {
                end_index = global_total_size - 1;  // 調整為總陣列最後索引
            }

            // 計算左半長 (2^(level - 1))
            int power_for_left = 1;
            for (int k = 0; k < (current_job.level - 1); k++) {
                power_for_left = power_for_left * 2;
            }
            left_half_length = power_for_left * global_chunk_size;  // 左半總長
            mid_index = start_index + left_half_length - 1;  // 中點索引

            merge(global_array, start_index, mid_index, end_index);
        }

        // 設定完成狀態 (直接存取陣列)
        int current_level;
        if (current_job.type == 0) {
            current_level = 0;  // 排序設 level 0
        } else {
            current_level = current_job.level;  // 合併用 job level
        }
        pthread_mutex_lock(&mutex);
        sorted_status[current_level][current_job.pos] = true;  // 直接標記已排序
        pthread_mutex_unlock(&mutex);
        sem_post(&complete_semaphore);  // 通知派遣者
    }
    return NULL;
}

int main() {
    FILE* input_file = fopen("input.txt", "r");
    int total;
    fscanf(input_file, "%d", &total);
    int* original_array = (int*)malloc(total * sizeof(int));
    for (int i = 0; i < total; i++) {
        fscanf(input_file, "%d", &original_array[i]);
    }
    fclose(input_file);

    int chunk_size = total / 8;  // 每個底層區塊標準大小

    for (int i = 1; i <= 8; i++) {
        int* current_array = (int*)malloc(total * sizeof(int));
        memcpy(current_array, original_array, total * sizeof(int));  // 複製陣列

        global_array = current_array;
        global_chunk_size = chunk_size;
        global_total_size = total;

        // 重置狀態陣列和佇列
        memset(sorted_status, 0, sizeof(sorted_status));  // 所有 sorted 為 false
        memset(issued_status, 0, sizeof(issued_status));  // 所有 issued 為 false
        job_queue.clear();  // 清空 vector 佇列

        done = false;

        pthread_mutex_init(&mutex, NULL);
        sem_init(&job_semaphore, 0, 0);
        sem_init(&complete_semaphore, 0, 0);

        int thread_ids[8];
        for (int j = 0; j < i; j++) {
            thread_ids[j] = j;
            pthread_create(&workers_array[j], NULL, worker_thread, &thread_ids[j]);
        }

        timeval start_time, end_time;
        gettimeofday(&start_time, NULL);  // 開始計時

        // 插入 8 個初始排序工作
        pthread_mutex_lock(&mutex);
        for (int i = 0; i < 8; i++) {
            Job initial_job;
            initial_job.type = 0;  // 排序
            initial_job.level = 0;
            initial_job.pos = i;
            job_queue.push_back(initial_job);  // 直接加入尾端
        }
        pthread_mutex_unlock(&mutex);
        for (int i = 0; i < 8; i++) {
            sem_post(&job_semaphore);  // 喚醒工作執行緒 (8 次)
        }

        // 派遣者迴圈：等待 15 個工作完成
        int completed_jobs = 0;
        while (completed_jobs < 15) {
            sem_wait(&complete_semaphore);  // 等待完成信號
            pthread_mutex_lock(&mutex);
            completed_jobs++;  // 遞增完成計數

            // 檢查並產生新合併工作 (level 1~3)
            for (int current_level = 1; current_level <= 3; current_level++) {
                // 計算該層總子陣列數 (2^(3 - level))
                int total_subarrays_at_level = 1;
                for (int k = 0; k < (3 - current_level); k++) {
                    total_subarrays_at_level = total_subarrays_at_level * 2;
                }
                for (int current_pos = 0; current_pos < total_subarrays_at_level; current_pos++) {
                    int left_child_pos = current_pos * 2;
                    int right_child_pos = current_pos * 2 + 1;
                    // 直接存取陣列
                    bool left_child_sorted = sorted_status[current_level - 1][left_child_pos];
                    bool right_child_sorted = sorted_status[current_level - 1][right_child_pos];
                    bool already_issued = issued_status[current_level][current_pos];
                    if (left_child_sorted && right_child_sorted && !already_issued) {  // 兩個孩子就緒且未產生
                        Job merge_job;
                        merge_job.type = 1;  // 合併
                        merge_job.level = current_level;
                        merge_job.pos = current_pos;
                        job_queue.push_back(merge_job);  // 直接加入尾端
                        sem_post(&job_semaphore);  // 喚醒一工作執行緒
                        issued_status[current_level][current_pos] = true;  // 直接標記已產生
                    }
                }
            }

            pthread_mutex_unlock(&mutex);
        }

        gettimeofday(&end_time, NULL);
        double execution_time_seconds = (end_time.tv_sec - start_time.tv_sec) + 
                                       (end_time.tv_usec - start_time.tv_usec) / 1000000.0;
        double execution_time_ms = execution_time_seconds * 1000;  // 轉為毫秒

        // 使用 cout 輸出時間
        cout << "With " << i << " threads: " 
             << fixed << setprecision(3) << execution_time_ms << " ms" << endl;

        // 終止工作執行緒
        pthread_mutex_lock(&mutex);
        done = true;
        pthread_mutex_unlock(&mutex);
        for (int j = 0; j < i; j++) {
            sem_post(&job_semaphore);  // 喚醒檢查 done
        }
        for (int j = 0; j < i; j++) {
            pthread_join(workers_array[j], NULL);  // 等待結束
        }

        // 清理同步原語
        pthread_mutex_destroy(&mutex);
        sem_destroy(&job_semaphore);
        sem_destroy(&complete_semaphore);

        // 寫入輸出檔案
        char output_filename[20];
        sprintf(output_filename, "output_%d.txt", i);
        FILE* output_file = fopen(output_filename, "w");
        for (int j = 0; j < total; j++) {
            fprintf(output_file, "%d ", current_array[j]);
        }
        fclose(output_file);
        free(current_array);
    }

    free(original_array);
}