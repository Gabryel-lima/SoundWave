#ifndef AUDIO_PLAYER_H
#define AUDIO_PLAYER_H

#include <stdint.h>
#include <stdbool.h>

typedef struct AudioPlayer AudioPlayer;

// Inicializa o player de áudio
// sample_rate: taxa de amostragem (ex: 44100)
// channels: número de canais (1 = mono, 2 = stereo)
AudioPlayer* audio_player_init(int sample_rate, int channels);

// Libera recursos do player
void audio_player_free(AudioPlayer* player);

// Reproduz samples de áudio
// samples: array de samples PCM (16-bit)
// num_samples: número de samples a reproduzir
// Retorna: número de samples realmente enfileirados
int audio_player_queue(AudioPlayer* player, const int16_t* samples, int num_samples);

// Retorna o número de samples ainda não reproduzidos na fila
int audio_player_get_queued_samples(AudioPlayer* player);

// Retorna o número total de samples que foram reproduzidos (enfileirados - na fila)
uint64_t audio_player_get_played_samples(AudioPlayer* player);

// Limpa a fila de áudio
void audio_player_clear(AudioPlayer* player);

// Pausa a reprodução
void audio_player_pause(AudioPlayer* player);

// Retoma a reprodução
void audio_player_resume(AudioPlayer* player);

// Verifica se está pausado
bool audio_player_is_paused(AudioPlayer* player);

#endif // AUDIO_PLAYER_H

