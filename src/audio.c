#include "audio.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

static int audio_callback(const void *input, void *output,
                           unsigned long frame_count,
                           const PaStreamCallbackTimeInfo *time_info,
                           PaStreamCallbackFlags status_flags,
                           void *user_data) {
    (void)output;
    (void)time_info;
    (void)status_flags;

    AudioEngine *audio = (AudioEngine *)user_data;
    const float *in = (const float *)input;

    if (!in) return paContinue;

    for (unsigned long i = 0; i < frame_count; i++) {
        float sample;
        if (audio->channels >= 2) {
            /* Mix stereo to mono */
            sample = (in[i * audio->channels] + in[i * audio->channels + 1]) * 0.5f;
        } else {
            sample = in[i * audio->channels];
        }

        audio->buffer[audio->write_pos] = sample;
        audio->write_pos = (audio->write_pos + 1) % audio->buffer_size;
    }

    audio->chunk_ready = true;
    return paContinue;
}

bool audio_init(AudioEngine *audio) {
    memset(audio, 0, sizeof(AudioEngine));

    PaError err = Pa_Initialize();
    if (err != paNoError) {
        fprintf(stderr, "PortAudio init failed: %s\n", Pa_GetErrorText(err));
        return false;
    }

    audio->buffer_size = 44100 * 2; /* 2 seconds buffer */
    audio->buffer = (float *)calloc(audio->buffer_size, sizeof(float));
    audio->current_chunk = NULL;

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
        printf("\n--- %s ---\n", api_info->name);
    }
    printf("\n");

    for (int i = 0; i < num_devices && audio->device_count < MAX_DEVICES; i++) {
        const PaDeviceInfo *info = Pa_GetDeviceInfo(i);
        if (!info) continue;

        const PaHostApiInfo *api = Pa_GetHostApiInfo(info->hostApi);
        const char *api_name = api ? api->name : "Unknown";

        bool is_input = info->maxInputChannels > 0;
        bool is_output = info->maxOutputChannels > 0;

        /* Include all input devices */
        /* Include output devices from WASAPI (can be used as loopback) */
        bool is_wasapi = false;
        if (api) {
            is_wasapi = (api->type == paWASAPI);
        }

        bool include = is_input;
        bool is_output_loopback = false;

        /* Include WASAPI output devices as loopback candidates */
        if (is_wasapi && is_output && !is_input) {
            include = true;
            is_output_loopback = true;
        }

        if (!include) continue;

        /* Detect loopback by name */
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

        /* Include host API in name for clarity */
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

    free(audio->current_chunk);
    audio->current_chunk = (float *)calloc(chunk_size, sizeof(float));

    /* Determine device and channels */
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
        /* WASAPI output device - try as loopback with output channels */
        audio->channels = dev_info->maxOutputChannels;
    }
    if (audio->channels <= 0) {
        fprintf(stderr, "Device has no channels\n");
        return false;
    }
    if (audio->channels > 2) audio->channels = 2;

    /* Use device native sample rate if available */
    if (dev_info->defaultSampleRate > 0) {
        audio->sample_rate = (int)dev_info->defaultSampleRate;
    }

    PaStreamParameters input_params;
    memset(&input_params, 0, sizeof(input_params));
    input_params.device = pa_device;
    input_params.channelCount = audio->channels;
    input_params.sampleFormat = paFloat32;
    input_params.suggestedLatency = dev_info->defaultLowInputLatency;

    /* Check for WASAPI loopback */
    const PaHostApiInfo *api_info = Pa_GetHostApiInfo(dev_info->hostApi);

#ifdef PA_USE_WASAPI
    PaWasapiStreamInfo wasapi_info;
    if (api_info && api_info->type == paWASAPI && dev_info->maxInputChannels == 0) {
        memset(&wasapi_info, 0, sizeof(wasapi_info));
        wasapi_info.size = sizeof(PaWasapiStreamInfo);
        wasapi_info.hostApiType = paWASAPI;
        wasapi_info.version = 1;
        wasapi_info.flags = paWinWasapiAutoConvert | paWinWasapiUseChannelMask;
        wasapi_info.flags |= 0x80000000; /* paWinWasapiLoopback - undocumented */
        input_params.hostApiSpecificStreamInfo = &wasapi_info;
        printf("Using WASAPI loopback mode\n");
    }
#else
    (void)api_info;
#endif

    PaError err = Pa_OpenStream(
        &audio->stream,
        &input_params,
        NULL,                    /* no output */
        audio->sample_rate,
        chunk_size,
        paClipOff,
        audio_callback,
        audio
    );

    if (err != paNoError) {
        fprintf(stderr, "Failed to open stream: %s\n", Pa_GetErrorText(err));

        /* Fallback to default device */
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
    printf("Audio started: %s @ %dHz, %dch\n",
           dev_info->name, audio->sample_rate, audio->channels);

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
    return audio_start(audio, device_index, audio->sample_rate, audio->chunk_size);
}

void audio_get_chunk(AudioEngine *audio, float *out, int size) {
    /* Copy the latest chunk_size samples from the circular buffer */
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