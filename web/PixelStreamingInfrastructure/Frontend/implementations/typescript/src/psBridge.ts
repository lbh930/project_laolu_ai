// psBridge.ts —— Ultra Debug 版本：用于 Pixel Streaming 2 Response 调试
// 保留 arcwareAdapter / epicAdapter 接口
// ⚡ 重点：每一个 emit / response / hook 都会打印详细日志

type PSAdapter = {
  emit: (data: any) => void;
  onResponse: (cb: (msg: any) => void) => void;
};

class PSBridge {
  private adapter: PSAdapter;
  private pending = new Map<string, (m: any) => void>();

  constructor(adapter: PSAdapter) {
    this.adapter = adapter;
    console.log('[⚡PSBridge] Initialized with adapter:', adapter);

    // 注册响应监听
    this.adapter.onResponse((raw) => {
      console.log(`[PSBridge] ⬇️ Adapter onResponse fired @${new Date().toISOString()}:`, raw);
      try {
        const msg = typeof raw === 'string' ? JSON.parse(raw) : raw;
        console.log('[PSBridge] ⬇️ Parsed response object:', msg);
        const id = msg?.requestId;
        if (id && this.pending.has(id)) {
          console.log(`[PSBridge] ✅ Matched pending requestId=${id}`);
          this.pending.get(id)!(msg);
          this.pending.delete(id);
        } else {
          console.warn(`[PSBridge] ❌ No pending request found for requestId=${id}`);
        }
      } catch (err) {
        console.warn('[PSBridge] ⚠️ Failed to parse response:', err, raw);
      }
    });
  }

  send(data: any) {
    const pretty = JSON.stringify(data, null, 2);
    console.log(`[PSBridge] ⬆️ Sending raw message @${new Date().toISOString()}:\n${pretty}`);
    this.adapter.emit(data);
  }

  request<T = any>(payload: Record<string, any>) {
    const requestId = `${Date.now()}-${Math.random().toString(36).slice(2)}`;
    const msg = { ...payload, requestId };
    const pretty = JSON.stringify(msg, null, 2);
    console.log(`[PSBridge] ⬆️ Sending request @${new Date().toISOString()}:\n${pretty}`);

    return new Promise<T>((resolve, reject) => {
      this.pending.set(requestId, (resp: any) => {
        console.log(`[⚡PSBridge] ⬅️ Response matched requestId=${requestId}:\n${JSON.stringify(resp, null, 2)}`);
        
        // 检查多种成功条件
        if (resp?.type?.endsWith('/ok') || resp?.ok === true || resp?.success === true || resp?.status === 'ok') {
          // 尝试多种可能的数据字段
          const result = resp.value !== undefined ? resp.value : 
                        resp.data !== undefined ? resp.data :
                        resp.result !== undefined ? resp.result :
                        resp;
          resolve(result as T);
        } else if (resp?.type?.endsWith('/error') || resp?.error || resp?.status === 'error') {
          reject(new Error(resp?.message || resp?.error || 'bridge error'));
        } else {
          // 如果没有明确的成功/失败标志，则当作成功处理
          console.log(`[⚡PSBridge] ✅ No explicit status, treating as success:`, resp);
          resolve(resp as T);
        }
      });
      
      // 设置超时
      setTimeout(() => {
        if (this.pending.has(requestId)) {
          this.pending.delete(requestId);
          reject(new Error(`Request timeout after 30s for requestId=${requestId}`));
        }
      }, 10000);
      
      this.adapter.emit(msg);
    });
  }
}

