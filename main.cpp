#include <iostream>
#include <functional>
#include <map>
#include <string>
#include <sstream>
#include <winsock2.h>
#include <fstream>

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

            std::string fullRequest;
            char buffer[2048];
            int bytesReceived;
            do {
                bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
                if (bytesReceived > 0) {
                    buffer[bytesReceived] = '\0';
                    fullRequest += buffer;
                }
            } while (bytesReceived == sizeof(buffer) - 1); 
            std::cout << "Received:\n" << fullRequest << std::endl;
            std::string request(fullRequest);
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
            // Check for Connection: keep-alive header
            // if (fullRequest.find("Connection: keep-alive") != std::string::npos) {
            //     // If keep-alive, don't close the socket yet. Instead, go back to reading from this client.
            //     continue;
            // }

            closesocket(clientSocket);
            
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
    #include <fstream>


    server.get("/example", [&server](const std::string& request, Response& response) {
        std::string buffer = "";
        {
            std::ifstream file("index.html");
            if (file.is_open()) {
                std::stringstream ss;
                ss << file.rdbuf();
                buffer = ss.str();
                file.close();
            }
        }
        response.setHeader("Content-Length", std::to_string(buffer.length()));
        response.setHeader("Content-Type", "text/html");
        response.append(buffer);
        std::cout << "Handled GET request: " << request << std::endl;
    });

    server.post("/example", [&server](const std::string& request, Response& response) {
        std::string data = server.extractDataFromBody(request);
        response.setHeader("Content-Type", "text/html");
        response.setHeader("Connection", "close");
        response.append("<h1>" +  data + "</h1>");
        std::cout << "Handled POST request: " << request << std::endl;
        std::cout << "Data: " << data << std::endl;
    });

    server.start();

    return 0;
}
