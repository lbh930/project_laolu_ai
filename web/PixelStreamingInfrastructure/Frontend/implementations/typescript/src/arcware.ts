import { ArcwareInit } from '@arcware-cloud/pixelstreaming-websdk';
import { createChatBar, wireChatBar } from './chatBar';

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
  const shareId = url.searchParams.get('shareId') || '';
  const projectId = url.searchParams.get('projectId') || undefined;

  if (!shareId) {
    const warn = document.createElement('div');
    warn.style.cssText = 'color:#fff;padding:16px;';
    warn.textContent = '缺少 shareId（URL 需带 ?shareId=...）';
    document.body.appendChild(warn);
    return;
  }

  bootArcware(shareId, projectId);
});
