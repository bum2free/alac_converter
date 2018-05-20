#include "log.h"
#include <stdio.h>
#include <time.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

Logger::Logger(uint32_t max_length) : max_length(max_length)
{
    buffer = new char[max_length];
    auto logger_ = spdlog::stdout_color_st("console");
    logger_->set_level(spdlog::level::debug);
    loggers.push_back(logger_);
}

Logger::Logger(uint32_t max_length, const std::string &error_file) : Logger(max_length)
{
    this->error_file = error_file;
    auto logger_ = spdlog::basic_logger_st("Basic_logger", error_file);
    logger_->set_level(spdlog::level::err);
    loggers.push_back(logger_);
}

int Logger::set_log_err_file(const std::string &error_file)
{
    auto logger_ = spdlog::basic_logger_st("Basic_logger", error_file);
    logger_->set_level(spdlog::level::err);
    loggers.push_back(logger_);

    return 0;
}

Logger::~Logger()
{
    if (buffer)
        delete[] buffer;
}

void Logger::operator()(int level, const char* fmt, ...)
{
    va_list argp;
    va_start(argp, fmt);
    int cnt = vsnprintf(buffer, max_length, fmt, argp);
    va_end(argp);
    for (auto i = loggers.begin(); i != loggers.end(); i++) {
        (*i)->log((spdlog::level::level_enum)level, buffer);
    }
}
