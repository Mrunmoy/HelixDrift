#include <cstdint>

volatile uint32_t g_heartbeat = 0;

int main() {
    while (true) {
        ++g_heartbeat;
    }
}
