#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#include "audio_decoder.h"
#include "fft_analyzer.h"
#include "color_mapper.h"
#include "visualizer.h"
#include "audio_player.h"

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 800
#define FFT_WINDOW_SIZE 2048
#define SAMPLES_PER_FRAME 512

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Uso: %s <arquivo_de_audio>\n", argv[0]);
        fprintf(stderr, "Exemplo: %s Feelings\\ V4.mp3\n", argv[0]);
        return 1;
    }
    
    const char* audio_file = argv[1];
    
    // Inicializa decodificador de áudio (para reprodução)
    printf("Inicializando decodificador de áudio...\n");
    AudioDecoder* decoder = audio_decoder_init(audio_file);
    if (!decoder || !audio_decoder_is_valid(decoder)) {
        fprintf(stderr, "Erro ao inicializar decodificador de áudio\n");
        return 1;
    }
    
    // Inicializa segundo decodificador para visualização (sincronizado)
    AudioDecoder* vis_decoder = audio_decoder_init(audio_file);
    if (!vis_decoder || !audio_decoder_is_valid(vis_decoder)) {
        fprintf(stderr, "Erro ao inicializar decodificador de visualização\n");
        audio_decoder_free(decoder);
        return 1;
    }
    
    int sample_rate = audio_decoder_get_sample_rate(decoder);
    printf("Taxa de amostragem: %d Hz\n", sample_rate);
    
    // Inicializa player de áudio
    printf("Inicializando player de áudio...\n");
    AudioPlayer* player = audio_player_init(sample_rate, 1);  // Mono
    if (!player) {
        fprintf(stderr, "Erro ao inicializar player de áudio\n");
        audio_decoder_free(vis_decoder);
        audio_decoder_free(decoder);
        return 1;
    }
    
    // Inicializa analisador FFT
    printf("Inicializando analisador FFT...\n");
    FFTAnalyzer* fft = fft_analyzer_init(sample_rate, FFT_WINDOW_SIZE);
    if (!fft) {
        fprintf(stderr, "Erro ao inicializar analisador FFT\n");
        audio_player_free(player);
        audio_decoder_free(vis_decoder);
        audio_decoder_free(decoder);
        return 1;
    }
    
    // Inicializa visualizador
    printf("Inicializando visualizador...\n");
    Visualizer* vis = visualizer_init(WINDOW_WIDTH, WINDOW_HEIGHT, "SoundWave - Visualização de Áudio");
    if (!vis) {
        fprintf(stderr, "Erro ao inicializar visualizador\n");
        fft_analyzer_free(fft);
        audio_player_free(player);
        audio_decoder_free(vis_decoder);
        audio_decoder_free(decoder);
        return 1;
    }
    
    // Buffers
    int16_t* audio_buffer = malloc(SAMPLES_PER_FRAME * sizeof(int16_t));
    int16_t* fft_buffer = malloc(FFT_WINDOW_SIZE * sizeof(int16_t));
    double* frequencies = malloc((FFT_WINDOW_SIZE / 2 + 1) * sizeof(double));
    RGBColor* colors = malloc(SAMPLES_PER_FRAME * sizeof(RGBColor));
    
    if (!audio_buffer || !fft_buffer || !frequencies || !colors) {
        fprintf(stderr, "Erro ao alocar buffers\n");
        if (colors) free(colors);
        if (frequencies) free(frequencies);
        if (fft_buffer) free(fft_buffer);
        if (audio_buffer) free(audio_buffer);
        visualizer_free(vis);
        fft_analyzer_free(fft);
        audio_player_free(player);
        audio_decoder_free(vis_decoder);
        audio_decoder_free(decoder);
        return 1;
    }
    
    // Buffer circular para acumular samples para FFT
    int16_t* fft_circular = malloc(FFT_WINDOW_SIZE * sizeof(int16_t));
    int fft_circular_pos = 0;
    bool fft_buffer_ready = false;
    
    if (!fft_circular) {
        fprintf(stderr, "Erro ao alocar buffer circular FFT\n");
        free(colors);
        free(frequencies);
        free(fft_buffer);
        free(audio_buffer);
        visualizer_free(vis);
        fft_analyzer_free(fft);
        audio_player_free(player);
        audio_decoder_free(vis_decoder);
        audio_decoder_free(decoder);
        return 1;
    }
    
    // Pré-carrega buffer de áudio antes de começar (cerca de 500ms)
    printf("Pré-carregando buffer de áudio...\n");
    const int preload_samples = sample_rate / 2;  // 500ms
    int16_t* preload_buffer = malloc(preload_samples * sizeof(int16_t));
    int preload_read = audio_decoder_read(decoder, preload_buffer, preload_samples);
    if (preload_read > 0) {
        audio_player_queue(player, preload_buffer, preload_read);
        // Sincroniza decoder de visualização (lê os mesmos samples)
        int16_t* vis_preload = malloc(preload_samples * sizeof(int16_t));
        audio_decoder_read(vis_decoder, vis_preload, preload_read);
        free(vis_preload);
    }
    free(preload_buffer);
    
    // Variáveis para sincronização
    uint64_t vis_decoder_position = preload_read;  // Posição atual do decoder de visualização
    
    printf("Iniciando visualização...\n");
    printf("Pressione ESC ou Q para sair\n");
    
    // Loop principal
    bool running = true;
    clock_t last_frame_time = clock();
    clock_t last_audio_time = clock();
    const double target_fps = 60.0;
    const double frame_time = 1.0 / target_fps;
    
    // Buffer mínimo de áudio (cerca de 200ms) para evitar stuttering
    const int min_buffer_samples = sample_rate / 5;
    const double audio_update_interval = 0.01;  // Atualiza áudio a cada 10ms
    
    while (running) {
        clock_t current_time = clock();
        double elapsed = ((double)(current_time - last_frame_time)) / CLOCKS_PER_SEC;
        double audio_elapsed = ((double)(current_time - last_audio_time)) / CLOCKS_PER_SEC;
        
        // Mantém buffer de áudio cheio (independente do FPS visual)
        if (audio_elapsed >= audio_update_interval) {
            int queued = audio_player_get_queued_samples(player);
            
            // Enfileira mais samples se o buffer estiver baixo
            while (queued < min_buffer_samples) {
                int16_t temp_buffer[1024];
                int temp_read = audio_decoder_read(decoder, temp_buffer, 1024);
                if (temp_read > 0) {
                    audio_player_queue(player, temp_buffer, temp_read);
                    queued += temp_read;
                } else {
                    // Fim do áudio, reinicia
                    printf("Fim do áudio. Reiniciando...\n");
                    audio_player_clear(player);
                    audio_decoder_rewind(decoder);
                    // Pré-carrega novamente
                    preload_buffer = malloc(preload_samples * sizeof(int16_t));
                    preload_read = audio_decoder_read(decoder, preload_buffer, preload_samples);
                    if (preload_read > 0) {
                        audio_player_queue(player, preload_buffer, preload_read);
                        // Sincroniza decoder de visualização
                        int16_t* vis_preload = malloc(preload_samples * sizeof(int16_t));
                        audio_decoder_read(vis_decoder, vis_preload, preload_read);
                        free(vis_preload);
                        vis_decoder_position = preload_read;
                    }
                    free(preload_buffer);
                    fft_circular_pos = 0;
                    fft_buffer_ready = false;
                    queued = audio_player_get_queued_samples(player);
                }
            }
            last_audio_time = current_time;
        }
        
        // Controle de FPS visual
        if (elapsed < frame_time) {
            continue;
        }
        last_frame_time = current_time;
        
        // Verifica se deve fechar
        if (visualizer_should_close(vis)) {
            running = false;
            break;
        }
        
        // Sincroniza decoder de visualização com a posição do áudio reproduzido
        uint64_t current_played = audio_player_get_played_samples(player);
        uint64_t samples_to_sync = current_played - vis_decoder_position;
        
        // Se a visualização está atrás do áudio, avança o decoder
        if (samples_to_sync > SAMPLES_PER_FRAME * 2) {
            // Avança lendo e descartando samples até sincronizar
            int16_t* sync_buffer = malloc(SAMPLES_PER_FRAME * sizeof(int16_t));
            while (samples_to_sync > SAMPLES_PER_FRAME && vis_decoder_position < current_played) {
                int sync_read = audio_decoder_read(vis_decoder, sync_buffer, SAMPLES_PER_FRAME);
                if (sync_read > 0) {
                    vis_decoder_position += sync_read;
                    samples_to_sync = current_played - vis_decoder_position;
                } else {
                    break;
                }
            }
            free(sync_buffer);
        }
        
        // Lê samples do decoder de visualização (sincronizado com áudio)
        int samples_read = audio_decoder_read(vis_decoder, audio_buffer, SAMPLES_PER_FRAME);
        
        if (samples_read == 0) {
            // Fim do áudio, reinicia ambos decoders
            audio_decoder_rewind(decoder);
            audio_decoder_rewind(vis_decoder);
            vis_decoder_position = 0;
            fft_circular_pos = 0;
            fft_buffer_ready = false;
            continue;
        }
        
        // Atualiza posição do decoder de visualização
        vis_decoder_position += samples_read;
        
        // Acumula samples no buffer circular para FFT
        for (int i = 0; i < samples_read; i++) {
            fft_circular[fft_circular_pos] = audio_buffer[i];
            fft_circular_pos = (fft_circular_pos + 1) % FFT_WINDOW_SIZE;
            
            if (fft_circular_pos == 0) {
                fft_buffer_ready = true;
            }
        }
        
        // Analisa frequências quando temos uma janela completa
        double dominant_freq = 0.0;
        if (fft_buffer_ready || fft_circular_pos >= FFT_WINDOW_SIZE) {
            // Copia buffer circular para análise (últimos FFT_WINDOW_SIZE samples)
            for (int i = 0; i < FFT_WINDOW_SIZE; i++) {
                int idx = (fft_circular_pos + i) % FFT_WINDOW_SIZE;
                fft_buffer[i] = fft_circular[idx];
            }
            
            dominant_freq = fft_analyzer_analyze(fft, fft_buffer, frequencies);
            
            // Calcula energias das bandas
            double low_energy = fft_analyzer_get_band_energy(fft, frequencies, 20.0, 200.0);
            double mid_energy = fft_analyzer_get_band_energy(fft, frequencies, 200.0, 2000.0);
            double high_energy = fft_analyzer_get_band_energy(fft, frequencies, 2000.0, 20000.0);
            
            // Gera cores para cada sample baseado na frequência dominante
            RGBColor base_color = color_mapper_frequency_to_rgb(dominant_freq);
            
            // Varia a cor baseada nas bandas para criar efeito mais interessante
            for (int i = 0; i < samples_read; i++) {
                // Modula a cor baseada na amplitude do sample
                double amplitude = fabs((double)audio_buffer[i] / 32768.0);
                
                // Mistura cor baseada em frequência com cor baseada em bandas
                RGBColor band_color = color_mapper_bands_to_rgb(low_energy, mid_energy, high_energy);
                
                colors[i].r = (uint8_t)(base_color.r * 0.7 + band_color.r * 0.3 * amplitude);
                colors[i].g = (uint8_t)(base_color.g * 0.7 + band_color.g * 0.3 * amplitude);
                colors[i].b = (uint8_t)(base_color.b * 0.7 + band_color.b * 0.3 * amplitude);
            }
        } else {
            // Ainda não temos janela completa, usa cor padrão
            RGBColor default_color = {128, 128, 255};
            for (int i = 0; i < samples_read; i++) {
                colors[i] = default_color;
            }
        }
        
        // Limpa tela
        visualizer_clear(vis);
        
        // Desenha múltiplas camadas de visualização
        if (fft_buffer_ready || fft_circular_pos >= FFT_WINDOW_SIZE) {
            // 1. Waveform fluida/ambient
            visualizer_draw_fluid_waveform(vis, audio_buffer, samples_read, frequencies, colors);
            
            // 2. Partículas
            visualizer_update_particles(vis, frequencies, FFT_WINDOW_SIZE / 2 + 1);
        } else {
            // Fallback: waveform simples enquanto carrega
            visualizer_draw_waveform_scroll(vis, audio_buffer, samples_read, colors);
        }
        
        // Atualiza tela
        visualizer_present(vis);
    }
    
    // Limpeza
    printf("Encerrando...\n");
    free(fft_circular);
    free(colors);
    free(frequencies);
    free(fft_buffer);
    free(audio_buffer);
    visualizer_free(vis);
    fft_analyzer_free(fft);
    audio_player_free(player);
    audio_decoder_free(vis_decoder);
    audio_decoder_free(decoder);
    
    return 0;
}

