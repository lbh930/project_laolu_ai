// 按钮状态枚举
type ButtonState = 'ready' | 'waiting' | 'sending' | 'sent';

export function createChatBar(): {
  root: HTMLDivElement; input: HTMLInputElement; btn: HTMLButtonElement;
  setSendEnabled: (ok: boolean)=>void; setButtonState: (state: ButtonState) => void; getCurrentState: () => ButtonState;
} {
  const root = document.createElement('div');
  Object.assign(root.style, {
    position: 'fixed', left: '0', right: '0', bottom: '0',
    padding: '8px 12px', background: 'rgba(0,0,0,0.55)',
    display: 'flex', gap: '8px', alignItems: 'center', zIndex: '2147483647',
    backdropFilter: 'saturate(120%) blur(2px)',
  } as CSSStyleDeclaration);

  const input = document.createElement('input');
  input.type = 'text';
  input.placeholder = '输入要发给数字人的文本...';
  Object.assign(input.style, {
    flex: '1', padding: '10px 12px', borderRadius: '8px',
    border: '1px solid #555', outline: 'none', background: '#111', color: '#eee',
  } as CSSStyleDeclaration);

  const btn = document.createElement('button');
  btn.textContent = '发送';
  Object.assign(btn.style, {
    padding: '10px 16px', borderRadius: '8px', border: 'none',
    cursor: 'pointer', fontWeight: '600', background: '#e6e6e6',
  } as CSSStyleDeclaration);

  let currentState: ButtonState = 'ready';

  // 根据状态更新按钮外观
  const updateButtonByState = (state: ButtonState) => {
    currentState = state;
    switch (state) {
      case 'ready':
        btn.disabled = false;
        btn.textContent = '发送';
        btn.style.opacity = '1.0';
        break;
      case 'waiting':
        btn.disabled = true;
        btn.textContent = '等待中…';
        btn.style.opacity = '0.5';
        break;
      case 'sending':
        btn.disabled = true;
        btn.textContent = '发送中...';
        btn.style.opacity = '0.6';
        break;
      case 'sent':
        btn.disabled = true;
        btn.textContent = '已发送';
        btn.style.opacity = '0.7';
        break;
    }
  };

  // ✅ 外部调用 setSendEnabled：设置按钮状态
  const setSendEnabled = (ok: boolean) => {
    updateButtonByState(ok ? 'ready' : 'waiting');
  };

  const setButtonState = (state: ButtonState) => {
    updateButtonByState(state);
  };

  const getCurrentState = (): ButtonState => {
    return currentState;
  };

  root.appendChild(input);
  root.appendChild(btn);
  return { root, input, btn, setSendEnabled, setButtonState, getCurrentState };
}

export function wireChatBar(
  send: (payload: any) => Promise<any> | void,
  input: HTMLInputElement,
  btn: HTMLButtonElement,
  opts?: { onDone?: ()=>void; setSendEnabled?: (enabled: boolean) => void; setButtonState?: (state: ButtonState) => void }
) {
  const stop = (e: Event) => e.stopPropagation();
  input.addEventListener('keydown', stop, { capture: true });
  input.addEventListener('keyup', stop, { capture: true });
  input.addEventListener('keypress', stop, { capture: true });

  let isSending = false;

  async function sendCurrent() {
    console.log('[ChatBar] ✅ sendCurrent called');
    const text = (input.value ?? '').trim();
    if (!text || isSending) return;

    isSending = true;
    opts?.setButtonState?.('sending');
    console.log('[ChatBar] 🚀 Sending started');

    try {
      await send({ type: 'chat', text });
      input.value = '';
      input.focus();
      console.log('[ChatBar] ✅ Message sent and input cleared');

      // 保持发送中状态，不立即恢复
      if (opts?.onDone) opts.onDone();

    } catch (err) {
      console.warn('[ChatBar] ❌ Send failed:', err);
      opts?.setButtonState?.('ready');
    } finally {
      isSending = false;
    }
  }

  input.addEventListener('keydown', (e: KeyboardEvent) => {
    if (e.key === 'Enter' && !btn.disabled) {
      e.preventDefault();
      sendCurrent();
    }
  });

  btn.addEventListener('click', () => {
    if (!btn.disabled) {
      sendCurrent();
    }
  });
}
