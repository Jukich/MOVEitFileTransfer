#include <iostream>
#include <curl/curl.h>
#include <string>
#include <thread>
#include <chrono>
#include <regex>
#include <set>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

#define ERROR_UNAUTHORIZED 401
#define ERROR_CONFLICTED 409
#define ERROR_SIZE_LIMIT_EXCEEDED 413
#define DEFAULT_HOST_NAME "https://testserver.moveitcloud.com/"


//config parameters
std::string username = "";
std::string password = "";
std::string localPath = "";
std::string hostname = "";
int maxRetryCount = INT_MAX;

//current user properties
std::string apiToken = "";
std::string homeFolderID = "";

size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* output)
{
    output->append((char*)contents, size * nmemb);
    return size * nmemb;
}

bool GetAuthorizationToken()
{
    const std::string apiTokenURL = hostname + "api/v1/token";
    std::string authData = "grant_type=password&username=" + username + "&password=" + password;
    std::string response;
    long respCode;

    CURL* curl = curl_easy_init();
    if (!curl) return false;

    curl_easy_setopt(curl, CURLOPT_URL, apiTokenURL.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, authData.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);

    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK)
    {
        std::cout << "Error in retreiving API token: " << curl_easy_strerror(res) << "\n";
    }

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &respCode);
    curl_easy_cleanup(curl);

    nlohmann::json responseParams;
    try 
    {
        responseParams = nlohmann::json::parse(response);
    }
    catch (nlohmann::json::parse_error& e) 
    {
        std::cout << "Response Code: " << respCode << "\n";
        std::cout << "Failed in retreiving API token. Error in parsing repsonse body.\n";
        return false;
    }
    
    bool success = respCode == 200;
    if (!success)
    {        
        std::cout << "Error in retreiving API token!\n";
        std::cout << "Response Code: " << respCode << "\n";
        std::cout << responseParams.dump(4) << "\n";
        return false;
    }


    if (responseParams.contains("access_token"))
    {
        apiToken = responseParams["access_token"];
        std::cout << "Authorization token retreived successfully!\n";
        //std::cout <<"API Token = " << apiToken << "\n";
        return true;
    }
    else
    {
        std::cout << "Error: API token not found in response body.\n";
        return false;
    }
}

bool GetUserInformation()
{
    const std::string apiTokenURL = hostname + "api/v1/users/self";
    std::string response;
    long respCode;

    struct curl_slist* headers = NULL;

    CURL* curl = curl_easy_init();
    if (!curl) return false;

    headers = curl_slist_append(headers, ("Authorization: Bearer " + apiToken).c_str());
    curl_easy_setopt(curl, CURLOPT_URL, apiTokenURL.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK)
    {
        std::cout << "Error in get user details: " << curl_easy_strerror(res) << "\n";
    }

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &respCode);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    nlohmann::json responseParams;
    try
    {
        responseParams = nlohmann::json::parse(response);
    }
    catch (nlohmann::json::parse_error& e)
    {
        std::cout << "Response Code: " << respCode << "\n";
        std::cout << "Failed in retreiving homeFolderID. Error in parsing repsonse body.\n";
        return false;
    }

    bool success = respCode == 200;
    if (!success)
    {
        std::cout << "Get user information failed\n";
        std::cout << "Response Code: " << respCode << "\n";
        std::cout << responseParams.dump(4) << "\n";
        return false;
    }

    if (responseParams.contains("homeFolderID"))
    {
        //homeFolderID = std::to_string(responseParams["homeFolderID"].get<int>());
        homeFolderID = responseParams["homeFolderID"].dump();
        std::cout << "Home Folder ID retreived successfully!\n";
        //std::cout << "Home folder ID = " << homeFolderID << "\n";
        return true;

    }
    else
    {
        std::cout << "Error: homeFolderID not found in response body.\n";
        return false;
    }


}

bool UploadFile(std::string localFilePath, uintmax_t fileSize, int& responseCode)
{
    if (apiToken == "" || homeFolderID == "")
    {
        return false;
    }

    if (!std::filesystem::exists(localFilePath))
    {
        std::cout << "Invalid local file path specified\n";
        return false;
    }

    const std::string uploadFileURL = hostname + "api/v1/folders/" + homeFolderID + "/files";

    CURL* curl;
    CURLcode res;
    std::string response;
    long respCode;
    struct curl_slist* headers = NULL;

    curl = curl_easy_init();
    if (!curl) return false;

    headers = curl_slist_append(headers, ("Authorization: Bearer " + apiToken).c_str());
    curl_mime* mime = curl_mime_init(curl);
    curl_mimepart* part = curl_mime_addpart(mime);
    curl_mime_name(part, "file");
    curl_mime_filedata(part, localFilePath.c_str());

    curl_easy_setopt(curl, CURLOPT_URL, uploadFileURL.c_str());
    curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, (curl_off_t)fileSize);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);

    std::cout << "Started uploading file \"" << localFilePath << "\"\n";
    //std::cout << "File size = " << fileSize / 1024 << "KB\n";
    std::cout << "Please wait...\n";

    res = curl_easy_perform(curl);
    if (res != CURLE_OK)
    {
        std::cout << "Upload failed: " << curl_easy_strerror(res) << "\n";
    }

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &respCode);

    curl_slist_free_all(headers);
    curl_mime_free(mime);
    curl_easy_cleanup(curl);

    bool success = respCode == 200 || respCode == 201;
    if (!success)
    {
        std::cout << "Upload file failed!\n";
        std::cout << "Response Code: " << respCode << "\n";
        try 
        {
            nlohmann::json responseParams = nlohmann::json::parse(response);
            std::cout << responseParams.dump(4) << "\n";
        }
        catch (nlohmann::json::parse_error& e) 
        {
            std::cout << response << "\n";
        }
    }
    else
    {
        std::cout << "File \"" << localFilePath << "\" uploaded successfully!\n";
    }
    responseCode = respCode;
    return success;
}

