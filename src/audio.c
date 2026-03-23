#include "audio.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

/* Timeout before we consider the stream might need recovery */
#define SILENCE_RECOVERY_SECONDS 30.0

static int audio_callback(const void *input, void *output,
                           unsigned long frame_count,
                           const PaStreamCallbackTimeInfo *time_info,
                           PaStreamCallbackFlags status_flags,
                           void *user_data) {
    (void)output;
    (void)time_info;

    AudioEngine *audio = (AudioEngine *)user_data;

    /* Track underflows / overflows but don't stop */
    if (status_flags & paInputOverflow) {
        audio->overflow_count++;
    }
    if (status_flags & paInputUnderflow) {
        audio->underflow_count++;
    }

    if (!input) {
        /* No input data — fill with silence but keep running */
        for (unsigned long i = 0; i < frame_count; i++) {
            audio->buffer[audio->write_pos] = 0.0f;
            audio->write_pos = (audio->write_pos + 1) % audio->buffer_size;
        }
        audio->chunk_ready = true;
        audio->silent_frames += (int)frame_count;
        return paContinue;
    }

    const float *in = (const float *)input;
    bool has_signal = false;

    for (unsigned long i = 0; i < frame_count; i++) {
        float sample;
        if (audio->channels >= 2) {
            sample = (in[i * audio->channels] + in[i * audio->channels + 1]) * 0.5f;
        } else {
            sample = in[i * audio->channels];
        }

        /* Check if we have any actual audio signal */
        if (fabsf(sample) > 0.00001f) {
            has_signal = true;
        }

        audio->buffer[audio->write_pos] = sample;
        audio->write_pos = (audio->write_pos + 1) % audio->buffer_size;
    }

    audio->chunk_ready = true;

    if (has_signal) {
        audio->silent_frames = 0;
        audio->had_signal = true;
    } else {
        audio->silent_frames += (int)frame_count;
    }

    /* ALWAYS return paContinue — never let the stream die */
    return paContinue;
}

bool audio_init(AudioEngine *audio) {
    memset(audio, 0, sizeof(AudioEngine));

    PaError err = Pa_Initialize();
    if (err != paNoError) {
        fprintf(stderr, "PortAudio init failed: %s\n", Pa_GetErrorText(err));
        return false;
    }

    audio->buffer_size = 44100 * 4; /* 4 seconds circular buffer */
    audio->buffer = (float *)calloc(audio->buffer_size, sizeof(float));
    audio->current_chunk = NULL;
    audio->silent_frames = 0;
    audio->had_signal = false;
    audio->overflow_count = 0;
    audio->underflow_count = 0;
    audio->recovery_attempts = 0;

    audio_scan_devices(audio);
    return true;
}

void audio_destroy(AudioEngine *audio) {
    audio_stop(audio);
    free(audio->buffer);
    free(audio->current_chunk);
    Pa_Terminate();
    memset(audio, 0, sizeof(AudioEngine));
}

