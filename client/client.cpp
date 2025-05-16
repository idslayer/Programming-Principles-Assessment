// File: client/client.cpp

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <filesystem>
#include <regex>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#define BUFFER_SIZE 8192

using namespace std;
namespace fs = filesystem;

// Read entire file into a string
bool readFile(const string& path, string& out) {
    ifstream ifs(path, ios::in | ios::binary);
    if (!ifs.is_open()) return false;
    ostringstream oss;
    oss << ifs.rdbuf();
    out = oss.str();
    return true;
}

// Send a single payload and print the result
void sendAndReceive(const string& serverIp, int serverPort,
                    const string& payload, const string& filename) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        cerr << "[ERROR] Failed to create socket for file " << filename << "\n";
        return;
    }
    sockaddr_in servAddr{};
    servAddr.sin_family = AF_INET;
    servAddr.sin_port   = htons(serverPort);
    if (inet_pton(AF_INET, serverIp.c_str(), &servAddr.sin_addr) <= 0) {
        cerr << "[ERROR] Invalid server IP: " << serverIp << " for file " << filename << "\n";
        close(sock);
        return;
    }
    if (connect(sock, (sockaddr*)&servAddr, sizeof(servAddr)) < 0) {
        cerr << "[ERROR] Connection to server failed for file " << filename << "\n";
        close(sock);
        return;
    }
    
    // Send payload
    ssize_t sent = send(sock, payload.c_str(), payload.size(), 0);
    if (sent != static_cast<ssize_t>(payload.size())) {
        cerr << "[ERROR] Only sent " << sent << " of " << payload.size()
                  << " bytes for file " << filename << "\n";
        close(sock);
        return;
    }
    shutdown(sock, SHUT_WR);  // signal EOF

    // Receive and print
    cout << "\n=== Analysis Result for " << filename << " ===\n";
    char buffer[BUFFER_SIZE];
    ssize_t received;
    while ((received = recv(sock, buffer, BUFFER_SIZE - 1, 0)) > 0) {
        buffer[received] = '\0';
        cout << buffer;
    }
    cout << "=== End of " << filename << " ===\n";
    close(sock);
}

int main() {
    string serverIp;
    int         serverPort;
    string analysis;    // USER | IP | LOG_LEVEL
    string fromDate;    // YYYY-MM-DD
    string toDate;      // YYYY-MM-DD
    string dirPath;

     // Interactive input
    cout << "Server IP (e.g. 127.0.0.1): ";
    getline(cin, serverIp);

    cout << "Server Port (e.g. 8080): ";
    string portStr;
    getline(cin, portStr);
    serverPort = stoi(portStr);

    cout << "Analysis type (USER, IP, or LOG_LEVEL): ";
    getline(cin, analysis);

    cout << "From date (YYYY-MM-DD) [leave blank for none]: ";
    getline(cin, fromDate);

    // Validate fromDate format
    if (!fromDate.empty()) {
        regex dateRegex(R"(^\d{4}-\d{2}-\d{2}$)");
        if (!regex_match(fromDate, dateRegex)) {
            cerr << "[ERROR] Invalid From date format. Expected YYYY-MM-DD\n";
            return 1;
        }
    }

    cout << "To date (YYYY-MM-DD) [leave blank for none]: ";
    getline(cin, toDate);

    // Validate toDate format
    if (!toDate.empty()) {
        regex dateRegex(R"(^\d{4}-\d{2}-\d{2}$)");
        if (!regex_match(toDate, dateRegex)) {
            cerr << "[ERROR] Invalid To date format. Expected YYYY-MM-DD\n";
            return 1;
        }
    }

    cout << "Log folder path: ";
    getline(cin, dirPath);

    // Check directory exists and not empty
    if (!fs::exists(dirPath) || !fs::is_directory(dirPath)) {
        cerr << "[ERROR] Log folder does not exist: " << dirPath << "\n";
        return 1;
    }
    // Iterate over log files in directory
    size_t fileCount = 0;
    for (auto& entry : fs::directory_iterator(dirPath)) {
        if (!entry.is_regular_file()) continue;
        string path = entry.path().string();
        string ext  = entry.path().extension().string();
        if (ext == ".json" || ext == ".xml" || ext == ".txt") {
            ++fileCount;
            // Read file content
            string fileContent;
            if (!readFile(path, fileContent)) {
                cerr << "[ERROR] Cannot open log file: " << path << "\n";
                continue;
            }

            // Build payload header + body
            ostringstream msg;
            msg << "TYPE:" << analysis << "\n";
            if (!fromDate.empty()) msg << "FROM:" << fromDate << "\n";
            if (!toDate.empty())   msg << "TO:"   << toDate   << "\n";
            msg << "\n";
            msg << fileContent;

            // Send and receive for this file
            sendAndReceive(serverIp, serverPort, msg.str(), entry.path().filename().string());
        }
    }
    if (fileCount == 0) {
        cerr << "[ERROR] No log files (.json, .xml, .txt) found in folder: " << dirPath << "\n";
        return 1;
    }

    return 0;
}