/* -------------------------------------------------------------------------- */
/* Pixel Streaming 2 Adapter                                                  */
/* -------------------------------------------------------------------------- */
function pixelStreaming2Adapter(stream: any): PSAdapter {
  console.log('[⚡PSBridge] pixelStreaming2Adapter init:', stream);
  
  // 深度检查 stream 对象结构
  console.log('[⚡PSBridge] Stream object structure analysis:');
  console.log('  - responseController:', stream?.responseController);
  console.log('  - inputHandler:', stream?.inputHandler);
  console.log('  - messageRouter:', stream?.messageRouter);
  console.log('  - dataChannelController:', stream?.dataChannelController);
  console.log('  - webRtcController:', stream?.webRtcController);
  console.log('  - _webRtcController:', stream?._webRtcController);
  
  // 如果有 webRtcController，也检查其结构
  const webRtc = stream?.webRtcController || stream?._webRtcController;
  if (webRtc) {
    console.log('[⚡PSBridge] WebRTC Controller structure:');
    console.log('  - responseController:', webRtc?.responseController);
    console.log('  - streamMessageController:', webRtc?.streamMessageController);
    console.log('  - dataChannelController:', webRtc?.dataChannelController);
  }

  const safeParse = (raw: any, source: string) => {
    try {
      const msg = typeof raw === 'string' ? JSON.parse(raw) : raw;
      return msg;
    } catch (err) {
      console.warn(`[⚠️PSBridge] JSON parse failed from ${source}:`, err, raw);
      return null;
    }
  };

  const addResponseHandler = (cb: (msg: any) => void) => {
    console.log('[⚡PSBridge] Installing response handlers...');
    
    // 检查可用的响应处理路径
    const webRtc = stream?.webRtcController || stream?._webRtcController;
    const responseController = stream?.responseController || webRtc?.responseController;
    const inputHandler = stream?.inputHandler || webRtc?.inputHandler;
    const messageRouter = stream?.messageRouter || webRtc?.messageRouter;
    const dataChannelController = stream?.dataChannelController || webRtc?.dataChannelController;

    console.log('[⚡PSBridge] Available response paths:');
    console.log('  responseController:', !!responseController);
    console.log('  inputHandler:', !!inputHandler);
    console.log('  messageRouter:', !!messageRouter);
    console.log('  dataChannelController:', !!dataChannelController);

    // 1. ResponseController 通道 (官方响应处理器)
    if (responseController?.addResponseEventListener) {
      const listenerName = `psbridge_${Date.now()}`;
      console.log('[⚡PSBridge] ✅ Listening via responseController');
      responseController.addResponseEventListener(listenerName, (response: string) => {
        console.log(`[⚡PSBridge] ⬇️ responseController received:`, response);
        const msg = safeParse(response, 'responseController');
        if (msg?.requestId) {
          console.log(`[⚡PSBridge] 📨 Forwarding responseController msg with requestId=${msg.requestId}`);
          cb(msg);
        } else {
          console.log(`[⚡PSBridge] ⚠️ Response without requestId:`, msg);
        }
      });
    } else {
      console.warn('[⚡PSBridge] ❌ responseController.addResponseEventListener not found');
    }

    // 2. InputHandler 通道 (备用)
    if (inputHandler?.addListener) {
      console.log('[⚡PSBridge] ✅ Listening via inputHandler (backup)');
      inputHandler.addListener((type: string, payload: any) => {
        console.log(`[⚡PSBridge] ⬇️ inputHandler event "${type}" payload:`, payload);
        const msg = safeParse(payload, 'inputHandler');
        if (msg?.requestId) cb(msg);
      });
    } else {
      console.warn('[⚡PSBridge] ❌ inputHandler.addListener not found');
    }

    // 3. MessageRouter 通道 (备用)
    if (messageRouter?.addResponseHandler) {
      console.log('[⚡PSBridge] ✅ Listening via messageRouter (backup)');
      messageRouter.addResponseHandler((payload: any) => {
        console.log('[⚡PSBridge] ⬇️ messageRouter received payload:', payload);
        const msg = safeParse(payload, 'messageRouter');
        if (msg?.requestId) cb(msg);
      });
    } else {
      console.warn('[⚡PSBridge] ❌ messageRouter.addResponseHandler not found');
    }

    // 4. 直接监听所有 DataChannel 消息（全局调试）
    const installDataChannelGlobalListener = () => {
      console.log('[⚡PSBridge] Installing global DataChannel message listener...');
      
      const controller = dataChannelController;
      if (!controller) {
        console.warn('[⚡PSBridge] ❌ No dataChannelController found for global listener');
        return;
      }

      // 尝试直接 hook DataChannel 的 onmessage
      const originalOnMessage = controller.handleOnMessage;
      if (originalOnMessage) {
        controller.handleOnMessage = function(event: MessageEvent) {
          console.log('[⚡PSBridge] 🔍 Global DataChannel message intercepted:', event.data);
          
          // 尝试解析消息
          if (typeof event.data === 'string') {
            const msg = safeParse(event.data, 'Global-DataChannel-String');
            if (msg?.requestId) {
              console.log(`[⚡PSBridge] 📨 Global intercept found requestId=${msg.requestId}`);
              cb(msg);
            }
          } else if (event.data instanceof ArrayBuffer) {
            // 对于 ArrayBuffer，尝试多种解码方式
            try {
              // 尝试 UTF-16 解码（Pixel Streaming 2 的响应格式）
              const utf16Text = new TextDecoder('utf-16').decode(event.data.slice(1));
              console.log('[⚡PSBridge] 🔍 ArrayBuffer UTF-16 decoded:', utf16Text);
              const msg = safeParse(utf16Text, 'Global-DataChannel-UTF16');
              if (msg?.requestId) {
                console.log(`[⚡PSBridge] 📨 Global UTF-16 found requestId=${msg.requestId}`);
                cb(msg);
              }
            } catch (e) {
              console.log('[⚡PSBridge] UTF-16 decode failed, trying UTF-8...');
              try {
                const utf8Text = new TextDecoder('utf-8').decode(event.data);
                console.log('[⚡PSBridge] 🔍 ArrayBuffer UTF-8 decoded:', utf8Text);
                const msg = safeParse(utf8Text, 'Global-DataChannel-UTF8');
                if (msg?.requestId) {
                  console.log(`[⚡PSBridge] 📨 Global UTF-8 found requestId=${msg.requestId}`);
                  cb(msg);
                }
              } catch (e2) {
                console.log('[⚡PSBridge] Both UTF-16 and UTF-8 decode failed for ArrayBuffer');
              }
            }
          }
          
          // 调用原始处理器
          return originalOnMessage.call(this, event);
        };
        console.log('[⚡PSBridge] ✅ Global DataChannel message interception installed');
      } else {
        console.warn('[⚡PSBridge] ❌ controller.handleOnMessage not found for global interception');
      }
    };

    // 延迟安装全局监听器
    setTimeout(installDataChannelGlobalListener, 500);

    // 5. DataChannel 直接监听 (原有的备用方法)
    setTimeout(() => {
      const controller = dataChannelController;
      const pc = controller?.peerConnection;
      console.log('[⚡PSBridge] Checking dataChannelController for direct hook:', controller);

      const hookChannel = (dc: RTCDataChannel) => {
        if (!dc) return;
        console.log(`[⚡PSBridge] Hooking DataChannel "${dc.label}" readyState=${dc.readyState}`);
        
        // 移除可能存在的旧监听器
        const existingHandler = (dc as any).__psbridgeHandler;
        if (existingHandler) {
          dc.removeEventListener('message', existingHandler);
        }
        
        const messageHandler = (evt: MessageEvent) => {
          console.log(`[⚡PSBridge] ⬇️ Direct hook message from DataChannel "${dc.label}":`, evt.data);
          const msg = safeParse(evt.data, `DataChannel(${dc.label})`);
          if (msg?.requestId) {
            console.log(`[⚡PSBridge] 📨 Direct hook forwarding msg with requestId=${msg.requestId}`);
            cb(msg);
          }
        };
        
        dc.addEventListener('message', messageHandler);
        (dc as any).__psbridgeHandler = messageHandler;
      };

      if (pc) {
        pc.ondatachannel = (e: RTCDataChannelEvent) => {
          console.log('[⚡PSBridge] ondatachannel event fired:', e.channel?.label);
          hookChannel(e.channel);
        };
      } else {
        console.warn('[⚡PSBridge] ⚠️ peerConnection not found yet');
      }

      if (controller?.dataChannel) {
        hookChannel(controller.dataChannel);
      } else {
        console.warn('[⚡PSBridge] ⚠️ No existing dataChannel found yet');
      }

      console.log('[⚡PSBridge] DataChannel direct hooks installed ✅');
    }, 1000);
  };

  return {
    emit: (data) => {
      const payload = typeof data === 'string' ? data : JSON.stringify(data);
      console.log(`[PSBridge] ⬆️ Adapter emit @${new Date().toISOString()}:\n${payload}`);
      if (stream.inputHandler?.emitUIInteraction) {
        console.log('[PSBridge] 🚀 Emitting via inputHandler.emitUIInteraction');
        stream.inputHandler.emitUIInteraction(payload);
      } else if (stream.emitUIInteraction) {
        console.log('[PSBridge] 🚀 Emitting via stream.emitUIInteraction');
        stream.emitUIInteraction(payload);
      } else {
        console.warn('[PSBridge] ❌ emit failed: no valid interaction method');
      }
    },
    onResponse: (cb) => addResponseHandler(cb),
  };
}

