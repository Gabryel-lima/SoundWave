#include "visualizer.h"
#include <stdlib.h>
#include <stdio.h>
#include <SDL2/SDL.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>

typedef struct {
    double x, y;
    double vx, vy;
    RGBColor color;
    double life;
    double max_life;
    double size;
} Particle;

struct Visualizer {
    SDL_Window* window;
    SDL_Renderer* renderer;
    int width;
    int height;
    bool should_close;
    
    // Buffer para scroll contínuo
    int16_t* waveform_buffer;
    RGBColor* color_buffer;
    int buffer_size;
    int buffer_pos;
    bool use_scroll;
    
    // Sistema de barras de frequência
    double* bar_heights;
    int max_bars;
    
    // Sistema de partículas
    Particle* particles;
    int max_particles;
    int num_particles;
    
    // Histórico de waveform para efeito fluido
    double* waveform_smooth;
    int smooth_buffer_size;
};

Visualizer* visualizer_init(int width, int height, const char* title) {
    Visualizer* vis = malloc(sizeof(Visualizer));
    if (!vis) {
        return NULL;
    }
    
    vis->width = width;
    vis->height = height;
    vis->should_close = false;
    vis->window = NULL;
    vis->renderer = NULL;
    vis->use_scroll = false;
    vis->waveform_buffer = NULL;
    vis->color_buffer = NULL;
    vis->buffer_size = 0;
    vis->buffer_pos = 0;
    
    // Inicializa SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "Erro ao inicializar SDL: %s\n", SDL_GetError());
        free(vis);
        return NULL;
    }
    
    // Cria janela
    vis->window = SDL_CreateWindow(
        title,
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        width,
        height,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );
    
    if (!vis->window) {
        fprintf(stderr, "Erro ao criar janela: %s\n", SDL_GetError());
        SDL_Quit();
        free(vis);
        return NULL;
    }
    
    // Cria renderer
    vis->renderer = SDL_CreateRenderer(vis->window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    
    if (!vis->renderer) {
        fprintf(stderr, "Erro ao criar renderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(vis->window);
        SDL_Quit();
        free(vis);
        return NULL;
    }
    
    // Aloca buffer para scroll contínuo (mantém uma janela de samples)
    vis->buffer_size = width * 2;  // Mantém 2x a largura da janela em samples
    vis->waveform_buffer = malloc(vis->buffer_size * sizeof(int16_t));
    vis->color_buffer = malloc(vis->buffer_size * sizeof(RGBColor));
    
    // Aloca buffer para barras de frequência
    vis->max_bars = 64;
    vis->bar_heights = malloc(vis->max_bars * sizeof(double));
    
    // Aloca sistema de partículas (aumentado)
    vis->max_particles = 500;  // Aumentado de 200 para 500
    vis->particles = malloc(vis->max_particles * sizeof(Particle));
    vis->num_particles = 0;
    
    // Aloca buffer para waveform suavizada
    vis->smooth_buffer_size = width;
    vis->waveform_smooth = malloc(vis->smooth_buffer_size * sizeof(double));
    
    if (!vis->waveform_buffer || !vis->color_buffer || !vis->bar_heights || 
        !vis->particles || !vis->waveform_smooth) {
        if (vis->waveform_smooth) free(vis->waveform_smooth);
        if (vis->particles) free(vis->particles);
        if (vis->bar_heights) free(vis->bar_heights);
        if (vis->color_buffer) free(vis->color_buffer);
        if (vis->waveform_buffer) free(vis->waveform_buffer);
        SDL_DestroyRenderer(vis->renderer);
        SDL_DestroyWindow(vis->window);
        SDL_Quit();
        free(vis);
        return NULL;
    }
    
    // Inicializa buffers com zeros
    memset(vis->waveform_buffer, 0, vis->buffer_size * sizeof(int16_t));
    memset(vis->color_buffer, 0, vis->buffer_size * sizeof(RGBColor));
    memset(vis->bar_heights, 0, vis->max_bars * sizeof(double));
    memset(vis->waveform_smooth, 0, vis->smooth_buffer_size * sizeof(double));
    vis->buffer_pos = 0;
    vis->use_scroll = true;  // Ativa modo scroll por padrão
    
    return vis;
}

void visualizer_free(Visualizer* vis) {
    if (!vis) return;
    
    if (vis->waveform_smooth) {
        free(vis->waveform_smooth);
    }
    if (vis->particles) {
        free(vis->particles);
    }
    if (vis->bar_heights) {
        free(vis->bar_heights);
    }
    if (vis->color_buffer) {
        free(vis->color_buffer);
    }
    if (vis->waveform_buffer) {
        free(vis->waveform_buffer);
    }
    if (vis->renderer) {
        SDL_DestroyRenderer(vis->renderer);
    }
    if (vis->window) {
        SDL_DestroyWindow(vis->window);
    }
    
    SDL_Quit();
    free(vis);
}

void visualizer_draw_waveform(Visualizer* vis, const int16_t* samples, int num_samples,
                               RGBColor* colors) {
    if (!vis || !samples || num_samples <= 0) {
        return;
    }
    
    // Calcula escala para desenhar a forma de onda
    double x_scale = (double)vis->width / num_samples;
    double y_scale = (double)vis->height / 2.0 / 32768.0;
    int center_y = vis->height / 2;
    
    // Desenha a forma de onda como uma linha colorida
    for (int i = 0; i < num_samples - 1; i++) {
        int x1 = (int)(i * x_scale);
        int x2 = (int)((i + 1) * x_scale);
        
        // Calcula posições Y com validação de limites
        int y1 = center_y - (int)(samples[i] * y_scale);
        int y2 = center_y - (int)(samples[i + 1] * y_scale);
        
        // Garante que Y está dentro dos limites da janela
        if (y1 < 0) y1 = 0;
        if (y1 >= vis->height) y1 = vis->height - 1;
        if (y2 < 0) y2 = 0;
        if (y2 >= vis->height) y2 = vis->height - 1;
        
        // Escolhe cor
        RGBColor color;
        if (colors && i < num_samples) {
            color = colors[i];
        } else {
            // Cor padrão branca se não houver cores
            color.r = 255;
            color.g = 255;
            color.b = 255;
        }
        
        // Desenha linha com a cor especificada
        SDL_SetRenderDrawColor(vis->renderer, color.r, color.g, color.b, 255);
        SDL_RenderDrawLine(vis->renderer, x1, y1, x2, y2);
    }
}

void visualizer_draw_waveform_scroll(Visualizer* vis, const int16_t* samples, int num_samples,
                                      RGBColor* colors) {
    if (!vis || !samples || num_samples <= 0) {
        return;
    }
    
    // Adiciona novos samples ao buffer circular
    for (int i = 0; i < num_samples; i++) {
        vis->waveform_buffer[vis->buffer_pos] = samples[i];
        if (colors && i < num_samples) {
            vis->color_buffer[vis->buffer_pos] = colors[i];
        } else {
            vis->color_buffer[vis->buffer_pos] = (RGBColor){255, 255, 255};
        }
        vis->buffer_pos = (vis->buffer_pos + 1) % vis->buffer_size;
    }
    
    // Calcula quantos samples visíveis temos
    int visible_samples = (vis->buffer_pos < vis->width) ? vis->buffer_pos : vis->buffer_size;
    if (visible_samples == 0) visible_samples = vis->buffer_size;
    
    // Calcula escala
    double x_scale = (double)vis->width / visible_samples;
    double y_scale = (double)vis->height / 2.0 / 32768.0;
    int center_y = vis->height / 2;
    
    // Desenha a forma de onda do buffer
    int start_idx = (vis->buffer_pos - visible_samples + vis->buffer_size) % vis->buffer_size;
    
    for (int i = 0; i < visible_samples - 1; i++) {
        int idx1 = (start_idx + i) % vis->buffer_size;
        int idx2 = (start_idx + i + 1) % vis->buffer_size;
        
        int x1 = (int)(i * x_scale);
        int x2 = (int)((i + 1) * x_scale);
        
        // Calcula posições Y com validação
        int y1 = center_y - (int)(vis->waveform_buffer[idx1] * y_scale);
        int y2 = center_y - (int)(vis->waveform_buffer[idx2] * y_scale);
        
        // Garante que Y está dentro dos limites
        if (y1 < 0) y1 = 0;
        if (y1 >= vis->height) y1 = vis->height - 1;
        if (y2 < 0) y2 = 0;
        if (y2 >= vis->height) y2 = vis->height - 1;
        
        // Desenha linha com cor do buffer
        RGBColor color = vis->color_buffer[idx1];
        SDL_SetRenderDrawColor(vis->renderer, color.r, color.g, color.b, 255);
        SDL_RenderDrawLine(vis->renderer, x1, y1, x2, y2);
    }
}

void visualizer_present(Visualizer* vis) {
    if (!vis || !vis->renderer) return;
    
    SDL_RenderPresent(vis->renderer);
}

void visualizer_clear(Visualizer* vis) {
    if (!vis || !vis->renderer) return;
    
    // Limpa com cor preta
    SDL_SetRenderDrawColor(vis->renderer, 0, 0, 0, 255);
    SDL_RenderClear(vis->renderer);
}

bool visualizer_should_close(Visualizer* vis) {
    if (!vis) return true;
    
    // Processa eventos SDL
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            vis->should_close = true;
        }
        if (event.type == SDL_KEYDOWN) {
            if (event.key.keysym.sym == SDLK_ESCAPE || event.key.keysym.sym == SDLK_q) {
                vis->should_close = true;
            }
        }
    }
    
    return vis->should_close;
}

