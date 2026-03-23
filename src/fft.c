#include "fft.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void fft_init(FFTProcessor *fft, int chunk_size, int sample_rate) {
    fft->chunk_size = chunk_size;
    fft->sample_rate = sample_rate;
    fft->magnitude_count = chunk_size / 2 + 1;

    fft->input_buffer = (float *)fftwf_malloc(sizeof(float) * chunk_size);
    fft->output = (fftwf_complex *)fftwf_malloc(sizeof(fftwf_complex) * fft->magnitude_count);
    fft->plan = fftwf_plan_dft_r2c_1d(chunk_size, fft->input_buffer, fft->output, FFTW_MEASURE);

    fft->magnitudes = (float *)calloc(fft->magnitude_count, sizeof(float));
    fft->bands = (float *)calloc(MAX_BANDS, sizeof(float));
    fft->smoothed_bands = (float *)calloc(MAX_BANDS, sizeof(float));
    fft->band_count = 0;

    /* Pre-compute Hanning window */
    fft->window = (float *)malloc(sizeof(float) * chunk_size);
    for (int i = 0; i < chunk_size; i++) {
        fft->window[i] = 0.5f * (1.0f - cosf(2.0f * (float)M_PI * i / (chunk_size - 1)));
    }
}

void fft_destroy(FFTProcessor *fft) {
    if (fft->plan) fftwf_destroy_plan(fft->plan);
    if (fft->input_buffer) fftwf_free(fft->input_buffer);
    if (fft->output) fftwf_free(fft->output);
    free(fft->magnitudes);
    free(fft->bands);
    free(fft->smoothed_bands);
    free(fft->window);
    memset(fft, 0, sizeof(FFTProcessor));
}

void fft_process(FFTProcessor *fft, const float *audio_data,
                 int num_bands, float smoothing, float sensitivity,
                 int min_freq, int max_freq) {

    if (num_bands > MAX_BANDS) num_bands = MAX_BANDS;
    fft->band_count = num_bands;

    /* Apply window function */
    for (int i = 0; i < fft->chunk_size; i++) {
        fft->input_buffer[i] = audio_data[i] * fft->window[i];
    }

    /* Execute FFT */
    fftwf_execute(fft->plan);

    /* Compute magnitudes */
    float inv_n = 1.0f / (float)fft->chunk_size;
    for (int i = 0; i < fft->magnitude_count; i++) {
        float re = fft->output[i][0];
        float im = fft->output[i][1];
        fft->magnitudes[i] = sqrtf(re * re + im * im) * inv_n;
    }

    /* Create logarithmically spaced frequency bands */
    if (min_freq < 20) min_freq = 20;
    if (max_freq > fft->sample_rate / 2) max_freq = fft->sample_rate / 2;

    float log_min = log10f((float)min_freq);
    float log_max = log10f((float)max_freq);
    float log_step = (log_max - log_min) / (float)num_bands;

    float freq_resolution = (float)fft->sample_rate / (float)fft->chunk_size;

    for (int i = 0; i < num_bands; i++) {
        float low_freq = powf(10.0f, log_min + log_step * i);
        float high_freq = powf(10.0f, log_min + log_step * (i + 1));

        int low_bin = (int)(low_freq / freq_resolution);
        int high_bin = (int)(high_freq / freq_resolution);

        if (low_bin < 0) low_bin = 0;
        if (high_bin >= fft->magnitude_count) high_bin = fft->magnitude_count - 1;
        if (low_bin > high_bin) low_bin = high_bin;

        /* Average magnitude in this band */
        float sum = 0.0f;
        int count = 0;
        for (int j = low_bin; j <= high_bin; j++) {
            sum += fft->magnitudes[j];
            count++;
        }

        fft->bands[i] = (count > 0) ? (sum / count) : 0.0f;
        fft->bands[i] *= sensitivity * 50.0f;
    }

    /* Smoothing */
    float alpha = 1.0f - smoothing;
    for (int i = 0; i < num_bands; i++) {
        fft->smoothed_bands[i] = alpha * fft->bands[i] +
                                  (1.0f - alpha) * fft->smoothed_bands[i];

        /* Clamp */
        if (fft->smoothed_bands[i] < 0.0f) fft->smoothed_bands[i] = 0.0f;
        if (fft->smoothed_bands[i] > 1.0f) fft->smoothed_bands[i] = 1.0f;
    }
}