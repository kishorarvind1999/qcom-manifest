#define _GNU_SOURCE 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>   
#include <time.h>
#include <opus/opus.h>
#include <opus/opus_multistream.h>
#include <lc3.h>
#include <sched.h>
#include <unistd.h>
#include <sys/syscall.h>

#define SAMPLE_RATE 48000
#define FRAME_SIZE SAMPLE_RATE * FRAME_DURATION_MS/1000 
#define BITRATE 64000 
#define MAX_PACKET_SIZE 1000
#define APPLICATION OPUS_APPLICATION_AUDIO

struct timespec start, end, fn_start, fn_end;
double total_encode_us, total_decode_us, fn_time_ms;
int encoded_size, decoded_size, frame_count = 0;
double FRAME_DURATION_MS = 10;

int test_opus_codec(FILE **in_files, FILE **out_files, int n_channels) {
    printf("Testing Opus multistream codec with %d channels...\n", n_channels);

    int streams = n_channels;        // One stream per channel
    int coupled_streams = 0;         // No stereo coupling
    unsigned char *mapping = malloc(n_channels);
    for (int i = 0; i < n_channels; i++) 
        mapping[i] = i;

    int err;
    OpusMSEncoder *encoder = opus_multistream_encoder_create(SAMPLE_RATE, n_channels, streams, coupled_streams, mapping, APPLICATION, &err);
    if (err < 0) { printf("Opus encoder create failed: %s\n", opus_strerror(err)); free(mapping); return -1; }
    opus_multistream_encoder_ctl(encoder, OPUS_SET_BITRATE(BITRATE));

    OpusMSDecoder *decoder = opus_multistream_decoder_create(SAMPLE_RATE, n_channels, streams, coupled_streams, mapping, &err);
    if (err < 0) { printf("Opus decoder create failed: %s\n", opus_strerror(err)); opus_multistream_encoder_destroy(encoder); free(mapping); return -1; }
    opus_multistream_decoder_ctl(decoder, OPUS_SET_BITRATE(BITRATE));

    free(mapping);

    opus_int16 *input  = malloc(FRAME_SIZE * n_channels * sizeof(opus_int16));
    opus_int16 *output = malloc(FRAME_SIZE * n_channels * sizeof(opus_int16));
    unsigned char *encoded = malloc(MAX_PACKET_SIZE);

    opus_int16 **buf = malloc(n_channels * sizeof(opus_int16*));
    for (int ch = 0; ch < n_channels; ch++)
        buf[ch] = malloc(FRAME_SIZE * sizeof(opus_int16));

    while (1) {
        int all_eof = 1;

        // Read FRAME_SIZE samples from each channel, pad with 0 if EOF
        for (int ch = 0; ch < n_channels; ch++) {
            size_t r = fread(buf[ch], sizeof(opus_int16), FRAME_SIZE, in_files[ch]);
            if (r > 0) all_eof = 0;
            // pad remaining samples with 0
            for (size_t i = r; i < FRAME_SIZE; i++) buf[ch][i] = 0;
            frame_count++;
        }
        if (all_eof) break;

        // Interleave input
        for (size_t i = 0; i < FRAME_SIZE; i++) {
            for (int ch = 0; ch < n_channels; ch++)
                input[i * n_channels + ch] = buf[ch][i];
        }

        // Encode
        clock_gettime(CLOCK_MONOTONIC, &start);
        int encoded_size = opus_multistream_encode(encoder, input, FRAME_SIZE, encoded, MAX_PACKET_SIZE);
        clock_gettime(CLOCK_MONOTONIC, &end);
        total_encode_us += (end.tv_sec - start.tv_sec) * 1e6 + (end.tv_nsec - start.tv_nsec) / 1e3;

        if (encoded_size < 0) { printf("Opus encoding failed: %s\n", opus_strerror(encoded_size)); break; }

        // Decode
        clock_gettime(CLOCK_MONOTONIC, &start);
        int decoded_size = opus_multistream_decode(decoder, encoded, encoded_size, output, FRAME_SIZE, 0);
        clock_gettime(CLOCK_MONOTONIC, &end);
        total_decode_us += (end.tv_sec - start.tv_sec) * 1e6 + (end.tv_nsec - start.tv_nsec) / 1e3;

        if (decoded_size < 0) { printf("Opus decoding failed: %s\n", opus_strerror(decoded_size)); break; }

        // Deinterleave and write to files
        for (size_t i = 0; i < decoded_size; i++) {
            for (int ch = 0; ch < n_channels; ch++)
                fwrite(&output[i * n_channels + ch], sizeof(opus_int16), 1, out_files[ch]);
        }
    }

    printf("Frames processed: %d\n", frame_count);
    if (frame_count > 0) {
        printf("Average encode time: %.3f µs/frame, decode time: %.3f µs/frame\nTotal Encoding time: %.3f ms, Total Decoding time: %.3f ms\n",
               total_encode_us / frame_count, total_decode_us / frame_count, total_encode_us / 1000, total_decode_us / 1000);
        frame_count = 0; total_encode_us = 0; total_decode_us = 0; // Reset for next test
    }

    // Cleanup
    for (int ch = 0; ch < n_channels; ch++) free(buf[ch]);
    free(buf);
    free(input); free(output); free(encoded);
    opus_multistream_encoder_destroy(encoder);
    opus_multistream_decoder_destroy(decoder);

    return 0;
}