int visualizer_get_width(Visualizer* vis) {
    if (!vis) return 0;
    return vis->width;
}

int visualizer_get_height(Visualizer* vis) {
    if (!vis) return 0;
    return vis->height;
}

void visualizer_draw_frequency_bars(Visualizer* vis, const double* frequencies, int num_bins, int num_bars) {
    if (!vis || !frequencies || num_bins <= 0 || num_bars <= 0) return;
    if (num_bars > vis->max_bars) num_bars = vis->max_bars;
    
    // Centraliza as barras no meio da tela
    int total_bar_width = (int)(vis->width * 0.8);  // Usa 80% da largura
    int bar_start_x = (vis->width - total_bar_width) / 2;  // Offset para centralizar
    
    int bar_width = total_bar_width / num_bars;
    int bar_spacing = 1;
    int actual_width = bar_width - bar_spacing;
    if (actual_width < 1) actual_width = 1;
    
    int bins_per_bar = num_bins / num_bars;
    if (bins_per_bar < 1) bins_per_bar = 1;
    
    const double decay = 0.90;
    const double rise_speed = 0.3;
    
    for (int i = 0; i < num_bars; i++) {
        double energy = 0.0;
        int start = i * bins_per_bar;
        int end = start + bins_per_bar;
        if (end > num_bins) end = num_bins;
        
        for (int j = start; j < end; j++) {
            energy += frequencies[j] * frequencies[j];
        }
        energy = sqrt(energy / bins_per_bar) / 50.0;
        if (energy > 1.0) energy = 1.0;
        
        if (energy > vis->bar_heights[i]) {
            vis->bar_heights[i] += (energy - vis->bar_heights[i]) * rise_speed;
        } else {
            vis->bar_heights[i] *= decay;
        }
        
        int height = (int)(vis->bar_heights[i] * vis->height * 0.85);
        if (height < 1) height = 1;
        
        // Centraliza horizontalmente
        int x = bar_start_x + i * bar_width + bar_spacing / 2;
        int center_y = vis->height / 2;
        int y = center_y - height / 2;
        
        int center_bin = (start + end) / 2;
        double freq = (double)center_bin * 44100.0 / 2048.0;
        RGBColor color = color_mapper_frequency_to_rgb(freq);
        
        // Adiciona mais variação de cores - mistura com cores complementares
        double hue_shift = (double)i / num_bars * 60.0;  // Variação de 60 graus no HSV
        RGBColor shifted_color = color_mapper_hsv_to_rgb(
            (freq < 200 ? 0 : (freq < 2000 ? 120 : 240)) + hue_shift,
            0.9,
            0.9
        );
        
        // Mistura cores
        color.r = (uint8_t)(color.r * 0.6 + shifted_color.r * 0.4);
        color.g = (uint8_t)(color.g * 0.6 + shifted_color.g * 0.4);
        color.b = (uint8_t)(color.b * 0.6 + shifted_color.b * 0.4);
        
        double brightness = vis->bar_heights[i];
        color.r = (uint8_t)(color.r * brightness);
        color.g = (uint8_t)(color.g * brightness);
        color.b = (uint8_t)(color.b * brightness);
        
        SDL_Rect rect = {x, y, actual_width, height};
        SDL_SetRenderDrawColor(vis->renderer, color.r, color.g, color.b, 255);
        SDL_RenderFillRect(vis->renderer, &rect);
    }
}

