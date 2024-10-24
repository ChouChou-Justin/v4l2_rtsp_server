#include "logger.h"

void logMessage(const std::string& message) {
    std::time_t now = std::time(nullptr);
    char timestamp[20];
    std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
    std::cout << timestamp << " - " << message << std::endl;
}
