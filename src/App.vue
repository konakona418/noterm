<script setup lang="ts">
import { ref, onMounted } from 'vue';
import { invoke, callback } from './webui-ext';
import TerminalContainer from './Components/TerminalContainer.vue';
import TabBar from './Components/TabBar.vue';

type Session = { title: string; ptyId?: number | null };

const sessions = ref<Session[]>([{ title: 'powershell', ptyId: null }]);
const active = ref(0);

// whether the native/webui connection is established
const webuiReady = ref(false);

// pending requests keyed by token -> session index
const pendingByToken = new Map<number, number>();
const idToIndex = new Map<number, number>();
let nextRequestToken = 1;

const terminalRefs = ref<any[]>([]);
// temporary buffer for outputs received before a terminal ref is ready
const pendingOutput = new Map<number, Uint8Array[]>();

function setTerminalRef(index: number) {
  return (el: any) => {
    terminalRefs.value[index] = el;
    // if this session already has a ptyId, flush any pending output
    const ptyId = sessions.value[index]?.ptyId;
    if (ptyId != null) {
      const pending = pendingOutput.get(ptyId);
      if (pending && pending.length > 0 && el && typeof el.writeOutput === 'function') {
        for (const chunk of pending) el.writeOutput(chunk);
        pendingOutput.delete(ptyId);
      }
    }
  };
}

function createTab(title = 'bash') {
  sessions.value.push({ title, ptyId: null });
  // select the newly created tab
  active.value = sessions.value.length - 1;
  // keep terminalRefs aligned
  terminalRefs.value.push(null);
}

function onRequestPty(index: number, cols: number, rows: number) {
  // Generate a token to correlate request/response
  const token = nextRequestToken++;
  pendingByToken.set(token, index);
  // Decide command based on session title and ask native to create a PTY with token;
  // native will call back with webui_created_pty(id, token)
  const title = sessions.value[index]?.title?.toLowerCase() ?? '';
  let cmd = 'powershell.exe';
  if (title.includes('powershell')) cmd = 'powershell.exe';
  else if (title.includes('bash')) cmd = 'bash';
  // send command as 4th argument
  invoke('webui_init_terminal', cols, rows, token, cmd)
    .then(() => {
      console.log('Requested PTY creation for session', index, 'with token', token);
    })
    .catch((err) => {
      console.error('Failed to request PTY creation:', err);
    });
}

function onTabClick(i: number) {
  active.value = i;
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
  idToIndex.set(id, idx);
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
  const idx = idToIndex.get(id);

  console.log('Received output for PTY id', id, 'mapped to session index', idx);

  if (idx === undefined) return;
  const child = terminalRefs.value[idx];
  if (child && typeof child.writeOutput === 'function') {
    child.writeOutput(payload);
  } else {
    console.warn('No terminal ref or writeOutput method for session index', idx, '- buffering output');
    // buffer until the component ref is ready
    const ptyId = id;
    const arr = pendingOutput.get(ptyId) ?? [];
    arr.push(payload);
    pendingOutput.set(ptyId, arr);
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
          <TabBar :tabs="sessions.map(s => ({ id: String(s.ptyId ?? ''), title: s.title }))" :activeIndex="active"
            @update:activeIndex="(i) => active = i" @tab-click="(t) => console.log('tab-click', t)"
            @add-tab="createTab('powershell')" />
        </div>
        <div id="buttons">
          <span class="button minimize" @click="minimize"></span>
          <span class="button close" @click="closeWin"></span>
        </div>
      </div>

      <div id="content">
        <div style="position:relative; width:100%; height:100%;">
          <component v-for="(s, i) in sessions" :is="TerminalContainer" :key="i" :ptyId="s.ptyId ?? null"
            :connected="webuiReady" @request-pty="(cols, rows) => onRequestPty(i, cols, rows)" :ref="setTerminalRef(i)"
            v-show="active === i" />
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
  background: rgba(30, 30, 30, 0.8);
  border-radius: 12px;
  backdrop-filter: blur(20px);
  -webkit-backdrop-filter: blur(20px);
  border: 1px solid rgba(255, 255, 255, 0.1);
  overflow: hidden;
}

#titlebar {
  height: 56px;
  background: rgba(0, 0, 0, 0.1);
  display: flex;
  align-items: center;
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

.close {
  background: #ff5f57;
}

.minimize {
  background: #ffbd2e;
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