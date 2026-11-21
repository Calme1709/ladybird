import {
    createClearBrowsingDataDialog,
    formatBrowsingDataSize,
    requestBrowsingDataSizes,
} from "../components/clear-browsing-data.js";
import { registerDialogDeepLink } from "./dialog-deep-link.js";

const browsingDataSettings = document.querySelector("#browsing-data-settings");
const browsingDataTotalSize = document.querySelector("#browsing-data-total-size");
const globalPrivacyControlToggle = document.querySelector("#global-privacy-control-toggle");

function loadSettings(settings) {
    if (Object.hasOwn(settings, "globalPrivacyControl")) {
        globalPrivacyControlToggle.checked = settings.globalPrivacyControl;
    }
}

const { dialog, open } = createClearBrowsingDataDialog(requestSizesAndUpdateTotal);

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
    onOpen: open,
});

browsingDataSettings.addEventListener("click", () => {
    location.hash = "clearBrowsingData";
});

globalPrivacyControlToggle.addEventListener("change", () => {
    ladybird.sendMessage("setGlobalPrivacyControl", globalPrivacyControlToggle.checked);
});

document.addEventListener("WebUILoaded", () => {
    if (location.hash !== "#clearBrowsingData") {
        refreshTotalBrowsingDataSize();
    }
});

document.addEventListener("WebUIMessage", event => {
    if (event.detail.name === "loadSettings") {
        loadSettings(event.detail.data);
    } else if (event.detail.name === "clearedBrowsingData") {
        refreshTotalBrowsingDataSize();
    }
});
