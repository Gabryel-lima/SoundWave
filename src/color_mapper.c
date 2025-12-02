#include "color_mapper.h"
#include <math.h>
#include <stdint.h>

// Mapeia frequência para cor usando escala logarítmica
// Baixas frequências (20-200 Hz) → Vermelho/Laranja
// Médias frequências (200-2000 Hz) → Amarelo/Verde
// Altas frequências (2000-20000 Hz) → Azul/Roxo
RGBColor color_mapper_frequency_to_rgb(double frequency) {
    if (frequency < 20.0) frequency = 20.0;
    if (frequency > 20000.0) frequency = 20000.0;
    
    // Usa escala logarítmica para melhor distribuição
    double log_freq = log10(frequency);
    double log_min = log10(20.0);
    double log_max = log10(20000.0);
    
    // Normaliza para 0-1
    double normalized = (log_freq - log_min) / (log_max - log_min);
    
    // Mapeia para HSV onde H varia de 0 (vermelho) a 240 (azul)
    double h = normalized * 240.0;  // 0 = vermelho, 120 = verde, 240 = azul
    double s = 0.8;  // Saturação alta
    double v = 0.9;  // Brilho alto
    
    return color_mapper_hsv_to_rgb(h, s, v);
}

RGBColor color_mapper_bands_to_rgb(double low_energy, double mid_energy, double high_energy) {
    // Normaliza as energias
    double total = low_energy + mid_energy + high_energy;
    if (total < 0.001) {
        // Sem energia, retorna preto
        RGBColor black = {0, 0, 0};
        return black;
    }
    
    double low_norm = low_energy / total;
    double mid_norm = mid_energy / total;
    double high_norm = high_energy / total;
    
    // Calcula cor ponderada
    RGBColor low_color = color_mapper_hsv_to_rgb(0, 0.8, 0.9);    // Vermelho
    RGBColor mid_color = color_mapper_hsv_to_rgb(120, 0.8, 0.9);  // Verde
    RGBColor high_color = color_mapper_hsv_to_rgb(240, 0.8, 0.9); // Azul
    
    RGBColor result;
    result.r = (uint8_t)(low_color.r * low_norm + mid_color.r * mid_norm + high_color.r * high_norm);
    result.g = (uint8_t)(low_color.g * low_norm + mid_color.g * mid_norm + high_color.g * high_norm);
    result.b = (uint8_t)(low_color.b * low_norm + mid_color.b * mid_norm + high_color.b * high_norm);
    
    return result;
}

RGBColor color_mapper_hsv_to_rgb(double h, double s, double v) {
    RGBColor rgb;
    
    // Garante que h está no intervalo [0, 360)
    while (h < 0) h += 360.0;
    while (h >= 360.0) h -= 360.0;
    
    int i = (int)(h / 60.0) % 6;
    double f = (h / 60.0) - i;
    double p = v * (1.0 - s);
    double q = v * (1.0 - f * s);
    double t = v * (1.0 - (1.0 - f) * s);
    
    double r, g, b;
    
    switch (i) {
        case 0:
            r = v; g = t; b = p;
            break;
        case 1:
            r = q; g = v; b = p;
            break;
        case 2:
            r = p; g = v; b = t;
            break;
        case 3:
            r = p; g = q; b = v;
            break;
        case 4:
            r = t; g = p; b = v;
            break;
        case 5:
        default:
            r = v; g = p; b = q;
            break;
    }
    
    rgb.r = (uint8_t)(r * 255.0);
    rgb.g = (uint8_t)(g * 255.0);
    rgb.b = (uint8_t)(b * 255.0);
    
    return rgb;
}

