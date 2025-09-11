export * from '@epicgames-ps/lib-pixelstreamingfrontend-ue5.6';
export * from '@epicgames-ps/lib-pixelstreamingfrontend-ui-ue5.6';

import { Config, PixelStreaming, Logger, LogLevel } from '@epicgames-ps/lib-pixelstreamingfrontend-ue5.6';
import { Application, PixelStreamingApplicationStyle } from '@epicgames-ps/lib-pixelstreamingfrontend-ui-ue5.6';

// ===== 全局调试入口（兼容你之前的做法） =====
declare global {
    interface Window {
        pixelStreaming?: PixelStreaming;
        arcwarePlayer?: any;
    }
}

/** 我自己的注释：动态创建底部聊天条，避免被 UI 模板覆盖/样式冲突 */
function createChatBar(): { root: HTMLDivElement; input: HTMLInputElement; btn: HTMLButtonElement } {
    const root = document.createElement('div');
    root.id = 'chat-bar';
    Object.assign(root.style, {
        position: 'fixed',
        left: '0',
        right: '0',
        bottom: '0',
        padding: '8px 12px',
        background: 'rgba(0,0,0,0.55)',
        display: 'flex',
        gap: '8px',
        alignItems: 'center',
        zIndex: '2147483647',
        backdropFilter: 'saturate(120%) blur(2px)',
    } as CSSStyleDeclaration);

    const input = document.createElement('input');
    input.id = 'chat-input';
    input.type = 'text';
    input.placeholder = '输入要发给数字人的文本...';
    Object.assign(input.style, {
        flex: '1',
        padding: '10px 12px',
        borderRadius: '8px',
        border: '1px solid #555',
        outline: 'none',
        background: '#111',
        color: '#eee',
    } as CSSStyleDeclaration);

    const btn = document.createElement('button');
    btn.id = 'chat-send';
    btn.textContent = '发送';
    Object.assign(btn.style, {
        padding: '10px 16px',
        borderRadius: '8px',
        border: 'none',
        cursor: 'pointer',
        fontWeight: '600',
        background: '#e6e6e6',
    } as CSSStyleDeclaration);

    root.appendChild(input);
    root.appendChild(btn);
    return { root, input, btn };
}

/** 我自己的注释：把键盘/点击事件封装，传一个 send 函数（Arcware/Epic 都能用） */
function wireChatBar(send: (payload: any) => void, input: HTMLInputElement, btn: HTMLButtonElement) {
    // 防止键盘事件透传到 UE（WASD/Enter 等）
    const stopToUE = (e: Event) => { e.stopPropagation(); };
    input.addEventListener('keydown', stopToUE, { capture: true });
    input.addEventListener('keyup', stopToUE, { capture: true });
    input.addEventListener('keypress', stopToUE, { capture: true });

    function sendCurrent() {
        const text = (input.value ?? '').trim();
        if (!text) return;
        const payload = { type: 'chat', text };
        try {
            send(payload);
            console.log('[PS] Sent chat payload to UE:', text);
        } catch (err) {
            console.error('[PS] Failed to emit UI interaction:', err);
        }
        input.value = '';
        input.focus();
    }

    // Enter 发送
    input.addEventListener('keydown', (e: KeyboardEvent) => {
        if (e.key === 'Enter') {
            e.preventDefault();
            sendCurrent();
        }
    });

    // 点击发送
    btn.addEventListener('click', () => sendCurrent());
}

/** 我自己的注释：最小的 DOM 容器，Epic/Arcware 都往里挂画面 */
function ensurePlayerContainer(): HTMLElement {
    let container = document.getElementById('player');
    if (!container) {
        container = document.createElement('div');
        container.id = 'player';
        Object.assign(container.style, { width: '100%', height: '100svh' });
        document.body.appendChild(container);
    }
    return container;
}

/** ------- Arcware 模式（shareId 存在时触发） ------- */
import * as Arcware from '@arcware-cloud/pixelstreaming-websdk'; // 顶部同文件其余 import 旁边加这一行

