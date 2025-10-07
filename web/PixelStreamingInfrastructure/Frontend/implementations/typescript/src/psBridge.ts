// psBridge.ts —— Pixel Streaming 前端桥接 & 请求/响应封装
type PSAdapter = {
  emit: (data: any) => void;
  onResponse: (cb: (msg: any) => void) => void;
};

class PSBridge {
  private adapter: PSAdapter;
  private pending = new Map<string, (m: any)=>void>();

  constructor(adapter: PSAdapter) {
    this.adapter = adapter;
    this.adapter.onResponse((raw) => {
      let msg = raw;
      try { msg = typeof raw === 'string' ? JSON.parse(raw) : raw; } catch {}
      const id = msg?.requestId;
      if (id && this.pending.has(id)) { this.pending.get(id)!(msg); this.pending.delete(id); }
    });
  }

  send(data: any) { this.adapter.emit(data); }

  request<T = any>(payload: Record<string, any>) {
    const requestId = String(Date.now()) + '-' + Math.random().toString(36).slice(2);
    const p = new Promise<T>((resolve, reject) => {
      this.pending.set(requestId, (resp: any) => {
        if (resp?.type?.endsWith('/ok')) resolve(resp.value as T);
        else reject(new Error(resp?.message || 'bridge error'));
      });
    });
    this.adapter.emit({ ...payload, requestId });
    return p;
  }
}

// 工具：构造 Arcware 适配器
export function arcwareAdapter(Application: any): PSAdapter {
  return {
    emit: (data) => Application.emitUIInteraction(data),
    onResponse: (cb) => Application.getApplicationResponse((m: any)=>cb(m)),
  };
}

// 工具：构造 Epic 适配器（基于 PixelStreaming stream）
export function epicAdapter(stream: any): PSAdapter {
  // Epic官方前端库通常提供 addResponseEventListener / onResponse
  const addResp = (cb: (m:any)=>void) => {
    if (typeof stream.addResponseEventListener === 'function') {
      stream.addResponseEventListener(cb);
    } else if (typeof stream.onResponse === 'function') {
      stream.onResponse(cb);
    } else {
      // 兜底：尝试通用事件接口
      stream.addEventListener?.('response', (e: any) => cb(e?.detail ?? e));
    }
  };
  return {
    emit: (data) => stream.emitUIInteraction(typeof data === 'string' ? data : JSON.stringify(data)),
    onResponse: (cb) => addResp(cb),
  };
}

export { PSBridge };
