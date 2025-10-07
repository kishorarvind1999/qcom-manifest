#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <PalApi.h>
#include <PalDefs.h>

#define SAMPLE_RATE 48000
#define CHANNELS 2
#define BIT_WIDTH 16
#define FORMAT PAL_AUDIO_FMT_PCM_S16_LE

int main() {
    int ret = 0;

    ret = pal_init();
    if (ret) {
        fprintf(stderr, "pal_init failed: %d\n", ret);
        return -1;
    }
    printf("PAL initialized\n");

    struct pal_stream_attributes stream_attr;
    memset(&stream_attr, 0, sizeof(stream_attr));
    stream_attr.type = PAL_STREAM_LOOPBACK;
    stream_attr.direction = PAL_AUDIO_INPUT_OUTPUT;
    stream_attr.in_media_config.sample_rate = SAMPLE_RATE;
    stream_attr.in_media_config.bit_width   = BIT_WIDTH;
    stream_attr.in_media_config.aud_fmt_id  = FORMAT;
    stream_attr.in_media_config.ch_info.channels = CHANNELS;
    stream_attr.in_media_config.ch_info.ch_map[0] = PAL_CHMAP_CHANNEL_FL;
    stream_attr.in_media_config.ch_info.ch_map[1] = PAL_CHMAP_CHANNEL_FR;
    stream_attr.out_media_config = stream_attr.in_media_config; // same config

    // Devices: capture + playback
    struct pal_device devices[2];
    memset(devices, 0, sizeof(devices));

    devices[0].id = PAL_DEVICE_IN_HANDSET_MIC;   // this maps to PRIMARY_MI2S_TX in your DT
    devices[0].config = stream_attr.in_media_config;

    devices[1].id = PAL_DEVICE_OUT_SPEAKER;      // this maps to PRIMARY_MI2S_RX
    devices[1].config = stream_attr.out_media_config;

    pal_stream_handle_t *stream = NULL;

    ret = pal_stream_open(&stream_attr, 2, devices, 0, NULL, NULL, 0, &stream);
    if (ret) {
        fprintf(stderr, "pal_stream_open failed: %d\n", ret);
        pal_deinit();
        return -1;
    }
    printf("Loopback stream opened successfully\n");

    ret = pal_stream_start(stream);
    if (ret) {
        fprintf(stderr, "pal_stream_start failed: %d\n", ret);
        pal_stream_close(stream);
        pal_deinit();
        return -1;
    }
    printf("Loopback started: capture -> playback path is active\n");

    // Let it run for 10 seconds
    sleep(10);

    pal_stream_stop(stream);
    pal_stream_close(stream);
    pal_deinit();

    printf("Loopback stopped and cleaned up\n");
    return 0;
}