void audio_scan_devices(AudioEngine *audio) {
    audio->device_count = 0;
    int num_devices = Pa_GetDeviceCount();

    if (num_devices < 0) {
        fprintf(stderr, "Error scanning devices: %s\n", Pa_GetErrorText(num_devices));
        return;
    }

    int num_apis = Pa_GetHostApiCount();
    printf("\n=== Audio Devices (%d total, %d host APIs) ===\n", num_devices, num_apis);

    for (int api = 0; api < num_apis; api++) {
        const PaHostApiInfo *api_info = Pa_GetHostApiInfo(api);
        if (!api_info) continue;
        printf("  Host API %d: %s\n", api, api_info->name);
    }
    printf("\n");

    for (int i = 0; i < num_devices && audio->device_count < MAX_DEVICES; i++) {
        const PaDeviceInfo *info = Pa_GetDeviceInfo(i);
        if (!info) continue;

        const PaHostApiInfo *api = Pa_GetHostApiInfo(info->hostApi);
        const char *api_name = api ? api->name : "Unknown";

        bool is_input = info->maxInputChannels > 0;
        bool is_output = info->maxOutputChannels > 0;

        bool is_wasapi = false;
        if (api) {
            is_wasapi = (api->type == paWASAPI);
        }

        bool include = is_input;
        bool is_output_loopback = false;

        if (is_wasapi && is_output && !is_input) {
            include = true;
            is_output_loopback = true;
        }

        if (!include) continue;

        bool is_loopback = is_output_loopback;
        const char *loopback_keywords[] = {
            "loopback", "stereo mix", "what u hear", "monitor",
            "cable output", "voicemeeter", "blackhole", "soundflower",
            "mixage", "mix", NULL
        };

        for (int k = 0; loopback_keywords[k]; k++) {
            const char *haystack = info->name;
            const char *needle = loopback_keywords[k];
            size_t nlen = strlen(needle);

            for (const char *p = haystack; *p; p++) {
                bool match = true;
                for (size_t c = 0; c < nlen && p[c]; c++) {
                    char h = p[c];
                    char n = needle[c];
                    if (h >= 'A' && h <= 'Z') h += 32;
                    if (n >= 'A' && n <= 'Z') n += 32;
                    if (h != n) { match = false; break; }
                }
                if (match) { is_loopback = true; break; }
            }
            if (is_loopback) break;
        }

        AudioDeviceInfo *dev = &audio->devices[audio->device_count];
        dev->index = i;
        snprintf(dev->name, MAX_DEVICE_NAME, "%s [%s]", info->name, api_name);
        dev->channels = is_output_loopback ? info->maxOutputChannels : info->maxInputChannels;
        dev->sample_rate = (int)info->defaultSampleRate;
        dev->is_loopback = is_loopback;
        dev->is_input = is_input;

        const char *tag = is_loopback ? "LOOPBACK" : "INPUT";
        printf("  [%3d] [%-8s] %s (%dch %dHz)\n",
               i, tag, dev->name, dev->channels, dev->sample_rate);

        audio->device_count++;
    }

    printf("\nFound %d usable audio device(s)\n\n", audio->device_count);
}

bool audio_start(AudioEngine *audio, int device_index, int sample_rate, int chunk_size) {
    audio_stop(audio);

    audio->sample_rate = sample_rate;
    audio->chunk_size = chunk_size;
    audio->device_index = device_index;
    audio->write_pos = 0;
    audio->chunk_ready = false;
    audio->silent_frames = 0;
    audio->had_signal = false;
    audio->overflow_count = 0;
    audio->underflow_count = 0;
    audio->recovery_attempts = 0;

    /* Clear the buffer */
    memset(audio->buffer, 0, sizeof(float) * audio->buffer_size);

    free(audio->current_chunk);
    audio->current_chunk = (float *)calloc(chunk_size, sizeof(float));

    PaDeviceIndex pa_device;
    if (device_index < 0) {
        pa_device = Pa_GetDefaultInputDevice();
        if (pa_device == paNoDevice) {
            fprintf(stderr, "No default input device\n");
            return false;
        }
    } else {
        pa_device = device_index;
    }

    const PaDeviceInfo *dev_info = Pa_GetDeviceInfo(pa_device);
    if (!dev_info) {
        fprintf(stderr, "Invalid device index %d\n", device_index);
        return false;
    }

    audio->channels = dev_info->maxInputChannels;
    if (audio->channels <= 0) {
        audio->channels = dev_info->maxOutputChannels;
    }
    if (audio->channels <= 0) {
        fprintf(stderr, "Device has no channels\n");
        return false;
    }
    if (audio->channels > 2) audio->channels = 2;

    if (dev_info->defaultSampleRate > 0) {
        audio->sample_rate = (int)dev_info->defaultSampleRate;
    }

    PaStreamParameters input_params;
    memset(&input_params, 0, sizeof(input_params));
    input_params.device = pa_device;
    input_params.channelCount = audio->channels;
    input_params.sampleFormat = paFloat32;
    /* Use higher latency for stability with virtual devices */
    input_params.suggestedLatency = dev_info->defaultHighInputLatency;

    const PaHostApiInfo *api_info = Pa_GetHostApiInfo(dev_info->hostApi);
    (void)api_info;

    PaError err = Pa_OpenStream(
        &audio->stream,
        &input_params,
        NULL,
        audio->sample_rate,
        chunk_size,
        paClipOff | paDitherOff,  /* Don't clip, don't dither — we handle it */
        audio_callback,
        audio
    );

    if (err != paNoError) {
        fprintf(stderr, "Failed to open stream: %s\n", Pa_GetErrorText(err));

        if (device_index >= 0) {
            fprintf(stderr, "Falling back to default device...\n");
            return audio_start(audio, -1, sample_rate, chunk_size);
        }
        return false;
    }

    err = Pa_StartStream(audio->stream);
    if (err != paNoError) {
        fprintf(stderr, "Failed to start stream: %s\n", Pa_GetErrorText(err));
        Pa_CloseStream(audio->stream);
        audio->stream = NULL;
        return false;
    }

    audio->running = true;
    printf("Audio started: %s @ %dHz, %dch (latency: %.1fms)\n",
           dev_info->name, audio->sample_rate, audio->channels,
           input_params.suggestedLatency * 1000.0);

    return true;
}