void visualizer_draw_fluid_waveform(Visualizer* vis, const int16_t* samples, int num_samples,
                                     const double* frequencies, RGBColor* colors) {
    if (!vis || !samples || num_samples <= 0) return;
    (void)frequencies;  // Parâmetro não usado ainda, mas pode ser usado no futuro
    
    double x_scale = (double)vis->width / num_samples;
    int center_y = vis->height / 2;
    
    // Suaviza a waveform usando média móvel
    for (int i = 0; i < num_samples && i < vis->smooth_buffer_size; i++) {
        double sample = (double)samples[i] / 32768.0;
        vis->waveform_smooth[i] = vis->waveform_smooth[i] * 0.7 + sample * 0.3;
    }
    
    // Desenha waveform com efeito fluido (linhas suaves com grossura variável)
    for (int i = 0; i < num_samples - 1; i++) {
        int x1 = (int)(i * x_scale);
        int x2 = (int)((i + 1) * x_scale);
        
        double y1_val = vis->waveform_smooth[i < vis->smooth_buffer_size ? i : 0];
        double y2_val = vis->waveform_smooth[(i + 1) < vis->smooth_buffer_size ? (i + 1) : 0];
        
        int y1 = center_y - (int)(y1_val * vis->height / 2.0);
        int y2 = center_y - (int)(y2_val * vis->height / 2.0);
        
        if (y1 < 0) y1 = 0;
        if (y1 >= vis->height) y1 = vis->height - 1;
        if (y2 < 0) y2 = 0;
        if (y2 >= vis->height) y2 = vis->height - 1;
        
        // Calcula grossura baseada na amplitude (varia de 3 a 10 pixels)
        double amp = (fabs(y1_val) + fabs(y2_val)) / 2.0;
        int line_thickness = (int)(3 + amp * 7.0);  // 3 + 0*7 = 3, 3 + 1*7 = 10
        if (line_thickness > 10) line_thickness = 10;
        if (line_thickness < 3) line_thickness = 3;
        
        RGBColor color;
        if (colors && i < num_samples) {
            color = colors[i];
            
            // Adiciona mais variação de cores - rotação de matiz baseada na posição
            double h = ((double)i / num_samples) * 360.0;
            RGBColor enhanced = color_mapper_hsv_to_rgb(h, 0.8, 0.9);
            
            // Mistura cores originais com cores rotacionadas
            color.r = (uint8_t)(color.r * 0.5 + enhanced.r * 0.5);
            color.g = (uint8_t)(color.g * 0.5 + enhanced.g * 0.5);
            color.b = (uint8_t)(color.b * 0.5 + enhanced.b * 0.5);
            
            // Adiciona brilho baseado na amplitude
            color.r = (uint8_t)(color.r * (0.4 + amp * 0.6));
            color.g = (uint8_t)(color.g * (0.4 + amp * 0.6));
            color.b = (uint8_t)(color.b * (0.4 + amp * 0.6));
        } else {
            // Cor padrão com variação
            double hue = ((double)i / num_samples) * 240.0;
            color = color_mapper_hsv_to_rgb(hue, 0.7, 0.8);
        }
        
        // Desenha linha com grossura variável
        SDL_SetRenderDrawColor(vis->renderer, color.r, color.g, color.b, 255);
        
        // Desenha múltiplas linhas paralelas para criar grossura
        for (int t = 0; t < line_thickness; t++) {
            int offset = t - line_thickness / 2;
            SDL_RenderDrawLine(vis->renderer, x1, y1 + offset, x2, y2 + offset);
            // Também desenha perpendicular para preencher melhor
            if (t > 0 && abs(y2 - y1) > 1) {
                SDL_RenderDrawLine(vis->renderer, x1 + offset, y1, x1 + offset, y2);
            }
        }
    }
}

