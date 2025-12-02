#include "audio_decoder.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>

struct AudioDecoder {
    AVFormatContext* format_ctx;
    AVCodecContext* codec_ctx;
    AVFrame* frame;
    AVPacket* packet;
    struct SwrContext* swr_ctx;
    
    int audio_stream_index;
    int sample_rate;
    int channels;
    bool valid;
    
    int16_t* resample_buffer;
    int resample_buffer_size;
    
    // Buffer circular para samples restantes
    int16_t* pending_buffer;
    int pending_size;
    int pending_pos;
};

static int find_audio_stream(AVFormatContext* format_ctx, AVCodecContext** codec_ctx) {
    int stream_index = -1;
    
    for (unsigned int i = 0; i < format_ctx->nb_streams; i++) {
        if (format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            stream_index = i;
            
            const AVCodec* codec = avcodec_find_decoder(format_ctx->streams[i]->codecpar->codec_id);
            if (!codec) {
                continue;
            }
            
            *codec_ctx = avcodec_alloc_context3(codec);
            if (!*codec_ctx) {
                continue;
            }
            
            if (avcodec_parameters_to_context(*codec_ctx, format_ctx->streams[i]->codecpar) < 0) {
                avcodec_free_context(codec_ctx);
                continue;
            }
            
            if (avcodec_open2(*codec_ctx, codec, NULL) < 0) {
                avcodec_free_context(codec_ctx);
                continue;
            }
            
            break;
        }
    }
    
    return stream_index;
}

AudioDecoder* audio_decoder_init(const char* filename) {
    AudioDecoder* decoder = calloc(1, sizeof(AudioDecoder));
    if (!decoder) {
        return NULL;
    }
    
    decoder->format_ctx = avformat_alloc_context();
    if (!decoder->format_ctx) {
        free(decoder);
        return NULL;
    }
    
    // Abre o arquivo de áudio
    if (avformat_open_input(&decoder->format_ctx, filename, NULL, NULL) < 0) {
        fprintf(stderr, "Erro ao abrir arquivo de áudio: %s\n", filename);
        avformat_free_context(decoder->format_ctx);
        free(decoder);
        return NULL;
    }
    
    // Encontra informações do stream
    if (avformat_find_stream_info(decoder->format_ctx, NULL) < 0) {
        fprintf(stderr, "Erro ao encontrar informações do stream\n");
        avformat_close_input(&decoder->format_ctx);
        free(decoder);
        return NULL;
    }
    
    // Encontra o stream de áudio
    decoder->audio_stream_index = find_audio_stream(decoder->format_ctx, &decoder->codec_ctx);
    if (decoder->audio_stream_index < 0 || !decoder->codec_ctx) {
        fprintf(stderr, "Erro ao encontrar stream de áudio\n");
        avformat_close_input(&decoder->format_ctx);
        free(decoder);
        return NULL;
    }
    
    decoder->sample_rate = decoder->codec_ctx->sample_rate;
    
    // Obtém número de canais (suprime warning de deprecação)
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    decoder->channels = decoder->codec_ctx->channels;
    #pragma GCC diagnostic pop
    
    // Configura resampler para converter para mono, 16-bit, 44100 Hz
    // (suprime warnings de deprecação para compatibilidade)
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    decoder->swr_ctx = swr_alloc_set_opts(NULL,
                                          AV_CH_LAYOUT_MONO,
                                          AV_SAMPLE_FMT_S16,
                                          44100,
                                          decoder->codec_ctx->channel_layout,
                                          decoder->codec_ctx->sample_fmt,
                                          decoder->codec_ctx->sample_rate,
                                          0, NULL);
    #pragma GCC diagnostic pop
    
    if (!decoder->swr_ctx || swr_init(decoder->swr_ctx) < 0) {
        fprintf(stderr, "Erro ao inicializar resampler\n");
        avcodec_free_context(&decoder->codec_ctx);
        avformat_close_input(&decoder->format_ctx);
        free(decoder);
        return NULL;
    }
    
    // Atualiza sample_rate para o resampled
    decoder->sample_rate = 44100;
    decoder->channels = 1;
    
    decoder->frame = av_frame_alloc();
    decoder->packet = av_packet_alloc();
    
    if (!decoder->frame || !decoder->packet) {
        swr_free(&decoder->swr_ctx);
        avcodec_free_context(&decoder->codec_ctx);
        avformat_close_input(&decoder->format_ctx);
        free(decoder);
        return NULL;
    }
    
    decoder->resample_buffer_size = 4096;
    decoder->resample_buffer = malloc(decoder->resample_buffer_size * sizeof(int16_t));
    decoder->pending_buffer = malloc(decoder->resample_buffer_size * sizeof(int16_t));
    decoder->pending_size = 0;
    decoder->pending_pos = 0;
    
    if (!decoder->resample_buffer || !decoder->pending_buffer) {
        if (decoder->pending_buffer) free(decoder->pending_buffer);
        if (decoder->resample_buffer) free(decoder->resample_buffer);
        av_packet_free(&decoder->packet);
        av_frame_free(&decoder->frame);
        swr_free(&decoder->swr_ctx);
        avcodec_free_context(&decoder->codec_ctx);
        avformat_close_input(&decoder->format_ctx);
        free(decoder);
        return NULL;
    }
    
    decoder->valid = true;
    return decoder;
}

