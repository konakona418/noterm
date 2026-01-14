<script setup lang="ts">
import { onBeforeUnmount, onMounted, ref } from "vue";
import { invoke, callback } from "./webui-ext"

import { Terminal } from "@xterm/xterm";
import { FitAddon } from "@xterm/addon-fit";
import { WebglAddon } from "@xterm/addon-webgl";
import { WebLinksAddon } from "@xterm/addon-web-links";
import "@xterm/xterm/css/xterm.css";

const terminalElement = ref<HTMLElement | null>(null);
let terminal: Terminal | null = null;
let fitAddon: FitAddon | null = null;
let webglAddon: WebglAddon | null = null;
let webLinksAddon: WebLinksAddon | null = null;

function debounce(fn: Function, delay: number) {
  let timer: number | null = null;
  return (...args: any[]) => {
    if (timer) clearTimeout(timer);
    timer = setTimeout(() => fn(...args), delay);
  };
}
const minimize = () => invoke("webui_minimize");
const closeWin = () => invoke("webui_close");

const handleResize = debounce(async () => {
  if (terminal && fitAddon) {
    fitAddon.fit();
    await invoke("webui_resize_terminal", terminal.cols, terminal.rows);
  }
}, 150);

onMounted(() => {
  if (terminalElement.value) {
    terminal = new Terminal({
      cols: 80,
      rows: 24,
      fontFamily: "Cascadia Mono, 'Microsoft YaHei Mono', 'PingFang SC', 'Source Han Sans SC', ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, 'Liberation Mono', 'Courier New', monospace",
      fontSize: 18,
      allowTransparency: true,
      theme: {
        background: '#00000000',
        foreground: '#FFFFFF',
      },
    });

    webglAddon = new WebglAddon();
    try {
      const webglAddon = new WebglAddon();
      terminal.loadAddon(webglAddon);
    } catch (e) {
      console.warn("WebGL addon could not be loaded, falling back to canvas", e);
    }

    fitAddon = new FitAddon();
    terminal.loadAddon(fitAddon);

    webLinksAddon = new WebLinksAddon();
    terminal.loadAddon(webLinksAddon);

    terminal.open(terminalElement.value);
    requestAnimationFrame(() => {
      if (fitAddon) {
        fitAddon.fit();
      }
    });

    window.addEventListener("resize", handleResize);

    terminal.onData(async (data) => {
      await invoke("webui_send_input", data);
      console.log("Terminal input:", data);
    });

    callback("webui_ready", async () => {
      if (terminal) {
        await invoke("webui_init_terminal", terminal.cols, terminal.rows);
        fitAddon?.fit();
        terminal.focus();
      }
    });

    callback("webui_receive_output", async (output: Uint8Array) => {
      console.log("Received output size:", output.byteLength);
      terminal?.write(output);
    });
  }
});

onBeforeUnmount(() => {
  window.removeEventListener("resize", handleResize);
  webglAddon?.dispose();
  terminal?.dispose();
});

</script>
<template>
  <div class="window-wrapper">
    <div id="ui-container">
      <div id="titlebar">
        <span id="title">NoTerm</span>
        <div id="buttons">
          <span class="button minimize" @click="minimize"></span>
          <span class="button close" @click="closeWin"></span>
        </div>
      </div>

      <div id="content">
        <div ref="terminalElement" class="terminal-container"></div>
        <div class="terminal-overlay"></div>
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
  padding: 12px;
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
  box-shadow: 0 20px 50px rgba(0, 0, 0, 0.3);
  overflow: hidden;
}

#titlebar {
  height: 38px;
  background: rgba(0, 0, 0, 0.1);
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 0 16px;
  flex-shrink: 0;
  -webkit-app-region: drag;
  user-select: none;
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

.terminal-container {
  position: absolute;
  top: 0;
  left: 0;
  right: 0;
  bottom: 0;
  background: transparent;
}

.terminal-overlay {
  position: absolute;
  top: 0;
  left: 0;
  right: 0;
  bottom: 0;
  pointer-events: none;
  background: radial-gradient(circle at center,
      transparent 0%,
      rgba(0, 0, 0, 0.15) 100%);

  backdrop-filter: blur(0.5px) brightness(1.2);
  z-index: 10;
}

.terminal-overlay::before {
  content: "";
  position: absolute;
  inset: 0;
  background: linear-gradient(rgba(18, 16, 16, 0) 50%,
      rgba(0, 0, 0, 0.1) 50%);
  background-size: 100% 6px;
  z-index: 11;
  animation: scanlines 0.8s linear infinite;
}

@keyframes scanlines {
  0% {
    background-position: 0 0;
  }

  100% {
    background-position: 0 6px;
  }
}

:deep(.xterm) {
  height: 100%;
  padding: 8px;
  filter: url(#crt-glow);
}

:deep(.xterm-viewport) {
  background-color: transparent;
  overflow: hidden;
}

:deep(.xterm-screen) {
  width: 100%;
  height: 100%;
}

:deep(*) {
  scrollbar-width: none;
  -ms-overflow-style: none;
}

:deep(*::-webkit-scrollbar) {
  display: none;
}

:deep(.xterm-viewport),
:deep(.xterm-screen),
:deep(.xterm-main-canvas),
:deep(.xterm-char-measure-element) {
  background-color: transparent;
}

:deep(.xterm-webgl-canvas) {
  background-color: transparent;
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