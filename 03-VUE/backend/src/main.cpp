#include <httplib.h>
#include <iostream>
#include <json.hpp>
#include "users.hpp"
#include "auth.hpp"

using json = nlohmann::json;

int main() {
    httplib::Server svr;

    // 配置CORS中间件
    svr.set_pre_routing_handler([](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "http://localhost:5173");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
        res.set_header("Access-Control-Allow-Credentials", "true");

        // 处理OPTIONS预检请求
        if (req.method == "OPTIONS") {
            res.status = 204;
            return httplib::Server::HandlerResponse::Handled;
        }
        return httplib::Server::HandlerResponse::Unhandled;
    });

    // POST /api/login - 登录接口
    svr.Post("/api/login", [](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string username = body["username"];
            std::string password = body["password"];

            auto user = findUser(username, password);
            if (user.has_value()) {
                std::string token = generateToken(username);
                json response = {
                    {"success", true},
                    {"token", token},
                    {"username", username}
                };
                res.set_content(response.dump(), "application/json");
            } else {
                json response = {
                    {"success", false},
                    {"message", "用户名或密码错误"}
                };
                res.status = 401;
                res.set_content(response.dump(), "application/json");
            }
        } catch (const std::exception& e) {
            json response = {
                {"success", false},
                {"message", "请求格式错误"}
            };
            res.status = 400;
            res.set_content(response.dump(), "application/json");
        }
    });

    // POST /api/logout - 注销接口
    svr.Post("/api/logout", [](const httplib::Request& req, httplib::Response& res) {
        json response = {
            {"success", true},
            {"message", "注销成功"}
        };
        res.set_content(response.dump(), "application/json");
    });

    // GET /api/verify - 验证token接口
    svr.Get("/api/verify", [](const httplib::Request& req, httplib::Response& res) {
        try {
            std::string auth_header = req.get_header_value("Authorization");
            if (auth_header.empty() || auth_header.substr(0, 7) != "Bearer ") {
                json response = {
                    {"success", false},
                    {"message", "未提供token"}
                };
                res.status = 401;
                res.set_content(response.dump(), "application/json");
                return;
            }

            std::string token = auth_header.substr(7);
            std::string username = verifyToken(token);

            json response = {
                {"success", true},
                {"username", username}
            };
            res.set_content(response.dump(), "application/json");
        } catch (const std::exception& e) {
            json response = {
                {"success", false},
                {"message", "token无效或已过期"}
            };
            res.status = 401;
            res.set_content(response.dump(), "application/json");
        }
    });

    std::cout << "服务器启动在 http://localhost:8080" << std::endl;
    svr.listen("0.0.0.0", 8080);

    return 0;
}