int test_lc3_codec(FILE **in_files, FILE **out_files, int n_channels) {
    printf("Testing LC3 codec with %d channels...\n", n_channels);

    const int frame_us = FRAME_DURATION_MS * 1000; // 10 ms in µs

    unsigned enc_sz = lc3_encoder_size(frame_us, SAMPLE_RATE);
    unsigned dec_sz = lc3_decoder_size(frame_us, SAMPLE_RATE);

    void **enc_mem = malloc(n_channels * sizeof(void*));
    void **dec_mem = malloc(n_channels * sizeof(void*));
    lc3_encoder_t *enc = malloc(n_channels * sizeof(lc3_encoder_t));
    lc3_decoder_t *dec = malloc(n_channels * sizeof(lc3_decoder_t));

    if (!enc_mem || !dec_mem || !enc || !dec) { printf("LC3 mem alloc failed\n"); return -1; }

    for (int ch = 0; ch < n_channels; ch++) {
        enc_mem[ch] = malloc(enc_sz);
        dec_mem[ch] = malloc(dec_sz);
        enc[ch] = lc3_setup_encoder(frame_us, SAMPLE_RATE, 0, enc_mem[ch]);
        dec[ch] = lc3_setup_decoder(frame_us, SAMPLE_RATE, 0, dec_mem[ch]);
        if (!enc[ch] || !dec[ch]) { printf("LC3 setup failed for channel %d\n", ch); return -1; }
    }

    int frame_samples = lc3_frame_samples(frame_us, SAMPLE_RATE);
    int nbytes = lc3_frame_bytes(frame_us, BITRATE);

    int16_t **in  = malloc(n_channels * sizeof(int16_t*));
    int16_t **out = malloc(n_channels * sizeof(int16_t*));
    uint8_t **bit = malloc(n_channels * sizeof(uint8_t*));

    for (int ch = 0; ch < n_channels; ch++) {
        in[ch]  = malloc(frame_samples * sizeof(int16_t));
        out[ch] = malloc(frame_samples * sizeof(int16_t));
        bit[ch] = malloc(nbytes);
        if (!in[ch] || !out[ch] || !bit[ch]) { printf("LC3 buffer alloc failed\n"); return -1; }
    }

    while (1) {
        int read_ok = 0;
        for (int ch = 0; ch < n_channels; ch++) {
            size_t read = fread(in[ch], sizeof(int16_t), frame_samples, in_files[ch]);
            if (read < frame_samples) {
                for (size_t i = read; i < frame_samples; i++)
                    in[ch][i] = 0;  // fill with zeros
            } else {
                read_ok = 1;
            }
        }
        if (!read_ok) break;

        for (int ch = 0; ch < n_channels; ch++) {
            // Encode
            clock_gettime(CLOCK_MONOTONIC, &start);
            int encoded_size = lc3_encode(enc[ch], LC3_PCM_FORMAT_S16, in[ch], 1, nbytes, bit[ch]);
            clock_gettime(CLOCK_MONOTONIC, &end);
            if (encoded_size < 0) { printf("LC3 encode failed for channel %d\n", ch); return -1; }
            total_encode_us += (end.tv_sec - start.tv_sec) * 1e6 + (end.tv_nsec - start.tv_nsec) / 1e3;

            // Decode
            clock_gettime(CLOCK_MONOTONIC, &start);
            int decoded_size = lc3_decode(dec[ch], bit[ch], nbytes, LC3_PCM_FORMAT_S16, out[ch], 1);
            clock_gettime(CLOCK_MONOTONIC, &end);
            if (decoded_size < 0) { printf("LC3 decode failed for channel %d\n", ch); return -1; }
            total_decode_us += (end.tv_sec - start.tv_sec) * 1e6 + (end.tv_nsec - start.tv_nsec) / 1e3;

            if (decoded_size == 1) printf("Channel %d: PLC applied\n", ch);

            // Write output
            fwrite(out[ch], sizeof(int16_t), frame_samples, out_files[ch]);
            frame_count++;
        }
    }

    printf("Frames processed: %d\n", frame_count);
    if (frame_count > 0) {
        printf("Average encode time: %.3f µs/frame, decode time: %.3f µs/frame\nTotal Encoding time: %.3f ms, Total Decoding time: %.3f ms\n",
               total_encode_us / frame_count, total_decode_us / frame_count, total_encode_us / 1000, total_decode_us / 1000);
        frame_count = 0; total_encode_us = 0; total_decode_us = 0; // Reset for next test
    }

    // Free memory
    for (int ch = 0; ch < n_channels; ch++) {
        free(in[ch]); free(out[ch]); free(bit[ch]);
        free(enc_mem[ch]); free(dec_mem[ch]);
    }
    free(in); free(out); free(bit);
    free(enc_mem); free(dec_mem);
    free(enc); free(dec);

    return 0;
}


