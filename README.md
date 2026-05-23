# NPC World C++

A C++20 NPC simulation engine with llama.cpp inference backend, featuring:
- Multi-agent dialogue system
- Three-tier memory (short-term / long-term / core)
- World clock & event scheduling
- Shared memory IPC (backend ↔ frontend)
- Dear ImGui + Direct3D 11 frontend
- SQLite3 persistence
- Behavior engine with 10+ modules

## Build

```bash
# Install dependencies
powershell -ExecutionPolicy Bypass -File scripts/setup.ps1

# Debug build
scripts/build_debug.bat

# Run
scripts/launch.bat
```