#include "Document.h"
#include <atomic>
#include <chrono>
#include <random>
#include <sstream>
namespace scalar {
std::string newId() {
    static std::atomic_uint64_t sequence{0};
    thread_local std::mt19937_64 rng(std::random_device{}());
    std::ostringstream out;
    out << std::hex << rng() << '-' << std::chrono::steady_clock::now().time_since_epoch().count() << '-' << sequence++;
    return out.str();
}
}
