#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include "fft.h"
//ref:https://raw.githubusercontent.com/cxong/tinydir/master/tinydir.h
#include "tinydir.h"
#include "timing.h"
#define DR_WAV_IMPLEMENTATION
//ref:https://raw.githubusercontent.com/mackron/dr_libs/master/dr_wav.h
#include "dr_wav.h"

int16_t *wavRead_int16(char *filename, uint32_t *sampleRate, uint64_t *totalSampleCount) {
    unsigned int channels;
    int16_t *buffer = drwav_open_and_read_file_s16(filename, &channels, sampleRate, totalSampleCount);
    if (buffer == 0) {
        fprintf(stderr, "ERROR\n");
        exit(1);
    }
    if (channels == 2) {
        int16_t *bufferSave = buffer;
        for (int i = 0; i < *totalSampleCount; i += 2) {
            *bufferSave++ = (int16_t) ((buffer[i] + buffer[i + 1]) >> 1);
        }
        *totalSampleCount = *totalSampleCount >> 1;
    }
    return buffer;
}


unsigned long hash(unsigned char *str) {
    unsigned long hash = 5381;
    int c;
    while (c = *str++)
        hash = ((hash << 5) + hash) + c;
    return hash;
}

int generateHashes(char *input_file, int **hashtable, int songid, size_t N, int freqbandWidth, int maxElems) {
    printf("reading %s \n", input_file);
    uint32_t sampleRate = 0;
    uint64_t samplesize = 0;
    int16_t *pcmdata = wavRead_int16(input_file, &sampleRate, &samplesize);
    float *inputBuffer = (float *) calloc(sizeof(float), N);
    fft_complex *outBuffer = (fft_complex *) calloc(sizeof(fft_complex), N);
    int sect = 0;
    int cnt = 0;
    int numHashes = 0;
    for (int i = 0; i < samplesize; i++) {
        if (sect < N) {
            inputBuffer[sect] = (float) pcmdata[i];
            sect++;
        } else {
            sect = 0;
            i -= 1;
            cnt++;
            fft_plan plan = fft_plan_dft_r2c_1d(N, inputBuffer, outBuffer, 0);
            fft_execute(plan);
            fft_destroy_plan(plan);
            int freq1 = 0, freq2 = 0, freq3 = 0, freq4 = 0, freq5 = 0;
            int pt1 = 0, pt2 = 0, pt3 = 0, pt4 = 0, pt5 = 0;
            int freqbandWidth2 = freqbandWidth * 2;
            int freqbandWidth3 = freqbandWidth * 3;
            int freqbandWidth4 = freqbandWidth * 4;
            int freqbandWidth5 = freqbandWidth * 5;
            int freqbandWidth6 = freqbandWidth * 6;
            for (int k = freqbandWidth; k < freqbandWidth6; k++) {
                int freq = (outBuffer[k].real > 0) ? (int) outBuffer[k].real : (int) (0 - outBuffer[k].real);
                int Magnitude = (int) (log10f((freq + 1)) * 1000);
                if (k >= freqbandWidth && k < freqbandWidth2 && Magnitude > freq1) {
                    freq1 = Magnitude;
                    pt1 = k;
                } else if (k >= freqbandWidth2 && k < freqbandWidth3 && Magnitude > freq2) {
                    freq2 = Magnitude;
                    pt2 = k;
                } else if (k >= freqbandWidth3 && k < freqbandWidth4 && Magnitude > freq3) {
                    freq3 = Magnitude;
                    pt3 = k;
                } else if (k >= freqbandWidth4 && k < freqbandWidth5 && Magnitude > freq4) {
                    freq4 = Magnitude;
                    pt4 = k;
                } else if (k >= freqbandWidth5 && k < freqbandWidth6 && Magnitude > freq5) {
                    freq5 = Magnitude;
                    pt5 = k;
                }
            }
            char buffer[50];
            sprintf(buffer, "%d%d%d%d%d", pt1, pt2, pt3, pt4, pt5);
            unsigned long hashresult = hash(buffer) % maxElems;
            int key = (int) hashresult;
            if (key < 0)
                printf("Invalid key %d\n", key);
            hashtable[key][songid]++;
            numHashes++;
        }
    }
    free(pcmdata);
    free(inputBuffer);
    free(outBuffer);
    return numHashes;
}


int main(int argc, char *argv[]) {
    printf("Audio Processing\n");
    printf("shazam audio hash\n");
    printf("blog: http://cpuimage.cnblogs.com/\n");
    int N = 512;
    int freqbandWidth = 50;
    int maxSongs = 10;
    size_t maxElems = 200000;
    int **hashTable;
    int i = 0, n = 0;
    float count = 0;
    int numsongs = 0;
    char filenames[maxSongs + 1][_TINYDIR_FILENAME_MAX];
    int filesizes[maxSongs + 1];
    int songScores[maxSongs + 1];
    float songMatch[maxSongs + 1];
    printf("running... \n");
    if (argc < 2) {
        printf("no excerpt file to open \n");
        exit(1);
    }
    double start_total = now();
    hashTable = (int **) calloc(maxElems, sizeof(int *));
    for (i = 0; i < maxElems; i++)
        hashTable[i] = (int *) calloc(maxSongs + 1, sizeof(int));
    printf("Generating hashes for original files.. \n");
    tinydir_dir dir;
    tinydir_open(&dir, "data");
    while (dir.has_next) {
        tinydir_file file;
        tinydir_readfile(&dir, &file);
        if (file.is_reg) {
            numsongs++;
            double startTime = now();
            filesizes[numsongs] = generateHashes(file.path, hashTable, numsongs, N, freqbandWidth, maxElems);
            size_t time_interval = (size_t) (calcElapsed(startTime, now()) * 1000);
            songScores[numsongs] = 0;
            printf("%d:%d hashes for %s\n", numsongs, filesizes[numsongs], file.path);
            printf("Time taken: %d seconds %d milliseconds\n", time_interval / 1000, time_interval % 1000);
            strcpy(filenames[numsongs], file.name);
        }
        tinydir_next(&dir);
    }
    tinydir_close(&dir);
    printf("Generating hashes for recorded file.. \n");
    generateHashes(argv[1], hashTable, 0, N, freqbandWidth, maxElems);
    printf("Calculating score.. \n");
    for (i = 0; i < maxElems; i++) {
        if (hashTable[i][0] > 0) {
            for (n = 1; n <= maxSongs; n++) {
                if (hashTable[i][n] >= hashTable[i][0])
                    songScores[n] = songScores[n] + hashTable[i][0];
                else
                    songScores[n] = songScores[n] + hashTable[i][n];;
            }
        }
    }
    for (i = 1; i <= numsongs; i++) {
        songMatch[i] = ((float) songScores[i]) / ((float) filesizes[i]);
        printf("Score for %s = %f\n", filenames[i], songMatch[i]);
        if (songMatch[i] > count) {
            count = songMatch[i];
            n = i;
        }
    }
    printf("Best Score: %s\n", filenames[n]);
    for (i = 0; i < maxElems; i++)
        free(hashTable[i]);
    free(hashTable);
    size_t msec = (size_t) (calcElapsed(start_total, now()) * 1000);
    printf("Total time taken: %d seconds %d milliseconds\n", msec / 1000, msec % 1000);

    return 0;
}