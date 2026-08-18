#include "httplib.h"
#include "ApiRouter.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

ApiRouter::ApiRouter(AuthService& auth)
    : auth_(auth) {}

void ApiRouter::registerRoutes(httplib::Server& svr) {
    svr.Post("/api/login",  [&](const httplib::Request& req, httplib::Response& res) { handleLogin(req, res); });
    svr.Post("/api/logout", [&](const httplib::Request& req, httplib::Response& res) { handleLogout(req, res); });
    svr.Get ("/api/verify", [&](const httplib::Request& req, httplib::Response& res) { handleVerify(req, res); });
}

void ApiRouter::handleLogin(const httplib::Request& req, httplib::Response& res) {
    try {
        auto body = json::parse(req.body);
        std::string username = body["username"];
        std::string password = body["password"];

        if (auth_.verifyUser(username, password)) {
            std::string token = auth_.generateToken(username);
            res.set_content(json({{"success", true}, {"token", token}, {"username", username}}).dump(), "application/json");
        } else {
            res.status = 401;
            res.set_content(json({{"success", false}, {"message", "用户名或密码错误"}}).dump(), "application/json");
        }
    } catch (const std::exception&) {
        res.status = 400;
        res.set_content(json({{"success", false}, {"message", "请求格式错误"}}).dump(), "application/json");
    }
}

void ApiRouter::handleLogout(const httplib::Request& req, httplib::Response& res) {
    res.set_content(json({{"success", true}, {"message", "注销成功"}}).dump(), "application/json");
}

void ApiRouter::handleVerify(const httplib::Request& req, httplib::Response& res) {
    try {
        std::string auth_header = req.get_header_value("Authorization");
        if (auth_header.empty() || auth_header.substr(0, 7) != "Bearer ") {
            res.status = 401;
            res.set_content(json({{"success", false}, {"message", "未提供token"}}).dump(), "application/json");
            return;
        }

        std::string token = auth_header.substr(7);
        std::string username = auth_.verifyToken(token);

        res.set_content(json({{"success", true}, {"username", username}}).dump(), "application/json");
    } catch (const std::exception&) {
        res.status = 401;
        res.set_content(json({{"success", false}, {"message", "token无效或已过期"}}).dump(), "application/json");
    }
}
