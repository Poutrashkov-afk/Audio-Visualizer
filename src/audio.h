#ifndef AUDIO_H
#define AUDIO_H

#include <portaudio.h>
#include <stdbool.h>
#include "settings.h"

typedef struct {
    PaStream *stream;
    int sample_rate;
    int chunk_size;
    int channels;

    float *buffer;
    int buffer_size;
    volatile int write_pos;

    float *current_chunk;
    bool chunk_ready;

    float rms_level;
    float peak_level;

    bool running;
    int device_index;

    /* Stream health tracking */
    volatile int silent_frames;     /* consecutive silent frames */
    volatile bool had_signal;       /* have we ever received actual audio? */
    int overflow_count;
    int underflow_count;
    int recovery_attempts;

    /* Device list */
    AudioDeviceInfo devices[MAX_DEVICES];
    int device_count;
} AudioEngine;

bool audio_init(AudioEngine *audio);
void audio_destroy(AudioEngine *audio);
bool audio_start(AudioEngine *audio, int device_index, int sample_rate, int chunk_size);
void audio_stop(AudioEngine *audio);
bool audio_restart_device(AudioEngine *audio, int device_index);
void audio_get_chunk(AudioEngine *audio, float *out, int size);
void audio_scan_devices(AudioEngine *audio);
void audio_compute_levels(AudioEngine *audio);
bool audio_check_health(AudioEngine *audio);
float audio_get_silence_duration(AudioEngine *audio);

#endif