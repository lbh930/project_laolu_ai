export * from '@epicgames-ps/lib-pixelstreamingfrontend-ue5.6';
export * from '@epicgames-ps/lib-pixelstreamingfrontend-ui-ue5.6';

import { Config, PixelStreaming, Logger, LogLevel } from '@epicgames-ps/lib-pixelstreamingfrontend-ue5.6';
import { Application as EpicApplication, PixelStreamingApplicationStyle } from '@epicgames-ps/lib-pixelstreamingfrontend-ui-ue5.6';

import { createChatBar, wireChatBar } from './chatBar';
import { PSBridge, epicAdapter } from './psBridge';

function ensurePlayerContainer(): HTMLElement {
  let c = document.getElementById('player');
  if (!c) { c = document.createElement('div'); c.id = 'player';
    Object.assign(c.style, { width: '100%', height: '100svh', background: '#000' }); document.body.appendChild(c); }
  return c;
}

function bootEpic() {
  Logger.InitLogging(LogLevel.Warning, true);
  const styles = new PixelStreamingApplicationStyle(); styles.applyStyleSheet();
  const config = new Config({ useUrlParams: true });
  const stream = new PixelStreaming(config);
  const app = new EpicApplication({ stream, onColorModeChanged: (isLight) => styles.setColorMode(isLight) });

  const container = ensurePlayerContainer();
  container.appendChild(app.rootElement);

  const bridge = new PSBridge(epicAdapter(stream));

  const { root, input, btn, setSendEnabled } = createChatBar();
  document.body.appendChild(root);

  wireChatBar((p)=>bridge.request({ ...p, type:'chat' }), input, btn, {
    beforeSend: async () => {
      try {
        const ok = await bridge.request<boolean>({
          type:'call',
          target: { by:'tag', value:'Avatar' },
          component:'HumanState',
          method:'CanReceiveNewMessage'
        });
        setSendEnabled(ok);
        return ok;
      } catch { setSendEnabled(false); return false; }
    }
  });

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

  (window as any).pixelStreaming = stream;
  (window as any).bridge = bridge;
  console.log('[Mode] Epic init ready.');
}

document.addEventListener('DOMContentLoaded', bootEpic);
