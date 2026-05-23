#include "training_logger.h"
#include <algorithm>
#include <sstream>
#include <cstdio>
#include <chrono>
#include <unordered_map>

void TrainingDataLogger::log(const TrainingRecord& record) {
    records_.push_back(record);

    if (records_.size() > MAX_RECORDS) {
        records_.erase(records_.begin(),
                       records_.begin() + (records_.size() - MAX_RECORDS));
    }

    printf("[训练数据] %s: \"%s\" (%s) | 总记录: %zu\n",
           record.npc_name.c_str(),
           record.output.spoken_text.substr(0, std::min<size_t>(40, record.output.spoken_text.size())).c_str(),
           record.output.local_or_llm.c_str(),
           records_.size());
}

std::vector<TrainingRecord> TrainingDataLogger::get_records(const std::string& npc_id) const {
    if (npc_id.empty()) return records_;

    std::vector<TrainingRecord> result;
    for (const auto& r : records_) {
        if (r.npc_id == npc_id) result.push_back(r);
    }
    return result;
}

TrainingStats TrainingDataLogger::get_stats() const {
    TrainingStats s;
    s.total = static_cast<int>(records_.size());

    std::unordered_map<std::string, int> by_npc;
    int llm = 0, local = 0;
    for (const auto& r : records_) {
        by_npc[r.npc_name]++;
        if (r.output.local_or_llm == "llm") llm++;
        else if (r.output.local_or_llm == "local") local++;
    }
    s.by_npc = by_npc;
    s.llm_count = llm;
    s.local_count = local;
    s.llm_rate = s.total > 0 ? static_cast<double>(llm) / s.total : 0.0;
    s.local_hit_rate = s.total > 0 ? static_cast<double>(local) / s.total : 0.0;
    return s;
}

std::string TrainingDataLogger::export_jsonl() const {
    std::string result;
    for (const auto& r : records_) {
        std::string line = "{";
        line += "\"instruction\":\"你是" + r.npc_name
             + "。当前情绪：" + r.state.immediate_emotion_type
             + "(强度" + std::to_string(static_cast<int>(r.state.immediate_emotion_intensity))
             + ")。心境：" + r.state.background_mood_label
             + "。精力：" + std::to_string(static_cast<int>(r.state.mental_energy)) + "/100。";
        if (!r.context.recent_dialog.empty()) {
            line += "对话上下文：";
            for (size_t i = 0; i < r.context.recent_dialog.size(); i++) {
                if (i > 0) line += " | ";
                line += r.context.recent_dialog[i];
            }
        }
        line += "\",";
        line += "\"input\":\"" + r.context.situation + "\",";
        line += "\"output\":\"" + r.output.spoken_text + "\",";
        line += "\"metadata\":{\"npcId\":\"" + r.npc_id
             + "\",\"timestamp\":" + std::to_string(r.timestamp)
             + ",\"mood\":{\"type\":\"" + r.state.immediate_emotion_type
             + "\",\"intensity\":" + std::to_string(static_cast<int>(r.state.immediate_emotion_intensity))
             + "},\"system1\":\"" + r.system1.gut_feeling
             + "\",\"system2\":\"" + (r.system2.should_speak ? "说话" : "不说话") + "\"}";
        line += "}\n";
        result += line;
    }
    return result;
}

std::string TrainingDataLogger::export_json() const {
    std::string result = "[\n";
    for (size_t i = 0; i < records_.size(); i++) {
        const auto& r = records_[i];
        if (i > 0) result += ",\n";
        result += "  {";
        result += "\"id\":\"" + r.id + "\",";
        result += "\"npcId\":\"" + r.npc_id + "\",";
        result += "\"npcName\":\"" + r.npc_name + "\",";
        result += "\"timestamp\":" + std::to_string(r.timestamp) + ",";
        result += "\"output\":{\"spokenText\":\"" + r.output.spoken_text
               + "\",\"moodAfter\":\"" + r.output.mood_after
               + "\",\"localOrLlm\":\"" + r.output.local_or_llm + "\"}";
        result += "}";
    }
    result += "\n]";
    return result;
}

void TrainingDataLogger::clear() {
    records_.clear();
    printf("[训练数据] 已清空\n");
}