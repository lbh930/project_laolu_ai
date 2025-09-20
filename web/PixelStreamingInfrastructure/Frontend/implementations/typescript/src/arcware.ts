import { ArcwareInit } from '@arcware-cloud/pixelstreaming-websdk';
import { createChatBar, wireChatBar } from './chatBar';

// 写死 shareId
const defaultShareId = "share-beeb6fb6-2bc4-4e14-8df7-6ea9d4f4f9c7";

declare global {
  interface Window {
    arcwareApp?: any;
  }
}

function ensurePlayerContainer(): HTMLElement {
  let container = document.getElementById('player');
  if (!container) {
    container = document.createElement('div');
    container.id = 'player';
    Object.assign(container.style, { width: '100%', height: '100svh', background: '#000' });
    document.body.appendChild(container);
  }
  return container;
}

function bootArcware(shareId: string, projectId?: string) {
  const container = ensurePlayerContainer();

  const { Config, PixelStreaming, Application } = ArcwareInit(
    { shareId, projectId },
    {
      initialSettings: {
        AutoConnect: true,
        AutoPlayVideo: true,
        StartVideoMuted: true,
        KeyboardInput: true,
        MouseInput: true,
        TouchInput: true,
        GamepadInput: false,
        XRControllerInput: false,
        UseMic: true
      },
      settings: {
        fullscreenButton: true,
        micButton: true,
        audioButton: true,
        settingsButton: true,
        infoButton: false,
        connectionStrengthIcon: true,
        loveLetterLogging: false
      }
    }
  );

  container.appendChild(Application.rootElement);

  const { root, input, btn } = createChatBar();
  document.body.appendChild(root);
  wireChatBar((payload) => Application.emitUIInteraction(payload), input, btn);

  Application.getApplicationResponse((resp: unknown) => {
    console.log('[UE->Web][Arcware Response]', resp);
  });

  (window as any).arcwareApp = { Config, PixelStreaming, Application };
}

document.addEventListener('DOMContentLoaded', () => {
  const url = new URL(location.href);
  // 优先 URL 上的 shareId，其次用 Cloudflare Pages 注入的环境变量

  //Print default sharedid
  console.log("Default ShareId: " + defaultShareId);
  
  const shareId = url.searchParams.get('shareId') || defaultShareId;
  const projectId = url.searchParams.get('projectId') || undefined;

  if (!shareId) {
    const warn = document.createElement('div');
    warn.style.cssText = 'color:#fff;padding:16px;';
    warn.textContent = '缺少 shareId（请在 Cloudflare Pages 环境变量 VITE_ARCWARE_SHARE_ID 中配置）';
    document.body.appendChild(warn);
    return;
  }

  // 启动 Arcware
  bootArcware(shareId, projectId);
});
