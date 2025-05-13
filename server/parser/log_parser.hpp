#ifndef LOG_PARSER_HPP
#define LOG_PARSER_HPP

#include <unordered_map>
#include <string>

enum class AnalysisType {
    BY_USER,
    BY_IP,
    BY_LOG_LEVEL
};

using namespace std;
// Interface (abstract base class) for all log parsers
class LogParser {
public:
    virtual ~LogParser() = default;

    // Parse the log file and return the analysis result
    virtual unordered_map<string, int> parse(AnalysisType type) = 0;
};

#endif // LOG_PARSER_HPP
