export * from '@epicgames-ps/lib-pixelstreamingfrontend-ue5.6';
export * from '@epicgames-ps/lib-pixelstreamingfrontend-ui-ue5.6';

import { Config, PixelStreaming, Logger, LogLevel } from '@epicgames-ps/lib-pixelstreamingfrontend-ue5.6';
import { Application as EpicApplication, PixelStreamingApplicationStyle } from '@epicgames-ps/lib-pixelstreamingfrontend-ui-ue5.6';

import { createChatBar, wireChatBar } from './chatBar';
import { PSBridge, epicAdapter } from './psBridge';
import { ensurePlayerContainer, createStateChecker, createChatBarOptions } from './shared';

function bootEpic() {
  Logger.InitLogging(LogLevel.Warning, true);
  const styles = new PixelStreamingApplicationStyle(); styles.applyStyleSheet();
  const config = new Config({ useUrlParams: true });
  const stream = new PixelStreaming(config);
  const app = new EpicApplication({ stream, onColorModeChanged: (isLight) => styles.setColorMode(isLight) });

  const container = ensurePlayerContainer();
  container.appendChild(app.rootElement);

  const bridge = new PSBridge(epicAdapter(stream));

  const { root, input, btn, setSendEnabled, setButtonState, getCurrentState } = createChatBar();
  document.body.appendChild(root);

  // 使用共享的状态检查器
  const { startPeriodicCheck } = createStateChecker(bridge, setSendEnabled);
  
  // 使用标准的聊天栏配置
  const chatBarOptions = createChatBarOptions(bridge, setSendEnabled, setButtonState, getCurrentState);

  wireChatBar((p)=>bridge.request({ ...p, type:'chat' }), input, btn, chatBarOptions);

  // 启动定时检查
  startPeriodicCheck();

  (window as any).pixelStreaming = stream;
  (window as any).bridge = bridge;
  console.log('[Mode] Epic init ready.');
}

document.addEventListener('DOMContentLoaded', bootEpic);