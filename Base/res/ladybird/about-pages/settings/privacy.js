import {
    createClearBrowsingDataDialog,
    formatBrowsingDataSize,
    requestBrowsingDataSizes,
} from "../components/clear-browsing-data.js";
import { registerDialogDeepLink, syncDialogs } from "./dialog-deep-link.js";

const browsingDataSettings = document.querySelector("#browsing-data-settings");
const browsingDataTotalSize = document.querySelector("#browsing-data-total-size");
const globalPrivacyControlToggle = document.querySelector("#global-privacy-control-toggle");

function loadSettings(settings) {
    if (Object.hasOwn(settings, "globalPrivacyControl")) {
        globalPrivacyControlToggle.checked = settings.globalPrivacyControl;
    }
}

const { dialog, open } = createClearBrowsingDataDialog(requestSizesAndUpdateTotal);

let privateBrowsing = null;
let latestSizeRequest;

async function requestSizesAndUpdateTotal(since) {
    const request = requestBrowsingDataSizes(since);
    latestSizeRequest = request;
    const sizes = await request;
    if (request === latestSizeRequest) {
        const size = formatBrowsingDataSize(sizes.totalCacheSize + sizes.totalSiteDataSize);
        browsingDataTotalSize.innerText = `Your browsing data is currently using ${size} of disk space`;
    }
    return sizes;
}

function refreshTotalBrowsingDataSize() {
    requestSizesAndUpdateTotal().catch(error => console.error("Failed to estimate browsing data sizes", error));
}

registerDialogDeepLink({
    hash: "clearBrowsingData",
    tab: "privacy",
    dialog,
    onBeforeOpen() {
        if (privateBrowsing) {
            history.replaceState(null, "", "#privacy");
            ladybird.sendMessage("clearBrowsingData", {});
        }
        return privateBrowsing === false;
    },
    onOpen: open,
});

browsingDataSettings.addEventListener("click", () => {
    if (privateBrowsing) {
        ladybird.sendMessage("clearBrowsingData", {});
    } else {
        location.hash = "clearBrowsingData";
    }
});

globalPrivacyControlToggle.addEventListener("change", () => {
    ladybird.sendMessage("setGlobalPrivacyControl", globalPrivacyControlToggle.checked);
});

document.addEventListener("WebUIMessage", event => {
    if (event.detail.name === "loadSettings") {
        loadSettings(event.detail.data);
    } else if (event.detail.name === "loadFeatures") {
        privateBrowsing = event.detail.data.privateBrowsing;
        if (privateBrowsing) {
            document.querySelector('label[for="browsing-data-settings"]').textContent = "Clear private browsing data";
            browsingDataTotalSize.classList.add("hidden");
        }
        browsingDataSettings.disabled = false;
        if (location.hash === "#clearBrowsingData") {
            syncDialogs();
        } else if (privateBrowsing === false) {
            refreshTotalBrowsingDataSize();
        }
    } else if (event.detail.name === "clearedBrowsingData") {
        refreshTotalBrowsingDataSize();
    }
});
