#ifndef AUDIO_DECODER_H
#define AUDIO_DECODER_H

#include <stdint.h>
#include <stdbool.h>

typedef struct AudioDecoder AudioDecoder;

// Inicializa o decodificador de áudio
AudioDecoder* audio_decoder_init(const char* filename);

// Libera recursos do decodificador
void audio_decoder_free(AudioDecoder* decoder);

// Decodifica próxima porção de áudio e retorna número de samples lidos
// samples: buffer de saída (deve ser alocado pelo chamador)
// num_samples: número máximo de samples a ler
// Retorna: número de samples realmente lidos (0 se fim do arquivo)
int audio_decoder_read(AudioDecoder* decoder, int16_t* samples, int num_samples);

// Retorna a taxa de amostragem do áudio
int audio_decoder_get_sample_rate(AudioDecoder* decoder);

// Verifica se o decodificador está válido
bool audio_decoder_is_valid(AudioDecoder* decoder);

// Reinicia o decodificador para o início do arquivo
void audio_decoder_rewind(AudioDecoder* decoder);

#endif // AUDIO_DECODER_H

