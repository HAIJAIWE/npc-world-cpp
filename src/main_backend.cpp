// ============================================================================
// npc_backend.exe — 全模块集成推理后端
//   世界时钟 → 自主行为 → 思维管线 → 对话引擎 → 三层记忆
//   → 过滤引擎 → 学习引擎 → 改进引擎 → 流言引擎 → 训练日志
// ============================================================================

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "llama.h"

#include "ipc/shared_memory.h"
#include "types/model_config.h"
#include "types/message.h"
#include "db/database.h"

#include "engine/model_engine.h"
#include "engine/model_registry.h"
#include "engine/world_clock.h"
#include "engine/dialogue_engine.h"
#include "engine/npc_memory.h"
#include "engine/world_state.h"
#include "engine/context_manager.h"
#include "engine/llm_cache.h"
#include "engine/api_client.h"
#include "engine/inference_pipeline.h"
#include "engine/training_logger.h"
#include "engine/intent_classifier.h"
#include "engine/react_loop.h"
#include "engine/memory_compressor.h"
#include "engine/user_preferences.h"
#include "engine/approval_gate.h"
#include "engine/multi_agent.h"
#include "engine/multimodal_perception.h"
#include "engine/workflow_engine.h"
#include "engine/filesystem_agent.h"
#include "engine/web_automation.h"
#include "engine/task_scheduler.h"
#include "engine/api_model_backend.h"
#include "engine/model_provider.h"
#include "engine/request_gate.h"

#include "behavior/npc_brain.h"
#include "behavior/npc_think.h"
#include "behavior/npc_autonomous.h"
#include "behavior/npc_filter.h"
#include "behavior/npc_gossip.h"
#include "behavior/npc_learning.h"
#include "behavior/npc_self_improve.h"
#include "behavior/npc_data_guard.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <windows.h>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <memory>

static std::string json_get_string(const std::string& json, const std::string& key);
static float       json_get_float(const std::string& json, const char* key, float def);
static int         json_get_int(const std::string& json, const char* key, int def);
static bool        parse_load_model_request(const std::string& json, npc::ModelConfig& cfg);
static bool        parse_chat_request(const std::string& json, std::string& prompt);
static std::string parse_npc_id(const std::string& json);

static npc::SharedMemory g_shm;
static bool              g_running = true;
static bool              g_world_active = false;

static std::shared_ptr<npc::Database> g_db;
static NPCBrainManager                g_brain_manager;
static std::string                    g_last_response;

static NPCThinkPipeline               g_think_pipeline;
static NPCFilterEngine                g_filter_engine;
static NPCGossipEngine                g_gossip_engine;
static NPCLearningEngine              g_learning_engine;
static NPCImprovementEngine           g_improve_engine;
static TrainingDataLogger             g_training_logger;
static LLMCache                   g_llm_cache(2000, 21600000);
static std::unique_ptr<NPCAutonomousEngine> g_autonomous_engine;
static std::unique_ptr<NPCDataGuard>  g_data_guard;
static bool                           g_pipeline_mode = false;
static std::string                    g_preferences_context;

static bool load_model_via_engine(const npc::ModelConfig& cfg);
static void unload_model();
static void process_requests();

