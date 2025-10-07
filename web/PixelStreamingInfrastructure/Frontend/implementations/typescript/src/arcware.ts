import { ArcwareInit } from '@arcware-cloud/pixelstreaming-websdk';
import { createChatBar, wireChatBar } from './chatBar';
import { PSBridge, arcwareAdapter } from './psBridge';

const defaultShareId = "share-beeb6fb6-2bc4-4e14-8df7-6ea9d4f4f9c7";

function ensurePlayerContainer(): HTMLElement {
  let c = document.getElementById('player');
  if (!c) { c = document.createElement('div'); c.id = 'player';
    Object.assign(c.style, { width: '100%', height: '100svh', background: '#000' }); document.body.appendChild(c); }
  return c;
}

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

  const { root, input, btn, setSendEnabled, setBusy } = createChatBar();
  document.body.appendChild(root);

  // 发送文本
  wireChatBar((p)=>bridge.request({ ...p, type:'chat' }), input, btn, {
    beforeSend: async () => {
      // 这里统一走“询问 UE 是否能接收新消息”
      try {
        const ok = await bridge.request<boolean>({
          type:'call',
          target: { by:'tag', value:'Avatar' },     // 你可以改成 by:'name'
          component:'HumanState',
          method:'CanReceiveNewMessage'
        });
        setSendEnabled(ok);
        return ok;
      } catch { setSendEnabled(false); return false; }
    }
  });

  // 定时刷新“是否可发”
  setInterval(async () => {
    try {
      const ok = await bridge.request<boolean>({
        type:'call',
        target: { by:'tag', value:'Avatar' },
        component:'HumanState',
        method:'CanReceiveNewMessage'
      });
      setSendEnabled(!!ok);
    } catch { setSendEnabled(false); }
  }, 500);

  (window as any).arcwareApp = { Config, PixelStreaming, Application, bridge };
}

document.addEventListener('DOMContentLoaded', () => {
  const url = new URL(location.href);
  const shareId = url.searchParams.get('shareId') || defaultShareId;
  const projectId = url.searchParams.get('projectId') || undefined;
  if (!shareId) { const warn = document.createElement('div'); warn.style.cssText = 'color:#fff;padding:16px;'; warn.textContent = '缺少 shareId'; document.body.appendChild(warn); return; }
  bootArcware(shareId, projectId);
});
