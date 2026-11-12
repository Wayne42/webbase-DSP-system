// -------------------------------------
// Hörtest: 8 Frequenzen, Ton wird von leise bis laut lauter.
// Ergebnis (Hörschwelle pro Frequenz) wird in Bell-Filter 1-8 umgerechnet
// und kann wie in app.js per POST an den ADAU geschickt werden.
// -------------------------------------

// Frequenzen des Tests, Reihenfolge = Filter 1 ... 8 (muss zu NUM_FILTERS passen)
const TEST_FREQUENCIES = [250, 500, 1000, 2000, 4000, 6000, 8000, 16000];

const LEVEL_MIN_DB = -80;      // Startlautstärke des Tons in dBFS
const LEVEL_MAX_DB = 0;        // Endlautstärke des Tons in dBFS
const RAMP_DB_PER_SEC = 5;     // Anstieg -> 16 s von -80 bis 0 dBFS
const PAUSE_MIN_MS = 1000;     // Zufällige Pause vor jedem Ton, damit man nicht "mitzählt"
const PAUSE_MAX_MS = 2500;
const FILTER_Q = 1.4;          // ca. 1 Oktave Bandbreite
const FILTER_TYPE = 'bell';
const STORAGE_KEY = 'hearingTestThresholds';

// Hörschwelle pro Frequenz in dBFS (null = noch nicht gemessen)
const thresholds = loadThresholds();

const $ = id => document.getElementById(id);


// -------------------------------------
// Helper
// -------------------------------------

const dbToLinear = db => Math.pow(10, db / 20);

const clamp = (value, min, max) => Math.min(max, Math.max(min, value));

function formatFreq(freq) {
    return freq >= 1000 ? `${freq / 1000} kHz` : `${freq} Hz`;
}

function loadThresholds() {
    try {
        const stored = JSON.parse(localStorage.getItem(STORAGE_KEY));
        if (Array.isArray(stored) && stored.length === TEST_FREQUENCIES.length) {
            return stored;
        }
    } catch (err) { /* localStorage nicht verfügbar -> ohne Speichern weiter */ }
    return TEST_FREQUENCIES.map(() => null);
}

function saveThresholds() {
    try {
        localStorage.setItem(STORAGE_KEY, JSON.stringify(thresholds));
    } catch (err) { /* ignorieren */ }
}

// Sendet einen POST-Request (wie sendPostRequest in app.js), gibt die Antwort als Promise zurück
async function postForm(path, data) {
    const response = await fetch(path, {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: new URLSearchParams(data)
    });
    const text = await response.text();
    if (!response.ok) {
        throw new Error(`${path}: ${response.status} ${text}`);
    }
    return text;
}


// -------------------------------------
// Audio (Web Audio API)
// -------------------------------------

let audioCtx = null;
let tone = null; // { osc, gain }

function getAudioContext() {
    if (!audioCtx) {
        audioCtx = new (window.AudioContext || window.webkitAudioContext)();
    }
    if (audioCtx.state === 'suspended') {
        audioCtx.resume();
    }
    return audioCtx;
}

// Startet einen Sinuston mit der Lautstärke levelDb
function startTone(freq, levelDb) {
    stopTone();
    const ctx = getAudioContext();
    const osc = ctx.createOscillator();
    const gain = ctx.createGain();

    osc.type = 'sine';
    osc.frequency.value = freq;
    // kurzer Fade-In gegen Knackser
    gain.gain.setValueAtTime(0.0001, ctx.currentTime);
    gain.gain.exponentialRampToValueAtTime(dbToLinear(levelDb), ctx.currentTime + 0.02);

    osc.connect(gain).connect(ctx.destination);
    osc.start();
    tone = { osc, gain };
    return tone;
}

function setToneLevel(levelDb) {
    if (!tone) return;
    const now = audioCtx.currentTime;
    tone.gain.gain.cancelScheduledValues(now);
    tone.gain.gain.setTargetAtTime(dbToLinear(levelDb), now, 0.01);
}

