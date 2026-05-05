#pragma once

#include <string>
#include <vector>

struct SrvUser {
    std::string username;
    std::string password;
};

class AuthService {
public:
    explicit AuthService(const std::string& secret);
    
    // Token管理
    std::string generateToken(const std::string& username);
    std::string verifyToken(const std::string& token) const;
    
    // 用户管理
    bool verifyUser(const std::string& username, const std::string& password);

private:
    std::string jwtSecret_;
    std::vector<SrvUser> users_;
};