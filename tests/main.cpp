#include "test_framework.hpp"

#include <exception>
#include <iomanip>

namespace swtest {

int runAll(const std::string& filter) {
    int passed = 0;
    int failed = 0;
    int skipped = 0;
    std::string currentSuite;

    for (const TestCase& test : registry()) {
        const std::string fullName = test.suite + "." + test.name;
        if (!filter.empty() && fullName.find(filter) == std::string::npos) {
            ++skipped;
            continue;
        }
        if (test.suite != currentSuite) {
            currentSuite = test.suite;
            std::cout << "\n[" << currentSuite << "]\n";
        }

        std::cout << "  " << std::left << std::setw(52) << test.name << std::flush;
        try {
            test.body();
            std::cout << "ok\n";
            ++passed;
        } catch (const AssertionFailure& failure) {
            std::cout << "FALHOU\n      " << failure.message << "\n";
            ++failed;
        } catch (const std::exception& error) {
            std::cout << "EXCECAO\n      " << error.what() << "\n";
            ++failed;
        }
    }

    std::cout << "\n" << std::string(62, '-') << "\n"
              << passed << " passaram, " << failed << " falharam";
    if (skipped > 0) std::cout << ", " << skipped << " filtrados";
    std::cout << "\n";
    return failed == 0 ? 0 : 1;
}

}  // namespace swtest

int main(int argc, char** argv) {
    const std::string filter = argc > 1 ? argv[1] : "";
    return swtest::runAll(filter);
}
