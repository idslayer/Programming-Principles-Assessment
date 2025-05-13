// File: server/parser/txt_parser.hpp
// TXTParser: Parses raw text log payloads and computes statistics based on AnalysisType.

#ifndef TXT_PARSER_HPP
#define TXT_PARSER_HPP

#include "log_parser.hpp"
#include <sstream>
#include <iostream>
#include <unordered_map>
#include <string>
#include <vector>

using namespace std;

/**
 * TXTParser implements LogParser to process plain-text logs of the format:
 *
 *  YYYY-MM-DD HH:MM:SS | LEVEL | Message text | UserID: #### | IP: ###.###.###.###
 *
 * Supports date-range filtering upstream; this parser focuses purely on splitting
 * each line and extracting the key field (user, IP, or log level).
 */
class TXTParser : public LogParser {
public:
    /**
     * Constructor
     * @param rawContent The entire text payload as a single string with multiple lines.
     */
    explicit TXTParser(const string& rawContent)
      : dataStr(rawContent) {}

    /**
     * Parses each line in the text payload and aggregates counts based on AnalysisType.
     * Expects each non-empty line to be delimited by "|" into exactly five parts.
     * Logs with fewer parts are skipped with a warning.
     *
     * @param type Dimension for analysis: BY_USER, BY_IP, or BY_LOG_LEVEL.
     * @return unordered_map where the key is user ID, IP address, or log level,
     *         and the value is the number of matching entries.
     */
    unordered_map<string, int> parse(AnalysisType type) override {
        unordered_map<string, int> result;
        istringstream stream(dataStr);
        string line;
        int lineNo = 0;

        // Process each line in the payload
        while (getline(stream, line)) {
            ++lineNo;
            if (line.empty()) continue;  // skip blank lines

            // Split the line into parts separated by '|'
            vector<string> parts;
            istringstream ls(line);
            string part;
            while (getline(ls, part, '|')) {
                // Trim leading/trailing whitespace
                size_t start = part.find_first_not_of(" \t");
                size_t end   = part.find_last_not_of(" \t");
                if (start == string::npos) parts.push_back("");
                else                            parts.push_back(part.substr(start, end - start + 1));
            }

            // Validate expected format: timestamp | level | message | UserID: X | IP: Y
            if (parts.size() < 5) {
                cerr << "[WARN] Skipping malformed line " << lineNo
                          << ": expected 5 fields but got " << parts.size()
                          << " => '" << line << "'\n";
                continue;
            }

            // Extract individual fields
            const string& timestamp = parts[0];           // e.g. "2024-09-30 22:51:48"
            const string& level     = parts[1];           // e.g. "INFO"
            const string& message   = parts[2];           // free-text message
            const string& userField = parts[3];           // "UserID: 2421"
            const string& ipField   = parts[4];           // "IP: 84.126.98.62"

            // Helper lambda to extract the numeric portion after ':'
            auto extractValue = [&](const string& field) {
                size_t pos = field.find(':');
                if (pos == string::npos) return string();
                string val = field.substr(pos + 1);
                // Trim again
                size_t s = val.find_first_not_of(" \t");
                size_t e = val.find_last_not_of(" \t");
                if (s == string::npos) return string();
                return val.substr(s, e - s + 1);
            };

            string userId = extractValue(userField);
            string ip     = extractValue(ipField);

            // Determine grouping key based on AnalysisType
            string key;
            switch (type) {
                case AnalysisType::BY_USER:
                    key = userId;
                    break;
                case AnalysisType::BY_IP:
                    key = ip;
                    break;
                case AnalysisType::BY_LOG_LEVEL:
                default:
                    key = level;
                    break;
            }
            // trim before push into unordered_map to avoid hidden \n in txt file
            size_t start = key.find_first_not_of(" \t\r\n");
            size_t end   = key.find_last_not_of(" \t\r\n");
            key = key.substr(start, end - start + 1);
            result[key]++;
        }

        return result;
    }

private:
    string dataStr;  ///< Raw text payload containing all log lines
};

#endif // TXT_PARSER_HPP
