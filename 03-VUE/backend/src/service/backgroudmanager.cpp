#include "backgroudmanager.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <filesystem>
#include <random>

namespace fs = std::filesystem;

void BackgroundManager::initialize() {
    std::vector<std::string> images;
    std::string backgroundsPath = "./backgrounds";

    // 遍历获取所有图片文件名
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

    if (!images.empty()) {
        // 随机选一个索引
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, images.size() - 1);
        int index = dis(gen);

        std::string selectedPath = images[index];

        // 确定 contentType
        std::string ext = fs::path(selectedPath).extension().string();
        m_contentType = "image/jpeg";
        if (ext == ".png") m_contentType = "image/png";
        else if (ext == ".webp") m_contentType = "image/webp";

        // 加载图片内容到内存
        std::ifstream file(selectedPath, std::ios::binary);
        if (file) {
            std::stringstream buffer;
            buffer << file.rdbuf();
            m_imageData = buffer.str();
            std::cout << "当前背景图: " << selectedPath << std::endl;
        } else {
            std::cout << "警告: 读取图片失败: " << selectedPath << std::endl;
        }
    } else {
        std::cout << "警告: backgrounds 目录没有找到图片文件" << std::endl;
    }
}

bool BackgroundManager::serveImage(std::string& data, std::string& contentType) const {
    if (m_imageData.empty()) {
        return false;
    }

    data = m_imageData;
    contentType = m_contentType;
    return true;
}
