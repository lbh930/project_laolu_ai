export function createChatBar(): {
  root: HTMLDivElement; input: HTMLInputElement; btn: HTMLButtonElement;
  setSendEnabled: (ok: boolean)=>void; setBusy: (b: boolean)=>void;
} {
  const root = document.createElement('div');
  Object.assign(root.style, {
    position: 'fixed', left: '0', right: '0', bottom: '0',
    padding: '8px 12px', background: 'rgba(0,0,0,0.55)',
    display: 'flex', gap: '8px', alignItems: 'center', zIndex: '2147483647',
    backdropFilter: 'saturate(120%) blur(2px)',
  } as CSSStyleDeclaration);

  const input = document.createElement('input');
  input.type = 'text'; input.placeholder = '输入要发给数字人的文本...';
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

  const setSendEnabled = (ok: boolean) => {
    btn.disabled = !ok;
    btn.style.opacity = ok ? '1.0' : '0.5';
    btn.textContent = ok ? '发送' : '等待中…';
  };

  const setBusy = (b: boolean) => {
    btn.disabled = b || btn.disabled;
    btn.style.opacity = b ? '0.6' : (btn.disabled ? '0.5' : '1.0');
  };

  root.appendChild(input); root.appendChild(btn);
  return { root, input, btn, setSendEnabled, setBusy };
}

export function wireChatBar(send: (payload: any) => Promise<any> | void,
  input: HTMLInputElement, btn: HTMLButtonElement,
  opts?: { beforeSend?: ()=>Promise<boolean>|boolean; onDone?: ()=>void }
) {
  const stop = (e: Event) => e.stopPropagation();
  input.addEventListener('keydown', stop, { capture: true });
  input.addEventListener('keyup', stop, { capture: true });
  input.addEventListener('keypress', stop, { capture: true });

  async function sendCurrent() {
    const text = (input.value ?? '').trim();
    if (!text) return;
    if (opts?.beforeSend && !(await opts.beforeSend())) return;
    try { await send({ type:'chat', text }); } catch {}
    input.value = ''; input.focus(); opts?.onDone?.();
  }
  input.addEventListener('keydown', (e: KeyboardEvent) => { if (e.key === 'Enter') { e.preventDefault(); sendCurrent(); } });
  btn.addEventListener('click', () => sendCurrent());
}
