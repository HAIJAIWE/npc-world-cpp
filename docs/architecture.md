# NPC World — 架构与启动文档

## 项目结构

```
npc-world-cpp/
├── 启动.bat                 ← 快捷入口 (双击启动)
├── CMakeLists.txt           ← 构建定义
├── .clangd                  ← C++ LSP 配置
├── scripts/                 ← 启动器 & 构建脚本
│   ├── launch.bat           ← 统一启动器 (Debug/Release 选择)
│   ├── build_debug.bat      ← Debug 构建
│   ├── build_release.bat    ← Release 构建
│   └── setup.ps1            ← 依赖安装 + CMake 配置
├── docs/
│   └── architecture.md      ← 本文档
├── src/
│   ├── main_backend.cpp     ← 后台入口
│   ├── main_frontend.cpp    ← 前端入口
│   ├── engine/              ← 引擎层 (11 模块)
│   │   ├── world_clock.*    ← 游戏世界时钟 + 事件调度
│   │   ├── dialogue_engine.* ← NPC 对话引擎 (核心循环)
│   │   ├── npc_memory.*     ← 三层记忆系统 (短期/长期/核心)
│   │   ├── model_engine.*   ← llama.cpp 封装
│   │   ├── model_registry.* ← NPC→模型绑定 + 动态切换
│   │   ├── api_client.*     ← 工具调用 (Function Calling)
│   │   ├── context_manager.*← Token 槽位 + LRU 淘汰
│   │   ├── inference_pipeline.* ← 多 Worker 推理队列
│   │   ├── training_logger.*← 训练数据记录
│   │   ├── world_state.*    ← 共享世界状态
│   │   └── llm_cache.*      ← LLM 响应缓存
│   ├── behavior/            ← 行为引擎层 (10 模块)
│   │   ├── npc_brain.*      ← NPC 大脑管理器
│   │   ├── npc_architecture.* ← 5层人格架构 (Id/Ego/Superego/Shadow/Persona)
│   │   ├── npc_identity_lock.* ← 人设锁验证
│   │   ├── npc_emotion.*    ← 情绪引擎
│   │   ├── npc_think.*      ← 思维管线 (System1/System2)
│   │   ├── npc_local_response.* ← 本地快速响应 (7场景)
│   │   ├── npc_autonomous.* ← 自主行为引擎
│   │   ├── npc_filter.*     ← 内容过滤 (剧透/诱导/越界)
│   │   ├── npc_gossip.*     ← 流言传播引擎
│   │   ├── npc_learning.*   ← 知识学习 + 认知失调
│   │   ├── npc_self_improve.* ← 行为模式检测 + 自我提升
│   │   └── npc_data_guard.* ← 数据完整性守护
│   ├── db/                  ← 数据库层
│   ├── ipc/                 ← 共享内存 IPC
│   └── types/               ← 公共类型定义
├── external/                ← 第三方依赖
│   ├── llama.cpp/           ← LLM 推理后端
│   ├── imgui/               ← UI 框架 (D3D11)
│   ├── sqlite3/             ← 数据库
│   └── nlohmann/            ← JSON 库
└── build/                   ← 编译产物
    ├── Debug/
    │   ├── npc_backend.exe
    │   └── npc_frontend.exe
    └── Release/
        ├── npc_backend.exe
        └── npc_frontend.exe
```

## 三层架构

```
┌─────────────────────────────────────────┐
│  🕐 世界时钟 / 事件调度器                │
│  1游戏分钟 = 1真实秒                     │
│  触发: 定时 / 位置接近 / 事件广播       │
└────────────┬────────────────────────────┘
             ▼
┌─────────────────────────────────────────┐
│  🗣️ NPC对话引擎 (核心循环)              │
│                                         │
│  for each active_conversation:          │
│    1. 选说话者 (轮询/情绪驱动/事件触发) │
│    2. 按 NPC 切换模型 / LoRA            │
│    3. 检索: 个人记忆 + 世界状态         │
│    4. 思维管线: System1 快速判断        │
│       └─ 命中 → 本地响应 (跳过LLM)      │
│    5. LLM 推理 (vLLM/InferencePipeline) │
│    6. 输出过滤 (剧透/诱导/越界)         │
│    7. 后处理: 学习 + 记忆 + 事件广播   │
│    8. 切换说话者 或 结束对话            │
└────────────┬────────────────────────────┘
             ▼
┌─────────────────────────────────────────┐
│  🗃️ 三层记忆系统 (按NPC隔离)            │
│  ├─ 短期: 最近10轮 (deque缓存)          │
│  ├─ 长期: 向量检索 (128维 embedding)    │
│  └─ 核心: 角色设定 (永久保存)           │
└─────────────────────────────────────────┘
```

