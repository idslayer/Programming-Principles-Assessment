// File: server/server.cpp

#include <iostream>
#include <thread>
#include <sstream>
#include <vector>
#include <string>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>

#include "parser/log_parser.hpp"
#include "parser/json_parser.hpp"
#include "parser/txt_parser.hpp"
#include "parser/xml_parser.hpp"
#include "parser/lib/nlohmann/json.hpp"

#define PORT 8080
#define BUFFER_SIZE 8192

using namespace std;
using json = nlohmann::json;

// Enumeration of supported log formats
enum class FileType { JSON, XML, TXT };

// Detect file type based on first non-whitespace character
FileType detectFileType(const string& body) {
    auto p = body.find_first_not_of(" \t\r\n");
    if (p == string::npos) return FileType::TXT;
    char c = body[p];
    if (c == '[' || c == '{') return FileType::JSON;
    if (c == '<')            return FileType::XML;
    return FileType::TXT;
}

// Filter JSON logs by date range (YYYY-MM-DD)
string filterJsonByDate(const string& jsonBody,
                             const string& fromDate,
                             const string& toDate) {
    json arr;
    try {
        istringstream iss(jsonBody);
        iss >> arr;
    } catch (...) {
         // Return original if parsing fails
        return jsonBody; 
    }
    json filtered = json::array();
    for (auto& entry : arr) {
        string ts = entry.value("timestamp", "");
        if (ts.size() >= 10) {
            string date = ts.substr(0, 10);
            if ((!fromDate.empty() && date < fromDate) ||
                (!toDate.empty()   && date > toDate)) {
                continue;
            }
        }
        filtered.push_back(entry);
    }
    return filtered.dump();
}

// Filter TXT lines by date range ("YYYY-MM-DD")
string filterTxtByDate(const string& txtBody,
                            const string& fromDate,
                            const string& toDate) {
    istringstream iss(txtBody);
    ostringstream oss;
    string line;
    while (getline(iss, line)) {
        if (line.size() < 10) continue;
        string date = line.substr(0, 10);
        if ((!fromDate.empty() && date < fromDate) ||
            (!toDate.empty()   && date > toDate)) {
            continue;
        }
        oss << line << "\n";
    }
    return oss.str();
}

// Filter XML payload by date range ("YYYY-MM-DD")
static string filterXmlByDate(const string& xmlBody,
                                   const string& fromDate,
                                   const string& toDate) {
    auto getTagValue = [&](const string& s, const string& tag) -> string {
        string open  = "<"  + tag + ">";
        string close = "</" + tag + ">";
        size_t p1 = s.find(open);
        if (p1 == string::npos) return "";
        p1 += open.size();
        size_t p2 = s.find(close, p1);
        if (p2 == string::npos) return "";
        return s.substr(p1, p2 - p1);
    };

    string filtered = "<logs>";
    size_t pos = 0;
    while (true) {
        // find the next <log>...</log> block
        size_t start = xmlBody.find("<log>", pos);
        if (start == string::npos) break;
        size_t end = xmlBody.find("</log>", start);
        if (end == string::npos) break;
        // include the closing tag
        size_t blockEnd = end + strlen("</log>");
        string logBlock = xmlBody.substr(start, blockEnd - start);

        // extract timestamp and its date prefix
        string ts = getTagValue(logBlock, "timestamp");
        string date = ts.size() >= 10 ? ts.substr(0, 10) : "";

        // apply filters
        bool keep = true;
        if (!fromDate.empty() && date < fromDate) keep = false;
        if (!toDate.empty()   && date > toDate)   keep = false;

        if (keep) {
            filtered += logBlock;
        }
        pos = blockEnd;
    }
    filtered += "</logs>";
    return filtered;
}