async function bootArcware(shareId: string, projectId?: string) {
  // 我自己的注释：避免改 tsconfig/webpack 配置，直接用静态导入，并对导出名做兜底
  const AW: any = Arcware as any;

  const container = ensurePlayerContainer();

  // 我自己的注释：不同版本的导出名不一致，这里做个优先级探测
  const Ctor =
    AW.Player ??
    AW.WebRtcPlayerController ??
    AW.default; // 再不行就用 default

  if (!Ctor) {
    console.error('[Arcware] 未找到可用构造函数（Player/WebRtcPlayerController/default 都不存在）');
    return;
  }

  const player: any = new Ctor({
    shareId,
    projectId,
    container,
    autoPlay: true,   // 我自己的注释：减少一次用户手势
    startMuted: true, // 我自己的注释：规避浏览器自动播放策略
  });

  // 聊天条：沿用你原来逻辑
  const { root, input, btn } = createChatBar();
  document.body.appendChild(root);
  wireChatBar((payload) => player.emitUIInteraction?.(payload), input, btn);

  // UE -> 前端：不同 SDK 名称可能不同，这里做可选链兜底
  player.addResponseEventListener?.((msg: any) => {
    console.log('[UE->Front][Arcware:addResponseEventListener]', msg);
  });
  player.on?.('response', (msg: any) => {
    console.log('[UE->Front][Arcware:on(response)]', msg);
  });

  // 防止 UI 更新把聊天条移除
  const mo = new MutationObserver(() => {
    if (!document.getElementById('chat-bar')) {
      const rebuilt = createChatBar();
      document.body.appendChild(rebuilt.root);
      wireChatBar((p) => player.emitUIInteraction?.(p), rebuilt.input, rebuilt.btn);
      console.warn('[PS] chat-bar was removed by DOM updates, re-inserted.');
    }
  });
  mo.observe(document.body, { childList: true, subtree: true });

  (window as any).arcwarePlayer = player; // 调试入口
  console.log('[Mode] Arcware WebSDK init done.');
}

/** ------- Epic 模式（默认） ------- */
function bootEpic() {
    // 低噪声日志
    Logger.InitLogging(LogLevel.Warning, true);

    const styles = new PixelStreamingApplicationStyle();
    styles.applyStyleSheet();

    const config = new Config({ useUrlParams: true });
    const stream = new PixelStreaming(config);

    const app = new Application({
        stream,
        onColorModeChanged: (isLight) => styles.setColorMode(isLight),
    });

    const container = ensurePlayerContainer();
    container.appendChild(app.rootElement);

    const { root, input, btn } = createChatBar();
    document.body.appendChild(root);
    wireChatBar((payload) => {
        const data = typeof payload === 'string' ? payload : JSON.stringify(payload);
        stream.emitUIInteraction(data);
    }, input, btn);

    // UE 侧顶层键是什么，这里就监听什么；可以按需再加
    const logAny = (name: string) => (resp: string) => {
    let obj: any = resp;
    if (typeof resp === 'string') { try { obj = JSON.parse(resp); } catch {} }
    console.log(`[UE->Front][Epic:${name}]`, obj);
    };

    // 常用事件名（Console/聊天/示例）；你也可以加自己的，比如 'face', 'tts'...
    const names = ['Console', 'chat', 'Example_Event_01'];

    // 我自己的注释：逐个注册；如果某个名字 UE 侧没有发，也只是静默
    for (const n of names) {
    (stream as any).addResponseEventListener(n, logAny(n));
    }

    (window as any).pixelStreaming = stream;

    // 防止 DOM 变更丢失聊天条
    const mo = new MutationObserver(() => {
        if (!document.getElementById('chat-bar')) {
            const rebuilt = createChatBar();
            document.body.appendChild(rebuilt.root);
            wireChatBar((p) => {
                const data = typeof p === 'string' ? p : JSON.stringify(p);
                stream.emitUIInteraction(data);
            }, rebuilt.input, rebuilt.btn);
            console.warn('[PS] chat-bar was removed by DOM updates, re-inserted.');
        }
    });
    mo.observe(document.body, { childList: true, subtree: true });

    console.log('[Mode] Epic PixelStreamingInfrastructure init done.');
}

/** ------- 启动入口 ------- */
document.body.onload = function () {
    const u = new URL(window.location.href);
    const shareId = u.searchParams.get('shareId') || undefined;
    const projectId = u.searchParams.get('projectId') || undefined;
    const forceEpic = u.searchParams.get('epic') === '1';
    
    console.log('forceEpic', forceEpic);
    console.log('sharedId', shareId);
    console.log('projectId', projectId);

    if (!forceEpic && shareId) {
        // Arcware 模式
        console.log('[Mode] Boosting Arcware');
        bootArcware(shareId, projectId);
    } else {
        // Epic 模式
        console.log('[Mode] Boosting Epic');
        bootEpic();
    }
};