## 启动器脚本

所有脚本位于 `scripts/`，从项目根目录调用：

| 脚本 | 功能 | 用法 |
|------|------|------|
| `scripts\launch.bat` | **统一启动器** — Debug/Release 选择菜单 | 双击 或 `scripts\launch.bat` |
| `scripts\build_debug.bat` | Debug 构建 (带调试符号) | `scripts\build_debug.bat` |
| `scripts\build_release.bat` | Release 构建 (优化后) | `scripts\build_release.bat` |
| `scripts\setup.ps1` | 一键安装依赖 + CMake 配置 | `powershell -ExecutionPolicy Bypass -File scripts\setup.ps1` |
| `启动.bat` | 根目录快捷入口 → `scripts\launch.bat` | 双击 |

### 启动流程

```
                    双击 启动.bat
                         │
                    scripts\launch.bat
                         │
              ┌──────────┼──────────┐
              │          │          │
         [1] Debug   [2] Release  [3] 仅后端
              │          │          │
         build\Debug  build\Release   │
              │          │          │
         ┌────┴────┐ ┌───┴────┐     │
         │ 后端    │ │ 后端    │     │
         │ /min    │ │ /min    │     │
         │         │ │         │     │
         │ 前端    │ │ 前端    │     │
         │ /wait   │ │ /wait   │     │
         └─────────┘ └─────────┘     │
              │          │          │
         前端关闭 → 自动结束后端      │
```

### 首次使用步骤

```
1. scripts\setup.ps1          ← 下载依赖 + CMake 配置
2. scripts\build_debug.bat    ← 编译 Debug 版
3. scripts\launch.bat         ← 选择 [1] Debug 启动
```

## 模块调用关系

```
main() 启动
  ├─ NPCDataGuard::scan_all_npcs()
  ├─ NPCAutonomousEngine 初始化
  ├─ ContextManager + DialogueEngine + LLMCache 就绪
  └─ ModelRegistry: 等待 NPC→模型绑定

process_requests() 主循环 (每秒)
  │
  ├─ WorldClock::tick()
  │   ├─ NPCAutonomousEngine::tick() ×每个NPC
  │   └─ NPCImprovementEngine::detect_patterns()
  │
  ├─ DialogueEngine::advanceConversation()
  │   ├─ ModelRegistry::ensureModelForNpc()  ← 动态模型切换
  │   └─ NPCGossipEngine::spread_after_chat()
  │
  └─ chat 请求处理:
      │
      ├─ NPCThinkPipeline::think()           ← System1 思维
      │   └─ LocalResponseEngine::try_local() ← 7场景本地回答
      │
      ├─ ModelEngine::generate() 或 InferencePipeline
      │
      └─ NPCFilterEngine::filter()
          ├─ NPCLearningEngine::process_knowledge()
          ├─ NPCImprovementEngine::evaluate()
          ├─ NpcMemoryStore::remember()       ← 三层记忆存储
          ├─ WorldStateManager::record_event()
          └─ TrainingDataLogger::log()
```

## 数据流

```
用户输入 → [Frontend]
  │
  │ IPC 共享内存
  ▼
[Backend] process_requests()
  │
  ├─ chat cmd:
  │   NPCThinkPipeline → LocalResponse (命中则跳过LLM)
  │   → ModelEngine.generate() → 流式 token
  │   → NPCFilterEngine → NPCLearningEngine
  │   → NpcMemoryStore (三层记忆存储)
  │   → IPC response → Frontend 流式显示
  │
  ├─ start_conversation cmd:
  │   DialogueEngine.startConversation()
  │   自动推进: selectSpeaker → buildPrompt → LLM → storeResult
  │
  └─ world_start cmd:
      WorldClock.start()
      世界时间开始推进，事件开始触发
```

## 数据库

```sql
-- NPC 大脑状态 (JSON 快照)
CREATE TABLE npc_brains (
    npc_id      TEXT PRIMARY KEY,
    brain_json  TEXT NOT NULL,
    updated_at  INTEGER NOT NULL
);

-- 三层记忆 (结构化的长期存储索引)
CREATE TABLE npc_memories (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    npc_id      TEXT NOT NULL,
    content     TEXT NOT NULL,
    tier        TEXT NOT NULL,
    importance  REAL DEFAULT 0.5,
    embedding   TEXT,
    created_at  INTEGER NOT NULL
);

-- NPC 模型绑定
CREATE TABLE npc_model_bindings (
    npc_id      TEXT PRIMARY KEY,
    model_path  TEXT NOT NULL,
    lora_path   TEXT,
    sampler_cfg TEXT
);
```
