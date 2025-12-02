#ifndef FFT_ANALYZER_H
#define FFT_ANALYZER_H

#include <stdint.h>

typedef struct FFTAnalyzer FFTAnalyzer;

// Inicializa o analisador FFT
// sample_rate: taxa de amostragem do áudio
// window_size: tamanho da janela FFT (ex: 2048)
FFTAnalyzer* fft_analyzer_init(int sample_rate, int window_size);

// Libera recursos do analisador
void fft_analyzer_free(FFTAnalyzer* analyzer);

// Analisa uma janela de samples de áudio
// samples: array de samples de áudio (tamanho = window_size)
// frequencies: array de saída com magnitudes por frequência (tamanho = window_size/2 + 1)
// Retorna: frequência dominante em Hz
double fft_analyzer_analyze(FFTAnalyzer* analyzer, const int16_t* samples, double* frequencies);

// Retorna o tamanho da janela
int fft_analyzer_get_window_size(FFTAnalyzer* analyzer);

// Retorna a frequência correspondente a um índice de bin FFT
double fft_analyzer_bin_to_frequency(FFTAnalyzer* analyzer, int bin_index);

// Calcula a energia em uma banda de frequência
double fft_analyzer_get_band_energy(FFTAnalyzer* analyzer, const double* frequencies, 
                                     double low_freq, double high_freq);

#endif // FFT_ANALYZER_H

