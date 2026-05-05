#pragma once

#include <string>

class BackgroundManager
{
public:
    BackgroundManager() = default;
    ~BackgroundManager() = default;

public:
    void initialize();
    bool serveImage(std::string& data, std::string& contentType) const;

private:
    std::string m_imageData;
    std::string m_contentType;
};