#ifndef __LOG_H__
#define __LOG_H__
#include <memory>
#include "spdlog/spdlog.h"
#include <stdint.h>
#include <string>
#include <vector>

//This is the same with enum spdlog::level_enum
#define LEVEL_DETAIL 0
#define LEVEL_DEBUG 1
#define LEVEL_INFO 2
#define LEVEL_WARN 3
#define LEVEL_ERROR 4

class Logger
{
public:
    Logger(uint32_t max_length);
    Logger(uint32_t max_length, const std::string &error_file);
    int set_log_err_file(const std::string &error_file);
    void operator()(int level, const char* fmt, ...);
    ~Logger();
private:
    void send_log();
    std::vector<std::shared_ptr<spdlog::logger>> loggers;
    char *buffer = nullptr;
    uint32_t max_length = 0;
    std::string error_file;
    uint8_t log_level = LEVEL_DEBUG;
};
#endif
