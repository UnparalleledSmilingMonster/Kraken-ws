#ifndef LOGGER_H_INCLUDED
#define LOGGER_H_INCLUDED

#include<fstream>
#include <map>


namespace LOG
{
    // type of log message specifier
    enum  TYPE
    {
        INFO,
        CHAT,
        WARNING,
        ERROR
    };
}

class Logger
{
public:
    Logger(const std::string& lf); // link logger to lf file
    void log(const std::string& str, LOG::TYPE type); // append str to log file and print it in console

private:
    std::ofstream logfile; // latestlog.txt
    std::map<LOG::TYPE, std::string> mLogTypeStampMap {{LOG::INFO, "[INFO]: "}, {LOG::CHAT, "[CHAT]: "}, {LOG::WARNING, "[WARNING]: "}, {LOG::ERROR, "[ERROR]: "}}; // log type specifier to string
};

#endif // LOGGER_H_INCLUDED
