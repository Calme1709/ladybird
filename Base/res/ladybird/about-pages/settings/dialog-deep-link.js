const REGISTRY = new Map();
let globalListenersInstalled = false;
let initialHash;

export function syncDialogs() {
    if (!window.ladybird) {
        return;
    }

    const activeHash = location.hash.slice(1);
    const active = REGISTRY.get(activeHash);
    if (activeHash !== initialHash) {
        initialHash = undefined;
    }

    for (const { dialog } of REGISTRY.values()) {
        if (dialog !== active?.dialog && dialog.open) {
            dialog.close();
        }
    }

    if (!active || active.onBeforeOpen?.() === false) {
        return;
    }

    if (activeHash === initialHash) {
        initialHash = undefined;
        history.replaceState(null, "", `#${active.tab}`);
        location.hash = activeHash;
        return;
    }

    active.onOpen?.();
    if (!active.dialog.open) {
        active.dialog.showModal();
    }
}

export function registerDialogDeepLink({ hash, tab, dialog, onBeforeOpen, onOpen }) {
    REGISTRY.set(hash, { tab, dialog, onBeforeOpen, onOpen });

    dialog.addEventListener("close", () => {
        if (location.hash.slice(1) === hash) {
            history.back();
        }
    });

    if (globalListenersInstalled) {
        return;
    }
    globalListenersInstalled = true;

    window.addEventListener("hashchange", syncDialogs);

    document.addEventListener("WebUILoaded", () => {
        initialHash = location.hash.slice(1);
        syncDialogs();
    });
}

export function tabForDeepLink(hash) {
    return REGISTRY.get(hash)?.tab ?? hash;
}
