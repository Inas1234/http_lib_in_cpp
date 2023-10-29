#include <iostream>
#include <functional>
#include <map>
#include <string>
#include <sstream>
#include <winsock2.h>
#include <fstream>
#include "HttpLib.h"

int main() {
    HTTPServer server(8080);


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
        response.appendToBody(buffer);
    });

    server.post("/example", [&server](const std::string& request, Response& response) {
        std::string body = "<!DOCTYPE html><html><h1>" +  request + "</h1></html>";
        response.setHeader("Content-Length", std::to_string(body.length()));
        response.setHeader("Content-Type", "text/html");

        response.appendToBody(body);
    });

    server.start();

    return 0;
}
