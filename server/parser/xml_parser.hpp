// File: server/parser/xml_parser.hpp
// XMLParser: Parses raw XML log payloads and computes statistics based on AnalysisType.

#ifndef XML_PARSER_HPP
#define XML_PARSER_HPP

#include "log_parser.hpp"
#include <string>
#include <unordered_map>
#include <iostream>

using namespace std;
// XMLParser inherits from LogParser to support polymorphic dispatch.
class XMLParser : public LogParser {
public:
    /**
     * Constructor
     * @param rawXml Entire XML payload as a single string, typically a <logs>...</logs> document.
     */
    explicit XMLParser(const string& rawXml)
      : dataStr(rawXml) {}

    /**
     * Parses the XML payload and returns a count map based on the specified AnalysisType.
     * It expects the payload to have multiple <log>...</log> entries.
     * Date-range filtering is applied before invoking this parser in server logic.
     *
     * @param type The dimension for analysis: BY_USER, BY_IP, or BY_LOG_LEVEL.
     * @return unordered_map where key=entity (user ID, IP, or log level), value=count.
     */
    unordered_map<string, int> parse(AnalysisType type) override {
        unordered_map<string, int> result;
        size_t pos = 0;

        // Lambda to extract inner text of a given XML tag
        auto getTagValue = [&](const string& s, const string& tag) -> string {
            string open  = "<" + tag + ">";
            string close = "</" + tag + ">";
            size_t p1 = s.find(open);
            if (p1 == string::npos) return "";
            p1 += open.size();
            size_t p2 = s.find(close, p1);
            if (p2 == string::npos) return "";
            return s.substr(p1, p2 - p1);
        };

        // Iterate through each <log> ... </log> block
        while (true) {
            size_t start = dataStr.find("<log>", pos);
            if (start == string::npos) break;  // no more log entries
            size_t end = dataStr.find("</log>", start);
            if (end == string::npos) break;    // malformed XML

            // Extract the content inside <log> ... </log>
            string entry = dataStr.substr(start + 5, end - (start + 5)); //  5 : length of <log>
            pos = end + 6;  // Move past 6 : length of </log>

            // Extract relevant fields
            string ts    = getTagValue(entry, "timestamp");   
            string level = getTagValue(entry, "log_level"); 
            string user  = getTagValue(entry, "user_id");     
            string ip    = getTagValue(entry, "ip_address");  

            // Choose the grouping key based on AnalysisType
            string key;
            switch (type) {
                case AnalysisType::BY_USER:
                    key = user;
                    break;
                case AnalysisType::BY_IP:
                    key = ip;
                    break;
                case AnalysisType::BY_LOG_LEVEL:
                default:
                    key = level;
                    break;
            }

            // Increment count if key is non-empty
            if (!key.empty()) {
                ++result[key];
            }
        }

        return result;
    }

private:
    string dataStr;  ///< Raw XML payload to be parsed
};

#endif // XML_PARSER_HPP
