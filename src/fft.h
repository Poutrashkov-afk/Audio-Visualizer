#ifndef FFT_H
#define FFT_H

#include <fftw3.h>

#define MAX_BANDS 512

typedef struct {
    int chunk_size;
    int sample_rate;

    float *input_buffer;     /* windowed audio samples */
    fftwf_complex *output;   /* FFT output */
    fftwf_plan plan;

    float *magnitudes;       /* raw FFT magnitudes */
    int magnitude_count;

    float *bands;            /* processed frequency bands */
    float *smoothed_bands;   /* smoothed output */
    int band_count;

    float *window;           /* Hanning window */
} FFTProcessor;

void fft_init(FFTProcessor *fft, int chunk_size, int sample_rate);
void fft_destroy(FFTProcessor *fft);
void fft_process(FFTProcessor *fft, const float *audio_data,
                 int num_bands, float smoothing, float sensitivity,
                 int min_freq, int max_freq);

#endif