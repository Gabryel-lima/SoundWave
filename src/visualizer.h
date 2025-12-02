#ifndef VISUALIZER_H
#define VISUALIZER_H

#include <stdint.h>
#include <stdbool.h>
#include "color_mapper.h"

typedef struct Visualizer Visualizer;

// Inicializa o visualizador
// width: largura da janela
// height: altura da janela
// title: título da janela
Visualizer* visualizer_init(int width, int height, const char* title);

// Libera recursos do visualizador
void visualizer_free(Visualizer* vis);

// Desenha a forma de onda com cores baseadas nas frequências
// samples: array de samples de áudio
// num_samples: número de samples
// frequencies: array de frequências por região (pode ser NULL para cor única)
// colors: array de cores correspondentes aos samples (pode ser NULL)
void visualizer_draw_waveform(Visualizer* vis, const int16_t* samples, int num_samples,
                               RGBColor* colors);

// Desenha forma de onda com scroll contínuo (mantém histórico)
// samples: novos samples a adicionar
// num_samples: número de samples
// colors: cores correspondentes
void visualizer_draw_waveform_scroll(Visualizer* vis, const int16_t* samples, int num_samples,
                                      RGBColor* colors);

// Atualiza a tela (chama SDL_RenderPresent)
void visualizer_present(Visualizer* vis);

// Limpa a tela com cor de fundo
void visualizer_clear(Visualizer* vis);

// Verifica se a janela deve ser fechada
bool visualizer_should_close(Visualizer* vis);

// Retorna a largura da janela
int visualizer_get_width(Visualizer* vis);

// Retorna a altura da janela
int visualizer_get_height(Visualizer* vis);

// Desenha forma de onda com scroll contínuo (mantém histórico)
// samples: novos samples a adicionar
// num_samples: número de samples
// colors: cores correspondentes
void visualizer_draw_waveform_scroll(Visualizer* vis, const int16_t* samples, int num_samples,
                                      RGBColor* colors);

// Desenha barras de frequência animadas
// frequencies: array de magnitudes de frequência do FFT
// num_bins: número de bins de frequência
// num_bars: número de barras a desenhar
void visualizer_draw_frequency_bars(Visualizer* vis, const double* frequencies, int num_bins, int num_bars);

// Desenha waveform fluida/ambient com efeitos
// samples: array de samples
// num_samples: número de samples
// frequencies: array de frequências para cores
// colors: cores correspondentes
void visualizer_draw_fluid_waveform(Visualizer* vis, const int16_t* samples, int num_samples,
                                     const double* frequencies, RGBColor* colors);

// Atualiza e desenha sistema de partículas
// frequencies: array de frequências para gerar partículas
// num_bins: número de bins
void visualizer_update_particles(Visualizer* vis, const double* frequencies, int num_bins);

#endif // VISUALIZER_H

