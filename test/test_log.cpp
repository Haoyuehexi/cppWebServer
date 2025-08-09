#include "../common/log.h"

int main() {
    Logger::init("./logs/test.log", DEBUG);

    LOG_DEBUG("This is a debug message");
    LOG_INFO("This is an info message");
    LOG_WARN("This is a warning message");
    LOG_ERROR("This is an error message");

    Logger::close();
    return 0;
}
