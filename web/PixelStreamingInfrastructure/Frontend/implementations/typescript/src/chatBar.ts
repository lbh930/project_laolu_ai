export function createChatBar(): { root: HTMLDivElement; input: HTMLInputElement; btn: HTMLButtonElement } {
  const root = document.createElement('div');
  root.id = 'chat-bar';
  Object.assign(root.style, {
    position: 'fixed', left: '0', right: '0', bottom: '0',
    padding: '8px 12px', background: 'rgba(0,0,0,0.55)',
    display: 'flex', gap: '8px', alignItems: 'center',
    zIndex: '2147483647', backdropFilter: 'saturate(120%) blur(2px)',
  } as CSSStyleDeclaration);

  const input = document.createElement('input');
  input.id = 'chat-input'; input.type = 'text';
  input.placeholder = '输入要发给数字人的文本...';
  Object.assign(input.style, {
    flex: '1', padding: '10px 12px', borderRadius: '8px',
    border: '1px solid #555', outline: 'none', background: '#111', color: '#eee',
  } as CSSStyleDeclaration);

  const btn = document.createElement('button');
  btn.id = 'chat-send'; btn.textContent = '发送';
  Object.assign(btn.style, {
    padding: '10px 16px', borderRadius: '8px', border: 'none',
    cursor: 'pointer', fontWeight: '600', background: '#e6e6e6',
  } as CSSStyleDeclaration);

  root.appendChild(input); root.appendChild(btn);
  return { root, input, btn };
}

export function wireChatBar(send: (payload: any) => void, input: HTMLInputElement, btn: HTMLButtonElement) {
  const stop = (e: Event) => e.stopPropagation();
  input.addEventListener('keydown', stop, { capture: true });
  input.addEventListener('keyup', stop, { capture: true });
  input.addEventListener('keypress', stop, { capture: true });

  function sendCurrent() {
    const text = (input.value ?? '').trim();
    if (!text) return;
    const payload = { type: 'chat', text };
    try { send(payload); console.log('[PS] Sent chat:', payload); }
    catch (err) { console.error('[PS] emit failed:', err); }
    input.value = ''; input.focus();
  }
  input.addEventListener('keydown', (e: KeyboardEvent) => { if (e.key === 'Enter') { e.preventDefault(); sendCurrent(); } });
  btn.addEventListener('click', () => sendCurrent());
}

export function keepChatBarAlive(restore: (send: (p:any)=>void) => void) {
  const mo = new MutationObserver(() => {
    if (!document.getElementById('chat-bar')) {
      const { createChatBar } = require('./chatBar');
      const rebuilt = createChatBar();
      document.body.appendChild(rebuilt.root);
      restore((p)=>{ /* real send injected by caller */ });
      console.warn('[PS] chat-bar re-inserted.');
    }
  });
  mo.observe(document.body, { childList: true, subtree: true });
}