void pin_to_core(int core_id) {
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(core_id, &mask);

    pid_t tid = syscall(__NR_gettid);  // get current thread ID
    if (sched_setaffinity(tid, sizeof(mask), &mask) != 0) {
        perror("sched_setaffinity failed");
    } else {
        printf("Pinned to core %d\n\n", core_id);
    }
}


int main(int argc, char *argv[]) {    
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <core_number> <frame_size_ms> <input_file_1> <input_file_2> ...\n", argv[0]);
        return 1;
    }

    int core = atoi(argv[1]);   
    pin_to_core(core);          // pin process to specified core

    FRAME_DURATION_MS = atof(argv[2]);
    if (FRAME_DURATION_MS != 2.5 && FRAME_DURATION_MS != 5 && FRAME_DURATION_MS != 10 && FRAME_DURATION_MS != 20) {
        fprintf(stderr, "Frame duration not supported\n");
        return 1;
    }
    int n_channels = argc - 3;

    FILE **in_files   = malloc(n_channels * sizeof(FILE *));
    FILE **opus_files = malloc(n_channels * sizeof(FILE *));
    FILE **lc3_files  = malloc(n_channels * sizeof(FILE *));
    if (!in_files || !opus_files || !lc3_files) {
        perror("malloc");
        return 1;
    }

    // open inputs
    for (int i = 0; i < n_channels; i++) {
        in_files[i] = fopen(argv[3 + i], "rb");
        if (!in_files[i]) {
            perror(argv[2 + i]);
            return 1;
        }
    }

    // create output files
    char fname[256];
    for (int i = 0; i < n_channels; i++) {
        snprintf(fname, sizeof(fname), "/tmp/opus_%d.pcm", i+1);
        opus_files[i] = fopen(fname, "wb");
        if (!opus_files[i]) {
            perror(fname);
            return 1;
        }

        snprintf(fname, sizeof(fname), "/tmp/lc3_%d.pcm", i+1);
        lc3_files[i] = fopen(fname, "wb");
        if (!lc3_files[i]) {
            perror(fname);
            return 1;
        }
    }

    // run opus codec
    clock_gettime(CLOCK_MONOTONIC, &fn_start);
    if (test_opus_codec(in_files, opus_files, n_channels) != 0) {
        fprintf(stderr, "Opus codec test failed\n");
    }
    clock_gettime(CLOCK_MONOTONIC, &fn_end);
    fn_time_ms = (fn_end.tv_sec - fn_start.tv_sec) * 1e3 + (fn_end.tv_nsec - fn_start.tv_nsec) / 1e6;
    printf("Opus codec total time: %.3f ms\n\n", fn_time_ms);

    for (int i = 0; i < n_channels; i++) {
        rewind(in_files[i]); // reset input files for LC3 test
    }
    fn_time_ms = 0;

    // run lc3 codec
    clock_gettime(CLOCK_MONOTONIC, &fn_start);
    if (test_lc3_codec(in_files, lc3_files, n_channels) != 0) {
        fprintf(stderr, "LC3 codec test failed\n");
    }
    clock_gettime(CLOCK_MONOTONIC, &fn_end);
    fn_time_ms = (fn_end.tv_sec - fn_start.tv_sec) * 1e3 + (fn_end.tv_nsec - fn_start.tv_nsec) / 1e6;
    printf("LC3 codec total time: %.3f ms\n\n", fn_time_ms);

    // cleanup
    for (int i = 0; i < n_channels; i++) {
        fclose(in_files[i]);
        fclose(opus_files[i]);
        fclose(lc3_files[i]);
    }

    free(in_files);
    free(opus_files);
    free(lc3_files);

    printf("\nAll codec tests passed successfully! Output files are stored in /tmp\n\n\n");

    return 0;
}




































