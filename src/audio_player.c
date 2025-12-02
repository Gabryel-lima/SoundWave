#include "audio_player.h"
#include <SDL2/SDL.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct AudioPlayer {
    SDL_AudioDeviceID device_id;
    SDL_AudioSpec spec;
    int sample_rate;
    int channels;
    bool paused;
    uint64_t total_samples_queued;  // Total de samples enfileirados desde o início
};

AudioPlayer* audio_player_init(int sample_rate, int channels) {
    AudioPlayer* player = malloc(sizeof(AudioPlayer));
    if (!player) {
        return NULL;
    }
    
    player->sample_rate = sample_rate;
    player->channels = channels;
    player->paused = false;
    player->device_id = 0;
    player->total_samples_queued = 0;
    
    // Inicializa SDL Audio se ainda não foi inicializado
    if (!SDL_WasInit(SDL_INIT_AUDIO)) {
        if (SDL_Init(SDL_INIT_AUDIO) < 0) {
            fprintf(stderr, "Erro ao inicializar SDL Audio: %s\n", SDL_GetError());
            free(player);
            return NULL;
        }
    }
    
    // Configura especificação de áudio desejada
    SDL_zero(player->spec);
    player->spec.freq = sample_rate;
    player->spec.format = AUDIO_S16SYS;  // 16-bit signed, system byte order
    player->spec.channels = channels;
    player->spec.samples = 4096;  // Tamanho do buffer
    player->spec.callback = NULL;  // Usa SDL_QueueAudio, não precisa de callback
    
    // Abre dispositivo de áudio (SDL2 gerencia a fila automaticamente com SDL_QueueAudio)
    player->device_id = SDL_OpenAudioDevice(NULL, 0, &player->spec, NULL, SDL_AUDIO_ALLOW_ANY_CHANGE);
    if (player->device_id == 0) {
        fprintf(stderr, "Erro ao abrir dispositivo de áudio: %s\n", SDL_GetError());
        free(player);
        return NULL;
    }
    
    // Inicia reprodução
    SDL_PauseAudioDevice(player->device_id, 0);
    
    return player;
}

void audio_player_free(AudioPlayer* player) {
    if (!player) return;
    
    if (player->device_id != 0) {
        SDL_CloseAudioDevice(player->device_id);
    }
    
    free(player);
}

int audio_player_queue(AudioPlayer* player, const int16_t* samples, int num_samples) {
    if (!player || !samples || num_samples <= 0) {
        return 0;
    }
    
    // Calcula tamanho em bytes
    int bytes = num_samples * sizeof(int16_t) * player->channels;
    
    // Enfileira os samples para reprodução
    int ret = SDL_QueueAudio(player->device_id, samples, bytes);
    if (ret < 0) {
        fprintf(stderr, "Erro ao enfileirar áudio: %s\n", SDL_GetError());
        return 0;
    }
    
    // Atualiza contador total
    player->total_samples_queued += num_samples;
    
    return num_samples;
}

int audio_player_get_queued_samples(AudioPlayer* player) {
    if (!player) return 0;
    
    Uint32 bytes_queued = SDL_GetQueuedAudioSize(player->device_id);
    return bytes_queued / sizeof(int16_t) / player->channels;
}

uint64_t audio_player_get_played_samples(AudioPlayer* player) {
    if (!player) return 0;
    
    int queued = audio_player_get_queued_samples(player);
    // Samples reproduzidos = total enfileirado - ainda na fila
    return player->total_samples_queued - queued;
}

void audio_player_clear(AudioPlayer* player) {
    if (!player || player->device_id == 0) return;
    SDL_ClearQueuedAudio(player->device_id);
    player->total_samples_queued = 0;  // Reseta contador ao limpar
}

void audio_player_pause(AudioPlayer* player) {
    if (!player || player->device_id == 0) return;
    player->paused = true;
    SDL_PauseAudioDevice(player->device_id, 1);
}

void audio_player_resume(AudioPlayer* player) {
    if (!player || player->device_id == 0) return;
    player->paused = false;
    SDL_PauseAudioDevice(player->device_id, 0);
}

bool audio_player_is_paused(AudioPlayer* player) {
    if (!player) return true;
    return player->paused;
}