/* -------------------------------------------------------------------------- */
/* Arcware Adapter（正确实现）                                               */
/* -------------------------------------------------------------------------- */
export function arcwareAdapter(Application: any): PSAdapter {
  console.log('[⚡PSBridge] arcwareAdapter init with Arcware Application:', Application);
  
  // 检查 Arcware Application 对象结构
  console.log('[⚡PSBridge] Arcware Application structure analysis:');
  console.log('  - emitUIInteraction:', typeof Application?.emitUIInteraction);
  console.log('  - getApplicationResponse:', typeof Application?.getApplicationResponse);
  console.log('  - rootElement:', Application?.rootElement);

  const safeParse = (raw: any, source: string) => {
    try {
      const msg = typeof raw === 'string' ? JSON.parse(raw) : raw;
      return msg;
    } catch (err) {
      console.warn(`[⚠️PSBridge] JSON parse failed from ${source}:`, err, raw);
      return null;
    }
  };

  return {
    emit: (data) => {
      const payload = typeof data === 'string' ? data : JSON.stringify(data);
      console.log(`[PSBridge] ⬆️ Arcware Adapter emit @${new Date().toISOString()}:\n${payload}`);
      
      if (Application?.emitUIInteraction) {
        console.log('[PSBridge] 🚀 Emitting via Arcware Application.emitUIInteraction');
        Application.emitUIInteraction(typeof data === 'string' ? JSON.parse(data) : data);
      } else {
        console.warn('[PSBridge] ❌ Arcware emit failed: Application.emitUIInteraction not found');
      }
    },
    
    onResponse: (cb) => {
      console.log('[⚡PSBridge] Installing Arcware response handler...');
      
      if (Application?.getApplicationResponse) {
        console.log('[⚡PSBridge] ✅ Listening via Arcware Application.getApplicationResponse');
        Application.getApplicationResponse((response: any) => {
          console.log(`[⚡PSBridge] ⬇️ Arcware Application response received:`, response);
          
          // Arcware 响应可能是字符串或对象
          const msg = safeParse(response, 'ArcwareApplication');
          if (msg?.requestId) {
            console.log(`[⚡PSBridge] 📨 Forwarding Arcware response with requestId=${msg.requestId}`);
            cb(msg);
          } else {
            console.log(`[⚡PSBridge] ⚠️ Arcware response without requestId:`, msg);
          }
        });
      } else {
        console.warn('[⚡PSBridge] ❌ Arcware Application.getApplicationResponse not found');
      }
    },
  };
}

