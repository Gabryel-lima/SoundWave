#include "fft_analyzer.h"
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <fftw3.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

struct FFTAnalyzer {
    int sample_rate;
    int window_size;
    fftw_plan plan;
    double* input;
    fftw_complex* output;
    double* window;  // Janela de Hanning para reduzir aliasing
};

FFTAnalyzer* fft_analyzer_init(int sample_rate, int window_size) {
    FFTAnalyzer* analyzer = malloc(sizeof(FFTAnalyzer));
    if (!analyzer) {
        return NULL;
    }
    
    analyzer->sample_rate = sample_rate;
    analyzer->window_size = window_size;
    
    // Aloca buffers para FFT
    analyzer->input = fftw_alloc_real(window_size);
    analyzer->output = fftw_alloc_complex(window_size / 2 + 1);
    
    if (!analyzer->input || !analyzer->output) {
        if (analyzer->input) fftw_free(analyzer->input);
        if (analyzer->output) fftw_free(analyzer->output);
        free(analyzer);
        return NULL;
    }
    
    // Cria plano FFT
    analyzer->plan = fftw_plan_dft_r2c_1d(window_size, analyzer->input, analyzer->output, FFTW_ESTIMATE);
    
    if (!analyzer->plan) {
        fftw_free(analyzer->input);
        fftw_free(analyzer->output);
        free(analyzer);
        return NULL;
    }
    
    // Precalcula janela de Hanning
    analyzer->window = malloc(window_size * sizeof(double));
    if (!analyzer->window) {
        fftw_destroy_plan(analyzer->plan);
        fftw_free(analyzer->input);
        fftw_free(analyzer->output);
        free(analyzer);
        return NULL;
    }
    
    for (int i = 0; i < window_size; i++) {
        analyzer->window[i] = 0.5 * (1.0 - cos(2.0 * M_PI * i / (window_size - 1)));
    }
    
    return analyzer;
}

void fft_analyzer_free(FFTAnalyzer* analyzer) {
    if (!analyzer) return;
    
    if (analyzer->plan) {
        fftw_destroy_plan(analyzer->plan);
    }
    if (analyzer->window) {
        free(analyzer->window);
    }
    if (analyzer->input) {
        fftw_free(analyzer->input);
    }
    if (analyzer->output) {
        fftw_free(analyzer->output);
    }
    
    free(analyzer);
}

double fft_analyzer_analyze(FFTAnalyzer* analyzer, const int16_t* samples, double* frequencies) {
    if (!analyzer || !samples || !frequencies) {
        return 0.0;
    }
    
    // Aplica janela e normaliza
    for (int i = 0; i < analyzer->window_size; i++) {
        analyzer->input[i] = (double)samples[i] * analyzer->window[i] / 32768.0;
    }
    
    // Executa FFT
    fftw_execute(analyzer->plan);
    
    // Calcula magnitudes e encontra frequência dominante
    double max_magnitude = 0.0;
    int max_bin = 0;
    double max_frequency = 0.0;
    
    for (int i = 0; i <= analyzer->window_size / 2; i++) {
        double real = analyzer->output[i][0];
        double imag = analyzer->output[i][1];
        double magnitude = sqrt(real * real + imag * imag);
        
        // Normaliza pela metade do tamanho da janela (exceto DC e Nyquist)
        if (i > 0 && i < analyzer->window_size / 2) {
            magnitude *= 2.0;
        }
        
        frequencies[i] = magnitude;
        
        // Encontra frequência dominante (ignora DC e frequências muito baixas)
        if (i > 1 && magnitude > max_magnitude) {
            max_magnitude = magnitude;
            max_bin = i;
        }
    }
    
    // Calcula frequência correspondente ao bin dominante
    if (max_bin > 0) {
        max_frequency = (double)max_bin * analyzer->sample_rate / analyzer->window_size;
    }
    
    return max_frequency;
}

int fft_analyzer_get_window_size(FFTAnalyzer* analyzer) {
    if (!analyzer) return 0;
    return analyzer->window_size;
}

double fft_analyzer_bin_to_frequency(FFTAnalyzer* analyzer, int bin_index) {
    if (!analyzer || bin_index < 0) return 0.0;
    return (double)bin_index * analyzer->sample_rate / analyzer->window_size;
}

double fft_analyzer_get_band_energy(FFTAnalyzer* analyzer, const double* frequencies, 
                                     double low_freq, double high_freq) {
    if (!analyzer || !frequencies) return 0.0;
    
    int low_bin = (int)(low_freq * analyzer->window_size / analyzer->sample_rate);
    int high_bin = (int)(high_freq * analyzer->window_size / analyzer->sample_rate);
    
    if (low_bin < 0) low_bin = 0;
    if (high_bin > analyzer->window_size / 2) high_bin = analyzer->window_size / 2;
    
    double energy = 0.0;
    for (int i = low_bin; i <= high_bin; i++) {
        energy += frequencies[i] * frequencies[i];
    }
    
    return sqrt(energy);
}

