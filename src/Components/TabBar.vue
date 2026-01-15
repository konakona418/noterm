<script setup lang="ts">
import { ref, watch, computed, onMounted, onBeforeUnmount } from 'vue';

import { Plus, X, ChevronDown } from 'lucide-vue-next';

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

onMounted(() => document.addEventListener('click', onDocumentClick));
onBeforeUnmount(() => document.removeEventListener('click', onDocumentClick));

function onTabClick(i: number) {
    active.value = i;
    emitEvent('update:activeIndex', i);
    // localTabs is computed, need to read value
    emitEvent('tab-click', (localTabs.value as any)[i]);
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
        <ul class="tab-list">
            <li v-for="(t, i) in localTabs" :key="t.id ?? i" :class="['tab', { active: active === i }]"
                @click="onTabClick(i)">
                <span class="tab-title">{{ t.title }}</span>
                <button class="tab-close" @click.stop="onClose(i)">
                    <X :size="14" />
                </button>
            </li>
        </ul>
        <div class="tab-actions" ref="menuRef">
            <div class="console-menu">
                <button class="tab-action" @click="emitAddTab">
                    <Plus :size="14" />
                </button>
                <button class="tab-action" @click.stop="showConsoleMenu = !showConsoleMenu" aria-haspopup="true"
                    :aria-expanded="showConsoleMenu">
                    <ChevronDown :size="14" />
                </button>
                <div v-if="showConsoleMenu" class="console-dropdown">
                    <ul>
                        <li v-for="c in consoles" :key="c.id">
                            <button class="console-item" @click="emitAddTabWithCommand(c.command)">{{ c.label
                            }}</button>
                        </li>
                    </ul>
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
}

.tab-list {
    display: flex;
    gap: 8px;
    list-style: none;
    margin: 0;
    padding: 0;
    align-items: center;
    -webkit-app-region: no-drag;
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
}

.tab:hover {
    background: rgba(255, 255, 255, 0.03);
    color: #fff;
}

.tab.active {
    background: linear-gradient(180deg, rgba(255, 255, 255, 0.04), rgba(255, 255, 255, 0.02));
    color: #fff;
    box-shadow: 0 -2px 0 rgba(255, 255, 255, 0.03) inset;
}

.tab-title {
    display: inline-block;
    padding-right: 2px;
}

.tab-actions {
    margin-left: auto;
}

.tab-action {
    background: transparent;
    border: none;
    color: rgba(255, 255, 255, 0.6);
    padding: 6px 8px;
    border-radius: 6px;
    cursor: pointer;
}

.tab-action:hover {
    background: rgba(255, 255, 255, 0.02);
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
    background: rgba(255, 255, 255, 0.03);
    color: #fff;
}

.console-menu {
    display: inline-flex;
    position: relative;
    gap: 6px;
}

.console-dropdown {
    position: absolute;
    right: 0;
    top: 36px;
    background: rgba(20, 20, 20, 0.95);
    border: 1px solid rgba(255, 255, 255, 0.04);
    border-radius: 8px;
    padding: 6px;
    z-index: 1000;
    min-width: 160px;
}

.console-dropdown ul {
    margin: 0;
    padding: 0;
    list-style: none;
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
}

.console-item:hover {
    background: rgba(255, 255, 255, 0.02);
}
</style>