int main(int argc, char* argv[]) {
    setvbuf(stdout, nullptr, _IONBF, 0);
    fprintf(stdout, "=== NPC World Backend v3.0.0 (三层架构) ===\n");

    if (!g_shm.initialize(npc::SharedMemoryMode::Server)) {
        fprintf(stderr, "[Backend] FATAL: 共享内存创建失败\n");
        return 1;
    }
    fprintf(stdout, "[Backend] 共享内存已创建 (IPC Ring Buffer)\n");

    g_db = std::make_shared<npc::Database>();
    if (g_db->open("npc_world.db")) {
        g_brain_manager.set_database(g_db);
        UserPreferences::instance().initialize(g_db);
        fprintf(stdout, "[Backend] 数据库就绪 (含用户偏好)\n");
    } else {
        fprintf(stderr, "[Backend] 数据库打开失败，NPC 大脑将不会持久化\n");
    }

    npc::ContextManager::instance().configure(6144, 1024, 32);
    fprintf(stdout, "[Backend] 上下文管理器就绪 (6144 tokens, 32 slots)\n");

    g_data_guard = std::make_unique<NPCDataGuard>(&g_brain_manager);
    g_autonomous_engine = std::make_unique<NPCAutonomousEngine>(g_brain_manager);
    fprintf(stdout, "[Backend] 行为引擎三层就绪: 思维管线 + 过滤 + 学习 + 改进 + 数据守护\n");

    npc::ConversationConfig defaultConvCfg;
    defaultConvCfg.speaker_mode = npc::SpeakerSelectionMode::RoundRobin;
    defaultConvCfg.max_turns = 20;
    defaultConvCfg.use_memory = true;
    defaultConvCfg.use_world_state = true;
    defaultConvCfg.use_cache = true;
    npc::DialogueEngine::instance().configure(defaultConvCfg);
    fprintf(stdout, "[Backend] 对话引擎就绪\n");

    fprintf(stdout, "[Backend] 架构: 世界时钟 → 对话引擎 → 三层记忆\n");

    npc::ReActLoop::instance().configure(npc::ReActConfig{});
    npc::MultiAgentOrchestrator::instance().configure(npc::MultiAgentConfig{});
    npc::ApprovalGate::instance().configure(60);
    npc::MemoryCompressor::instance().configure(npc::CompressionConfig{});
    fprintf(stdout, "[Backend] 智能体四大能力就绪: 感知 | 规划 | 记忆 | 行动\n");

    npc::FileSystemAgent::instance().configure("D:\\AI_Studio_Workspace");
    npc::WebAutomation::instance().configure("NPC-World-WebAgent/1.0", 15000);
    npc::TaskScheduler::instance().configure(1000);
    fprintf(stdout, "[Backend] 本地自动化能力就绪: 文件系统 | 网络 | 任务调度\n");

    npc::RequestGate::instance().configureDefault(npc::GateConfig{});
    fprintf(stdout, "[Backend] 模型API层就绪: 支持OpenAI兼容协议 + 多Provider + 并发限流\n");

    process_requests();

    unload_model();
    g_shm.shutdown();
    fprintf(stdout, "[Backend] 已关闭\n");
    return 0;
}

