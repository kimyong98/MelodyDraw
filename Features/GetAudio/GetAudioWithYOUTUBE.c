#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>
#include <time.h>
#include <direct.h>

#pragma comment(lib, "winmm.lib")

#define RECORD_TIME 5 // 녹음 시간(초)
#define SAMPLE_RATE 44100 // 샘플레이트(Hz)
#define BITS_PER_SAMPLE 16 // 샘플당 비트 수
#define NUM_CHANNELS 1 // 채널 수(모노)
#define BUFFER_SIZE (SAMPLE_RATE * NUM_CHANNELS * (BITS_PER_SAMPLE / 8) * RECORD_TIME)

typedef struct {
    char chunkID[4];       // "RIFF"
    unsigned long chunkSize;  // 4 + (8 + SubChunk1Size) + (8 + SubChunk2Size)
    char format[4];        // "WAVE"
    char subchunk1ID[4];   // "fmt "
    unsigned long subchunk1Size;  // 16 for PCM
    unsigned short audioFormat;   // PCM = 1
    unsigned short numChannels;   // Mono = 1, Stereo = 2
    unsigned long sampleRate;     // 44100, 48000, etc.
    unsigned long byteRate;       // == SampleRate * NumChannels * BitsPerSample/8
    unsigned short blockAlign;    // == NumChannels * BitsPerSample/8
    unsigned short bitsPerSample; // 8 bits = 8, 16 bits = 16, etc.
    char subchunk2ID[4];   // "data"
    unsigned long subchunk2Size;  // == NumSamples * NumChannels * BitsPerSample/8
} WAVHeader;

void get_current_time_string(char* buffer, int len) {
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    snprintf(buffer, len, "%04d_%02d_%02d_%02d_%02d_%02d",
        tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
        tm.tm_hour, tm.tm_min, tm.tm_sec);
}

void write_wav_header(FILE* fp, int dataSize) {
    WAVHeader header;

    memcpy(header.chunkID, "RIFF", 4);
    header.chunkSize = 36 + dataSize;
    memcpy(header.format, "WAVE", 4);

    memcpy(header.subchunk1ID, "fmt ", 4);
    header.subchunk1Size = 16;
    header.audioFormat = 1; // PCM
    header.numChannels = 1;
    header.sampleRate = SAMPLE_RATE;
    header.byteRate = SAMPLE_RATE * NUM_CHANNELS * (BITS_PER_SAMPLE / 8);
    header.blockAlign = NUM_CHANNELS * (BITS_PER_SAMPLE / 8);
    header.bitsPerSample = BITS_PER_SAMPLE;

    memcpy(header.subchunk2ID, "data", 4);
    header.subchunk2Size = dataSize;

    fwrite(&header, sizeof(WAVHeader), 1, fp);
}

void record_audio(const char* filename) {
    HWAVEIN hWaveIn;
    WAVEFORMATEX pFormat;
    WAVEHDR WaveInHdr;
    char buffer[BUFFER_SIZE];
    MMRESULT result;

    // 녹음 형식 설정
    pFormat.wFormatTag = WAVE_FORMAT_PCM;
    pFormat.nChannels = NUM_CHANNELS;
    pFormat.nSamplesPerSec = SAMPLE_RATE;
    pFormat.nAvgBytesPerSec = SAMPLE_RATE * NUM_CHANNELS * (BITS_PER_SAMPLE / 8);
    pFormat.nBlockAlign = NUM_CHANNELS * (BITS_PER_SAMPLE / 8);
    pFormat.wBitsPerSample = BITS_PER_SAMPLE;
    pFormat.cbSize = 0;

    // 녹음 장치 열기
    result = waveInOpen(&hWaveIn, WAVE_MAPPER, &pFormat, 0L, 0L, WAVE_FORMAT_DIRECT);
    if (result) {
        printf("녹음 장치 열기 실패: %d\n", result);
        return;
    }

    // WAVEHDR 설정
    WaveInHdr.lpData = buffer;
    WaveInHdr.dwBufferLength = sizeof(buffer);
    WaveInHdr.dwBytesRecorded = 0;
    WaveInHdr.dwUser = 0L;
    WaveInHdr.dwFlags = 0L;
    WaveInHdr.dwLoops = 0L;

    // 버퍼 준비
    result = waveInPrepareHeader(hWaveIn, &WaveInHdr, sizeof(WAVEHDR));
    if (result) {
        printf("버퍼 준비 실패: %d\n", result);
        waveInClose(hWaveIn);
        return;
    }

    result = waveInAddBuffer(hWaveIn, &WaveInHdr, sizeof(WAVEHDR));
    if (result) {
        printf("버퍼 추가 실패: %d\n", result);
        waveInUnprepareHeader(hWaveIn, &WaveInHdr, sizeof(WAVEHDR));
        waveInClose(hWaveIn);
        return;
    }

    // 녹음 시작
    result = waveInStart(hWaveIn);
    if (result) {
        printf("녹음 시작 실패: %d\n", result);
        waveInUnprepareHeader(hWaveIn, &WaveInHdr, sizeof(WAVEHDR));
        waveInClose(hWaveIn);
        return;
    }

    Sleep(RECORD_TIME * 1000);
    waveInStop(hWaveIn);

    // 버퍼 해제
    waveInUnprepareHeader(hWaveIn, &WaveInHdr, sizeof(WAVEHDR));
    waveInClose(hWaveIn);

    // 파일로 저장
    FILE* fp = fopen(filename, "wb");
    if (fp) {
        write_wav_header(fp, sizeof(buffer));
        fwrite(buffer, sizeof(buffer), 1, fp);
        fclose(fp);
        printf("녹음 종료\n");
    }
    else {
        printf("파일 저장 실패\n");
        return;
    }
}