void audio_decoder_free(AudioDecoder* decoder) {
    if (!decoder) return;
    
    if (decoder->pending_buffer) {
        free(decoder->pending_buffer);
    }
    if (decoder->resample_buffer) {
        free(decoder->resample_buffer);
    }
    if (decoder->packet) {
        av_packet_free(&decoder->packet);
    }
    if (decoder->frame) {
        av_frame_free(&decoder->frame);
    }
    if (decoder->swr_ctx) {
        swr_free(&decoder->swr_ctx);
    }
    if (decoder->codec_ctx) {
        avcodec_free_context(&decoder->codec_ctx);
    }
    if (decoder->format_ctx) {
        avformat_close_input(&decoder->format_ctx);
    }
    
    free(decoder);
}

int audio_decoder_read(AudioDecoder* decoder, int16_t* samples, int num_samples) {
    if (!decoder || !decoder->valid || !samples) {
        return 0;
    }
    
    int samples_read = 0;
    
    // Primeiro, usa samples pendentes do buffer
    if (decoder->pending_size > 0) {
        int to_copy = (decoder->pending_size < num_samples) ? 
                       decoder->pending_size : num_samples;
        memcpy(samples, decoder->pending_buffer + decoder->pending_pos, 
               to_copy * sizeof(int16_t));
        samples_read += to_copy;
        decoder->pending_pos += to_copy;
        decoder->pending_size -= to_copy;
        
        if (decoder->pending_size == 0) {
            decoder->pending_pos = 0;
        }
    }
    
    // Continua lendo frames até ter samples suficientes
    while (samples_read < num_samples) {
        int ret = av_read_frame(decoder->format_ctx, decoder->packet);
        if (ret < 0) {
            // Fim do arquivo ou erro
            break;
        }
        
        if (decoder->packet->stream_index == decoder->audio_stream_index) {
            ret = avcodec_send_packet(decoder->codec_ctx, decoder->packet);
            if (ret < 0) {
                av_packet_unref(decoder->packet);
                continue;
            }
            
            ret = avcodec_receive_frame(decoder->codec_ctx, decoder->frame);
            if (ret < 0) {
                av_packet_unref(decoder->packet);
                continue;
            }
            
            // Resample para mono, 16-bit, 44100 Hz
            int out_count = swr_convert(decoder->swr_ctx,
                                       (uint8_t**)&decoder->resample_buffer,
                                       decoder->resample_buffer_size,
                                       (const uint8_t**)decoder->frame->data,
                                       decoder->frame->nb_samples);
            
            if (out_count > 0) {
                int needed = num_samples - samples_read;
                int to_copy = (out_count < needed) ? out_count : needed;
                
                // Copia samples para o buffer de saída
                memcpy(samples + samples_read, decoder->resample_buffer, 
                       to_copy * sizeof(int16_t));
                samples_read += to_copy;
                
                // Armazena samples restantes no buffer pendente
                if (out_count > to_copy) {
                    int remaining = out_count - to_copy;
                    if (decoder->pending_size + remaining <= decoder->resample_buffer_size) {
                        memcpy(decoder->pending_buffer + decoder->pending_pos + decoder->pending_size,
                               decoder->resample_buffer + to_copy,
                               remaining * sizeof(int16_t));
                        decoder->pending_size += remaining;
                    }
                }
            }
        }
        
        av_packet_unref(decoder->packet);
    }
    
    return samples_read;
}

int audio_decoder_get_sample_rate(AudioDecoder* decoder) {
    if (!decoder) return 0;
    return decoder->sample_rate;
}

bool audio_decoder_is_valid(AudioDecoder* decoder) {
    return decoder && decoder->valid;
}

void audio_decoder_rewind(AudioDecoder* decoder) {
    if (!decoder || !decoder->valid) return;
    
    av_seek_frame(decoder->format_ctx, -1, 0, AVSEEK_FLAG_BACKWARD);
    avcodec_flush_buffers(decoder->codec_ctx);
    decoder->pending_size = 0;
    decoder->pending_pos = 0;
}

