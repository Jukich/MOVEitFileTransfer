#include <iostream>
#include <curl/curl.h>
#include <string>
#include <thread>
#include <chrono>
#include <regex>
#include <map>
#include <set>
#include <filesystem>

#define ERROR_UNAUTHORIZED 401
#define ERROR_CONFLICTED 409
#define ERROR_SIZE_LIMIT_EXCEEDED 413

const std::string username = "interview.julien.aleksiev";
const std::string passwd = "3]$t3Z}w";

std::string apiToken = "";
std::string homeFolderID = "";

std::string localPath = "D:\\work";
const int maxRetryCount = 10;

size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* output)
{
    output->append((char*)contents, size * nmemb);
    return size * nmemb;
}

std::map<std::string, std::string> ParseResponse(std::string reply)
{
    //optimally should use a json parser
    std::map<std::string, std::string> responseParams;

    std::regex rgx("\"([^\"]+)\":\s*(.+?)[,}]");
    std::smatch match;

    while (std::regex_search(reply, match, rgx))
    {
        if (match.size() < 3) continue;
        responseParams[match[1].str()] = match[2].str();
        reply = match.suffix();
    }
    return responseParams;
}

void PrintResponse(std::map<std::string, std::string>& responseParams)
{
    for (const auto& param : responseParams)
    {
        std::cout << "\t" << param.first << ": " << param.second << "\n";
    }
}

bool GetAuthorizationToken()
{
    const std::string apiTokenURL = "https://testserver.moveitcloud.com/api/v1/token";
    std::string authData = "grant_type=password&username=" + username + "&password=" + passwd;
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
        std::cout << "Error in retreiving token: " << curl_easy_strerror(res) << "\n";
    }

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &respCode);
    curl_easy_cleanup(curl);

    std::map<std::string, std::string> responseParams = ParseResponse(response);
    bool success = respCode == 200;
    if (!success)
    {
        std::cout << "Error in retreiving token!\n";
        std::cout << "Response Code: " << respCode << "\n";
        PrintResponse(responseParams);
        return false;
    }


    if (responseParams.find("access_token") != responseParams.end())
    {
        apiToken = responseParams["access_token"];

        //Remove quotes
        apiToken.erase(0, 1);
        apiToken.erase(apiToken.size() - 1);

        std::cout << "Authorization token retreived successfully!\n";
        //std::cout <<"API Token = " << apiToken << "\n";
        return true;
    }
    else
    {
        std::cout << "Error: Token not found in response body.\n";
        return false;
    }
}

bool GetUserInformation()
{
    const std::string apiTokenURL = "https://testserver.moveitcloud.com/api/v1/users/self";
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

    std::map<std::string, std::string> responseParams = ParseResponse(response);
    bool success = respCode == 200;
    if (!success)
    {
        std::cout << "Get user details failed\n";
        std::cout << "Response Code: " << respCode << "\n";
        PrintResponse(responseParams);
        return false;
    }

    if (responseParams.find("homeFolderID") != responseParams.end())
    {
        homeFolderID = responseParams["homeFolderID"];
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

    const std::string uploadFileURL = "https://testserver.moveitcloud.com/api/v1/folders/" + homeFolderID + "/files";

    CURL* curl;
    CURLcode res;
    std::string response;
    long respCode;
    struct curl_slist* headers = NULL;

    curl = curl_easy_init();
    if (!curl) return false;

    headers = curl_slist_append(headers, ("Authorization: Bearer " + apiToken + "t").c_str());
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

    std::cout << "Started uploading file \"" << localFilePath << "\"...\n";
    std::cout << "File size = " << fileSize << "\n";

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
        std::cout << "\tResponse Code: " << respCode << "\n";
        std::map<std::string, std::string> responseParams = ParseResponse(response);
        if (respCode == ERROR_SIZE_LIMIT_EXCEEDED)
        {
            std::cout << response << "\n";
        }
        else
        {
            PrintResponse(responseParams);
        }
    }
    else
    {
        std::cout << "File \"" << localFilePath << "\" uploaded successfully!\n";
    }
    responseCode = respCode;
    return success;
}

int main()
{

    if (!std::filesystem::exists(localPath))
    {
        std::cout << "Invalid local path specified\n";
        return 0;
    }

    std::set<std::string> lastExistingFilesSet;

    for (const auto& localFile : std::filesystem::directory_iterator(localPath))
    {
        lastExistingFilesSet.insert(localFile.path().string());
    }

    if (!GetAuthorizationToken())
    {
        return 0;
    }
    if (!GetUserInformation())
    {
        return 0;
    }

    int retryCount = maxRetryCount;

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
                    return 0;
                }

                if (responseCode == ERROR_UNAUTHORIZED)
                {                    
                    if (!GetAuthorizationToken())
                    {
                        return 0;
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
        lastExistingFilesSet = std::move(currentFilesSet);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    return 0;
}

//curl - k --request POST --url https ://your-transfer-server/api/v1/token --data "grant_type=password&username=emmacurtis&password=1a2B3cA1b2C3"
