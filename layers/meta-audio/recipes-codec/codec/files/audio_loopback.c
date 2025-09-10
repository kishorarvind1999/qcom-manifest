#include <stdio.h>
#include <stdlib.h>
#include <alsa/asoundlib.h>
#include <pthread.h>

#define FRAMES 1024
#define SAMPLE_RATE 48000
#define CHANNELS 1
#define FORMAT SND_PCM_FORMAT_S16_LE

// Playback thread arguments
typedef struct {
    const char *filename;
    const char *device;
} playback_args_t;

// Capture thread arguments
typedef struct {
    const char *outfile;
    const char *device;
} capture_args_t;

// Playback thread: read from file and play through ALSA
void *playback_thread(void *arg) {
    playback_args_t *args = (playback_args_t *)arg;
    snd_pcm_t *handle;
    snd_pcm_hw_params_t *params;
    FILE *infile;
    int rc, dir;
    snd_pcm_uframes_t frames = FRAMES;
    char *buffer;
    int size;

    infile = fopen(args->filename, "rb");
    if (!infile) {
        perror("failed to open input file");
        pthread_exit(NULL);
    }

    // Open playback device
    rc = snd_pcm_open(&handle, args->device, SND_PCM_STREAM_PLAYBACK, 0);
    if (rc < 0) {
        fprintf(stderr, "unable to open playback device: %s\n", snd_strerror(rc));
        pthread_exit(NULL);
    }

    // Set playback params
    snd_pcm_hw_params_alloca(&params);
    snd_pcm_hw_params_any(handle, params);
    snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(handle, params, FORMAT);
    snd_pcm_hw_params_set_channels(handle, params, CHANNELS);
    unsigned int rate = SAMPLE_RATE;
    snd_pcm_hw_params_set_rate_near(handle, params, &rate, &dir);
    snd_pcm_hw_params_set_period_size_near(handle, params, &frames, &dir);
    rc = snd_pcm_hw_params(handle, params);
    if (rc < 0) {
        fprintf(stderr, "unable to set playback params: %s\n", snd_strerror(rc));
        pthread_exit(NULL);
    }

    snd_pcm_hw_params_get_period_size(params, &frames, &dir);
    size = frames * 2 * CHANNELS; // 2 bytes/sample
    buffer = malloc(size);

    printf("Playback thread: playing %s on %s\n", args->filename, args->device);

    while ((rc = fread(buffer, 1, size, infile)) > 0) {
        rc = snd_pcm_writei(handle, buffer, frames);
        if (rc == -EPIPE) {
            fprintf(stderr, "playback underrun\n");
            snd_pcm_prepare(handle);
        } else if (rc < 0) {
            fprintf(stderr, "error from write: %s\n", snd_strerror(rc));
        }
    }

    printf("Playback finished\n");

    free(buffer);
    fclose(infile);
    snd_pcm_close(handle);
    pthread_exit(NULL);
}

// Capture thread: record from loopback and store to file
void *capture_thread(void *arg) {
    capture_args_t *args = (capture_args_t *)arg;
    snd_pcm_t *handle;
    snd_pcm_hw_params_t *params;
    FILE *outfile;
    int rc, dir;
    snd_pcm_uframes_t frames = FRAMES;
    char *buffer;
    int size;

    outfile = fopen(args->outfile, "wb");
    if (!outfile) {
        perror("failed to open output file");
        pthread_exit(NULL);
    }

    // Open capture device
    rc = snd_pcm_open(&handle, args->device, SND_PCM_STREAM_CAPTURE, 0);
    if (rc < 0) {
        fprintf(stderr, "unable to open capture device: %s\n", snd_strerror(rc));
        pthread_exit(NULL);
    }

    // Set capture params
    snd_pcm_hw_params_alloca(&params);
    snd_pcm_hw_params_any(handle, params);
    snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(handle, params, FORMAT);
    snd_pcm_hw_params_set_channels(handle, params, CHANNELS);
    unsigned int rate = SAMPLE_RATE;
    snd_pcm_hw_params_set_rate_near(handle, params, &rate, &dir);
    snd_pcm_hw_params_set_period_size_near(handle, params, &frames, &dir);
    rc = snd_pcm_hw_params(handle, params);
    if (rc < 0) {
        fprintf(stderr, "unable to set capture params: %s\n", snd_strerror(rc));
        pthread_exit(NULL);
    }

    snd_pcm_hw_params_get_period_size(params, &frames, &dir);
    size = frames * 2 * CHANNELS;
    buffer = malloc(size);

    printf("Capture thread: recording from %s to %s\n", args->device, args->outfile);

    while (1) {
        rc = snd_pcm_readi(handle, buffer, frames);
        if (rc == -EPIPE) {
            fprintf(stderr, "capture overrun\n");
            snd_pcm_prepare(handle);
        } else if (rc < 0) {
            fprintf(stderr, "error from read: %s\n", snd_strerror(rc));
        }
        fwrite(buffer, 1, size, outfile);
        if (rc < 1024) {
            printf("Captured %d frames\n", rc);
        }
    }

    free(buffer);
    fclose(outfile);
    snd_pcm_close(handle);
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <playback_device> <capture_device>\n", argv[0]);
        return 1; // Return an error code
    }

    pthread_t t1, t2;
    playback_args_t pb_args = { "/usr/share/codec/69.wav", argv[1] };
    capture_args_t cp_args = { "/tmp/output.pcm", argv[2] };

    pthread_create(&t1, NULL, playback_thread, &pb_args);
    pthread_create(&t2, NULL, capture_thread, &cp_args);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    return 0;
}
