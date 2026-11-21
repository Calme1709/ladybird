import { getByteFormatter } from "../../utils.js";
import clearBrowsingDataStyles from "./clear-browsing-data.css" with { type: "css" };

document.adoptedStyleSheets.push(clearBrowsingDataStyles);

export const formatBrowsingDataSize = getByteFormatter(unit => ({
    unitDisplay: unit === "byte" ? "long" : "short",
    maximumFractionDigits: 1,
})).formatBytes;

export function requestBrowsingDataSizes(since) {
    return ladybird.request("estimateBrowsingDataSizes", { since });
}

export function createClearBrowsingDataDialog(requestSizes = requestBrowsingDataSizes) {
    const template = document.createElement("template");
    template.innerHTML = `
        <dialog class="clear-browsing-data-dialog" closedby="any" aria-labelledby="clear-browsing-data-title">
            <div class="dialog-header">
                <h3 id="clear-browsing-data-title" class="dialog-title">Clear browsing data</h3>
                <button id="browsing-data-settings-close" class="dialog-button" aria-label="Close">&times;</button>
            </div>
            <div class="dialog-body">
                <div class="input-field-container control-row">
                    <label for="clear-browsing-data-time-range">Remove&nbsp;browsing&nbsp;data&nbsp;from:</label>
                    <select id="clear-browsing-data-time-range">
                        <option value="lastHour">Last hour</option>
                        <option value="last4Hours">Last 4 hours</option>
                        <option value="today">Today</option>
                        <option value="all" selected>All time</option>
                    </select>
                </div>
                <div class="input-field-container control-row">
                    <input id="clear-browsing-data-cached-files" type="checkbox" value="" checked />
                    <label for="clear-browsing-data-cached-files">
                        Cached files<span id="clear-browsing-data-cached-files-size"></span>
                        <p class="description">Remove items that help pages load faster</p>
                    </label>
                </div>
                <div class="input-field-container control-row">
                    <input id="clear-browsing-data-history" type="checkbox" value="" checked />
                    <label for="clear-browsing-data-history">
                        Browsing history
                        <p class="description">Remove visited pages from history and address bar suggestions</p>
                    </label>
                </div>
                <div class="input-field-container control-row">
                    <input id="clear-browsing-data-download-history" type="checkbox" value="" checked />
                    <label for="clear-browsing-data-download-history">
                        Download history
                        <p class="description">Remove records of downloaded files, but keep the files themselves</p>
                    </label>
                </div>
                <div class="input-field-container control-row">
                    <input id="clear-browsing-data-site-data" type="checkbox" value="" checked />
                    <label for="clear-browsing-data-site-data">
                        Cookies and site data<span id="clear-browsing-data-site-data-size"></span>
                        <p class="description">Remove items that may sign you out of most sites</p>
                    </label>
                </div>
                <div class="button-container">
                    <button id="clear-browsing-data-remove-data" class="secondary-button">Remove data</button>
                </div>
            </div>
        </dialog>
    `;
    const dialog = template.content.firstElementChild;
    document.body.append(dialog);

    const clearBrowsingDataCachedFiles = dialog.querySelector("#clear-browsing-data-cached-files");
    const clearBrowsingDataCachedFilesSize = dialog.querySelector("#clear-browsing-data-cached-files-size");
    const clearBrowsingDataDownloadHistory = dialog.querySelector("#clear-browsing-data-download-history");
    const clearBrowsingDataHistory = dialog.querySelector("#clear-browsing-data-history");
    const clearBrowsingDataRemoveData = dialog.querySelector("#clear-browsing-data-remove-data");
    const clearBrowsingDataSiteData = dialog.querySelector("#clear-browsing-data-site-data");
    const clearBrowsingDataSiteDataSize = dialog.querySelector("#clear-browsing-data-site-data-size");
    const clearBrowsingDataTimeRange = dialog.querySelector("#clear-browsing-data-time-range");

    function computeTimeRange() {
        const now = Temporal.Now.zonedDateTimeISO();

        switch (clearBrowsingDataTimeRange.value) {
            case "lastHour":
                return now.subtract({ hours: 1 });
            case "last4Hours":
                return now.subtract({ hours: 4 });
            case "today":
                return now.startOfDay();
            case "all":
                return null;
            default:
                console.error(`Unrecognized time range: ${clearBrowsingDataTimeRange.value}`);
                return now;
        }
    }

    let latestEstimate = 0;

    function estimateBrowsingDataSizes() {
        const estimate = ++latestEstimate;
        const since = computeTimeRange();

        Promise.try(requestSizes, since?.epochMilliseconds)
            .then(sizes => {
                if (estimate !== latestEstimate) return;

                clearBrowsingDataCachedFilesSize.innerText = ` (remove ${formatBrowsingDataSize(sizes.cacheSizeSinceRequestedTime)})`;
                clearBrowsingDataSiteDataSize.innerText = ` (remove ${formatBrowsingDataSize(sizes.siteDataSizeSinceRequestedTime)})`;
            })
            .catch(error => console.error("Failed to estimate browsing data sizes", error));
    }

    dialog.querySelector("#browsing-data-settings-close").addEventListener("click", () => {
        dialog.close();
    });

    // FIXME: Remove this once closedby="any" triggers for Esc key.
    dialog.addEventListener("keydown", event => {
        if (event.key === "Escape") {
            dialog.close();
        }
    });

    clearBrowsingDataTimeRange.addEventListener("change", estimateBrowsingDataSizes);

    function setRemoveDataEnabledState() {
        clearBrowsingDataRemoveData.disabled =
            !clearBrowsingDataCachedFiles.checked &&
            !clearBrowsingDataHistory.checked &&
            !clearBrowsingDataDownloadHistory.checked &&
            !clearBrowsingDataSiteData.checked;
    }

    clearBrowsingDataCachedFiles.addEventListener("change", setRemoveDataEnabledState);
    clearBrowsingDataHistory.addEventListener("change", setRemoveDataEnabledState);
    clearBrowsingDataDownloadHistory.addEventListener("change", setRemoveDataEnabledState);
    clearBrowsingDataSiteData.addEventListener("change", setRemoveDataEnabledState);

    clearBrowsingDataRemoveData.addEventListener("click", () => {
        const since = computeTimeRange();

        ladybird.sendMessage("clearBrowsingData", {
            since: since?.epochMilliseconds,
            cachedFiles: clearBrowsingDataCachedFiles.checked,
            history: clearBrowsingDataHistory.checked,
            downloadHistory: clearBrowsingDataDownloadHistory.checked,
            siteData: clearBrowsingDataSiteData.checked,
        });

        dialog.close();
    });

    return {
        dialog,
        open() {
            estimateBrowsingDataSizes();
            if (!dialog.open) dialog.showModal();
        },
    };
}