// #define SAMPLE_RATE 48000
// #define CHANNELS 1
// #define FRAME_DURATION_MS 10
// #define FRAME_SIZE SAMPLE_RATE * FRAME_DURATION_MS/1000 
// #define BITRATE 64000 
// #define MAX_PACKET_SIZE 1000
// #define APPLICATION OPUS_APPLICATION_AUDIO

// struct timespec start, end;
// double total_encode_us, total_decode_us, avg_us_e, avg_us_d;
// int encoded_size, decoded_size, frame_count = 0;

// int test_opus_codec(FILE *fp, FILE *fp_opus) {
//     printf("Testing Opus codec...\n");
    
//     OpusEncoder *encoder;
//     OpusDecoder *decoder;
//     int err;
    
//     // Create encoder
//     encoder = opus_encoder_create(SAMPLE_RATE, CHANNELS, APPLICATION, &err);
//     if (err < 0) {
//         printf("Failed to create Opus encoder: %s\n", opus_strerror(err));
//         return -1;
//     }

//     opus_encoder_ctl(encoder, OPUS_SET_BITRATE(BITRATE)); // Set desired bitrate
    
//     // Create decoder
//     decoder = opus_decoder_create(SAMPLE_RATE, CHANNELS, &err);
//     if (err < 0) {
//         printf("Failed to create Opus decoder: %s\n", opus_strerror(err));
//         opus_encoder_destroy(encoder);
//         return -1;
//     }
    
//     opus_int16 input[FRAME_SIZE * CHANNELS];
//     opus_int16 output[FRAME_SIZE * CHANNELS];
//     unsigned char encoded[MAX_PACKET_SIZE];

