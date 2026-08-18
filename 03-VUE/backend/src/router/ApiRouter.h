#pragma once
#include "httplib.h"
#include "service/authservice.h"

class ApiRouter {
public:
    ApiRouter(AuthService& auth);
    void registerRoutes(httplib::Server& svr);

private:
    void handleLogin(const httplib::Request& req, httplib::Response& res);
    void handleLogout(const httplib::Request& req, httplib::Response& res);
    void handleVerify(const httplib::Request& req, httplib::Response& res);

private:
    AuthService& auth_;
};