// Handle each client connection in its own thread
void handleClient(int clientSocket) {
    cout << "[INFO] Client connected (thread "
              << this_thread::get_id() << ")\n";

    // 1) Receive full request payload
    string recvBuf;
    recvBuf.reserve(BUFFER_SIZE);
    char buffer[BUFFER_SIZE];
    ssize_t n;
    while ((n = recv(clientSocket, buffer, sizeof(buffer), 0)) > 0) {
        recvBuf.append(buffer, n);
    }
    if (recvBuf.empty()) {
        cerr << "[ERROR] Empty payload\n";
        close(clientSocket);
        return;
    }

    // 2) Split header/body on blank line "\n\n"
    size_t hdrEnd = recvBuf.find("\n\n");
    if (hdrEnd == string::npos) {
        cerr << "[ERROR] Invalid payload (no header/body separator)\n";
        close(clientSocket);
        return;
    }
    string header   = recvBuf.substr(0, hdrEnd);
    string body     = recvBuf.substr(hdrEnd + 2);

    // 3) Parse header lines for analysis type and optional dates: TYPE, FROM, TO
    string analysisStr, fromDate, toDate;
    {
        istringstream hs(header);
        string line;
        while (getline(hs, line)) {
            if (line.rfind("TYPE:", 0) == 0) {
                analysisStr = line.substr(5);
            } else if (line.rfind("FROM:", 0) == 0) {
                fromDate = line.substr(5);
            } else if (line.rfind("TO:",   0) == 0) {
                toDate   = line.substr(3);
            }
        }
    }

    // 4) Determine AnalysisType
    AnalysisType type = AnalysisType::BY_LOG_LEVEL;
    if (analysisStr == "USER")     type = AnalysisType::BY_USER;
    else if (analysisStr == "IP")   type = AnalysisType::BY_IP;

    cout << "[INFO] Analysis=" << analysisStr
              << "  From=" << (fromDate.empty() ? "NONE" : fromDate)
              << "  To="   << (toDate.empty()   ? "NONE" : toDate)
              << "\n";

    // 5) Auto-detect format: JSON, XML, or TXT
    FileType ft = detectFileType(body);
   
    // 6) Filter body by date-range then select appropriate parser
    LogParser* parser = nullptr;
    switch (ft) {
        case FileType::JSON: {
            string filtered = filterJsonByDate(body, fromDate, toDate);
            parser = new JSONParser(filtered);
            break;
        }
        case FileType::TXT: {
            string filtered = filterTxtByDate(body, fromDate, toDate);
            parser = new TXTParser(filtered);
            break;
        }
        case FileType::XML: {
            string filtered = filterXmlByDate(body, fromDate, toDate);
            parser = new XMLParser(filtered);
            break;
        }
    }

    if (!parser) {
        cerr << "[ERROR] Failed to create parser\n";
        close(clientSocket);
        return;
    }

    // 7) Parse and get results
    auto result = parser->parse(type);
    delete parser;

    // 8) Send results back to client
    ostringstream resp;
    if (result.empty()) {
        resp << "[INFO] No entries matched your query.\n";
    } else {
        for (const auto& pair : result) {
            resp << pair.first << ": ";
            resp << pair.second << "\n";
         }
    }
    string out = resp.str();
    send(clientSocket, out.c_str(), out.size(), 0);

    cout << "[INFO] Done, closing connection\n";
    close(clientSocket);
}


int main() {
    // Create listening TCP socket
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1) {
        cerr << "[ERROR] Cannot create socket.\n";
        return 1;
    }

    // Bind to PORT on all interfaces
    sockaddr_in serverAddr{};
    serverAddr.sin_family      = AF_INET;
    serverAddr.sin_port        = htons(PORT);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == -1) {
        cerr << "[ERROR] Bind failed.\n";
        return 1;
    }

    // Start listening for connections
    if (listen(serverSocket, 10) == -1) {
        cerr << "[ERROR] Listen failed.\n";
        return 1;
    }
    cout << "[INFO] Server listening on port " << PORT << "...\n";

    // Main accept loop: spawn a detached thread per client
    while (true) {
        sockaddr_in clientAddr{};
        socklen_t clientLen = sizeof(clientAddr);
        int clientSocket = accept(serverSocket,
                                  (struct sockaddr*)&clientAddr,
                                  &clientLen);
        if (clientSocket == -1) {
            cerr << "[ERROR] Accept failed.\n";
            continue;
        }
        // create a new thread
        thread t(handleClient, clientSocket);
        t.detach();
    }

    // Never reached under normal operation
    close(serverSocket);
    return 0;
}