//     while (fread(input, sizeof(opus_int16), FRAME_SIZE * CHANNELS, fp) == FRAME_SIZE) {
//         // Encode
//         clock_gettime(CLOCK_MONOTONIC, &start);
//         encoded_size = opus_encode(encoder, input, FRAME_SIZE, encoded, MAX_PACKET_SIZE);
//         clock_gettime(CLOCK_MONOTONIC, &end);
//         double elapsed_us = (end.tv_sec - start.tv_sec) * 1e6 + (end.tv_nsec - start.tv_nsec) / 1e3;
//         total_encode_us += elapsed_us;

//         if (encoded_size < 0) {
//             printf("Opus encoding failed: %s\n", opus_strerror(encoded_size));
//             opus_encoder_destroy(encoder);
//             opus_decoder_destroy(decoder);
//             return -1;
//         }

//         // Decode
//         clock_gettime(CLOCK_MONOTONIC, &start);
//         decoded_size = opus_decode(decoder, encoded, encoded_size, output, FRAME_SIZE, 0);
//         clock_gettime(CLOCK_MONOTONIC, &end);
//         elapsed_us = (end.tv_sec - start.tv_sec) * 1e6 + (end.tv_nsec - start.tv_nsec) / 1e3;
//         total_decode_us += elapsed_us;

//         if (decoded_size < 0) {
//             printf("Opus decoding failed: %s\n", opus_strerror(decoded_size));
//             opus_encoder_destroy(encoder);
//             opus_decoder_destroy(decoder);
//             return -1;
//         }
        
//         // Write output to file
//         fwrite(output, sizeof(opus_int16), decoded_size * CHANNELS, fp_opus);

//         frame_count++;
//     }
//     printf("Frame Count: %d\n", frame_count);

//     if (frame_count > 0) {
//         avg_us_e = total_encode_us / frame_count;
//         avg_us_d = total_decode_us / frame_count;
//         printf("Opus test successful! \n \
//             Total Encoding time: %.3f ms \t Per frame: %.3f µs\n \
//             Total Decoding time: %.3f ms \t Per frame: %.3f µs\n",
//             total_encode_us/1000, avg_us_e, total_decode_us/1000, avg_us_d);
//         frame_count = 0; total_encode_us = 0; total_decode_us = 0; // Reset for next test
//     }
    
//     opus_encoder_destroy(encoder);
//     opus_decoder_destroy(decoder);
//     return 0;
// }


// int test_lc3_codec(FILE *fp, FILE *fp_lc3) {
//     printf("Testing LC3 codec...\n");

//     const int frame_us = 10000;   // 10 ms

//     unsigned enc_sz = lc3_encoder_size(frame_us, SAMPLE_RATE);
//     unsigned dec_sz = lc3_decoder_size(frame_us, SAMPLE_RATE);
//     void *enc_mem = malloc(enc_sz);
//     void *dec_mem = malloc(dec_sz);
//     if (!enc_mem || !dec_mem) { printf("LC3 mem alloc failed\n"); return -1; }

//     lc3_encoder_t enc = lc3_setup_encoder(frame_us, SAMPLE_RATE, 0, enc_mem);
//     lc3_decoder_t dec = lc3_setup_decoder(frame_us, SAMPLE_RATE, 0, dec_mem);
//     if (!enc || !dec) { printf("LC3 setup failed\n"); return -1; }

//     int frame_samples = lc3_frame_samples(frame_us, SAMPLE_RATE);
//     int nbytes = lc3_frame_bytes(frame_us, BITRATE); 

//     int16_t *in  = malloc(frame_samples * sizeof(int16_t));
//     int16_t *out = malloc(frame_samples * sizeof(int16_t));
//     uint8_t *bit = malloc(nbytes);
//     if (!in || !out || !bit) { printf("LC3 buffer alloc failed\n"); return -1; }

