#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <curl/curl.h>

#define CHUNK_SIZE (1024 * 1024 * 2)  

void get_filename_from_url(const char *url, char *filename, size_t max_length) {
    const char *last_slash = strrchr(url, '/'); 
    if (last_slash && *(last_slash + 1)) {
        strncpy(filename, last_slash + 1, max_length - 1);
        filename[max_length - 1] = '\0'; // Null-terminate
    } else {
        strcpy(filename, "downloaded_file.bin"); // Default naming
    }
}

typedef struct {
    char *url;
    long start;
    long end;
    char filename[50];
} DownloadTask;

size_t write_callback(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    return fwrite(ptr, size, nmemb, stream);
}

void *download_chunk(void *arg) {
    DownloadTask *task = (DownloadTask *)arg;
    CURL *curl = curl_easy_init();
    if (!curl) return NULL;

    FILE *file = fopen(task->filename, "wb");
    if (!file) return NULL;

    char range[50];
    sprintf(range, "%ld-%ld", task->start, task->end);
    
    curl_easy_setopt(curl, CURLOPT_URL, task->url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);
    curl_easy_setopt(curl, CURLOPT_RANGE, range);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);

    curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    fclose(file);
    
    return NULL;
}

void merge_files(int thread_count, const char *output_filename) {
    FILE *output = fopen(output_filename, "wb");
    if (!output) {
        printf("Çıktı dosyası oluşturulamadı: %s\n", output_filename);
        return;
    }

    char buffer[1024];
    size_t bytesRead;

    for (int i = 0; i < thread_count; i++) {
        char filename[20];
        sprintf(filename, "part_%d", i);
        FILE *part = fopen(filename, "rb");

        if (!part) {
            printf("Parça dosyası bulunamadı: %s (zaten silinmiş olabilir)\n", filename);
            continue;
        }

        while ((bytesRead = fread(buffer, 1, sizeof(buffer), part)) > 0) {
            fwrite(buffer, 1, bytesRead, output);
        }
        fclose(part);
        remove(filename); // Delete partial files after merge
    }

    fclose(output);
    printf("Bütün parçalar birleştirildi: %s\n", output_filename);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Kullanım: %s <URL> <THREAD_COUNT>\n", argv[0]);
        return 1;
    }

    char *url = argv[1];
    int thread_count = atoi(argv[2]);

    CURL *curl = curl_easy_init();
    curl_off_t filesize = 0;

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_perform(curl);

    char output_filename[256];
    get_filename_from_url(url, output_filename, sizeof(output_filename));
    printf("İndirilecek dosya adı: %s\n", output_filename);

    if (curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &filesize) != CURLE_OK || filesize <= 0) {
        printf("Dosya boyutu alınamadı. Tahmini bir boyut kullanılarak çok iş parçacıklı indirme yapılacak...\n");
        filesize = CHUNK_SIZE * thread_count * 2;
    }

    curl_easy_cleanup(curl);
    
    pthread_t threads[thread_count];
    DownloadTask tasks[thread_count];

    long chunk_size = filesize / thread_count;
    for (int i = 0; i < thread_count; i++) {
        tasks[i].url = url;
        tasks[i].start = i * chunk_size;
        tasks[i].end = (i + 1) * chunk_size - 1;
        sprintf(tasks[i].filename, "part_%d", i);
        pthread_create(&threads[i], NULL, download_chunk, &tasks[i]);
    }

    for (int i = 0; i < thread_count; i++) {
        pthread_join(threads[i], NULL);
    }

    merge_files(thread_count, output_filename);
    printf("Bütün işlem tamamlandı! Çıktı dosyası: %s\n", output_filename);
    return 0;
}
