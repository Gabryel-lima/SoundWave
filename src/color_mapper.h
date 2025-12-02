#ifndef COLOR_MAPPER_H
#define COLOR_MAPPER_H

#include <stdint.h>

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} RGBColor;

// Mapeia uma frequência para uma cor RGB
// frequency: frequência em Hz
// Retorna: cor RGB correspondente
RGBColor color_mapper_frequency_to_rgb(double frequency);

// Mapeia múltiplas frequências (bandas) para uma cor RGB
// low_freq: frequência baixa (Hz)
// mid_freq: frequência média (Hz)
// high_freq: frequência alta (Hz)
// Retorna: cor RGB baseada nas proporções das bandas
RGBColor color_mapper_bands_to_rgb(double low_freq, double mid_freq, double high_freq);

// Converte HSV para RGB
RGBColor color_mapper_hsv_to_rgb(double h, double s, double v);

#endif // COLOR_MAPPER_H

