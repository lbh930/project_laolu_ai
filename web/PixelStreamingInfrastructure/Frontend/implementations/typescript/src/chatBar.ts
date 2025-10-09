export function createChatBar(): {
  root: HTMLDivElement; input: HTMLInputElement; btn: HTMLButtonElement;
  setSendEnabled: (ok: boolean)=>void;
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

  // ✅ 外部调用 setSendEnabled：设置按钮状态
  const setSendEnabled = (ok: boolean) => {
    btn.disabled = !ok;
    btn.textContent = ok ? '发送222' : '等待中…';
    btn.style.opacity = ok ? '1.0' : '0.5';
  };

  root.appendChild(input);
  root.appendChild(btn);
  return { root, input, btn, setSendEnabled };
}

export function wireChatBar(
  send: (payload: any) => Promise<any> | void,
  input: HTMLInputElement,
  btn: HTMLButtonElement,
  opts?: { onDone?: ()=>void; setSendEnabled?: (enabled: boolean) => void }
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
    btn.disabled = true;
    btn.textContent = '发送中...';
    btn.style.opacity = '0.6';
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
      btn.disabled = false;
      btn.textContent = '发送';
      btn.style.opacity = '1.0';
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
