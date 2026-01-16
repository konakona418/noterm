<script setup lang="ts">
import { ref, watch, computed, onMounted, onBeforeUnmount } from 'vue';

import { Plus, X, ChevronDown, Terminal } from 'lucide-vue-next';

const props = defineProps<{
    tabs?: Array<{ id?: string; title: string }>;
    activeIndex?: number;
}>();
const emitEvent = defineEmits<{
    (event: 'update:activeIndex', index: number): void;
    (event: 'tab-click', tab: { id?: string; title: string }): void;
    (event: 'add-tab', command?: string): void;
    (event: 'close-tab', index: number): void;
}>();

const defaultTabs = [
    { id: '1', title: 'bash' },
    { id: '2', title: 'PowerShell' },
    { id: '3', title: 'New Tab' }
];

const localTabs = computed(() => props.tabs ?? defaultTabs);

const active = ref(props.activeIndex ?? 0);
watch(() => props.activeIndex, (v) => { if (typeof v === 'number') active.value = v; });
const tabListRef = ref<HTMLElement | null>(null);
// dropdown state for console selection
const showConsoleMenu = ref(false);
const menuRef = ref<HTMLElement | null>(null);
const consoles = [
    { id: 'powershell', label: 'PowerShell', command: 'powershell.exe' },
    { id: 'cmd', label: 'Command Prompt (cmd)', command: 'cmd.exe' },
];

function onDocumentClick(e: MouseEvent) {
    if (!menuRef.value) return;
    if (!menuRef.value.contains(e.target as Node)) showConsoleMenu.value = false;
}

// attach wheel-to-scroll behavior for horizontal scrolling
onMounted(() => {
    document.addEventListener('click', onDocumentClick);
    const el = tabListRef.value;
    if (el) {
        const onWheel = (e: WheelEvent) => {
            // only intercept vertical wheel when horizontal scrollable
            if (Math.abs(e.deltaY) > 0) {
                // if content is wider than container
                if (el.scrollWidth > el.clientWidth) {
                    e.preventDefault();
                    el.scrollLeft += e.deltaY;
                }
            }
        };
        el.addEventListener('wheel', onWheel, { passive: false });
        // store handler for cleanup
        (el as any).__onWheel = onWheel;
    }
});
onBeforeUnmount(() => {
    document.removeEventListener('click', onDocumentClick);
    const el = tabListRef.value;
    if (el && (el as any).__onWheel) {
        el.removeEventListener('wheel', (el as any).__onWheel as EventListener);
        delete (el as any).__onWheel;
    }
});

function onTabClick(i: number) {
    active.value = i;
    emitEvent('update:activeIndex', i);
    // localTabs is computed, need to read value
    emitEvent('tab-click', (localTabs.value as any)[i]);
}

function iconForTitle(title?: string) {
    // return an object with component and optional class for styling
    if (!title) return { comp: Terminal, cls: '' };
    const t = title.toLowerCase();
    if (t.includes('powershell') || t.includes('pwsh')) return { comp: Terminal, cls: 'pwsh' };
    if (t.includes('command') || t.includes('cmd')) return { comp: Terminal, cls: '' };
    if (t.includes('bash')) return { comp: Terminal, cls: '' };
    return { comp: Terminal, cls: '' };
}

function onClose(i: number) {
    emitEvent('close-tab', i);
}

function emitAddTab() {
    // default add: powershell
    emitEvent('add-tab', 'powershell.exe');
}

function emitAddTabWithCommand(cmd: string) {
    console.log('Adding tab with command:', cmd);

    emitEvent('add-tab', cmd);
    showConsoleMenu.value = false;
}
</script>

<template>
    <nav class="tabbar">
        <ul class="tab-list" ref="tabListRef">
            <li v-for="(t, i) in localTabs" :key="t.id ?? i" :class="['tab', { active: active === i }]"
                @click="onTabClick(i)">
                <component :is="iconForTitle(t.title).comp" :class="['tab-icon', iconForTitle(t.title).cls]" :size="14"
                    :stroke-width="4" />
                <span class="tab-title">{{ t.title }}</span>
                <button class="tab-close" @click.stop="onClose(i)">
                    <X :size="14" :stroke-width="4" />
                </button>
            </li>
        </ul>
        <div class="tab-actions" ref="menuRef">
            <div class="console-menu">
                <button class="tab-action" @click="emitAddTab">
                    <Plus :size="14" :stroke-width="4" />
                </button>
                <button class="tab-action" @click.stop="showConsoleMenu = !showConsoleMenu" aria-haspopup="true"
                    :aria-expanded="showConsoleMenu">
                    <ChevronDown :size="14" :stroke-width="4" />
                </button>
                <div v-if="showConsoleMenu">
                    <div class="console-dropdown">
                        <ul>
                            <li v-for="c in consoles" :key="c.id">
                                <button class="console-item" @click="emitAddTabWithCommand(c.command)">
                                    {{ c.label }}</button>
                            </li>
                        </ul>
                    </div>
                </div>
            </div>
        </div>
    </nav>
