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
#define FORMAT PAL_AUDIO_FMT_PCM_S16_LE


static pal_device_id_t playback_device = PAL_DEVICE_OUT_BLUETOOTH_BLE;
static pal_device_id_t capture_device  = PAL_DEVICE_IN_BLUETOOTH_BLE;

static struct pal_stream_attributes stream_attr = {
    .type = PAL_STREAM_PCM,
    .direction = PAL_AUDIO_OUTPUT,
    .in_media_config = {
        .sample_rate = SAMPLE_RATE,
        .bit_width = 16,
        .num_channels = CHANNELS,
        .aud_fmt_id = FORMAT,
    },
    .out_media_config = {
        .sample_rate = SAMPLE_RATE,
        .bit_width = 16,
        .num_channels = CHANNELS,
        .aud_fmt_id = FORMAT,
    },
};

typedef struct {
    const char *filename;
    pal_stream_handle_t *stream;
} playback_args_t;

typedef struct {
    const char *outfile;
    pal_stream_handle_t *stream;
} capture_args_t;


int main() {
    pal_init();

    pal_stream_handle_t *pb_stream = NULL;
    pal_stream_handle_t *cp_stream = NULL;

    // Open playback
    pal_stream_open(&stream_attr, 1, &playback_device, 0, NULL, 0, &pb_stream);
    // Open capture
    stream_attr.direction = PAL_AUDIO_INPUT;
    pal_stream_open(&stream_attr, 1, &capture_device, 0, NULL, 0, &cp_stream);

    pthread_t t1, t2;
    playback_args_t pb_args = { "/usr/share/codec/69.wav", pb_stream };
    capture_args_t cp_args = { "/tmp/output.pcm", cp_stream };

    pthread_create(&t1, NULL, playback_thread, &pb_args);
    pthread_create(&t2, NULL, capture_thread, &cp_args);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    pal_stream_close(pb_stream);
    pal_stream_close(cp_stream);
    pal_deinit();

    return 0;
}


// Playback thread
void *playback_thread(void *arg) {
    playback_args_t *args = (playback_args_t *)arg;
    FILE *infile = fopen(args->filename, "rb");
    if (!infile) {
        perror("failed to open input file");
        pthread_exit(NULL);
    }

    size_t size = FRAMES * CHANNELS * 2; // 2 bytes/sample
    char *buffer = malloc(size);

    printf("Playback thread: playing %s\n", args->filename);

    while (fread(buffer, 1, size, infile) > 0) {
        ssize_t written = pal_stream_write(args->stream, buffer, size);
        if (written < 0) {
            fprintf(stderr, "PAL playback error %zd\n", written);
            break;
        }
    }

    free(buffer);
    fclose(infile);
    pthread_exit(NULL);
}

// Capture thread
void *capture_thread(void *arg) {
    capture_args_t *args = (capture_args_t *)arg;
    FILE *outfile = fopen(args->outfile, "wb");
    if (!outfile) {
        perror("failed to open output file");
        pthread_exit(NULL);
    }

    size_t size = FRAMES * CHANNELS * 2;
    char *buffer = malloc(size);

    printf("Capture thread: recording to %s\n", args->outfile);

    while (1) {
        ssize_t read = pal_stream_read(args->stream, buffer, size);
        if (read < 0) {
            fprintf(stderr, "PAL capture error %zd\n", read);
            break;
        }
        fwrite(buffer, 1, read, outfile);
    }

    free(buffer);
    fclose(outfile);
    pthread_exit(NULL);
}
















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






