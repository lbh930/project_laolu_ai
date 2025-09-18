export * from '@epicgames-ps/lib-pixelstreamingfrontend-ue5.6';
export * from '@epicgames-ps/lib-pixelstreamingfrontend-ui-ue5.6';

import { Config, PixelStreaming, Logger, LogLevel } from '@epicgames-ps/lib-pixelstreamingfrontend-ue5.6';
import { Application as EpicApplication, PixelStreamingApplicationStyle } from '@epicgames-ps/lib-pixelstreamingfrontend-ui-ue5.6';

import { createChatBar, wireChatBar } from './chatBar';

declare global {
  interface Window {
    pixelStreaming?: PixelStreaming;
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


function bootEpic() {
  Logger.InitLogging(LogLevel.Warning, true);

  const styles = new PixelStreamingApplicationStyle();
  styles.applyStyleSheet();

  const config = new Config({ useUrlParams: true });
  const stream = new PixelStreaming(config);
  const app = new EpicApplication({ stream, onColorModeChanged: (isLight) => styles.setColorMode(isLight) });

  const container = ensurePlayerContainer();
  container.appendChild(app.rootElement);

  const { root, input, btn } = createChatBar();
  document.body.appendChild(root);
  wireChatBar((payload) => {
    const data = typeof payload === 'string' ? payload : JSON.stringify(payload);
    stream.emitUIInteraction(data);
  }, input, btn);

  (window as any).pixelStreaming = stream;
  console.log('[Mode] Epic init done.');
}

document.addEventListener('DOMContentLoaded', () => {
  bootEpic();
});
