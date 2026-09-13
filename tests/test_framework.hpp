#pragma once

// Harness de teste minimo, sem dependencias.
//
// O plano sugeria Catch2 ou GoogleTest. A escolha aqui e deliberada: os testes
// sao a UNICA evidencia de que a matematica do projeto esta correta, e precisam
// rodar em qualquer maquina sem rede e sem instalar nada. Um framework de 120
// linhas que sempre roda vale mais que um completo que as vezes nao esta
// disponivel. Se um dia fizer falta (parametrizacao, fixtures, matchers),
// trocar e barato -- as assercoes sao propositalmente genericas.

#include <cmath>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace swtest {

struct TestCase {
    std::string suite;
    std::string name;
    std::function<void()> body;
};

// Falha de assercao. Lancar (em vez de so contar) aborta o teste no primeiro
// erro, evitando cascatas de falhas derivadas que escondem a causa raiz.
struct AssertionFailure {
    std::string message;
};

inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> cases;
    return cases;
}

struct Registrar {
    Registrar(const char* suite, const char* name, std::function<void()> body) {
        registry().push_back(TestCase{suite, name, std::move(body)});
    }
};

[[noreturn]] inline void fail(const std::string& message) {
    throw AssertionFailure{message};
}

inline void checkTrue(bool condition, const char* expression, const char* file, int line) {
    if (condition) return;
    std::ostringstream out;
    out << file << ":" << line << ": esperado verdadeiro: " << expression;
    fail(out.str());
}

template <typename A, typename B>
void checkEqual(const A& actual, const B& expected, const char* expression, const char* file,
                int line) {
    if (actual == expected) return;
    std::ostringstream out;
    out << file << ":" << line << ": " << expression << "\n"
        << "         obtido  : " << actual << "\n"
        << "         esperado: " << expected;
    fail(out.str());
}

// Comparacao com tolerancia ABSOLUTA. Toda chamada tem de passar a tolerancia
// explicitamente: um valor padrao escondido e o jeito mais facil de transformar
// um teste numerico em teatro.
inline void checkNear(double actual, double expected, double tolerance, const char* expression,
                      const char* file, int line) {
    const double difference = std::abs(actual - expected);
    if (difference <= tolerance) return;
    std::ostringstream out;
    out.precision(12);
    out << file << ":" << line << ": " << expression << "\n"
        << "         obtido   : " << actual << "\n"
        << "         esperado : " << expected << " +/- " << tolerance << "\n"
        << "         diferenca: " << difference;
    fail(out.str());
}

int runAll(const std::string& filter);

}  // namespace swtest

#define SW_CONCAT_INNER(a, b) a##b
#define SW_CONCAT(a, b) SW_CONCAT_INNER(a, b)

#define TEST(suite, name)                                                              \
    static void SW_CONCAT(sw_test_, __LINE__)();                                       \
    static const ::swtest::Registrar SW_CONCAT(sw_registrar_, __LINE__)(               \
        #suite, #name, &SW_CONCAT(sw_test_, __LINE__));                                \
    static void SW_CONCAT(sw_test_, __LINE__)()

#define CHECK(condition) ::swtest::checkTrue((condition), #condition, __FILE__, __LINE__)
#define CHECK_EQ(actual, expected) \
    ::swtest::checkEqual((actual), (expected), #actual " == " #expected, __FILE__, __LINE__)
#define CHECK_NEAR(actual, expected, tolerance)                                        \
    ::swtest::checkNear((actual), (expected), (tolerance),                             \
                        #actual " ~= " #expected " (+/- " #tolerance ")", __FILE__, __LINE__)
#define FAIL(message) ::swtest::fail(message)
