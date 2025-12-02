#include "visualizer.h"
#include <stdlib.h>
#include <stdio.h>
#include <SDL2/SDL.h>
#include <string.h>
#include <stdbool.h>

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
    
    if (!vis->waveform_buffer || !vis->color_buffer) {
        if (vis->color_buffer) free(vis->color_buffer);
        if (vis->waveform_buffer) free(vis->waveform_buffer);
        SDL_DestroyRenderer(vis->renderer);
        SDL_DestroyWindow(vis->window);
        SDL_Quit();
        free(vis);
        return NULL;
    }
    
    // Inicializa buffer com zeros
    memset(vis->waveform_buffer, 0, vis->buffer_size * sizeof(int16_t));
    memset(vis->color_buffer, 0, vis->buffer_size * sizeof(RGBColor));
    vis->buffer_pos = 0;
    vis->use_scroll = true;  // Ativa modo scroll por padrão
    
    return vis;
}

void visualizer_free(Visualizer* vis) {
    if (!vis) return;
    
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

