<script setup lang="ts">
import { invoke } from './webui-ext';
import TerminalContainer from './Components/TerminalContainer.vue';
import TabBar from './Components/TabBar.vue';

const tabs = [
  { id: '1', title: 'bash' },
  { id: '2', title: 'PowerShell' },
  { id: '3', title: '+' }
];

const minimize = () => invoke('webui_minimize');
const closeWin = () => invoke('webui_close');
</script>
<template>
  <div class="window-wrapper">
    <div id="ui-container">
      <div id="titlebar">
        <div class="titlebar-left">
          <span id="title">NoTerm</span>
          <TabBar :tabs="tabs" />
        </div>
        <div id="buttons">
          <span class="button minimize" @click="minimize"></span>
          <span class="button close" @click="closeWin"></span>
        </div>
      </div>

      <div id="content">
        <TerminalContainer />
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