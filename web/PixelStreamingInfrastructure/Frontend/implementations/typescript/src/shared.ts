import { PSBridge } from './psBridge';

/**
 * 确保页面上有一个播放器容器
 */
export function ensurePlayerContainer(): HTMLElement {
  let c = document.getElementById('player');
  if (!c) { 
    c = document.createElement('div'); 
    c.id = 'player';
    Object.assign(c.style, { width: '100%', height: '100svh', background: '#000' }); 
    document.body.appendChild(c); 
  }
  return c;
}

// 全局状态检查器 - 真正的单例模式
class GlobalStateChecker {
  private static instance: GlobalStateChecker | null = null;
  private bridge: PSBridge | null = null;
  private setSendEnabled: ((enabled: boolean) => void) | null = null;
  private intervalId: NodeJS.Timeout | null = null;
  private nextCheckTime: number = 0;
  private isChecking: boolean = false;

  // 私有构造函数，防止外部直接创建实例
  private constructor() {}

  // 获取单例实例
  static getInstance(): GlobalStateChecker {
    if (!GlobalStateChecker.instance) {
      GlobalStateChecker.instance = new GlobalStateChecker();
    }
    return GlobalStateChecker.instance;
  }

  init(bridge: PSBridge, setSendEnabled: (enabled: boolean) => void) {
    // 如果已经初始化过，先停止之前的检查
    if (this.intervalId) {
      this.stop();
    }
    
    this.bridge = bridge;
    this.setSendEnabled = setSendEnabled;
    this.startPeriodicCheck();
    console.log('[GlobalStateChecker] 🔄 Initialized with new bridge and callback');
  }

  private async doCheck(): Promise<boolean> {
    if (!this.bridge || !this.setSendEnabled) {
      console.warn('[GlobalStateChecker] ❌ Not initialized');
      return false;
    }

    if (this.isChecking) {
      console.log('[GlobalStateChecker] ⏳ Already checking, skipping...');
      return false;
    }

    this.isChecking = true;
    try {
      console.log('[GlobalStateChecker] 🔍 Checking CanReceiveNewMessage...');
      const response = await this.bridge.request<boolean>({
        type:'call',
        target: { by:'tag', value:'Avatar' },
        component:'HumanState',
        method:'CanReceiveNewMessage'
      });
      
      const canReceive = !!response;
      console.log('[GlobalStateChecker] ✅ CanReceiveNewMessage result:', canReceive);
      this.setSendEnabled(canReceive);
      return canReceive;
    } catch (error) { 
      console.warn('[GlobalStateChecker] ❌ CanReceiveNewMessage failed:', error);
      this.setSendEnabled(false); 
      return false; 
    } finally {
      this.isChecking = false;
    }
  }

  private startPeriodicCheck() {
    if (this.intervalId) {
      clearInterval(this.intervalId);
    }

    this.intervalId = setInterval(async () => {
      const now = Date.now();
      if (now >= this.nextCheckTime) {
        console.log('[GlobalStateChecker] 🔄 Periodic check...');
        await this.doCheck();
        this.nextCheckTime = Date.now() + 1000; // 下次检查时间
      } else {
        console.log('[GlobalStateChecker] ⏸️ Skipping check, waiting for scheduled time...');
      }
    }, 1000);

    this.nextCheckTime = Date.now() + 1000; // 初始化下次检查时间
  }

  // 延后下一次检查
  delayNextCheck(delayMs: number) {
    this.nextCheckTime = Date.now() + delayMs;
    console.log(`[GlobalStateChecker] ⏰ Next check delayed by ${delayMs}ms`);
  }

  stop() {
    if (this.intervalId) {
      clearInterval(this.intervalId);
      this.intervalId = null;
    }
  }
}

// 获取全局单例实例
const globalStateChecker = GlobalStateChecker.getInstance();

/**
 * 创建统一的状态检查函数，用于检查UE是否能接收新消息
 */
export function createStateChecker(bridge: PSBridge, setSendEnabled: (enabled: boolean) => void) {
  // 初始化全局状态检查器
  globalStateChecker.init(bridge, setSendEnabled);

  return {
    /**
     * 启动定时检查（全局单例模式，重复调用无效）
     */
    startPeriodicCheck: () => {
      console.log('[StateChecker] 🔄 Periodic check already managed by global checker');
    }
  };
}

/**
 * 通用的聊天栏配置选项
 */
export interface ChatBarOptions {
  onDone?: () => void;
  setSendEnabled: (enabled: boolean) => void;
  setButtonState?: (state: 'ready' | 'waiting' | 'sending' | 'sent') => void;
}

/**
 * 创建标准的聊天栏配置选项
 */
export function createChatBarOptions(
  bridge: PSBridge, 
  setSendEnabled: (enabled: boolean) => void, 
  setButtonState?: (state: 'ready' | 'waiting' | 'sending' | 'sent') => void,
  getCurrentState?: () => 'ready' | 'waiting' | 'sending' | 'sent'
): ChatBarOptions {
  createStateChecker(bridge, setSendEnabled);
  
  return {
    onDone: () => {
      console.log('[ChatBar] ✅ Send completed, checking current state...');
      // 只有在当前状态是发送中才可以改成已发送
      const currentState = getCurrentState?.();
      if (currentState === 'sending') {
        console.log('[ChatBar] 📤 Current state is sending, changing to sent');
        setButtonState?.('sent');
      } else {
        console.log('[ChatBar] ⚠️ Current state is not sending, skipping state change. Current:', currentState);
      }
      // 发送完成后延后下一次状态检查，给UE时间处理消息
      globalStateChecker.delayNextCheck(1000);
    },
    setSendEnabled: setSendEnabled,
    setButtonState: setButtonState
  };
}