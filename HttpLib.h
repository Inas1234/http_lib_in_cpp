#ifndef HTTPLIB_H
#define HTTPLIB_H

#include <iostream>
#include <functional>
#include <map>
#include <string>
#include <sstream>
#include <winsock2.h>
#include <fstream>
#include <vector>

#pragma comment(lib, "ws2_32.lib")


class Response {
    std::stringstream headers;
    std::stringstream body;

public:
    Response() {
        headers << "HTTP/1.1 200 OK\r\n";
        headers << "Content-Type: text/plain\r\n";
    }

    void setHeader(const std::string& header, const std::string& value) {
        headers << header << ": " << value << "\r\n";
    }

    void setBody(const std::string& content) {
        body.str(""); // clear any existing content
        body << content;
    }

    void appendToBody(const std::string& content) {
        body << content;
    }

    const std::string str() const {
        std::stringstream fullResponse;
        fullResponse << headers.str();
        fullResponse << "Content-Length: " << body.str().length() << "\r\n"; // Append Content-Length
        fullResponse << "\r\n"; // Separate headers from body
        fullResponse << body.str();
        return fullResponse.str();
    }



};


class HTTPServer {
public:
    using RequestHandler = std::function<void(const std::string& request, Response& response)>;

    HTTPServer(int port) : port(port) {}

    void get(const std::string& route, RequestHandler handler) {
        getHandlers[route] = handler;
    }

    void post(const std::string& route, RequestHandler handler) {
        postHandlers[route] = handler;
    }

    std::string extractDataFromBody(const std::string& httpRequest) {
        const std::string delimiter = "\r\n\r\n";
        size_t pos = httpRequest.find(delimiter);

        if (pos != std::string::npos) {
            return httpRequest.substr(pos + delimiter.size());
        } else {
            return ""; // or return some error indication if preferred
        }
    }



    void start() {
        WSADATA wsaData;
        int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (result != 0) {
            std::cerr << "WSAStartup failed with error: " << result << std::endl;
            return;
        }

        SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (listenSocket == INVALID_SOCKET) {
            std::cerr << "Socket creation failed with error: " << WSAGetLastError() << std::endl;
            WSACleanup();
            return;
        }

        sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(port);
        serverAddr.sin_addr.s_addr = INADDR_ANY;

        if (bind(listenSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
            std::cerr << "Bind failed with error: " << WSAGetLastError() << std::endl;
            closesocket(listenSocket);
            WSACleanup();
            return;
        }

        if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
            std::cerr << "Listen failed with error: " << WSAGetLastError() << std::endl;
            closesocket(listenSocket);
            WSACleanup();
            return;
        }

        std::cout << "Server is running on port " << port << "..." << std::endl;

        while (true) {
            SOCKET clientSocket = accept(listenSocket, NULL, NULL);
            if (clientSocket == INVALID_SOCKET) {
                std::cerr << "Accept failed with error: " << WSAGetLastError() << std::endl;
                closesocket(listenSocket);
                WSACleanup();
                return;
            }
            std::vector<char> buffer(2048);

           do{
                std::string fullRequest = readFromSocket(clientSocket, buffer);
               
                std::cout << "Received request: " << std::endl << fullRequest << std::endl;
                std::string request(fullRequest);
                std::string method;
                std::string route;
                std::string version;
                std::stringstream ss(request);
                std::string line;
                std::getline(ss, line);

                std::stringstream lineStream(line);
                lineStream >> method >> route >> version;

                if (method != "GET" && method != "POST") {
                    std::cerr << "Unexpected method received: " << method << std::endl;
                    return;
                }

                std::cout << "Method: " << method << std::endl;
                std::cout << "Route: " << route << std::endl;
                std::cout << "Version: " << version << std::endl;
                std::string responseStr;
                if (method == "GET") {
                    auto iter = getHandlers.find(route);
                    if (iter != getHandlers.end()) {
                        Response response;
                        iter->second(buffer.data(), response);
                        responseStr = response.str();
                        send(clientSocket, responseStr.c_str(), responseStr.length(), 0);
                    }
                }
                else if (method == "POST") {
                    auto iter = postHandlers.find(route);
                    if (iter != postHandlers.end()) {
                        std::string contentLengthHeader = "Content-Length: ";
                        size_t contentLengthPos = fullRequest.find(contentLengthHeader);
                        if (contentLengthPos != std::string::npos) {
                            size_t endPos = fullRequest.find("\r\n", contentLengthPos);
                            std::string contentLengthStr = fullRequest.substr(contentLengthPos + contentLengthHeader.length(), endPos - contentLengthPos - contentLengthHeader.length());
                            int contentLength = std::stoi(contentLengthStr);
                            std::cout << "Content-Length: " << contentLength << std::endl;
                            
                            if(contentLength > 0) {
                                // Assuming postDataBuffer is a char array with appropriate size.
                                char postDataBuffer[contentLength + 1];  // +1 for the null-terminator
                                memset(postDataBuffer, 0, sizeof(postDataBuffer)); // Initialize the buffer
                                
                                int totalBytesRead = 0;
                                while (totalBytesRead < contentLength) {
                                    int bytesRead = recv(clientSocket, postDataBuffer + totalBytesRead, contentLength - totalBytesRead, 0);
                                    if (bytesRead <= 0) {
                                        perror("Error or connection closed during recv");
                                        break;
                                    }
                                    totalBytesRead += bytesRead;
                                }
                                std::cout << "Post data: " << postDataBuffer << std::endl;

                                if(totalBytesRead != contentLength) {
                                    std::cout << "Warning: Not all bytes read. Expected " << contentLength << ", but read " << totalBytesRead << std::endl;
                                }
                                Response response;
                                iter->second(postDataBuffer, response);
                                std::cout << "Iter route (key): " << iter->first << std::endl;
                                responseStr = response.str();
                                std::cout << "RESPONSE: " << responseStr << std::endl;  

                                int bytesSent = 0;
                                while (bytesSent < responseStr.length()) {
                                    int result = send(clientSocket, responseStr.c_str() + bytesSent, responseStr.length() - bytesSent, 0);
                                    if (result <= 0) {
                                        std::cerr << "Failed to send data or connection closed." << std::endl;
                                        break;
                                    }
                                    bytesSent += result;
                                }

                            }
                            

                        }
                        
                    }
                }
                else {
                    std::cout << "Invalid method: " << method << std::endl;
                }

           }while(handleKeepAlive(clientSocket, buffer));
   
            closesocket(clientSocket);

        }

        closesocket(listenSocket);
        WSACleanup();
    }

private:
    int port;
    std::map<std::string, RequestHandler> getHandlers;
    std::map<std::string, RequestHandler> postHandlers;

    std::string readFromSocket(SOCKET socket, std::vector<char>& buffer) {
        std::string fullRequest;
        int bytesReceived;
        do {
            bytesReceived = recv(socket, buffer.data(), buffer.size() - 1, 0);
            if (bytesReceived > 0) {
                buffer[bytesReceived] = '\0';
                fullRequest += std::string(buffer.data());
            }
        } while (bytesReceived == buffer.size() - 1);
        return fullRequest;
    }

    bool handleKeepAlive(SOCKET socket, std::vector<char>& buffer) {
        u_long nonBlocking = 1;
        if (ioctlsocket(socket, FIONBIO, &nonBlocking) != 0) { // Set non-blocking
            std::cerr << "Error setting socket to non-blocking mode." << std::endl;
            return false;
        }

        int n = recv(socket, buffer.data(), buffer.size() - 1, MSG_PEEK);
        return n > 0;  
    }


};

#endif // HTTPLIB_H