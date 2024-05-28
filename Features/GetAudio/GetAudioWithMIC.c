#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>
#include <time.h>
#include <direct.h>

#pragma comment(lib, "winmm.lib")

#define RECORD_TIME 5 // ���� �ð�(��)
#define SAMPLE_RATE 44100 // ���÷���Ʈ(Hz)
#define BITS_PER_SAMPLE 16 // ���ô� ��Ʈ ��
#define NUM_CHANNELS 1 // ä�� ��(���)
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

    // ���� ���� ����
    pFormat.wFormatTag = WAVE_FORMAT_PCM;
    pFormat.nChannels = NUM_CHANNELS;
    pFormat.nSamplesPerSec = SAMPLE_RATE;
    pFormat.nAvgBytesPerSec = SAMPLE_RATE * NUM_CHANNELS * (BITS_PER_SAMPLE / 8);
    pFormat.nBlockAlign = NUM_CHANNELS * (BITS_PER_SAMPLE / 8);
    pFormat.wBitsPerSample = BITS_PER_SAMPLE;
    pFormat.cbSize = 0;

    // ���� ��ġ ����
    result = waveInOpen(&hWaveIn, WAVE_MAPPER, &pFormat, 0L, 0L, WAVE_FORMAT_DIRECT);
    if (result) {
        printf("���� ��ġ ���� ����: %d\n", result);
        return;
    }

    // WAVEHDR ����
    WaveInHdr.lpData = buffer;
    WaveInHdr.dwBufferLength = sizeof(buffer);
    WaveInHdr.dwBytesRecorded = 0;
    WaveInHdr.dwUser = 0L;
    WaveInHdr.dwFlags = 0L;
    WaveInHdr.dwLoops = 0L;

    // ���� �غ�
    result = waveInPrepareHeader(hWaveIn, &WaveInHdr, sizeof(WAVEHDR));
    if (result) {
        printf("���� �غ� ����: %d\n", result);
        waveInClose(hWaveIn);
        return;
    }

    result = waveInAddBuffer(hWaveIn, &WaveInHdr, sizeof(WAVEHDR));
    if (result) {
        printf("���� �߰� ����: %d\n", result);
        waveInUnprepareHeader(hWaveIn, &WaveInHdr, sizeof(WAVEHDR));
        waveInClose(hWaveIn);
        return;
    }

    // ���� ����
    result = waveInStart(hWaveIn);
    if (result) {
        printf("���� ���� ����: %d\n", result);
        waveInUnprepareHeader(hWaveIn, &WaveInHdr, sizeof(WAVEHDR));
        waveInClose(hWaveIn);
        return;
    }

    Sleep(RECORD_TIME * 1000);
    waveInStop(hWaveIn);

    // ���� ����
    waveInUnprepareHeader(hWaveIn, &WaveInHdr, sizeof(WAVEHDR));
    waveInClose(hWaveIn);

    // ���Ϸ� ����
    FILE* fp = fopen(filename, "wb");
    if (fp) {
        write_wav_header(fp, sizeof(buffer));
        fwrite(buffer, sizeof(buffer), 1, fp);
        fclose(fp);
        printf("���� ����\n");
    }
    else {
        printf("���� ���� ����\n");
        return;
    }
}

void play_audio(const char* filename) {
    HWAVEOUT hWaveOut;
    WAVEFORMATEX pFormat;
    WAVEHDR WaveOutHdr;
    char buffer[BUFFER_SIZE];
    MMRESULT result;

    // ���� ����
    FILE* fp = fopen(filename, "rb");
    if (!fp) {
        printf("���� ���� ����\n");
        return;
    }

    // WAV ��� �ǳʶٱ�
    fseek(fp, sizeof(WAVHeader), SEEK_SET);
    fread(buffer, sizeof(buffer), 1, fp);
    fclose(fp);

    // ��� ���� ����
    pFormat.wFormatTag = WAVE_FORMAT_PCM;
    pFormat.nChannels = NUM_CHANNELS;
    pFormat.nSamplesPerSec = SAMPLE_RATE;
    pFormat.nAvgBytesPerSec = SAMPLE_RATE * NUM_CHANNELS * (BITS_PER_SAMPLE / 8);
    pFormat.nBlockAlign = NUM_CHANNELS * (BITS_PER_SAMPLE / 8);
    pFormat.wBitsPerSample = BITS_PER_SAMPLE;
    pFormat.cbSize = 0;

    // ��� ��ġ ����
    result = waveOutOpen(&hWaveOut, WAVE_MAPPER, &pFormat, 0L, 0L, WAVE_FORMAT_DIRECT);
    if (result) {
        printf("��� ��ġ ���� ����: %d\n", result);
        return;
    }

    // WAVEHDR ����
    WaveOutHdr.lpData = buffer;
    WaveOutHdr.dwBufferLength = sizeof(buffer);
    WaveOutHdr.dwFlags = 0L;
    WaveOutHdr.dwLoops = 0L;

    // ���� �غ�
    result = waveOutPrepareHeader(hWaveOut, &WaveOutHdr, sizeof(WAVEHDR));
    if (result) {
        printf("���� �غ� ����: %d\n", result);
        waveOutClose(hWaveOut);
        return;
    }

    // ��� ����
    result = waveOutWrite(hWaveOut, &WaveOutHdr, sizeof(WAVEHDR));
    if (result) {
        printf("��� ���� ����: %d\n", result);
        waveOutUnprepareHeader(hWaveOut, &WaveOutHdr, sizeof(WAVEHDR));
        waveOutClose(hWaveOut);
        return;
    }

    // ����� ���� ������ ���
    while (!(WaveOutHdr.dwFlags & WHDR_DONE)) {
        Sleep(100);
    }

    // ���� ����
    waveOutUnprepareHeader(hWaveOut, &WaveOutHdr, sizeof(WAVEHDR));
    waveOutClose(hWaveOut);

    printf("��� ����\n");
}

int main() {
    char command;
    char base_filename[100];
    char filename[200];
    char filepath[300];
    int counter = 1;

    while (1) {
        printf("����� �Է��ϼ��� (a: ����, b: ���, q: ����): ");
        scanf(" %c", &command);

        if (command == 'a') {
            // ������ ���� �̸� ����
            get_current_time_string(base_filename, sizeof(base_filename));
            snprintf(filename, sizeof(filename), "%s_%d.wav", base_filename, counter);
            _getcwd(filepath, sizeof(filepath));
            snprintf(filepath + strlen(filepath), sizeof(filepath) - strlen(filepath), "\\%s", filename);

            record_audio(filepath);

            counter++; // ���� ������ ���� ī���� ����
        }
        else if (command == 'b') {
            // ���� �ֱٿ� ������ ���� ���
            if (counter > 1) {
                snprintf(filename, sizeof(filename), "%s_%d.wav", base_filename, counter - 1);
                _getcwd(filepath, sizeof(filepath));
                snprintf(filepath + strlen(filepath), sizeof(filepath) - strlen(filepath), "\\%s", filename);

                play_audio(filepath);
            }
            else {
                printf("����� ������ �����ϴ�.\n");
            }
        }
        else if (command == 'q') {
            break;
        }
        else {
            printf("�߸��� ����Դϴ�.\n");
        }
    }

    return 0;
}
