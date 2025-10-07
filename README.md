# laolu.ai – Digital Human with UE5 + Pixel Streaming

An experimental project to showcase a **photorealistic digital human** in the browser, powered by **Unreal Engine 5.6 + Pixel Streaming**.  
Supports **AI-driven conversation, speech synthesis, and lip-sync visemes**.

---

## ✨ Features
- **Unreal Engine 5.6 + Pixel Streaming** for real-time, film-quality rendering.
- **MetaHuman integration** for realistic facial and body animation.
- **Realtime Voice Pipeline**:
  - ASR (Automatic Speech Recognition)
  - LLM (AI dialogue)
  - TTS with viseme timeline for lip-sync
- **Custom UE Component (C++)** to map viseme events to Morph Targets.
- **Session Gateway (Go)** for authentication and instance orchestration.
- **Web Client (React/WebRTC)** to play video/audio and synchronize visemes.

## Proposed Project Structrue
laolu-ai/
├─ ue/ # Unreal Engine 5.6 project
│ ├─ Source/Laolu/ # C++ custom components (e.g., MouthAnimComponent)
│ └─ Content/ # MetaHuman assets, Blueprints, animations
│
├─ gateway/ # Go session gateway (JWT auth, instance orchestration)
│ └─ main.go
│
├─ signal/ # Go WebSocket signaling server (room forwarding)
│ └─ main.go
│
├─ voice-pipeline/ # Python speech pipeline (ASR / LLM / TTS + viseme)
│ ├─ main.py
│ └─ adapters/ # Adapters for ASR/TTS/LLM providers
│
├─ web/ # React frontend client
│ ├─ src/
│ │ ├─ pixel/ # Pixel Streaming client
│ │ ├─ audio/ # VAD / ASR / TTS / viseme scheduler
│ │ └─ App.jsx
│ └─ index.html
│
├─ docs/ # Design notes, diagrams, setup guides
├─ .env.example # Example environment configuration
├─ README.md
└─ LICENSE

## For Testing:

# Local test frontend:

cd web\PixelStreamingInfrastructure\Frontend
npm run build

cd web\PixelStreamingInfrastructure\SignallingWebServer
serve -s www -l 5173

http://localhost:5173?shareId=share-beeb6fb6-2bc4-4e14-8df7-6ea9d4f4f9c7
