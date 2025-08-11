#include "./common/config.h"
#include "./common/log.h"

int main() {
    Config config;
    ConfigLoader::load("config.json", config);

    Logger::init(config.logging.file, config.logging.level);
    
    return 0;
}