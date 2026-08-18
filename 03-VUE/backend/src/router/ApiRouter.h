#pragma once
#include "httplib.h"
#include "service/authservice.h"
#include "service/backgroudmanager.h"

class ApiRouter {
public:
    ApiRouter(AuthService& auth, BackgroundManager& bg);
    void registerRoutes(httplib::Server& svr);

private:
    void handleLogin(const httplib::Request& req, httplib::Response& res);
    void handleLogout(const httplib::Request& req, httplib::Response& res);
    void handleVerify(const httplib::Request& req, httplib::Response& res);
    void handleBackground(const httplib::Request& req, httplib::Response& res);

private:
    AuthService& auth_;
    BackgroundManager& bgManager_;
};
