// Compara as funcoes de mapeamento sobre a escala cromatica, e responde de forma
// verificavel a pergunta 2 do plano: "o mapeamento logaritmico preserva melhor
// as relacoes musicais percebidas?"
//
// A resposta curta e nao -- nao a mais forte delas, a equivalencia de oitava.
// Este programa mostra por que.

#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

#include "soundwave/color/color_engine.hpp"
#include "soundwave/mapping/mappers.hpp"
#include "soundwave/render/image.hpp"
#include "soundwave/render/plots.hpp"

using namespace soundwave;

namespace {

double colourDistance(Rgb a, Rgb b) {
    // Distancia euclidiana grosseira em sRGB. Nao e perceptualmente uniforme
    // (CIEDE2000 seria), mas basta para uma comparacao de ordem de grandeza e
    // nao esconde nada atras de uma formula que o leitor nao conhece.
    const double dr = static_cast<double>(a.r) - b.r;
    const double dg = static_cast<double>(a.g) - b.g;
    const double db = static_cast<double>(a.b) - b.b;
    return std::sqrt(dr * dr + dg * dg + db * db);
}

}  // namespace

int main() {
    const MappingDomain domain;
    const ColorEngine engine;

    std::cout << "A ASSIMETRIA QUE GOVERNA TUDO\n"
              << std::string(72, '=') << "\n"
              << std::fixed << std::setprecision(3)
              << "  faixa audivel : " << domain.sourceMinHz << " a " << domain.sourceMaxHz
              << " Hz = " << domain.sourceOctaves() << " oitavas\n"
              << "  faixa visivel : " << (domain.targetMinHz / 1e12) << " a "
              << (domain.targetMaxHz / 1e12) << " THz = " << domain.targetOctaves() << " oitava\n"
              << "  compressao    : " << domain.compressionRatio() << "x\n\n"
              << "  Qualquer mapeamento monotonico espreme ~10 oitavas em menos de 1.\n"
              << "  Uma oitava musical vira apenas "
              << ((std::pow(2.0, 1.0 / domain.compressionRatio()) - 1.0) * 100.0)
              << "% em f_EM.\n\n";

    const LinearMapper linear(domain);
    const LogMapper logarithmic(domain);
    const OctaveMapper octave(domain, 440.0);

    // ---- Oitavas de A: o teste decisivo ------------------------------------
    std::cout << "OITAVAS DE A -- a mesma nota, oito registros\n"
              << std::string(72, '=') << "\n"
              << std::left << std::setw(10) << "f (Hz)" << std::setw(22) << "logaritmico"
              << "oitava (classe de altura)\n"
              << std::string(72, '-') << "\n";

    Rgb firstLinear{};
    Rgb firstLog{};
    Rgb firstOctave{};
    double maxLinearDistance = 0.0;
    double maxLogDistance = 0.0;
    double maxOctaveDistance = 0.0;
    bool first = true;

    for (double f = 55.0; f <= 14080.0; f *= 2.0) {
        const Rgb linearColour = engine.fromEmFrequency(linear.map(f)).rgb;
        const Rgb logColour = engine.fromEmFrequency(logarithmic.map(f)).rgb;
        const Rgb octaveColour = engine.fromEmFrequency(octave.map(f)).rgb;
        if (first) {
            firstLinear = linearColour;
            firstLog = logColour;
            firstOctave = octaveColour;
            first = false;
        }
        maxLinearDistance = std::max(maxLinearDistance, colourDistance(firstLinear, linearColour));
        maxLogDistance = std::max(maxLogDistance, colourDistance(firstLog, logColour));
        maxOctaveDistance = std::max(maxOctaveDistance, colourDistance(firstOctave, octaveColour));

        std::ostringstream logText;
        logText << "(" << static_cast<int>(logColour.r) << "," << static_cast<int>(logColour.g)
                << "," << static_cast<int>(logColour.b) << ")";
        std::ostringstream octaveText;
        octaveText << "(" << static_cast<int>(octaveColour.r) << ","
                   << static_cast<int>(octaveColour.g) << "," << static_cast<int>(octaveColour.b)
                   << ")";

        std::cout << std::left << std::setw(10) << std::setprecision(1) << f << std::setw(22)
                  << logText.str() << octaveText.str() << "\n";
    }

    std::cout << "\n  dispersao maxima de cor entre oitavas:\n"
              << "    linear      : " << std::setprecision(1) << maxLinearDistance << "\n"
              << "    logaritmico : " << maxLogDistance << "\n"
              << "    oitava      : " << maxOctaveDistance << "\n\n"
              << "  Conclusao: o mapeamento logaritmico preserva a ORDEM e as RAZOES de\n"
              << "  altura, mas nao a equivalencia de oitava. O mapeamento por classe de\n"
              << "  altura preserva a equivalencia, ao custo da monotonicidade e da\n"
              << "  invertibilidade. Sao respostas a perguntas diferentes -- nenhuma das\n"
              << "  duas e 'a correta'.\n\n";

    // ---- Escala cromatica dentro de uma oitava -----------------------------
    std::cout << "ESCALA CROMATICA DE A4 (uma oitava)\n"
              << std::string(72, '=') << "\n"
              << std::left << std::setw(6) << "nota" << std::setw(10) << "f (Hz)" << std::setw(16)
              << "linear" << std::setw(16) << "logaritmico" << "oitava\n"
              << std::string(72, '-') << "\n";

    const char* names[] = {"A4", "A#4", "B4", "C5", "C#5", "D5", "D#5", "E5", "F5", "F#5", "G5",
                           "G#5", "A5"};
    for (int semitone = 0; semitone <= 12; ++semitone) {
        const double f = 440.0 * std::pow(2.0, semitone / 12.0);
        auto text = [](Rgb c) {
            std::ostringstream out;
            out << "(" << static_cast<int>(c.r) << "," << static_cast<int>(c.g) << ","
                << static_cast<int>(c.b) << ")";
            return out.str();
        };
        std::cout << std::left << std::setw(6) << names[semitone] << std::setw(10)
                  << std::setprecision(1) << f << std::setw(16)
                  << text(engine.fromEmFrequency(linear.map(f)).rgb) << std::setw(16)
                  << text(engine.fromEmFrequency(logarithmic.map(f)).rgb)
                  << text(engine.fromEmFrequency(octave.map(f)).rgb) << "\n";
    }

    std::cout << "\n  Repare na coluna 'linear': 440 a 880 Hz ocupa 2% da faixa audivel em\n"
              << "  escala linear, entao a escala inteira recebe praticamente a mesma cor.\n"
              << "  E por isso que LinearMapper existe no projeto como contra-exemplo.\n\n";

    // ---- Reguas em PNG -----------------------------------------------------
    PlotSettings plot;
    plot.width = 1000;
    plot.height = 140;

    struct Entry {
        const char* file;
        const FrequencyMapper* mapper;
    };
    const Entry entries[] = {{"ruler_linear.png", &linear},
                             {"ruler_logarithmic.png", &logarithmic},
                             {"ruler_octave.png", &octave}};

    for (const Entry& entry : entries) {
        std::string error;
        if (writePng(entry.file, renderMappingRuler(*entry.mapper, engine, plot), &error)) {
            std::cout << "gravado " << entry.file << "\n";
        } else {
            std::cerr << "erro ao gravar " << entry.file << ": " << error << "\n";
            return 1;
        }
    }
    std::cout << "\n  As marcas brancas embaixo de cada regua sao as oitavas de A.\n"
              << "  Em ruler_octave.png todas caem sobre a mesma cor; nas outras, nao.\n";
    return 0;
}
