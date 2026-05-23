#include "engine/multimodal_perception.h"
#include <sstream>
#include <algorithm>

namespace npc {

MultimodalPerception& MultimodalPerception::instance() {
    static MultimodalPerception inst;
    return inst;
}

std::string ImageAnalysis::toPromptContext() const {
    std::ostringstream oss;
    oss << "[图片分析]\n";
    if (!description.empty()) oss << "描述: " << description << "\n";
    if (has_error) oss << "检测到错误: " << error_info << "\n";
    if (!detected_text.empty()) {
        oss << "识别文字:\n";
        for (const auto& t : detected_text) oss << "  - " << t << "\n";
    }
    if (!detected_objects.empty()) {
        oss << "检测对象: ";
        for (size_t i = 0; i < detected_objects.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << detected_objects[i];
        }
        oss << "\n";
    }
    oss << "置信度: " << (int)(confidence * 100) << "%\n";
    return oss.str();
}

ImageAnalysis MultimodalPerception::analyzeImage(const std::string& image_path) {
    if (m_vision_cb) {
        return m_vision_cb(image_path);
    }

    ImageAnalysis fallback;
    fallback.description = "未配置视觉模型回调。图片路径: " + image_path;
    fallback.confidence = 0.0f;

    auto pos = image_path.find_last_of("/\\");
    std::string filename = (pos != std::string::npos) ? image_path.substr(pos + 1) : image_path;

    if (filename.find("error") != std::string::npos) {
        fallback.has_error = true;
        fallback.error_info = "文件名暗示含有错误截图";
    }

    return fallback;
}

DocumentParseResult MultimodalPerception::parseDocument(const std::string& file_path) {
    if (m_doc_cb) {
        return m_doc_cb(file_path);
    }

    DocumentParseResult fallback;
    fallback.title = "未配置文档解析回调";
    fallback.structured_text = "文件: " + file_path;
    return fallback;
}

std::string MultimodalPerception::enhancePrompt(const std::string& prompt,
                                                  const std::string& image_path) {
    if (image_path.empty()) return prompt;
    auto analysis = analyzeImage(image_path);
    return analysis.toPromptContext() + "\n\n---\n" + prompt;
}

} // namespace npc