function stopTone() {
    if (!tone) return;
    const { osc, gain } = tone;
    const now = audioCtx.currentTime;
    gain.gain.cancelScheduledValues(now);
    gain.gain.setTargetAtTime(0, now, 0.01); // kurzer Fade-Out gegen Knackser
    osc.stop(now + 0.1);
    tone = null;
}


// -------------------------------------
// Hörtest-Ablauf
// -------------------------------------

const test = {
    running: false,
    phase: 'idle',     // 'pause' = warten auf Ton, 'ramp' = Ton läuft
    index: 0,          // aktuelle Frequenz
    rampStart: 0,      // audioCtx.currentTime beim Start der Rampe
    pauseTimer: null,
    animFrame: null
};

// Aktuelle Lautstärke der Rampe in dBFS
function currentRampLevel() {
    const elapsed = audioCtx.currentTime - test.rampStart;
    return clamp(LEVEL_MIN_DB + elapsed * RAMP_DB_PER_SEC, LEVEL_MIN_DB, LEVEL_MAX_DB);
}

function showLevel(levelDb) {
    $('testLevel').textContent = levelDb === null ? '– dBFS' : `${levelDb.toFixed(1)} dBFS`;
    const percent = levelDb === null ? 0 :
        (levelDb - LEVEL_MIN_DB) / (LEVEL_MAX_DB - LEVEL_MIN_DB) * 100;
    $('testLevelFill').style.width = `${clamp(percent, 0, 100)}%`;
}

// Der "Hören"-Button ist während des ganzen Tests aktiv, damit man am Button
// nicht erkennen kann, wann der Ton startet
function setTestControls(running) {
    $('testStart').disabled = running;
    $('testCancel').disabled = !running;
    $('testHeard').disabled = !running;
}

function startTest() {
    getAudioContext(); // muss in einem Klick-Event erzeugt werden
    test.running = true;
    test.index = 0;
    setTestControls(true);
    $('outputTest').textContent = '';
    nextFrequency();
}

function nextFrequency(note = '') {
    if (test.index >= TEST_FREQUENCIES.length) {
        finishTest();
        return;
    }
    const freq = TEST_FREQUENCIES[test.index];
    $('testFreq').textContent = formatFreq(freq);
    $('testProgress').textContent = `${test.index + 1} / ${TEST_FREQUENCIES.length}`;
    $('outputTest').textContent = note + 'Warte auf den Ton …';
    schedulePause();
}

// Zufällige Pause, danach startet die Rampe der aktuellen Frequenz
function schedulePause() {
    clearTimeout(test.pauseTimer);
    test.phase = 'pause';
    showLevel(LEVEL_MIN_DB); // gleiche Anzeige wie beim Rampenstart -> verrät den Start nicht

    const pause = PAUSE_MIN_MS + Math.random() * (PAUSE_MAX_MS - PAUSE_MIN_MS);
    test.pauseTimer = setTimeout(() => startRamp(TEST_FREQUENCIES[test.index]), pause);
}

function startRamp(freq) {
    const { gain } = startTone(freq, LEVEL_MIN_DB);
    const ctx = audioCtx;
    const duration = (LEVEL_MAX_DB - LEVEL_MIN_DB) / RAMP_DB_PER_SEC;

    test.phase = 'ramp';
    test.rampStart = ctx.currentTime;
    gain.gain.cancelScheduledValues(ctx.currentTime);
    gain.gain.setValueAtTime(dbToLinear(LEVEL_MIN_DB), ctx.currentTime);
    gain.gain.exponentialRampToValueAtTime(dbToLinear(LEVEL_MAX_DB), ctx.currentTime + duration);

    const update = () => {
        const level = currentRampLevel();
        showLevel(level);
        if (level >= LEVEL_MAX_DB) {
            // Maximum erreicht, ohne dass der Ton gehört wurde
            recordThreshold(LEVEL_MAX_DB, false);
            return;
        }
        test.animFrame = requestAnimationFrame(update);
    };
    test.animFrame = requestAnimationFrame(update);
}