void play_audio(const char* filename) {
    HWAVEOUT hWaveOut;
    WAVEFORMATEX pFormat;
    WAVEHDR WaveOutHdr;
    char buffer[BUFFER_SIZE];
    MMRESULT result;

    // 파일 열기
    FILE* fp = fopen(filename, "rb");
    if (!fp) {
        printf("파일 열기 실패\n");
        return;
    }

    // WAV 헤더 건너뛰기
    fseek(fp, sizeof(WAVHeader), SEEK_SET);
    fread(buffer, sizeof(buffer), 1, fp);
    fclose(fp);

    // 재생 형식 설정
    pFormat.wFormatTag = WAVE_FORMAT_PCM;
    pFormat.nChannels = NUM_CHANNELS;
    pFormat.nSamplesPerSec = SAMPLE_RATE;
    pFormat.nAvgBytesPerSec = SAMPLE_RATE * NUM_CHANNELS * (BITS_PER_SAMPLE / 8);
    pFormat.nBlockAlign = NUM_CHANNELS * (BITS_PER_SAMPLE / 8);
    pFormat.wBitsPerSample = BITS_PER_SAMPLE;
    pFormat.cbSize = 0;

    // 재생 장치 열기
    result = waveOutOpen(&hWaveOut, WAVE_MAPPER, &pFormat, 0L, 0L, WAVE_FORMAT_DIRECT);
    if (result) {
        printf("재생 장치 열기 실패: %d\n", result);
        return;
    }

    // WAVEHDR 설정
    WaveOutHdr.lpData = buffer;
    WaveOutHdr.dwBufferLength = sizeof(buffer);
    WaveOutHdr.dwFlags = 0L;
    WaveOutHdr.dwLoops = 0L;

    // 버퍼 준비
    result = waveOutPrepareHeader(hWaveOut, &WaveOutHdr, sizeof(WAVEHDR));
    if (result) {
        printf("버퍼 준비 실패: %d\n", result);
        waveOutClose(hWaveOut);
        return;
    }

    // 재생 시작
    result = waveOutWrite(hWaveOut, &WaveOutHdr, sizeof(WAVEHDR));
    if (result) {
        printf("재생 시작 실패: %d\n", result);
        waveOutUnprepareHeader(hWaveOut, &WaveOutHdr, sizeof(WAVEHDR));
        waveOutClose(hWaveOut);
        return;
    }

    // 재생이 끝날 때까지 대기
    while (!(WaveOutHdr.dwFlags & WHDR_DONE)) {
        Sleep(100);
    }

    // 버퍼 해제
    waveOutUnprepareHeader(hWaveOut, &WaveOutHdr, sizeof(WAVEHDR));
    waveOutClose(hWaveOut);

    printf("재생 종료\n");
}

int main() {
    char command;
    char base_filename[100];
    char filename[200];
    char filepath[300];
    int counter = 1;

    while (1) {
        printf("명령을 입력하세요 (a: 녹음, b: 재생, q: 종료): ");
        scanf(" %c", &command);

        if (command == 'a') {
            // 고유한 파일 이름 생성
            get_current_time_string(base_filename, sizeof(base_filename));
            snprintf(filename, sizeof(filename), "%s_%d.wav", base_filename, counter);
            _getcwd(filepath, sizeof(filepath));
            snprintf(filepath + strlen(filepath), sizeof(filepath) - strlen(filepath), "\\%s", filename);

            record_audio(filepath);

            counter++; // 다음 녹음을 위한 카운터 증가
        }
        else if (command == 'b') {
            // 가장 최근에 녹음된 파일 재생
            if (counter > 1) {
                snprintf(filename, sizeof(filename), "%s_%d.wav", base_filename, counter - 1);
                _getcwd(filepath, sizeof(filepath));
                snprintf(filepath + strlen(filepath), sizeof(filepath) - strlen(filepath), "\\%s", filename);

                play_audio(filepath);
            }
            else {
                printf("재생할 파일이 없습니다.\n");
            }
        }
        else if (command == 'q') {
            break;
        }
        else {
            printf("잘못된 명령입니다.\n");
        }
    }

    return 0;
}
