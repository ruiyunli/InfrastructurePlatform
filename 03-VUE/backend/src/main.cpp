#include <httplib.h>
#include <iostream>
#include <locale>
#include <clocale>
#include "service/backgroudmanager.h"
#include "service/authservice.h"
#include "router/ApiRouter.h"

#ifdef _WIN32
#include <windows.h>
#endif

// 设置控制台为 UTF-8 编码（跨平台）
void setConsoleUtf8() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#else
    setlocale(LC_ALL, "C.UTF-8");
#endif
}

int main() {
    setConsoleUtf8();

    AuthService authService("my_secret_key_2026", "users.json");

    BackgroundManager bgManager;
    bgManager.initialize();

    httplib::Server svr;

    // 配置CORS中间件
    svr.set_pre_routing_handler([](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "http://localhost:5173");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
        res.set_header("Access-Control-Allow-Credentials", "true");

        if (req.method == "OPTIONS") {
            res.status = 204;
            return httplib::Server::HandlerResponse::Handled;
        }
        return httplib::Server::HandlerResponse::Unhandled;
    });

    ApiRouter apiRouter(authService, bgManager);
    apiRouter.registerRoutes(svr);

    std::cout << "服务器启动在 http://localhost:8080" << std::endl;
    svr.listen("0.0.0.0", 8080);

    return 0;
}
