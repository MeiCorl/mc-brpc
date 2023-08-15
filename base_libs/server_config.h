#pragma once

#define INIT_LOGGER(log_file_name)                                                                 \
    logging::LoggingSettings logging_settings;                                                     \
    logging_settings.logging_dest = logging::LOG_TO_FILE;                                          \
    std::string log_path("/data/log/" + log_file_name + "/" + log_file_name + ".log");             \
    logging_settings.log_file = log_path.c_str();                                                  \
    if (!logging::InitLogging(logging_settings)) {                                                 \
        LOG(ERROR) << "Failed to init logging with log_dir " << log_path;                          \
        exit(-1);                                                                                  \
    }