void audio_stop(AudioEngine *audio) {
    if (audio->stream) {
        Pa_StopStream(audio->stream);
        Pa_CloseStream(audio->stream);
        audio->stream = NULL;
    }
    audio->running = false;
}

bool audio_restart_device(AudioEngine *audio, int device_index) {
    printf("Switching audio device to index: %d\n", device_index);
    return audio_start(audio, device_index, audio->sample_rate, audio->chunk_size);
}

void audio_get_chunk(AudioEngine *audio, float *out, int size) {
    int read_pos = audio->write_pos - size;
    if (read_pos < 0) read_pos += audio->buffer_size;

    for (int i = 0; i < size; i++) {
        int idx = (read_pos + i) % audio->buffer_size;
        out[i] = audio->buffer[idx];
    }
}

void audio_compute_levels(AudioEngine *audio) {
    float rms = 0.0f;
    float peak = 0.0f;

    for (int i = 0; i < audio->chunk_size; i++) {
        float s = audio->current_chunk[i];
        rms += s * s;
        float abs_s = fabsf(s);
        if (abs_s > peak) peak = abs_s;
    }

    audio->rms_level = sqrtf(rms / audio->chunk_size);
    audio->peak_level = peak;
}

bool audio_check_health(AudioEngine *audio) {
    /*
     * Check if the audio stream is still healthy.
     * Returns true if everything is fine.
     * Returns false if we need to restart the stream.
     *
     * We only restart if:
     *   1. The stream was previously receiving audio (had_signal)
     *   2. It's been silent for a VERY long time (30+ seconds)
     *   3. The stream itself reports it's not active
     *
     * We do NOT restart on normal pause/silence because
     * Voicemeeter keeps the stream alive even during silence.
     */

    if (!audio->stream || !audio->running) {
        return false;
    }

    /* Check if PortAudio says the stream is still active */
    PaError active = Pa_IsStreamActive(audio->stream);
    if (active == 0) {
        /* Stream stopped unexpectedly */
        printf("Audio stream stopped unexpectedly, restarting...\n");
        audio->recovery_attempts++;

        if (audio->recovery_attempts > 5) {
            printf("Too many recovery attempts, giving up\n");
            return false;
        }

        /* Restart on same device */
        audio_start(audio, audio->device_index,
                    audio->sample_rate, audio->chunk_size);
        return audio->running;
    }

    if (active < 0) {
        /* Error checking stream status */
        fprintf(stderr, "Stream health check error: %s\n",
                Pa_GetErrorText(active));
    }

    /* Stream is active — everything is fine */
    /* Reset recovery counter when we have signal */
    if (audio->had_signal && audio->silent_frames == 0) {
        audio->recovery_attempts = 0;
    }

    return true;
}

float audio_get_silence_duration(AudioEngine *audio) {
    if (audio->sample_rate <= 0) return 0.0f;
    return (float)audio->silent_frames / (float)audio->sample_rate;
}