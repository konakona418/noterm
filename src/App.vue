<script setup lang="ts">
import { ref, onMounted } from 'vue';
import { Minimize2, X } from 'lucide-vue-next';
import { invoke, callback } from './webui-ext';
import TerminalContainer from './Components/TerminalContainer.vue';
import TabBar from './Components/TabBar.vue';

type Session = { uid: string; title: string; ptyId?: number | null; command?: string };

const genUid = () => (typeof crypto !== 'undefined' && typeof (crypto as any).randomUUID === 'function') ? (crypto as any).randomUUID() : (Date.now().toString(36) + Math.random().toString(36).slice(2));

const sessions = ref<Session[]>([{ uid: genUid(), title: 'PowerShell', ptyId: null, command: 'powershell.exe' }]);
const active = ref(0);

// whether the native/webui connection is established
const webuiReady = ref(false);

// pending requests keyed by token -> session index
const pendingByToken = new Map<number, number>();
// map PTY id -> session uid so we can route output to the correct component
const idToUid = new Map<number, string>();
let nextRequestToken = 1;

const terminalRefs = ref<Record<string, any>>({});
// temporary buffer for outputs received before a terminal ref is ready
const pendingOutput = new Map<number, Uint8Array[]>();

function setTerminalRef(uid: string) {
  return (el: any) => {
    terminalRefs.value[uid] = el;
    // if this session already has a ptyId, flush any pending output
    const idx = sessions.value.findIndex((s) => s.uid === uid);
    const ptyId = idx >= 0 ? sessions.value[idx]?.ptyId : null;
    if (ptyId != null) {
      const pending = pendingOutput.get(ptyId);
      if (pending && pending.length > 0 && el && typeof el.writeOutput === 'function') {
        for (const chunk of pending) el.writeOutput(chunk);
        pendingOutput.delete(ptyId);
      }
    }
  };
}

function displayTitleFromCommand(command?: string) {
  if (!command) return 'Shell';
  const cmd = command.toLowerCase();
  if (cmd.includes('pwsh')) return 'PowerShell';
  if (cmd.includes('powershell')) return 'PowerShell';
  if (cmd.includes('cmd')) return 'Command Prompt';
  if (cmd.includes('bash') || cmd.includes('wsl')) return 'Bash';
  return command.split(/[\\/\\\\ ]+/).pop() ?? 'Shell';
}

function createTab(command?: string) {
  const cmd = command ?? 'powershell.exe';
  const title = displayTitleFromCommand(cmd);
  console.log('Creating tab with command:', cmd, 'title:', title);

  sessions.value.push({ uid: genUid(), title, ptyId: null, command: cmd });
  // select the newly created tab
  active.value = sessions.value.length - 1;
}

function onRequestPty(index: number, cols: number, rows: number) {
  // Generate a token to correlate request/response
  const token = nextRequestToken++;
  pendingByToken.set(token, index);
  // Decide command based on session title and ask native to create a PTY with token;
  // native will call back with webui_created_pty(id, token)
  const session = sessions.value[index];
  const cmd = session?.command ?? 'powershell.exe';
  // send command as 4th argument
  invoke('webui_init_terminal', cols, rows, token, cmd)
    .then(() => {
      console.log('Requested PTY creation for session', index, 'with token', token);
    })
    .catch((err) => {
      console.error('Failed to request PTY creation:', err);
    });
}

const minimize = () =>
  invoke('webui_minimize').catch((err) => {
    console.error('Failed to minimize window:', err);
  });
const closeWin = () =>
  invoke('webui_close')
    .catch((err) => {
      console.error('Failed to close window:', err);
    });

function handleCreated(id: number, token: number) {
  const idx = pendingByToken.get(token);
  if (idx === undefined) {
    console.warn('Received created PTY id with unknown token:', id, token);
    return;
  }
  pendingByToken.delete(token);
  sessions.value[idx].ptyId = id;
  idToUid.set(id, sessions.value[idx].uid);
}

function handleOutputAvailable(id: number) {
  // ask native to send the bytes for this PTY (native will respond via webui_receive_output)
  console.log('Output available for PTY id', id);
  invoke('webui_pull_output', id)
    .catch((err) => {
      console.error('Failed to pull output for PTY id', id, ':', err);
    });
}

function handleReceiveOutput(data: Uint8Array) {
  if (data.byteLength < 4) return;
  const dv = new DataView(data.buffer, data.byteOffset, data.byteLength);
  const id = dv.getInt32(0, true);
  const payload = new Uint8Array(data.buffer, data.byteOffset + 4, data.byteLength - 4);
  const uid = idToUid.get(id);

  console.log('Received output for PTY id', id, 'mapped to session uid', uid);

  if (uid === undefined) return;
  const child = terminalRefs.value[uid];
  if (child && typeof child.writeOutput === 'function') {
    child.writeOutput(payload);
  } else {
    console.warn('No terminal ref or writeOutput method for session uid', uid, '- buffering output');
    // buffer until the component ref is ready
    const ptyId = id;
    const arr = pendingOutput.get(ptyId) ?? [];
    arr.push(payload);
    pendingOutput.set(ptyId, arr);
  }
}

