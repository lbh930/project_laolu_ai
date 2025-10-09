import { ArcwareInit } from '@arcware-cloud/pixelstreaming-websdk';
import { createChatBar, wireChatBar } from './chatBar';
import { PSBridge, arcwareAdapter } from './psBridge';
import { ensurePlayerContainer, createStateChecker, createChatBarOptions } from './shared';

const defaultShareId = "share-beeb6fb6-2bc4-4e14-8df7-6ea9d4f4f9c7";

function bootArcware(shareId: string, projectId?: string) {
  const container = ensurePlayerContainer();
  const { Config, PixelStreaming, Application } = ArcwareInit(
    { shareId, projectId },
    {
      initialSettings: { AutoConnect: true, AutoPlayVideo: true, StartVideoMuted: false, KeyboardInput: true, MouseInput: true, TouchInput: true, UseMic: true },
      settings: { fullscreenButton:false, micButton:false, audioButton:false, settingsButton:false, infoButton:false, connectionStrengthIcon:false }
    }
  );
  container.appendChild(Application.rootElement);

  const bridge = new PSBridge(arcwareAdapter(Application));

  const { root, input, btn, setSendEnabled, setButtonState, getCurrentState } = createChatBar();
  document.body.appendChild(root);

  // 使用共享的状态检查器
  const { startPeriodicCheck } = createStateChecker(bridge, setSendEnabled);
  
  // 使用标准的聊天栏配置
  const chatBarOptions = createChatBarOptions(bridge, setSendEnabled, setButtonState, getCurrentState);

  wireChatBar((p)=>bridge.request({ ...p, type:'chat' }), input, btn, chatBarOptions);

  // 启动定时检查
  startPeriodicCheck();

  (window as any).arcwareApp = { Config, PixelStreaming, Application, bridge };
}

document.addEventListener('DOMContentLoaded', () => {
  const url = new URL(location.href);
  const shareId = url.searchParams.get('shareId') || defaultShareId;
  const projectId = url.searchParams.get('projectId') || undefined;
  if (!shareId) { const warn = document.createElement('div'); warn.style.cssText = 'color:#fff;padding:16px;'; warn.textContent = '缺少 shareId'; document.body.appendChild(warn); return; }
  bootArcware(shareId, projectId);
});
