# SoundWave - Visualizador de Áudio em Tempo Real

Sistema de análise de áudio em C que decodifica arquivos de áudio (WAV e MP3), realiza análise de frequências usando FFT, e exibe uma visualização em tempo real mostrando a forma de onda com cores mapeadas pelas frequências dominantes.

## Características

- Decodificação de arquivos WAV e MP3 usando FFmpeg
- Análise de frequências em tempo real usando FFT (FFTW3)
- Visualização gráfica da forma de onda com cores baseadas em frequências
- Suporte para baixas, médias e altas frequências com mapeamento de cores
- Interface gráfica usando SDL2
- Renderização em tempo real a 60 FPS

## Requisitos

### Bibliotecas Necessárias

- **FFmpeg** (libavformat, libavcodec, libavutil, libswresample) - Decodificação de áudio
- **FFTW3** - Transformada rápida de Fourier
- **SDL2** - Interface gráfica e renderização
- **GCC** - Compilador C
- **Make** - Sistema de build

### Instalação das Dependências

#### Ubuntu/Debian

```bash
sudo apt-get update
sudo apt-get install -y \
    libavformat-dev \
    libavcodec-dev \
    libavutil-dev \
    libswresample-dev \
    libfftw3-dev \
    libsdl2-dev \
    pkg-config \
    gcc \
    make
```

Ou usando o Makefile:

```bash
make install-deps
```

#### Fedora/RHEL

```bash
sudo dnf install -y \
    ffmpeg-devel \
    fftw-devel \
    SDL2-devel \
    gcc \
    make \
    pkg-config
```

#### macOS (usando Homebrew)

```bash
brew install ffmpeg fftw sdl2 pkg-config
```

## Compilação

Para compilar o projeto:

```bash
make
```

O executável será gerado em `bin/soundwave`.

## Uso

Execute o programa fornecendo um arquivo de áudio como argumento:

```bash
./bin/soundwave <arquivo_de_audio>
```

Exemplo:

```bash
./bin/soundwave "Feelings V4.mp3"
./bin/soundwave audio.wav
```

### Controles

- **ESC** ou **Q**: Sair do programa
- **Fechar janela**: Sair do programa

## Arquitetura

O projeto está organizado nos seguintes módulos:

### audio_decoder.c/h
- Decodifica arquivos WAV e MP3 usando FFmpeg
- Converte para formato unificado (mono, 16-bit, 44100 Hz)
- Fornece interface para leitura sequencial de samples

### fft_analyzer.c/h
- Realiza análise FFT em janelas de tempo (2048 samples)
- Extrai magnitudes por banda de frequência
- Identifica frequências dominantes
- Calcula energia em bandas específicas (baixo, médio, agudo)

### color_mapper.c/h
- Mapeia frequências para cores RGB usando espaço HSV
- Baixas frequências (20-200 Hz) → Vermelho/Laranja
- Médias frequências (200-2000 Hz) → Amarelo/Verde
- Altas frequências (2000-20000 Hz) → Azul/Roxo
- Suporte para mapeamento baseado em múltiplas bandas

### visualizer.c/h
- Cria janela gráfica usando SDL2
- Renderiza forma de onda com cores por frequência
- Atualiza visualização em tempo real (60 FPS)
- Gerencia eventos de entrada (teclado, mouse)

### main.c
- Ponto de entrada do programa
- Orquestra todos os componentes
- Loop principal de visualização
- Sincronização com taxa de amostragem do áudio

## Fluxo de Dados

```
Arquivo de áudio (WAV/MP3)
    ↓
Decodificador (FFmpeg)
    ↓
Buffer PCM (mono, 16-bit, 44100 Hz)
    ↓
Análise FFT (FFTW3)
    ↓
Frequências dominantes + Bandas de energia
    ↓
Mapeamento de Cores
    ↓
Visualização SDL2 (Forma de onda colorida)
```

## Parâmetros Ajustáveis

No arquivo `main.c`, você pode ajustar:

- `WINDOW_WIDTH` / `WINDOW_HEIGHT`: Tamanho da janela de visualização
- `FFT_WINDOW_SIZE`: Tamanho da janela FFT (recomendado: 2048 ou 4096)
- `SAMPLES_PER_FRAME`: Número de samples processados por frame
- `target_fps`: Taxa de atualização desejada (padrão: 60 FPS)

## Troubleshooting

### Erro: "Erro ao abrir arquivo de áudio"
- Verifique se o arquivo existe e o caminho está correto
- Certifique-se de que o formato é suportado (WAV ou MP3)

### Erro de compilação relacionado a bibliotecas
- Verifique se todas as dependências estão instaladas
- Use `pkg-config --cflags --libs <biblioteca>` para verificar se estão configuradas corretamente

### Visualização muito lenta
- Reduza `FFT_WINDOW_SIZE` (ex: de 4096 para 2048)
- Reduza `SAMPLES_PER_FRAME`
- Compile com otimizações: `make CFLAGS="-O3"`

### Sem áudio ou visualização estática
- Verifique se o arquivo de áudio é válido
- Teste com outro arquivo de áudio
- Verifique os logs de erro no terminal

## Limitações

- Processamento em tempo real pode ter latência dependendo do hardware
- FFT é computacionalmente intensiva - requer CPU razoável
- Não há reprodução de áudio - apenas visualização
- Buffer circular simples pode descartar alguns samples em altas taxas de processamento

## Melhorias Futuras

- Reprodução de áudio simultânea
- Múltiplos estilos de visualização (espectrograma, barras, circular)
- Exportação de vídeo da visualização
- Interface gráfica com controles
- Suporte para mais formatos de áudio

## Licença

Este projeto é fornecido como está, para fins educacionais e de demonstração.

## Autor

SoundWave - Visualizador de Áudio em Tempo Real

