<script setup lang="ts">
import { ref, watch } from 'vue';

const props = defineProps<{
    tabs?: Array<{ id?: string; title: string }>;
    activeIndex?: number;
}>();
const emitEvent = defineEmits<{
    (event: 'update:activeIndex', index: number): void;
    (event: 'tab-click', tab: { id?: string; title: string }): void;
}>();

const localTabs = props.tabs ?? [
    { id: '1', title: 'bash' },
    { id: '2', title: 'PowerShell' },
    { id: '3', title: 'New Tab' }
];

const active = ref(props.activeIndex ?? 0);
watch(() => props.activeIndex, (v) => { if (typeof v === 'number') active.value = v; });

function onTabClick(i: number) {
    active.value = i;
    emitEvent('update:activeIndex', i);
    emitEvent('tab-click', localTabs[i]);
}
</script>

<template>
    <nav class="tabbar">
        <ul class="tab-list">
            <li v-for="(t, i) in localTabs" :key="t.id ?? i" :class="['tab', { active: active === i }]"
                @click="onTabClick(i)">
                <span class="tab-title">{{ t.title }}</span>
            </li>
        </ul>
        <div class="tab-actions">
            <button class="tab-action">⌄</button>
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
</style>