static void process_requests() {
    fprintf(stdout, "[Backend] 请求监听器已启动\n");

    uint8_t request_buf[4096];
    auto lastWorldTick = std::chrono::steady_clock::now();

    while (g_running) {
        if (g_world_active) {
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration<double>(now - lastWorldTick).count() >= 1.0) {
                WorldClock::instance().tick();
                lastWorldTick = now;

                for (const auto& npc_id : g_brain_manager.get_all_brain_ids()) {
                    NPCBrain* brain = g_brain_manager.get_brain(npc_id);
                    if (brain && g_autonomous_engine) {
                        g_autonomous_engine->tick(*brain);
                    }

                    if (WorldClock::instance().now().minute % 30 == 0 && brain) {
                        g_improve_engine.detect_patterns(*brain);
                    }

                    npc::MemoryCompressor::instance().compress(npc_id);
                }

                npc::TaskScheduler::instance().tick();

                auto activeConvs = npc::DialogueEngine::instance().activeConversationIds();
                for (const auto& convId : activeConvs) {
                    npc::DialogueEngine::instance().advanceConversation(convId);
                }

                if (WorldClock::instance().now().minute % 30 == 0) {
                    for (const auto& convId : activeConvs) {
                        auto* conv = npc::DialogueEngine::instance().getConversation(convId);
                        if (conv && !conv->history.empty()) {
                            std::vector<std::string> listener_ids;
                            for (const auto& pid : conv->participant_ids) {
                                if (pid != conv->history.back().speaker_id) {
                                    listener_ids.push_back(pid);
                                }
                            }
                            if (!listener_ids.empty()) {
                                g_gossip_engine.spread_after_chat(
                                    conv->history.back().speaker_id,
                                    listener_ids,
                                    g_brain_manager.get_brains());
                            }
                        }
                    }
                }
            }
        }

        if (g_shm.has_request()) {
            uint32_t req_id = 0;
            uint32_t bytes = g_shm.read_entry(req_id, request_buf, sizeof(request_buf));

            if (bytes > 0) {
                std::string json_str(reinterpret_cast<char*>(request_buf), bytes);
                fprintf(stdout, "[Backend] 收到请求: %s\n", json_str.c_str());

                if (json_str.find("\"load_model\"") != std::string::npos) {
                    npc::ModelConfig cfg;
                    if (parse_load_model_request(json_str, cfg)) {
                        fprintf(stdout, "[Backend] 加载模型: %s\n", cfg.model_path.c_str());

                        if (load_model_via_engine(cfg)) {
                            fprintf(stdout, "[Backend] 模型加载成功 (via ModelEngine)\n");
                            const char* test_prompt = "你好，请简短介绍你自己。";
                            g_shm.reset_ring_buffer();
                            auto result = npc::ModelEngine::instance().generate(test_prompt);
                            for (const auto& tok : result.tokens) {
                                g_shm.write_entry(0xFFFFFFFF, tok.text.data(), (uint32_t)tok.text.size());
                                g_shm.signal_has_data();
                            }
                            g_last_response = result.text;
                            g_shm.signal_done();
                        } else {
                            fprintf(stderr, "[Backend] 模型加载失败\n");
                            g_shm.set_error("加载失败");
                            g_shm.signal_done();
                        }
                    } else {
                        g_shm.set_error("无法解析");
                        g_shm.signal_done();
                    }
                } else if (json_str.find("\"chat\"") != std::string::npos ||
                           json_str.find("\"generate\"") != std::string::npos) {
                    std::string prompt;
                    if (parse_chat_request(json_str, prompt)) {
                        auto intent = npc::IntentClassifier::instance().classify(prompt);
                        fprintf(stdout, "[Intent] %s (%.0f%%) route=%d\n",
                            intent.category_name(), intent.confidence * 100,
                            intent.should_route_directly);

                        UserPreferences::instance().extractFromMessage(prompt);
                        g_preferences_context = UserPreferences::instance().formatForPrompt();

                        if (!npc::ModelEngine::instance().isModelLoaded()) {
                            g_shm.set_error("模型未加载");
                            g_shm.signal_done();
                        } else {
                            std::string npc_id = parse_npc_id(json_str);
                            std::string enhanced_prompt = prompt;
                            std::string inner_thought;
                            bool used_local = false;
                            std::string local_response;

                            if (!npc_id.empty()) {
                                NPCBrain* brain = g_brain_manager.get_brain(npc_id);
                                if (brain) {
                                    ThoughtResult thought = g_think_pipeline.think(*brain, prompt);
                                    inner_thought = thought.inner_thought;
                                    used_local = thought.used_local_response;
                                    if (used_local) {
                                        local_response = thought.local_response.response;
                                        fprintf(stdout, "[Think] NPC(%s) local hit: %s\n",
                                            npc_id.c_str(), thought.local_response.response_type.c_str());
                                        g_think_pipeline.apply_state_cascade(*brain);
                                    }

                                    DecisionResult decision = g_brain_manager.process_message(npc_id, prompt);
                                    if (inner_thought.empty()) {
                                        inner_thought = decision.inner_thought;
                                    }

                                    PromptConfig pcfg = g_brain_manager.build_prompt(npc_id, prompt);
                                    if (!pcfg.system_prompt.empty()) {
                                        enhanced_prompt = pcfg.system_prompt + "\n\n" + pcfg.user_message + "\n\n" + brain->personality.name + ":";
                                    }
                                }
                            }

                            if (!g_preferences_context.empty()) {
                                enhanced_prompt = g_preferences_context + "\n" + enhanced_prompt;
                            }

                            auto& memory = npc::NpcMemoryStore::instance();
                            auto memResults = memory.retrieve(npc_id, prompt, 3, true);
                            if (!memResults.empty()) {
                                std::string memContext;
                                for (const auto& r : memResults) {
                                    memContext += "- " + r.memory.content + "\n";
                                }
                                enhanced_prompt += "\n[记忆]\n" + memContext;
                            }

                            int max_gen = json_get_int(json_str, "max_tokens", 512);

                            npc::ModelParams params;
                            auto* profile = npc::ModelRegistry::instance().getProfile(npc_id);
                            bool is_remote = (profile && profile->is_remote);

                            if (is_remote) {
                                params = profile->sampling_params;
                                params.temperature = json_get_float(json_str, "temperature", params.temperature);
                                params.top_p = json_get_float(json_str, "top_p", params.top_p);
                                params.n_predict = max_gen;

                                g_shm.reset_ring_buffer();
                                g_last_response.clear();

                                fprintf(stdout, "[Chat] NPC %s -> remote provider '%s'\n",
                                    npc_id.c_str(), profile->remote_provider.c_str());

                                std::vector<std::pair<std::string, std::string>> messages;
                                messages.push_back({"user", enhanced_prompt});

                                auto rpcResult = npc::ModelProvider::instance().generateMessages(
                                    profile->remote_provider, messages, "",
                                    [](const std::string& text, bool done) -> bool {
                                        g_shm.reset_ring_buffer();
                                        g_shm.write_entry(0, text.data(), (uint32_t)text.size());
                                        g_shm.signal_has_data();
                                        if (!done) g_last_response = text;
                                        return true;
                                    });
                                g_last_response = rpcResult.success ? rpcResult.text : ("[API错误] " + rpcResult.error);
                                fprintf(stdout, "[Chat] Remote response: %zu chars, %.0fms, source=%d\n",
                                    rpcResult.text.size(), rpcResult.elapsed_ms, (int)rpcResult.source);
                            } else {
                                if (profile) {
                                    params = profile->sampling_params;
                                    npc::ModelRegistry::instance().ensureModelForNpc(npc_id);
                                } else {
                                    params = npc::ModelEngine::instance().getDefaultParams();
                                }
                                params.temperature = json_get_float(json_str, "temperature", params.temperature);
                                params.top_p = json_get_float(json_str, "top_p", params.top_p);
                                params.top_k = json_get_int(json_str, "top_k", params.top_k);
                                params.n_predict = max_gen;

                                g_shm.reset_ring_buffer();
                                g_last_response.clear();

                                if (used_local && !local_response.empty()) {
                                    g_last_response = local_response;
                                } else if (g_pipeline_mode) {
                                    npc::InferenceRequest req;
                                    req.id = 0;
                                    req.systemPrompt = "";
                                    req.userPrompt = enhanced_prompt;
                                    req.params = params;

                                    std::string collected;
                                    bool pipeline_done = false;
                                    npc::InferencePipeline::instance().submit(req,
                                        [&](const npc::InferenceResponse& resp) {
                                            if (resp.success) {
                                                collected = resp.text;
                                            }
                                            pipeline_done = true;
                                        });

                                    while (!pipeline_done) {
                                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                                    }
                                    g_last_response = collected;
                                } else {
                                    auto result = npc::ModelEngine::instance().generate(
                                        enhanced_prompt, &params,
                                        [](const std::string& text, bool done) -> bool {
                                            g_shm.reset_ring_buffer();
                                            g_shm.write_entry(0, text.data(), (uint32_t)text.size());
                                            g_shm.signal_has_data();
                                            if (!done) g_last_response = text;
                                            return true;
                                        });
                                    g_last_response = result.text;
                                }
                            }

                            if (!npc_id.empty()) {
                                NPCBrain* brain = g_brain_manager.get_brain(npc_id);
                                if (brain) {
                                    FilterResult filter = g_filter_engine.filter_offline(*brain, g_last_response);
                                    if (filter.severity == FilterResult::BLOCKED) {
                                        g_last_response = "[内部过滤: " + filter.reason + "]";
                                        fprintf(stdout, "[Filter] NPC(%s) blocked: %s\n", npc_id.c_str(), filter.reason.c_str());
                                    } else if (filter.severity >= FilterResult::WARN) {
                                        fprintf(stdout, "[Filter] NPC(%s) warning: %s\n", npc_id.c_str(), filter.reason.c_str());
                                    }

                                    g_brain_manager.process_response(npc_id, prompt, g_last_response);

                                    float importance = 0.5f;
                                    auto domain = "chat";
                                    g_learning_engine.process_new_knowledge(*brain, domain,
                                        prompt + " => " + g_last_response, "interaction", importance);
                                    g_improve_engine.evaluate_outcome(*brain, g_last_response, prompt);

                                    memory.remember(npc_id,
                                        "回复: " + g_last_response,
                                        importance, 0.0f, "neutral", {}, {"chat"});

                                    WorldStateManager::instance().record_event(
                                        "chat", g_last_response, 0.3f, {}, 0.5f);

                                    TrainingRecord record;
                                    record.id = std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
                                    record.timestamp = (int64_t)time(nullptr);
                                    record.npc_id = npc_id;
                                    record.npc_name = brain->personality.name;
                                    record.state.immediate_emotion_type = brain->emotion.immediate_emotion.type;
                                    record.state.immediate_emotion_intensity = brain->emotion.immediate_emotion.intensity;
                                    record.state.background_mood_label = brain->emotion.background_mood.label;
                                    record.state.background_mood_valence = brain->emotion.background_mood.valence;
                                    record.state.mood_intensity = brain->emotion.mood_intensity;
                                    record.state.mental_energy = brain->mental_energy;
                                    record.state.system2_fatigue = 0.0f;
                                    record.state.stress_level = brain->emotion.stress_level;
                                    record.context.topic = prompt;
                                    record.output.spoken_text = g_last_response;
                                    record.output.local_or_llm = used_local ? "local" : "llm";
                                    record.system1.gut_feeling = inner_thought;
                                    record.system2.should_speak = true;
                                    record.output.mood_after = brain->emotion.immediate_emotion.type;
                                    g_training_logger.log(record);
                                }
                            }

                            g_shm.signal_done();
                        }
                    } else {
                        g_shm.set_error("无法解析");
                        g_shm.signal_done();
                    }
                } else if (json_str.find("\"world_start\"") != std::string::npos) {
                    std::string worldName = json_get_string(json_str, "world_name");
                    fprintf(stdout, "[Backend] 启动世界: %s\n", worldName.c_str());
                    WorldClock::instance().configure(60.0f);
                    WorldClock::instance().start();
                    g_world_active = true;
                    fprintf(stdout, "[Backend] 世界时钟已启动 (1游戏分钟=1真实秒)\n");
                    g_shm.signal_done();
                } else if (json_str.find("\"world_pause\"") != std::string::npos) {
                    WorldClock::instance().pause();
                    g_world_active = false;
                    fprintf(stdout, "[Backend] 世界时钟已暂停\n");
                    g_shm.signal_done();
                } else if (json_str.find("\"world_resume\"") != std::string::npos) {
                    WorldClock::instance().resume();
                    g_world_active = true;
                    fprintf(stdout, "[Backend] 世界时钟已恢复\n");
                    g_shm.signal_done();
                } else if (json_str.find("\"start_conversation\"") != std::string::npos) {
                    std::string topic = json_get_string(json_str, "topic");
                    std::string location = json_get_string(json_str, "location");
                    std::vector<std::string> ids, names;
                    std::string idsJson = json_get_string(json_str, "participant_ids");
                    std::string namesJson = json_get_string(json_str, "participant_names");
                    {
                        size_t start = 0, end;
                        while ((end = idsJson.find(',', start)) != std::string::npos) {
                            ids.push_back(idsJson.substr(start, end - start));
                            start = end + 1;
                        }
                        if (start < idsJson.size()) ids.push_back(idsJson.substr(start));
                    }
                    {
                        size_t start = 0, end;
                        while ((end = namesJson.find(',', start)) != std::string::npos) {
                            names.push_back(namesJson.substr(start, end - start));
                            start = end + 1;
                        }
                        if (start < namesJson.size()) names.push_back(namesJson.substr(start));
                    }
                    std::string convId = npc::DialogueEngine::instance().startConversation(
                        topic, location, ids, names);
                    fprintf(stdout, "[Backend] 对话已启动: %s\n", convId.c_str());
                    g_shm.signal_done();
                } else if (json_str.find("\"advance_conversation\"") != std::string::npos) {
                    std::string convId = json_get_string(json_str, "conversation_id");
                    npc::DialogueEngine::instance().advanceConversation(convId);
                    g_shm.signal_done();
                } else if (json_str.find("\"world_stats\"") != std::string::npos) {
                    auto t = WorldClock::instance().now();
                    fprintf(stdout, "[Backend] 世界时间: %s, 活跃对话: %zu, 模型配置: %zu NPC\n",
                        t.to_string().c_str(),
                        npc::DialogueEngine::instance().activeConversationIds().size(),
                        npc::ModelRegistry::instance().profileCount());
                    g_shm.signal_done();
                } else if (json_str.find("\"assign_npc_model\"") != std::string::npos) {
                    std::string npcId = json_get_string(json_str, "npc_id");
                    std::string modelPath = json_get_string(json_str, "model_path");
                    std::string loraPath = json_get_string(json_str, "lora_path");
                    npc::ModelParams params;
                    params.temperature = json_get_float(json_str, "temperature", 0.8f);
                    params.top_p = json_get_float(json_str, "top_p", 0.95f);
                    params.top_k = json_get_int(json_str, "top_k", 40);
                    params.n_predict = json_get_int(json_str, "max_tokens", 256);
                    auto result = npc::ModelRegistry::instance().assignNpc(
                        npcId, modelPath, loraPath, &params);
                    fprintf(stdout, "[Backend] NPC模型分配: %s -> %s (switched=%d)\n",
                        npcId.c_str(), modelPath.c_str(), result.model_switched);
                    g_shm.signal_done();
                } else if (json_str.find("\"npc_model_list\"") != std::string::npos) {
                    auto profiles = npc::ModelRegistry::instance().allProfiles();
                    fprintf(stdout, "[Backend] NPC模型配置 (%zu):\n", profiles.size());
                    for (const auto& p : profiles) {
                        fprintf(stdout, "  %s -> %s (lora=%s)\n",
                            p.npc_id.c_str(), p.model_path.c_str(),
                            p.lora_path.empty() ? "none" : p.lora_path.c_str());
                    }
                    g_shm.signal_done();
                } else if (json_str.find("\"tools_init\"") != std::string::npos) {
                    npc::ApiToolRegistry::instance().registerBuiltinTools();
                    fprintf(stdout, "[Backend] 内置工具已注册 (%zu tools)\n",
                        npc::ApiToolRegistry::instance().toolCount());
                    g_shm.signal_done();
                } else if (json_str.find("\"register_tool\"") != std::string::npos) {
                    std::string toolName = json_get_string(json_str, "tool_name");
                    std::string toolDesc = json_get_string(json_str, "description");
                    std::string toolUrl = json_get_string(json_str, "url");
                    std::string toolMethod = json_get_string(json_str, "method");
                    if (toolMethod.empty()) toolMethod = "GET";
                    npc::ApiToolRegistry::instance().registerTool(toolName, toolDesc, toolUrl, toolMethod);
                    fprintf(stdout, "[Backend] 工具已注册: %s (%s %s)\n",
                        toolName.c_str(), toolMethod.c_str(), toolUrl.c_str());
                    g_shm.signal_done();
                } else if (json_str.find("\"tools_list\"") != std::string::npos) {
                    auto tools = npc::ApiToolRegistry::instance().allTools();
                    fprintf(stdout, "[Backend] 已注册工具 (%zu):\n", tools.size());
                    for (const auto& t : tools) {
                        fprintf(stdout, "  %s: %s [%s %s]\n",
                            t.name.c_str(), t.description.c_str(), t.method.c_str(), t.url.c_str());
                    }
                    g_shm.signal_done();
                } else if (json_str.find("\"pipeline_start\"") != std::string::npos) {
                    int workers = json_get_int(json_str, "workers", 1);
                    if (npc::ModelEngine::instance().isModelLoaded()) {
                        npc::InferencePipeline::instance().start(workers);
                        g_pipeline_mode = true;
                        fprintf(stdout, "[Backend] 推理管道已启动 (workers=%d)\n", workers);
                    } else {
                        fprintf(stderr, "[Backend] 管道启动失败: 模型未加载\n");
                    }
                    g_shm.signal_done();
                } else if (json_str.find("\"pipeline_stop\"") != std::string::npos) {
                    npc::InferencePipeline::instance().stop();
                    g_pipeline_mode = false;
                    fprintf(stdout, "[Backend] 推理管道已停止\n");
                    g_shm.signal_done();
                } else if (json_str.find("\"pipeline_stats\"") != std::string::npos) {
                    auto stats = npc::InferencePipeline::instance().getStats();
                    fprintf(stdout, "[Pipeline] 请求: %lld, Tokens: %lld, 速率: %.1f tok/s, 延迟: %.1f ms, 待处理: %zu\n",
                        stats.totalRequests, stats.totalTokens, stats.avgTokensPerSec,
                        stats.avgLatencyMs, npc::InferencePipeline::instance().pendingCount());
                    g_shm.signal_done();
                } else if (json_str.find("\"integrity_check\"") != std::string::npos) {
                    std::string target = json_get_string(json_str, "npc_id");
                    if (!target.empty()) {
                        auto report = g_data_guard->check_integrity(target);
                        fprintf(stdout, "[DataGuard] NPC(%s): healthy=%d, issues=%zu\n",
                            target.c_str(), report.healthy, report.issues.size());
                        for (const auto& issue : report.issues) {
                            fprintf(stdout, "  - %s\n", issue.c_str());
                        }
                    } else {
                        auto reports = g_data_guard->scan_all_npcs();
                        int unhealthy = 0;
                        for (const auto& r : reports) if (!r.healthy) unhealthy++;
                        fprintf(stdout, "[DataGuard] 全局扫描: %zu NPC, %d unhealthy\n", reports.size(), unhealthy);
                    }
                    g_shm.signal_done();
                } else if (json_str.find("\"think_stats\"") != std::string::npos) {
                    auto stats = g_think_pipeline.get_stats();
                    fprintf(stdout, "[Think] 总思考: %d, 本地命中: %d, 记忆访问: %d\n",
                        stats.total_thoughts, stats.local_hits, stats.total_memories_accessed);
                    g_shm.signal_done();
                } else if (json_str.find("\"training_stats\"") != std::string::npos) {
                    auto stats = g_training_logger.get_stats();
                    fprintf(stdout, "[Training] 总记录: %d, LLM: %d, 本地: %d, 命中率: %.1f%%\n",
                        stats.total, stats.llm_count, stats.local_count, stats.local_hit_rate * 100.0);
                    for (const auto& [npc_id, count] : stats.by_npc) {
                        fprintf(stdout, "  %s: %d\n", npc_id.c_str(), count);
                    }
                    g_shm.signal_done();
                } else if (json_str.find("\"training_export\"") != std::string::npos) {
                    std::string jsonl = g_training_logger.export_jsonl();
                    fprintf(stdout, "[Training] JSONL导出 (%zu 字节)\n", jsonl.size());
                    if (!jsonl.empty()) {
                        FILE* fp = fopen("training_data.jsonl", "a");
                        if (fp) { fwrite(jsonl.data(), 1, jsonl.size(), fp); fclose(fp);
                            fprintf(stdout, "[Training] 已追加到 training_data.jsonl\n"); }
                    }
                    g_shm.signal_done();
                } else if (json_str.find("\"improvement_report\"") != std::string::npos) {
                    std::string target = json_get_string(json_str, "npc_id");
                    if (!target.empty()) {
                        auto intents = g_improve_engine.get_intents(target);
                        fprintf(stdout, "[Improve] NPC(%s) 改进意图: %zu\n", target.c_str(), intents.size());
                        for (const auto& intent : intents) {
                            fprintf(stdout, "  %s: %s (confidence=%.0f, attempts=%d)\n",
                                intent.pattern_type.c_str(), intent.description.c_str(),
                                intent.confidence, intent.attempts);
                        }
                    } else {
                        fprintf(stdout, "[Improve] 需要指定 npc_id\n");
                    }
                    g_shm.signal_done();
                } else if (json_str.find("\"init_brain\"") != std::string::npos) {
                    std::string npc_id = json_get_string(json_str, "npc_id");
                    std::string name = json_get_string(json_str, "name");
                    std::string role = json_get_string(json_str, "role");
                    std::string style = json_get_string(json_str, "speaking_style");
                    if (!npc_id.empty() && !name.empty()) {
                        std::vector<std::string> traits, values;
                        std::string traits_json = json_get_string(json_str, "traits");
                        if (!traits_json.empty()) {
                            size_t start = 0, end;
                            while ((end = traits_json.find(',', start)) != std::string::npos) {
                                traits.push_back(traits_json.substr(start, end - start));
                                start = end + 1;
                            }
                            if (start < traits_json.size()) traits.push_back(traits_json.substr(start));
                        }
                        std::string values_json = json_get_string(json_str, "values");
                        if (!values_json.empty()) {
                            size_t start = 0, end;
                            while ((end = values_json.find(',', start)) != std::string::npos) {
                                values.push_back(values_json.substr(start, end - start));
                                start = end + 1;
                            }
                            if (start < values_json.size()) values.push_back(values_json.substr(start));
                        }
                        NPCBrain brain = g_brain_manager.init_brain(npc_id, name, role, traits, style, values);
                        g_brain_manager.save_brain(npc_id);
                        fprintf(stdout, "[Brain] NPC(%s) 大脑初始化完成\n", npc_id.c_str());
                    }
                    g_shm.signal_done();
                }

                // ... (remaining 70+ IPC commands similarly omitted for brevity)

                else {
                    g_shm.set_error("未知命令");
                    g_shm.signal_done();
                }
            }
            g_shm.clear_has_request();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

static bool load_model_via_engine(const npc::ModelConfig& cfg) {
    auto& engine = npc::ModelEngine::instance();
    if (!engine.initialize()) {
        fprintf(stderr, "[Backend] ModelEngine 初始化失败\n");
        return false;
    }
    npc::ModelParams params;
    params.temperature = cfg.temperature;
    params.top_p = cfg.top_p;
    params.top_k = cfg.top_k;
    params.min_p = cfg.min_p;
    params.n_ctx = cfg.n_ctx;
    params.n_batch = cfg.n_batch;
    params.n_ubatch = cfg.n_ubatch;
    params.n_gpu_layers = cfg.n_gpu_layers;
    params.n_threads = cfg.n_threads;
    params.n_predict = cfg.n_predict;
    params.penalty_repeat = cfg.penalty_repeat;
    params.seed = cfg.seed;
    params.flash_attn = cfg.flash_attn;
    params.no_perf = cfg.no_perf;
    params.min_keep = cfg.min_keep;
    engine.setDefaultParams(params);
    return engine.loadModel(cfg.model_path);
}

static bool load_model(const npc::ModelConfig& cfg) {
    return load_model_via_engine(cfg);
}

static void unload_model() {
    npc::ModelEngine::instance().unloadModel();
    npc::ModelEngine::instance().shutdown();
}

static std::string json_unescape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            switch (s[i + 1]) {
                case '\\': out += '\\'; break;
                case '"':  out += '"';  break;
                case 'n':  out += '\n'; break;
                case 't':  out += '\t'; break;
                default:   out += s[i + 1]; break;
            }
            ++i;
        } else { out += s[i]; }
    }
    return out;
}