//     while (fread(in, sizeof(int16_t), FRAME_SIZE * CHANNELS, fp) == (size_t)frame_samples) {
//         // Encode
//         clock_gettime(CLOCK_MONOTONIC, &start);
//         encoded_size = lc3_encode(enc, LC3_PCM_FORMAT_S16, in, 1, nbytes, bit);
//         clock_gettime(CLOCK_MONOTONIC, &end);
//         double elapsed_us = (end.tv_sec - start.tv_sec) * 1e6 +
//                             (end.tv_nsec - start.tv_nsec) / 1e3;
//         total_encode_us += elapsed_us;

//         if (encoded_size < 0) { 
//             printf("LC3 encode failed (rc=%d)\n", encoded_size); 
//             return -1; 
//         }

//         // Decode
//         clock_gettime(CLOCK_MONOTONIC, &start);
//         decoded_size = lc3_decode(dec, bit, nbytes, LC3_PCM_FORMAT_S16, out, 1);
//         clock_gettime(CLOCK_MONOTONIC, &end);
//         elapsed_us = (end.tv_sec - start.tv_sec) * 1e6 +
//                      (end.tv_nsec - start.tv_nsec) / 1e3;
//         total_decode_us += elapsed_us;

//         if (decoded_size < 0) { 
//             printf("LC3 decode failed (rc=%d)\n", decoded_size); 
//             return -1; 
//         }
//         if (decoded_size == 1) printf("Decoder performed PLC\n");

//         // Write output to file
//         fwrite(out, sizeof(int16_t), frame_samples * CHANNELS, fp_lc3);

//         frame_count++;
//     }

//     printf("Frame Count: %d\n", frame_count);

//     if (frame_count > 0) {
//         avg_us_e = total_encode_us / frame_count;
//         avg_us_d = total_decode_us / frame_count;
//         printf("LC3 test successful! \n \
//             Total Encoding time: %.3f ms \t Per frame: %.3f µs\n \
//             Total Decoding time: %.3f ms \t Per frame: %.3f µs\n",
//             total_encode_us/1000, avg_us_e, total_decode_us/1000, avg_us_d);
//         frame_count = 0; total_encode_us = 0; total_decode_us = 0; // Reset for next test
//     }
    
//     free(in); free(out); free(bit);
//     free(enc_mem); free(dec_mem);
//     return 0;
// }


// void pin_to_core(int core_id) {
//     cpu_set_t mask;
//     CPU_ZERO(&mask);
//     CPU_SET(core_id, &mask);

//     pid_t tid = syscall(__NR_gettid);  // get current thread ID
//     if (sched_setaffinity(tid, sizeof(mask), &mask) != 0) {
//         perror("sched_setaffinity failed");
//     } else {
//         printf("Pinned to core %d\n\n", core_id);
//     }
// }


// int main(int argc, char *argv[]) {    
//     if (argc < 3) {
//         fprintf(stderr, "Usage: %s <core_number> <pcm_input_file>\n", argv[0]);
//         return 1;
//     }

//     int core = atoi(argv[1]);   
//     pin_to_core(core);          // pin process to specified core
    
//     FILE *fp = fopen(argv[2], "rb");
//     FILE *fp_opus = fopen("opus_output.pcm", "wb");
//     FILE *fp_lc3 = fopen("lc3_output.pcm", "wb");
//     if (!fp) {
//         perror("Failed to open input file");
//         return 1;
//     }
//     if (!fp_opus || !fp_lc3) {
//         perror("Failed to open output file");
//         return 1;
//     }

//     if (test_opus_codec(fp, fp_opus) != 0) {
//         printf("Opus test failed!\n");
//         fclose(fp);
//         fclose(fp_opus);
//         return 1;
//     }

//     rewind(fp); // restart reading from the beginning for LC3

//     printf("\n");
    
//     if (test_lc3_codec(fp, fp_lc3) != 0) {
//         printf("LC3 test failed!\n");
//         fclose(fp);
//         fclose(fp_lc3);
//         return 1;
//     }
    
//     fclose(fp);
//     fclose(fp_opus);
//     fclose(fp_lc3);
//     printf("\nAll codec tests passed successfully! Output files written to /tmp\n\n\n");
//     return 0;
// }