/* -------------------------------------------------------------------------- */
/* Epic Adapter（接口保留，但内部同 PS2）                                     */
/* -------------------------------------------------------------------------- */
export function epicAdapter(stream: any): PSAdapter {
  console.log('[PSBridge] epicAdapter → using Pixel Streaming 2 logic internally.');
  return pixelStreaming2Adapter(stream);
}

export { PSBridge };

// 调试工具函数 - 添加到全局以便在浏览器控制台中使用
(window as any).debugPSBridge = (stream: any) => {
  console.log('🔍 PSBridge Debug Tool - Analyzing stream object...');
  console.log('Stream object:', stream);
  
  // 检查 WebRTC 控制器
  const webRtc = stream?.webRtcController || stream?._webRtcController;
  console.log('WebRTC Controller:', webRtc);
  
  // 检查响应控制器
  const responseController = stream?.responseController || webRtc?.responseController;
  console.log('Response Controller:', responseController);
  if (responseController) {
    console.log('  - addResponseEventListener:', typeof responseController.addResponseEventListener);
    console.log('  - responseEventListeners:', responseController.responseEventListeners);
  }
  
  // 检查数据通道控制器
  const dataChannelController = stream?.dataChannelController || webRtc?.dataChannelController;
  console.log('DataChannel Controller:', dataChannelController);
  if (dataChannelController) {
    console.log('  - dataChannel:', dataChannelController.dataChannel);
    console.log('  - handleOnMessage:', typeof dataChannelController.handleOnMessage);
    console.log('  - peerConnection:', dataChannelController.peerConnection);
  }
  
  // 检查流消息控制器
  const streamMessageController = webRtc?.streamMessageController;
  console.log('Stream Message Controller:', streamMessageController);
  if (streamMessageController) {
    console.log('  - fromStreamerHandlers:', streamMessageController.fromStreamerHandlers);
    console.log('  - fromStreamerMessages:', streamMessageController.fromStreamerMessages);
  }
  
  // 尝试监听所有可能的响应
  if (responseController?.addResponseEventListener) {
    const debugListener = 'debug_' + Date.now();
    responseController.addResponseEventListener(debugListener, (response: any) => {
      console.log('🔍 Debug listener caught response:', response);
    });
    console.log('✅ Debug response listener installed as:', debugListener);
  }
  
  return {
    stream,
    webRtc,
    responseController,
    dataChannelController,
    streamMessageController
  };
};