</template>

<style scoped>
.tabbar {
    display: flex;
    align-items: center;
    gap: 12px;
    padding: 0px 12px;
    background: transparent;
    max-width: 100%;
}

.tab-list {
    display: flex;
    gap: 8px;
    list-style: none;
    margin: 0;
    padding: 0;
    align-items: center;
    -webkit-app-region: no-drag;
    overflow-x: auto;
    overflow-y: hidden;
    white-space: nowrap;
    flex: 1 1 auto;
    -webkit-overflow-scrolling: touch;
    flex-wrap: nowrap;
    min-width: 0;
    max-width: 100%;
    /* allow flex child to shrink and enable overflow */
    scrollbar-width: none;
    -ms-overflow-style: none;
}

.tab-list::-webkit-scrollbar {
    display: none;
    height: 0;
}


.tab-icon.pwsh {
    color: #2e85e2;
}

.tab {
    display: inline-flex;
    align-items: center;
    gap: 8px;
    padding: 6px 12px;
    border-radius: 8px 8px 0 0;
    color: rgba(255, 255, 255, 0.75);
    font-size: 13px;
    cursor: pointer;
    user-select: none;
    background: transparent;
    transition: background 120ms ease, color 120ms ease;
    flex: 0 0 auto;
    /* prevent shrinking so tabs keep size during scroll */
}

.tab:hover {
    background: rgba(255, 255, 255, 0.03);
    color: #fff;
}

.tab.active {
    background: linear-gradient(180deg, rgba(255, 255, 255, 0.3), rgba(255, 255, 255, 0.2));
    color: #fff;
    box-shadow: 0 -2px 0 rgba(255, 255, 255, 0.03) inset;
}

.tab-title {
    display: inline-block;
    padding-right: 2px;
}

.tab-icon {
    margin-right: 6px;
    color: rgba(255, 255, 255, 0.75);
    display: inline-block;
}

.tab-actions {
    margin-left: auto;
    flex: 0 0 auto;
    /* keep action area visible and prevent being pushed out */
}

.tab-action {
    background: transparent;
    border: none;
    color: rgba(255, 255, 255, 0.6);
    padding: 0 8px;
    border-radius: 6px;
    cursor: pointer;
}

.tab-action:hover {
    color: #fff;
}

.tab-close {
    margin-left: 8px;
    background: transparent;
    border: none;
    color: rgba(255, 255, 255, 0.6);
    cursor: pointer;
    padding: 2px 6px;
    border-radius: 6px;
}

:deep(.tab-close svg),
:deep(.tab-action svg) {
    display: block;
    color: inherit;
}

.tab-close:hover {
    color: #fff;
}

.console-menu {
    display: inline-flex;
    position: relative;
    align-items: flex-end;
    gap: 6px;
}

.console-dropdown {
    position: absolute;
    right: 0;
    top: 24px;
    border-radius: 8px;
    padding: 6px;
    z-index: 1000;
    min-width: 160px;
    overflow: hidden;
    border: 1px solid rgba(255, 255, 255, 0.04);
    box-shadow: 0 8px 24px rgba(0, 0, 0, 0.5);
    background: rgba(30, 30, 30, 0.6);
}

.console-dropdown ul {
    margin: 0;
    padding: 0;
    list-style: none;
}

.console-dropdown::before {
    content: '';
    position: absolute;
    inset: 0;
    pointer-events: none;
    background: rgba(0, 0, 0, 0.2);
    -webkit-backdrop-filter: blur(6px);
    backdrop-filter: blur(6px);
    z-index: 0;
}

.console-dropdown::after {
    content: '';
    position: absolute;
    inset: 0;
    pointer-events: none;
    opacity: 0.04;
    background-image: url("data:image/svg+xml;utf8,<svg xmlns='http://www.w3.org/2000/svg' width='64' height='64'><filter id='n'><feTurbulence baseFrequency='0.8' numOctaves='2' stitchTiles='stitch'/><feColorMatrix type='saturate' values='0'/></filter><rect width='100%' height='100%' filter='url(%23n)' opacity='0.6' fill='white'/></svg>");
    background-repeat: repeat;
    z-index: 0;
}

.console-item {
    display: block;
    width: 100%;
    text-align: left;
    padding: 8px 10px;
    background: transparent;
    border: none;
    color: rgba(255, 255, 255, 0.9);
    cursor: pointer;
    border-radius: 6px;
    position: relative;
    z-index: 1;
    /* ensure controls sit above the frosted pseudo-layers */
}

.console-item:hover {
    background: rgba(255, 255, 255, 0.02);
}
</style>