function onHeard() {
    if (!test.running) return;

    if (test.phase === 'pause') {
        // Geklickt, obwohl noch kein Ton läuft -> Fehlalarm, Pause neu starten
        $('outputTest').textContent = 'Zu früh – es lief noch kein Ton. Warte auf den Ton …';
        schedulePause();
        return;
    }
    if (test.phase === 'ramp') {
        recordThreshold(currentRampLevel(), true);
    }
}

function recordThreshold(levelDb, heard) {
    cancelAnimationFrame(test.animFrame);
    stopTone();
    test.phase = 'pause';

    const rounded = Math.round(levelDb);
    thresholds[test.index] = rounded;
    saveThresholds();
    updateBandRow(test.index);

    const note = heard ? '' :
        `${formatFreq(TEST_FREQUENCIES[test.index])} nicht gehört (max. ${LEVEL_MAX_DB} dBFS gespeichert). `;

    test.index++;
    nextFrequency(note);
}

function stopTest(message) {
    clearTimeout(test.pauseTimer);
    cancelAnimationFrame(test.animFrame);
    stopTone();
    test.running = false;
    test.phase = 'idle';
    setTestControls(false);
    showLevel(null);
    $('outputTest').textContent = message;
}

function finishTest() {
    stopTest('Test abgeschlossen. Ergebnisse können jetzt angepasst und an den DSP gesendet werden.');
    $('testFreq').textContent = '–';
    if (dsp.active) sendAllFilters();
}


// -------------------------------------
// Ergebnisse / manuelles Patchen
// -------------------------------------

let playingBand = null; // Index der Frequenz, die gerade probegehört wird

// Gain pro Frequenz: Differenz zur besten (leisesten) Hörschwelle, begrenzt auf maxBoost
function computeGains() {
    const measured = thresholds.filter(t => t !== null);
    const reference = measured.length ? Math.min(...measured) : 0;
    const maxBoost = parseFloat($('maxBoost').value);
    return thresholds.map(t => t === null ? 0 : clamp(t - reference, 0, maxBoost));
}

function bandPart(index, role) {
    return document.querySelector(`#band${index} [data-role="${role}"]`);
}

function updateBandRow(index) {
    const t = thresholds[index];
    bandPart(index, 'slider').value = t === null ? LEVEL_MIN_DB : t;
    bandPart(index, 'threshold').textContent = t === null ? '–' : `${t} dBFS`;
    updateGainLabels();
}

function updateGainLabels() {
    computeGains().forEach((gain, i) => {
        bandPart(i, 'gain').textContent = `+${gain.toFixed(1)} dB`;
    });
}

function togglePlayBand(index) {
    const wasPlaying = playingBand === index;
    stopBandPlayback();
    if (wasPlaying || test.running) return;

    const level = parseFloat(bandPart(index, 'slider').value);
    startTone(TEST_FREQUENCIES[index], level);
    playingBand = index;
    bandPart(index, 'play').classList.add('active');
    bandPart(index, 'play').textContent = '■';
}

function stopBandPlayback() {
    if (playingBand === null) return;
    stopTone();
    bandPart(playingBand, 'play').classList.remove('active');
    bandPart(playingBand, 'play').textContent = '▶';
    playingBand = null;
}

// Slider bewegt: Wert übernehmen und ggf. Probeton nachführen
function onSliderInput(index) {
    const level = parseFloat(bandPart(index, 'slider').value);
    thresholds[index] = level;
    bandPart(index, 'threshold').textContent = `${level} dBFS`;
    updateGainLabels();
    if (playingBand === index) setToneLevel(level);
}

// Slider losgelassen: speichern und bei aktivem DSP sofort senden
function onSliderChange() {
    saveThresholds();
    if (dsp.active) sendAllFilters();
}