function closeTab(i: number) {
  if (i < 0 || i >= sessions.value.length) return;
  const session = sessions.value[i];
  const ptyId = session.ptyId;
  if (ptyId != null) {
    invoke('webui_close_pty', ptyId).catch((err) => {
      console.error('Failed to close PTY', ptyId, err);
    });
    // cleanup maps/buffers
    idToUid.delete(ptyId);
    pendingOutput.delete(ptyId);
  }
  // remove session and terminal ref
  const uid = session.uid;
  sessions.value.splice(i, 1);
  // remove ref entry for this uid
  delete terminalRefs.value[uid];
  // rebuild idToUid map to keep mappings accurate
  idToUid.clear();
  sessions.value.forEach((s) => {
    if (s.ptyId != null) idToUid.set(s.ptyId, s.uid);
  });

  // If no sessions remain, exit the app
  if (sessions.value.length === 0) {
    // close the window / host
    closeWin();
    return;
  }

  // adjust active index: if the closed tab was before the current active, shift active left
  if (active.value > i) {
    active.value = Math.max(0, active.value - 1);
  } else if (active.value === i) {
    // if we closed the active tab, select the one at the same index (or previous if at end)
    active.value = Math.min(i, sessions.value.length - 1);
  }
}

onMounted(() => {
  callback('webui_created_pty', (id: number, token: number) => handleCreated(id, token));
  callback('webui_output_available', (id: number) => handleOutputAvailable(id));
  callback('webui_receive_output', (data: Uint8Array) => handleReceiveOutput(data));
  callback('webui_ready', () => {
    console.log('webui ready');
    webuiReady.value = true;
  });
});

</script>
<template>
  <div class="window-wrapper">
    <div id="ui-container">
      <div id="titlebar">
        <div class="titlebar-left">
          <span id="title">NoTerm</span>
          <TabBar :tabs="sessions.map(s => ({ id: s.uid, title: s.title }))" :activeIndex="active"
            @update:activeIndex="(i) => active = i" @tab-click="(t) => console.log('tab-click', t)"
            @add-tab="(cmd) => createTab(cmd)" @close-tab="(i) => closeTab(i)" />
        </div>
        <div id="buttons">
          <button class="icon-btn" @click="minimize" aria-label="Minimize">
            <Minimize2 :size="14" :stroke-width="4" />
          </button>
          <button class="icon-btn" @click="closeWin" aria-label="Close">
            <X :size="14" :stroke-width="4" />
          </button>
        </div>
      </div>

      <div id="content">
        <div style="position:relative; width:100%; height:100%;">
          <component v-for="(s, i) in sessions" :is="TerminalContainer" :key="s.uid" :ptyId="s.ptyId ?? null"
            :connected="webuiReady" @request-pty="(cols, rows) => onRequestPty(i, cols, rows)"
            :ref="setTerminalRef(s.uid)" v-show="active === i" />
        </div>
      </div>
    </div>
  </div>

  <svg style="display: none;">
    <defs>
      <filter id="crt-glow">
        <feGaussianBlur stdDeviation="8" result="blur" />
        <feComposite in="SourceGraphic" in2="blur" operator="over" />
      </filter>
      <filter id="noiseFilter">
        <feTurbulence type="fractalNoise" baseFrequency="0.65" numOctaves="3" stitchTiles="stitch" />
      </filter>
    </defs>
  </svg>
</template>

<style scoped>
.window-wrapper {
  position: fixed;
  top: 0;
  left: 0;
  right: 0;
  bottom: 0;
  background: transparent;
  display: flex;
  box-sizing: border-box;
}

#ui-container {
  flex: 1;
  display: flex;
  flex-direction: column;
  background: rgba(30, 30, 30, 0.7);
  border-radius: 12px;
  backdrop-filter: blur(20px);
  -webkit-backdrop-filter: blur(20px);
  border: 1px solid rgba(255, 255, 255, 0.1);
  overflow: hidden;
}

#titlebar {
  height: 40px;
  background: rgba(0, 0, 0, 0.2);
  display: flex;
  align-items: flex-end;
  justify-content: space-between;
  padding: 0 12px;
  flex-shrink: 0;
  -webkit-app-region: drag;
  user-select: none;
  border-bottom: 1px solid rgba(255, 255, 255, 0.03);
}

.titlebar-left {
  display: flex;
  align-items: center;
  gap: 12px;
  max-width: calc(100% - 175px);
}

#title {
  font-size: 12px;
  color: rgba(255, 255, 255, 0.6);
}

#buttons {
  display: flex;
  gap: 10px;
  -webkit-app-region: no-drag;
}

.button {
  width: 12px;
  height: 12px;
  border-radius: 50%;
  cursor: pointer;
}

.icon-btn {
  background: transparent;
  border: none;
  color: rgba(255, 255, 255, 0.7);
  padding: 6px;
  border-radius: 6px;
  cursor: pointer;
}

.icon-btn:hover {
  color: #fff;
}

#content {
  flex: 1;
  position: relative;
  margin: 0;
  padding: 0;
  background: transparent;
  overflow: hidden;
}
</style>

<style>
html,
body {
  margin: 0;
  padding: 0;
  width: 100%;
  height: 100%;
  overflow: hidden;
  background: transparent !important;
}

@font-face {
  font-family: 'Cascadia Mono';
  src: url('./assets/fonts/CaskaydiaMonoNerdFontMono.ttf') format('truetype');
  font-weight: normal;
  font-style: normal;
  font-display: block;
}
</style>