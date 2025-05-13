// File: server/parser/json_parser.hpp
// JSONParser: Parses raw JSON log payloads and computes statistics based on AnalysisType.

#ifndef JSON_PARSER_HPP
#define JSON_PARSER_HPP

#include "log_parser.hpp"
#include <sstream>
#include <iostream>
#include <unordered_map>
#include <string>
#include "lib/nlohmann/json.hpp"  // Header-only JSON library by nlohmann

using namespace std;
// JSONParser extends the abstract LogParser interface to
// handle JSON-formatted logs loaded entirely in-memory.
class JSONParser : public LogParser {
public:
    /**
     * Constructor
     * @param rawJson The complete JSON payload as a single string, typically an array of log objects.
     */
    explicit JSONParser(const string& rawJson)
        : dataStr(rawJson) {}

    /**
     * Parses the JSON payload and returns aggregated counts based on the specified AnalysisType.
     * Date-range filtering is expected to be applied at the server layer before this method is called.
     *
     * @param type Dimension for analysis: BY_USER, BY_IP, or BY_LOG_LEVEL.
     * @return An unordered_map where each key is a user ID, IP address, or log level,
     *         and the value is the count of matching log entries.
     */
    unordered_map<string, int> parse(AnalysisType type) override {
        // Result map: group -> count
        unordered_map<string, int> result;
        nlohmann::json j;

        // 1) Attempt to deserialize the JSON string into a JSON object
        try {
            istringstream iss(dataStr);
            iss >> j;
        } catch (const exception& e) {
            cerr << "[ERROR] JSON parsing failed: " << e.what() << "\n";
            return {};
        }

        // 2) Iterate through each element in the JSON array
        for (const auto& entry : j) {
            string key;
            // 3) Select grouping key based on AnalysisType
            switch (type) {
                case AnalysisType::BY_USER:
                    // Convert numeric user_id to string
                    key = to_string(entry["user_id"].get<int>());
                    break;
                case AnalysisType::BY_IP:
                    key = entry["ip_address"].get<string>();
                    break;
                case AnalysisType::BY_LOG_LEVEL:
                default:
                    key = entry["log_level"].get<string>();
                    break;
            }
            // 4) Increment count for this key
            if (!key.empty()) {
                ++result[key];
            }
        }
        return result;
    }

private:
    string dataStr;  ///< Raw JSON payload stored in-memory
};

#endif // JSON_PARSER_HPP
