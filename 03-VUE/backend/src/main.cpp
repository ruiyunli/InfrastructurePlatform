#include <httplib.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <json.hpp>
#include <random>
#include <filesystem>
#include "users.hpp"
#include "auth.hpp"

using json = nlohmann::json;
namespace fs = std::filesystem;

// 服务器启动时随机选择的背景图
std::string currentBackgroundPath;
std::string currentContentType;

// 获取所有背景图文件
std::vector<std::string> getBackgroundImages() {
    std::vector<std::string> images;
    std::string backgroundsPath = "./backgrounds";

    if (fs::exists(backgroundsPath) && fs::is_directory(backgroundsPath)) {
        for (const auto& entry : fs::directory_iterator(backgroundsPath)) {
            if (entry.is_regular_file()) {
                std::string ext = entry.path().extension().string();
                if (ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".webp") {
                    images.push_back(entry.path().string());
                }
            }
        }
    }
    return images;
}

// 初始化背景图 - 服务器启动时调用一次
void initBackground() {
    auto images = getBackgroundImages();
    if (!images.empty()) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, images.size() - 1);
        int index = dis(gen);

        currentBackgroundPath = images[index];
        std::string ext = fs::path(currentBackgroundPath).extension().string();
        currentContentType = "image/jpeg";
        if (ext == ".png") currentContentType = "image/png";
        else if (ext == ".webp") currentContentType = "image/webp";

        std::cout << "当前背景图: " << currentBackgroundPath << std::endl;
    } else {
        std::cout << "警告: backgrounds 目录没有找到图片文件" << std::endl;
    }
}

int main() {
    // 初始化背景图
    initBackground();

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

    // GET /api/background - 获取背景图接口
    svr.Get("/api/background", [](const httplib::Request& req, httplib::Response& res) {
        if (currentBackgroundPath.empty()) {
            json response = {
                {"success", false},
                {"message", "没有可用的背景图"}
            };
            res.status = 404;
            res.set_content(response.dump(), "application/json");
            return;
        }

        // 读取图片文件
        std::ifstream file(currentBackgroundPath, std::ios::binary);
        if (!file) {
            json response = {
                {"success", false},
                {"message", "读取图片失败"}
            };
            res.status = 500;
            res.set_content(response.dump(), "application/json");
            return;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string imageData = buffer.str();

        res.set_header("Content-Type", currentContentType);
        res.set_content(imageData, currentContentType);
    });

    // 设置静态文件目录用于背景图
    svr.set_mount_point("/backgrounds", "./backgrounds");

    std::cout << "服务器启动在 http://localhost:8080" << std::endl;
    svr.listen("0.0.0.0", 8080);

    return 0;
}