bool ParseConfig()
{
    wchar_t currentPath[MAX_PATH];
    GetModuleFileNameW(NULL, currentPath, MAX_PATH);
    std::string cfgFilePath = std::filesystem::path(currentPath).parent_path().string() + "\\config.txt";

    if (!std::filesystem::exists(cfgFilePath))
    {
        std::cout << "Invalid config path. Please put the \"config.txt\" file in the same directory as the \".exe\".\n";
        return false;
    }

    std::ifstream localFileStream(cfgFilePath);
    std::string line;

    std::regex rgx(R"(([a-zA-Z0-9_]+)\s*=\s*\"?([^\"\s]+)\"?)");
    std::smatch match;

    while (std::getline(localFileStream, line))
    {
        if (!std::regex_search(line, match, rgx))
        {
            continue;
        }

        std::string prop = match[1];
        std::string value = match[2];

        if (prop == "username")
        {
            username = value;
        }
        else if (prop == "password")
        {
            password = value;
        }
        else if (prop == "local_path")
        {
            localPath = value;
        }
        else if (prop == "hostname")
        {
            hostname = value;
        }
        else if (prop == "max_retry_count")
        {
            try 
            {
                maxRetryCount = std::stoi(value);
            }
            catch (std::exception& e) 
            {
                std::cout << "Invalid max_retry_value specified in config! Using default value (infinite)." << '\n';
            }
        }
    }

    if (username == "" || password == "" || localPath == "")
    {
        std::cout << "Invalid credentials! Please check \"config.txt\".\n";
        return false;
    }
    if (hostname == "") 
    {
        std::cout << std::string("\"hostname\" not specified in config. Using default host name: \"") + DEFAULT_HOST_NAME + "\"\n";
        hostname = DEFAULT_HOST_NAME;
    }
    std::cout << "Config parameters parsed successfully!\n";

    return true;
}

void CheckFiles() 
{

    if (!GetAuthorizationToken() || !GetUserInformation())
    {
        return;
    }

    int waitMsgCount = 0;
    int retryCount = maxRetryCount;
    std::set<std::string> lastExistingFilesSet;

    for (const auto& localFile : std::filesystem::directory_iterator(localPath))
    {
        lastExistingFilesSet.insert(localFile.path().string());
    }

    while (true)
    {
        std::set<std::string> currentFilesSet;
        std::vector<std::string> newFiles;

        for (const auto& localFile : std::filesystem::directory_iterator(localPath))
        {
            std::string filePath = localFile.path().string();

            currentFilesSet.insert(filePath);

            if (lastExistingFilesSet.find(filePath) == lastExistingFilesSet.end())
            {
                newFiles.push_back(filePath);
            }
        }

        for (const std::string& newFile : newFiles)
        {
            std::cout << "\r";
            int responseCode = 0;
            const uintmax_t fileSize = std::filesystem::file_size(newFile);
            if (UploadFile(newFile, fileSize, responseCode))
            {
                retryCount = maxRetryCount;
            }
            else
            {
                retryCount--;
                if (retryCount == 0)
                {
                    std::cout << "Max retry count reached.\nShutting down...\n";
                    return;
                }

                if (responseCode == ERROR_UNAUTHORIZED)
                {
                    if (!GetAuthorizationToken())
                    {
                        return;
                    }
                }
                else if (responseCode == ERROR_CONFLICTED)
                {
                    //File cannot be uploaded, ignore it.
                    //Think for possible way to replace existing file
                    retryCount = maxRetryCount;
                    continue;
                }
                else if (responseCode == ERROR_SIZE_LIMIT_EXCEEDED)
                {
                    //File cannot be uploaded, ignore it.
                    retryCount = maxRetryCount;
                    continue;
                }
                //Remove file from the set as it is not uploaded yet
                currentFilesSet.erase(newFile);
            }
        }

        std::cout << "\rWaiting for a new file" << std::string(waitMsgCount, '.') << "   " << std::flush;
        waitMsgCount = (waitMsgCount + 1) % 4;


        lastExistingFilesSet = std::move(currentFilesSet);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}
int main(int argc, char* argv[])
{    
    if (!ParseConfig())
    {
        system("pause");
        return 0;
    }
    if (!std::filesystem::exists(localPath))
    {
        std::cout << "Invalid local path specified! Path \"" << localPath << "\" does not exist.\n";
        system("pause");
        return 0;
    }   

    CheckFiles();
  
    system("pause");
    return 0;
}