static std::string json_get_string(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\":\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return "";
    pos += search.length();
    size_t end = json.find('"', pos);
    if (end == std::string::npos) return "";
    return json_unescape(json.substr(pos, end - pos));
}

static float json_get_float(const std::string& json, const char* key, float def) {
    std::string search = "\"" + std::string(key) + "\":";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return def;
    pos += search.length();
    size_t end = json.find_first_of(",}", pos);
    if (end == std::string::npos) return def;
    return (float)atof(json.substr(pos, end - pos).c_str());
}

static int json_get_int(const std::string& json, const char* key, int def) {
    std::string search = "\"" + std::string(key) + "\":";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return def;
    pos += search.length();
    size_t end = json.find_first_of(",}", pos);
    if (end == std::string::npos) return def;
    return atoi(json.substr(pos, end - pos).c_str());
}

static bool parse_load_model_request(const std::string& json, npc::ModelConfig& cfg) {
    std::string path = json_get_string(json, "path");
    if (path.empty()) { fprintf(stderr, "[Backend] 请求缺少 model path\n"); return false; }
    cfg.model_path = path;
    cfg.temperature = json_get_float(json, "temperature", 0.8f);
    cfg.top_p = json_get_float(json, "top_p", 0.95f);
    cfg.top_k = json_get_int(json, "top_k", 40);
    cfg.min_p = json_get_float(json, "min_p", 0.05f);
    cfg.n_ctx = json_get_int(json, "context_length", 8192);
    cfg.n_batch = json_get_int(json, "batch_size", 512);
    cfg.n_ubatch = cfg.n_batch;
    cfg.n_gpu_layers = json_get_int(json, "ngl", 0);
    cfg.seed = json_get_int(json, "seed", -1);
    cfg.n_threads = (int)json_get_float(json, "threads", 0);
    cfg.n_predict = json_get_int(json, "max_tokens", 2048);
    cfg.penalty_repeat = json_get_float(json, "repeat_penalty", 1.1f);
    cfg.no_perf = false;
    cfg.flash_attn = true;
    cfg.min_keep = 1;
    return true;
}

static bool parse_chat_request(const std::string& json, std::string& prompt) {
    prompt = json_get_string(json, "prompt");
    if (prompt.empty()) prompt = json_get_string(json, "message");
    if (prompt.empty()) { fprintf(stderr, "[Backend] 聊天请求缺少 prompt\n"); return false; }
    return true;
}

static std::string parse_npc_id(const std::string& json) {
    return json_get_string(json, "npc_id");
}