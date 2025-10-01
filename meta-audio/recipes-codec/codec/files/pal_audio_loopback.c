#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <PalApi.h>
#include <PalDefs.h>
#include <string.h>

#define FRAMES 1024
#define SAMPLE_RATE 48000
#define CHANNELS 1
#define BIT_WIDTH 16
#define FORMAT PAL_AUDIO_FMT_PCM_S16_LE

#define PLAY_FILENAME  "/usr/share/codec/69.wav"
#define CAPTURE_OUT    "/tmp/pal_capture_output.pcm"

pal_stream_handle_t *pb_stream = NULL;
pal_stream_handle_t *cp_stream = NULL;

int main() {
    pal_init();

    FILE* infile = fopen("/usr/share/codec/69.wav", "rb");
    if (!infile) {
        perror("failed to open input file");
        pal_deinit();
        return -1;
    }
    else {
        printf("Opened input file %s for reading\n", "/usr/share/codec/69.wav");
    }

    struct pal_stream_attributes stream_attr;
    memset(&stream_attr, 0, sizeof(stream_attr));
    stream_attr.type = PAL_STREAM_LOOPBACK;
    stream_attr.direction = PAL_AUDIO_OUTPUT;
    stream_attr.in_media_config.sample_rate = SAMPLE_RATE;
    stream_attr.in_media_config.bit_width = BIT_WIDTH;
    stream_attr.in_media_config.aud_fmt_id = FORMAT;
    stream_attr.out_media_config.sample_rate = SAMPLE_RATE;
    stream_attr.out_media_config.bit_width = BIT_WIDTH;
    stream_attr.out_media_config.aud_fmt_id = FORMAT;   

    struct pal_device device;
    memset(&device, 0, sizeof(device));
    device.id = PAL_DEVICE_IN_FM_TUNER;
    device.config.sample_rate = SAMPLE_RATE;
    device.config.bit_width = BIT_WIDTH;
    device.config.aud_fmt_id = PAL_AUDIO_FMT_PCM_S16_LE;
    device.config.ch_info.channels = CHANNELS;

    printf("PAL Device ID: %d\n", device.id);

    // Open playback
    int ret = pal_stream_open(&stream_attr, 1, &device, 0, NULL, NULL, 0, &pb_stream);
    if (ret) {
        fprintf(stderr, "pal_stream_open failed: %d\n", ret);
        pal_deinit();
        return -1;
    }

    printf("Playback stream opened successfully\n");

    size_t size = FRAMES * CHANNELS * (BIT_WIDTH / 8);
    char *buffer = malloc(size);

    struct pal_buffer pal_buf;
    memset(&pal_buf, 0, sizeof(pal_buf));
    pal_buf.buffer = buffer;
    pal_buf.size = size;

    printf("Playing %s\n", "/usr/share/codec/69.wav");

    while (fread(buffer, 1, size, infile) > 0) {
        ssize_t written = pal_stream_write(pb_stream, &pal_buf);
        if (written < 0) {
            fprintf(stderr, "pal_stream_write error %zd\n", written);
            break;
        }
    }

    printf("Playback finished\n");

    free(buffer);
    fclose(infile);

    pal_stream_close(pb_stream);
    pal_deinit();

    return 0;
}

























// typedef struct {
//     const char *filename;
//     pal_stream_handle_t *stream;
// } playback_args_t;

// typedef struct {
//     const char *outfile;
//     pal_stream_handle_t *stream;
// } capture_args_t;

// // Playback thread
// void *playback_thread(void *arg) {
//     playback_args_t *args = (playback_args_t *)arg;
//     FILE *infile = fopen(args->filename, "rb");
//     if (!infile) {
//         perror("failed to open input file");
//         pthread_exit(NULL);
//     }

//     size_t size = FRAMES * CHANNELS * 2; // 2 bytes/sample
//     char *buffer = malloc(size);

//     printf("Playback thread: playing %s\n", args->filename);

//     while (fread(buffer, 1, size, infile) > 0) {
//         ssize_t written = pal_stream_write(args->stream, buffer, size);
//         if (written < 0) {
//             fprintf(stderr, "PAL playback error %zd\n", written);
//             break;
//         }
//     }

//     free(buffer);
//     fclose(infile);
//     pthread_exit(NULL);
// }

// // Capture thread
// void *capture_thread(void *arg) {
//     capture_args_t *args = (capture_args_t *)arg;
//     FILE *outfile = fopen(args->outfile, "wb");
//     if (!outfile) {
//         perror("failed to open output file");
//         pthread_exit(NULL);
//     }

//     size_t size = FRAMES * CHANNELS * 2;
//     char *buffer = malloc(size);

//     printf("Capture thread: recording to %s\n", args->outfile);

//     while (1) {
//         ssize_t read = pal_stream_read(args->stream, buffer, size);
//         if (read < 0) {
//             fprintf(stderr, "PAL capture error %zd\n", read);
//             break;
//         }
//         fwrite(buffer, 1, read, outfile);
//     }

//     free(buffer);
//     fclose(outfile);
//     pthread_exit(NULL);
// }
















// int main() {
//     pal_init();

//     pal_stream_handle_t *pb_stream = NULL;
//     pal_stream_handle_t *cp_stream = NULL;

//     // Open playback
//     if (pal_stream_open(&stream_attr, 1, &playback_device, 0, NULL, 0, &pb_stream) != PAL_STATUS_SUCCESS) {
//         fprintf(stderr, "Failed to open playback stream\n");
//         pal_deinit();
//         return -1;
//     }

//     // Open capture
//     stream_attr.direction = PAL_AUDIO_INPUT;
//     if (pal_stream_open(&stream_attr, 1, &capture_device, 0, NULL, 0, &cp_stream) != PAL_STATUS_SUCCESS) {
//         fprintf(stderr, "Failed to open capture stream\n");
//         pal_stream_close(pb_stream);
//         pal_deinit();
//         return -1;
//     }

//     size_t size = FRAMES * CHANNELS * 2; // 2 bytes/sample
//     char *buffer = malloc(size);
//     if (!buffer) {
//         fprintf(stderr, "Memory allocation failed\n");
//         pal_stream_close(pb_stream);
//         pal_stream_close(cp_stream);
//         pal_deinit();
//         return -1;
//     }

//     printf("Starting loopback test. Press Ctrl+C to stop.\n");

//     while (1) {
//         ssize_t read = pal_stream_read(cp_stream, buffer, size);
//         if (read < 0) {
//             fprintf(stderr, "PAL capture error %zd\n", read);
//             break;
//         }

//         ssize_t written = pal_stream_write(pb_stream, buffer, read);
//         if (written < 0) {
//             fprintf(stderr, "PAL playback error %zd\n", written);
//             break;
//         }
//     }

//     free(buffer);
//     pal_stream_close(pb_stream);
//     pal_stream_close(cp_stream);
//     pal_deinit();

//     return 0;
// }