void visualizer_update_particles(Visualizer* vis, const double* frequencies, int num_bins) {
    if (!vis || !frequencies || num_bins <= 0) return;
    
    // Remove partículas mortas
    int write_idx = 0;
    for (int i = 0; i < vis->num_particles; i++) {
        if (vis->particles[i].life > 0) {
            vis->particles[write_idx++] = vis->particles[i];
        }
    }
    vis->num_particles = write_idx;
    
    // Gera novas partículas baseadas em frequências altas
    double high_energy = 0.0;
    for (int i = num_bins / 2; i < num_bins; i++) {
        high_energy += frequencies[i];
    }
    high_energy /= (num_bins / 2);
    
    // Aumenta geração de partículas (mais partículas por frame)
    if (high_energy > 0.05 && vis->num_particles < vis->max_particles - 10) {
        int new_particles = (int)(high_energy * 8);  // Aumentado de 3 para 8
        if (new_particles > 15) new_particles = 15;  // Aumentado de 5 para 15
        
        for (int i = 0; i < new_particles && vis->num_particles < vis->max_particles; i++) {
            Particle* p = &vis->particles[vis->num_particles++];
            p->x = (double)(rand() % vis->width);
            p->y = (double)(rand() % vis->height);
            p->vx = (double)(rand() % 200 - 100) / 50.0;
            p->vy = (double)(rand() % 200 - 100) / 50.0;
            p->life = 1.0;
            p->max_life = 1.0;
            p->size = (double)(rand() % 5 + 2);
            
            // Cor mais variada - usa toda a gama de frequências
            int freq_bin = rand() % num_bins;
            double freq = (double)freq_bin * 44100.0 / 2048.0;
            
            // Adiciona variação de matiz para mais cores
            double hue_variation = (double)(rand() % 60) - 30;  // Variação de ±30 graus
            RGBColor base_color = color_mapper_frequency_to_rgb(freq);
            double base_hue = (freq < 200 ? 0 : (freq < 2000 ? 120 : 240));
            RGBColor varied_color = color_mapper_hsv_to_rgb(base_hue + hue_variation, 0.9, 0.9);
            
            // Mistura cores para mais variedade
            p->color.r = (uint8_t)(base_color.r * 0.6 + varied_color.r * 0.4);
            p->color.g = (uint8_t)(base_color.g * 0.6 + varied_color.g * 0.4);
            p->color.b = (uint8_t)(base_color.b * 0.6 + varied_color.b * 0.4);
        }
    }
    
    // Atualiza partículas existentes
    for (int i = 0; i < vis->num_particles; i++) {
        Particle* p = &vis->particles[i];
        
        p->x += p->vx;
        p->y += p->vy;
        p->vy += 0.2;  // Gravidade suave
        p->life -= 0.02;
        
        // Bounce nas bordas
        if (p->x < 0 || p->x >= vis->width) p->vx *= -0.8;
        if (p->y < 0 || p->y >= vis->height) p->vy *= -0.8;
        
        if (p->x < 0) p->x = 0;
        if (p->x >= vis->width) p->x = vis->width - 1;
        if (p->y < 0) p->y = 0;
        if (p->y >= vis->height) p->y = vis->height - 1;
        
        // Desenha partícula
        if (p->life > 0) {
            double alpha = p->life / p->max_life;
            RGBColor color = p->color;
            color.r = (uint8_t)(color.r * alpha);
            color.g = (uint8_t)(color.g * alpha);
            color.b = (uint8_t)(color.b * alpha);
            
            SDL_SetRenderDrawColor(vis->renderer, color.r, color.g, color.b, (uint8_t)(alpha * 255));
            
            // Desenha círculo simples (quadrado pequeno)
            int size = (int)(p->size * alpha);
            if (size > 0) {
                SDL_Rect rect = {(int)p->x - size/2, (int)p->y - size/2, size, size};
                SDL_RenderFillRect(vis->renderer, &rect);
            }
        }
    }
}

