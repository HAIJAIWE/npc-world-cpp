#pragma once
#include <string>
#include <vector>
#include <functional>
#include <cstdint>

namespace npc {

struct ImageAnalysis {
    std::string description;
    std::vector<std::string> detected_text;
    std::vector<std::string> detected_objects;
    std::string error_info;
    bool has_error = false;
    float confidence = 0.0f;

    std::string toPromptContext() const;
};

struct DocumentParseResult {
    std::string title;
    std::vector<std::string> sections;
    std::string structured_text;
    int page_count = 0;
    int table_count = 0;
};

using VisionCallback = std::function<ImageAnalysis(const std::string& image_path)>;
using DocumentCallback = std::function<DocumentParseResult(const std::string& file_path)>;

class MultimodalPerception {
public:
    static MultimodalPerception& instance();

    void setVisionCallback(VisionCallback cb) { m_vision_cb = std::move(cb); }
    void setDocumentCallback(DocumentCallback cb) { m_doc_cb = std::move(cb); }

    ImageAnalysis analyzeImage(const std::string& image_path);
    DocumentParseResult parseDocument(const std::string& file_path);

    bool hasVisionCapability() const { return !!m_vision_cb; }
    bool hasDocumentCapability() const { return !!m_doc_cb; }

    std::string enhancePrompt(const std::string& prompt, const std::string& image_path = "");

private:
    VisionCallback m_vision_cb;
    DocumentCallback m_doc_cb;
};

} // namespace npc