function createBandRows() {
    const template = $('bandTemplate');
    const list = $('bandList');

    TEST_FREQUENCIES.forEach((freq, i) => {
        const row = template.content.firstElementChild.cloneNode(true);
        row.id = `band${i}`;
        row.querySelector('[data-role="freq"]').textContent = formatFreq(freq);
        list.appendChild(row);

        bandPart(i, 'slider').addEventListener('input', () => onSliderInput(i));
        bandPart(i, 'slider').addEventListener('change', onSliderChange);
        bandPart(i, 'play').addEventListener('click', () => togglePlayBand(i));
    });

    // Erst wenn alle Zeilen existieren, Werte und Gain-Labels füllen
    TEST_FREQUENCIES.forEach((freq, i) => updateBandRow(i));
}


// -------------------------------------
// Übertragung an den ADAU (wie app.js: /filterN und /bypassFilterN)
// -------------------------------------

const dsp = {
    active: false,
    sending: false,
    pending: false // während eines Sendevorgangs erneut angefordert
};

function setDspButton() {
    const button = $('dspToggle');
    button.classList.toggle('active', dsp.active);
    button.textContent = dsp.active ? 'An DSP senden: An' : 'An DSP senden: Aus';
}

// Sendet nacheinander (ESP-Webserver verarbeitet nur eine Anfrage gleichzeitig)
async function runSequential(job) {
    if (dsp.sending) {
        dsp.pending = true;
        return;
    }
    dsp.sending = true;
    try {
        await job();
    } catch (err) {
        console.error(err);
        $('outputDsp').textContent = 'Fehler: ' + err.message;
    } finally {
        dsp.sending = false;
    }
    if (dsp.pending) {
        dsp.pending = false;
        dsp.active ? sendAllFilters() : bypassAllFilters();
    }
}

function sendAllFilters() {
    return runSequential(async () => {
        const gains = computeGains();
        for (let i = 0; i < TEST_FREQUENCIES.length; i++) {
            const n = i + 1;
            $('outputDsp').textContent = `Sende Filter ${n} / ${TEST_FREQUENCIES.length} …`;
            await postForm(`/filter${n}`, {
                [`freq${n}`]: TEST_FREQUENCIES[i],
                [`gain${n}`]: gains[i].toFixed(1),
                [`q${n}`]: FILTER_Q,
                [`type${n}`]: FILTER_TYPE
            });
            await postForm(`/bypassFilter${n}`, { [`bypass${n}`]: 0 });
        }
        $('outputDsp').textContent = 'Gesendet: ' + gains.map((g, i) =>
            `${formatFreq(TEST_FREQUENCIES[i])} +${g.toFixed(1)} dB`).join(', ');
    });
}

function bypassAllFilters() {
    return runSequential(async () => {
        for (let n = 1; n <= TEST_FREQUENCIES.length; n++) {
            $('outputDsp').textContent = `Bypass Filter ${n} / ${TEST_FREQUENCIES.length} …`;
            await postForm(`/bypassFilter${n}`, { [`bypass${n}`]: 1 });
        }
        $('outputDsp').textContent = 'Alle Filter auf Bypass';
    });
}

function toggleDsp() {
    dsp.active = !dsp.active;
    setDspButton();
    dsp.active ? sendAllFilters() : bypassAllFilters();
}


// -------------------------------------
// Event Handlers
// -------------------------------------

$('testStart').addEventListener('click', () => {
    stopBandPlayback();
    startTest();
});
$('testCancel').addEventListener('click', () => stopTest('Test abgebrochen'));
$('testHeard').addEventListener('click', onHeard);

// Leertaste = "Ich höre den Ton" (nur während des Tests)
document.addEventListener('keydown', event => {
    if (event.code === 'Space' && test.running) {
        event.preventDefault();
        onHeard();
    }
});

$('maxBoost').addEventListener('input', () => {
    $('maxBoostValue').textContent = `${$('maxBoost').value} dB`;
    updateGainLabels();
});
$('maxBoost').addEventListener('change', () => {
    if (dsp.active) sendAllFilters();
});

$('dspToggle').addEventListener('click', toggleDsp);

// Initialisierung
$('qValue').textContent = FILTER_Q;
$('maxBoostValue').textContent = `${$('maxBoost').value} dB`;
createBandRows();
setDspButton();
