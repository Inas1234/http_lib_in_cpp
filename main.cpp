#include <iostream>
#include <functional>
#include <map>
#include <string>
#include <sstream>
#include <winsock2.h>

#pragma comment(lib, "ws2_32.lib")


class Response {
    std::stringstream data;

public:
    Response() {
        data << "HTTP/1.1 200 OK\r\n";
        data << "Content-Type: text/plain\r\n";
    }

    void setHeader(const std::string& header, const std::string& value) {
        data << header << ": " << value << "\r\n";
    }

    void append(const std::string& content) {
        data << "\r\n" << content;
    }

    std::string str() const {
        return data.str();
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

            char buffer[2048];
            int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
            if (bytesReceived > 0) {
                buffer[bytesReceived] = '\0';
                std::cout << "Received:\n" << buffer << std::endl;

                // Parse HTTP request here to determine method, route, etc.

                std::string request(buffer);

                std::string method;
                std::string route;
                std::string version;

                std::stringstream ss(request);

                ss >> method;
                ss >> route;
                ss >> version;

                std::cout << "Method: " << method << std::endl;
                std::cout << "Route: " << route << std::endl;
                std::cout << "Version: " << version << std::endl;



                std::string responseStr;

                if (method == "GET") {
                    auto iter = getHandlers.find(route);
                    if (iter != getHandlers.end()) {
                        Response response;
                        iter->second(buffer, response);
                        responseStr = response.str();
                        send(clientSocket, responseStr.c_str(), responseStr.length(), 0);
                    }
                }
                else if (method == "POST") {
                    auto iter = postHandlers.find(route);
                    if (iter != postHandlers.end()) {
                        Response response;
                        iter->second(buffer, response);
                        responseStr = response.str();
                        send(clientSocket, responseStr.c_str(), responseStr.length(), 0);
                    }
                }
                else {
                    std::cout << "Invalid method: " << method << std::endl;
                }


                closesocket(clientSocket);
            }
        }

        closesocket(listenSocket);
        WSACleanup();
    }

private:
    int port;
    std::map<std::string, RequestHandler> getHandlers;
    std::map<std::string, RequestHandler> postHandlers;
};

int main() {
    HTTPServer server(8080);

    server.post("/example", [&server](const std::string& request, Response& response) {
        std::string data = server.extractDataFromBody(request);

        response.append(data);
        std::cout << "Handled POST request: " << request << std::endl;

        std::cout << "Data: " << data << std::endl;

    });

    server.start();

    return 0;
}
