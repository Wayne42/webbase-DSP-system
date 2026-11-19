// -------------------------------------
// Globale Variablen
// -------------------------------------

// WebSocket zum selben Host wie die Seite (funktioniert per IP und per mDNS),
// Fallback auf mDNS-Namen, falls die Seite lokal geöffnet wurde
const esp32Host = location.hostname || 'esp32.local';

// Anzahl der Filter, muss zu NUM_FILTERS in der Firmware (include/IO.h) passen
const FILTER_COUNT = 8;

// Grenzen für die Parameter
const GAIN_LIMIT = 30;
const FILTER_LIMITS = {
    freqMin: 10,
    freqMax: 24000,
    gainMin: -45,
    gainMax: 45
};

const $ = id => document.getElementById(id);


// -------------------------------------
// localStorage (Eingaben merken)
// Alle Schlüssel beginnen mit "dsp." – hoertest.js nutzt dieselben Filter-Schlüssel
// -------------------------------------

const STORAGE_PREFIX = 'dsp.';

const store = {
    get(key, fallback = null) {
        try {
            const raw = localStorage.getItem(STORAGE_PREFIX + key);
            return raw === null ? fallback : JSON.parse(raw);
        } catch (err) {
            return fallback;
        }
    },
    set(key, value) {
        try {
            localStorage.setItem(STORAGE_PREFIX + key, JSON.stringify(value));
        } catch (err) { /* localStorage nicht verfügbar -> ohne Speichern weiter */ }
    },
    clear() {
        try {
            Object.keys(localStorage)
                .filter(key => key.startsWith(STORAGE_PREFIX))
                .forEach(key => localStorage.removeItem(key));
        } catch (err) { /* ignorieren */ }
    }
};


// -------------------------------------
// Helper Functions
// -------------------------------------

// Zahl mit optionalem Komma oder Punkt als Dezimaltrenner
function isValidNumber(str) {
    return /^-?\d+(?:[.,]\d+)?$/.test(String(str).trim());
}

// Komma -> Punkt, Leerzeichen entfernen
function normalizeNumber(str) {
    return String(str).trim().replace(',', '.');
}

// Statuszeile unter einer Karte setzen (type: 'ok' | 'error' | 'info')
function setStatus(outputElementID, text, type = 'info') {
    const el = $(outputElementID);
    el.textContent = text;
    el.dataset.type = type;
}

// POST-Request mit Formulardaten, Ergebnis in der Statuszeile anzeigen
function sendPostRequest(path, data, outputElementID) {
    setStatus(outputElementID, 'Sende …', 'info');
    return fetch(path, {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: new URLSearchParams(data)
    })
        .then(response => {
            if (!response.ok) {
                throw new Error('Serverantwort war nicht OK: ' + response.statusText);
            }
            return response.text();
        })
        .then(text => {
            console.log(`Antwort vom Server (${path}):`, text);
            setStatus(outputElementID, '✓ ' + text, 'ok');
        })
        .catch(err => {
            console.error(`Fehler (${path}):`, err);
            setStatus(outputElementID, 'Fehler: ' + err.message, 'error');
        });
}

// Enter in einem Eingabefeld löst den zugehörigen Senden-Button aus
function sendOnEnter(input, button) {
    input.addEventListener('keydown', event => {
        if (event.key === 'Enter') {
            event.preventDefault();
            button.click();
        }
    });
}


// -------------------------------------
// Pegelanzeige (LEDs)
// -------------------------------------

function handleChannelLevel(value, textElementId, signalLEDId) {
    const textElement = $(textElementId);
    const led = $(signalLEDId);
    let number;

    if (value === 'inf') {
        number = Infinity;
    } else if (value === '-inf') {
        number = -Infinity;
    } else {
        number = parseFloat(value);
    }

    if (Number.isNaN(number)) {
        textElement.textContent = 'NaN';
    } else if (!isFinite(number)) {
        textElement.textContent = '-∞ dBFS';
    } else {
        textElement.textContent = number.toFixed(1) + ' dBFS';
    }
    updateSignalLed(led, number);
}

function updateSignalLed(signalLed, level) {
    signalLed.classList.remove('green', 'yellow', 'red', 'black');

    if (!Number.isFinite(level)) {
        signalLed.classList.add('black');
        return;
    }
    if (level <= -70) {
        // LED bleibt grau
    } else if (level >= -40 && level < -10) {
        signalLed.classList.add('green');
    } else if (level >= -10 && level < -4) {
        signalLed.classList.add('yellow');
    } else if (level >= -4) {
        signalLed.classList.add('red');
    }
}


// -------------------------------------
// Pre / Post Gain
// Slider und Textfeld sind gekoppelt, der Wert wird gespeichert
// -------------------------------------

function setupGain(name) {
    // name = 'Pre' | 'Post'
    const key = name.toLowerCase() + 'Gain';      // preGain / postGain (Route + Parameter)
    const slider = $(`gain${name}Slider`);
    const input = $(`gain${name}Input`);
    const button = $(`gain${name}Send`);
    const outputId = `outputGain${name}`;

    const saved = store.get(key);
    if (saved !== null) {
        input.value = saved;
        if (isValidNumber(saved)) slider.value = normalizeNumber(saved);
    }

    slider.addEventListener('input', () => {
        input.value = slider.value;
        store.set(key, input.value);
    });
    // Loslassen des Sliders sendet direkt
    slider.addEventListener('change', () => button.click());

    input.addEventListener('input', () => {
        store.set(key, input.value);
        if (isValidNumber(input.value)) slider.value = normalizeNumber(input.value);
    });
    sendOnEnter(input, button);

    button.addEventListener('click', () => {
        const value = input.value === '' ? slider.value : input.value;
        if (!isValidNumber(value)) {
            setStatus(outputId, 'Keine gültige Zahl (z. B. 0,5 oder -3)', 'error');
            return;
        }
        const normalized = normalizeNumber(value);
        const num = parseFloat(normalized);
        if (num < -GAIN_LIMIT || num > GAIN_LIMIT) {
            setStatus(outputId, `Nur Werte von -${GAIN_LIMIT} bis ${GAIN_LIMIT} dB`, 'error');
            return;
        }
        input.value = value;
        store.set(key, value);
        sendPostRequest(`/${key}`, { [key]: normalized }, outputId);
    });
}

setupGain('Pre');
setupGain('Post');


// -------------------------------------
// Filter
// -------------------------------------

// Gespeicherter Zustand eines Filters: { type, freq, gain, q, bypass }
function loadFilter(n) {
    return store.get(`filter${n}`, {});
}

function saveFilter(n, patch) {
    store.set(`filter${n}`, { ...loadFilter(n), ...patch });
}

// Erzeugt die Karte für Filter n (1-basiert) aus dem Template und vergibt die IDs
// IDs: freqN, gainN, qN, typeFilterN, bypassFilterN, filterNsend, outputFilterN
function createFilterCard(template, n) {
    const card = template.content.firstElementChild.cloneNode(true);
    const part = role => card.querySelector(`[data-role="${role}"]`);

    card.id = `filterCard${n}`;
    part('title').textContent = `Filter ${n}`;

    const ids = {
        bypass: `bypassFilter${n}`,
        type: `typeFilter${n}`,
        freq: `freq${n}`,
        gain: `gain${n}`,
        q: `q${n}`,
        send: `filter${n}send`,
        output: `outputFilter${n}`
    };
    for (const [role, id] of Object.entries(ids)) {
        part(role).id = id;
    }
    part('typeLabel').htmlFor = ids.type;
    part('freqLabel').htmlFor = ids.freq;
    part('gainLabel').htmlFor = ids.gain;
    part('qLabel').htmlFor = ids.q;

    return card;
}

// Bypass-Zustand in Button, Karte und Sprungnavigation anzeigen
function setBypassButton(n, isBypassed) {
    const button = $(`bypassFilter${n}`);
    button.classList.toggle('active', isBypassed);
    button.textContent = isBypassed ? 'Bypassed' : 'Bypass';
    $(`filterCard${n}`).classList.toggle('is-bypassed', isBypassed);
    $(`chipFilter${n}`).classList.toggle('is-bypassed', isBypassed);
}

// Liest, prüft und sendet die Parameter von Filter n
function sendFilter(n) {
    const outputId = `outputFilter${n}`;
    const inputs = ['freq', 'gain', 'q'].map(name => $(`${name}${n}`));
    const rawValues = inputs.map(input => input.value);

    if (rawValues.some(val => !isValidNumber(val))) {
        setStatus(outputId, 'Bitte Frequenz, Gain und Q als Zahl eingeben', 'error');
        return;
    }

    const normalized = rawValues.map(normalizeNumber);
    const [freq, gain, q] = normalized.map(parseFloat);

    if (freq < FILTER_LIMITS.freqMin || freq > FILTER_LIMITS.freqMax) {
        setStatus(outputId, `Frequenz: ${FILTER_LIMITS.freqMin} bis ${FILTER_LIMITS.freqMax} Hz`, 'error');
        return;
    }
    if (gain < FILTER_LIMITS.gainMin || gain > FILTER_LIMITS.gainMax) {
        setStatus(outputId, `Gain: ${FILTER_LIMITS.gainMin} bis ${FILTER_LIMITS.gainMax} dB`, 'error');
        return;
    }
    if (q <= 0) {
        setStatus(outputId, 'Q muss größer als 0 sein', 'error');
        return;
    }

    const type = $(`typeFilter${n}`).value;
    sendPostRequest(`/filter${n}`, {
        [`freq${n}`]: normalized[0],
        [`gain${n}`]: normalized[1],
        [`q${n}`]: normalized[2],
        [`type${n}`]: type
    }, outputId);

    // Neue Werte heben den Bypass auf
    setBypassButton(n, false);
    saveFilter(n, { bypass: false });
}

// Bypass von Filter n umschalten (1 = Bypass aktiv, 0 = Bypass aus)
function toggleBypass(n) {
    const isBypassed = !$(`bypassFilter${n}`).classList.contains('active');
    setBypassButton(n, isBypassed);
    saveFilter(n, { bypass: isBypassed });
    sendPostRequest(`/bypassFilter${n}`, { [`bypass${n}`]: isBypassed ? 1 : 0 }, `outputFilter${n}`);
}

// Alle Filter-Karten und Sprung-Chips erzeugen, gespeicherte Werte eintragen
(() => {
    const template = $('filterTemplate');
    const grid = $('filterGrid');
    const chipPost = $('chipPost');

    for (let n = 1; n <= FILTER_COUNT; n++) {
        grid.appendChild(createFilterCard(template, n));

        const chip = document.createElement('a');
        chip.href = `#filterCard${n}`;
        chip.className = 'chip';
        chip.id = `chipFilter${n}`;
        chip.textContent = `F${n}`;
        chipPost.before(chip);

        // Gespeicherte Werte vorbefüllen und Änderungen direkt speichern
        const saved = loadFilter(n);
        for (const name of ['freq', 'gain', 'q']) {
            const input = $(`${name}${n}`);
            if (saved[name] !== undefined) input.value = saved[name];
            input.addEventListener('input', () => saveFilter(n, { [name]: input.value }));
            sendOnEnter(input, $(`filter${n}send`));
        }
        const typeSelect = $(`typeFilter${n}`);
        if (saved.type && typeSelect.querySelector(`option[value="${saved.type}"]`)) {
            typeSelect.value = saved.type;
        }
        typeSelect.addEventListener('change', () => saveFilter(n, { type: typeSelect.value }));

        setBypassButton(n, saved.bypass === true);

        $(`filter${n}send`).addEventListener('click', () => sendFilter(n));
        $(`bypassFilter${n}`).addEventListener('click', () => toggleBypass(n));
    }
})();


// -------------------------------------
// Mute
// -------------------------------------

(() => {
    const checkbox = $('muteCheckbox');
    checkbox.checked = store.get('mute', false) === true;

    checkbox.addEventListener('change', () => {
        store.set('mute', checkbox.checked);
        const value = checkbox.checked ? 0 : 1; // 0 = stumm, 1 = Ton an
        sendPostRequest('/mute', { mute: value }, 'outputMute');
    });
})();


// -------------------------------------
// Gespeicherte Werte löschen
// -------------------------------------

$('resetStorage').addEventListener('click', () => {
    if (confirm('Alle gespeicherten Eingaben löschen? Die Werte im DSP bleiben unverändert.')) {
        store.clear();
        location.reload();
    }
});


// -------------------------------------
// Sprungnavigation: aktiven Abschnitt beim Scrollen markieren
// -------------------------------------

(() => {
    const subnav = $('subnav');
    const chips = [...subnav.querySelectorAll('.chip')];
    const targets = chips.map(chip => document.querySelector(chip.getAttribute('href')));

    function setActive(index) {
        chips.forEach((chip, i) => chip.classList.toggle('active', i === index));
        // aktiven Chip in der (horizontal scrollbaren) Leiste sichtbar halten
        const chip = chips[index];
        const left = chip.offsetLeft - (subnav.clientWidth - chip.offsetWidth) / 2;
        subnav.scrollTo({ left, behavior: 'smooth' });
    }

    function update() {
        const offset = $('subnav').getBoundingClientRect().bottom + 40;
        let active = 0;
        targets.forEach((el, i) => {
            if (el.getBoundingClientRect().top <= offset) active = i;
        });
        // ganz unten angekommen -> letzten Abschnitt markieren
        if (window.innerHeight + window.scrollY >= document.body.scrollHeight - 2) {
            active = targets.length - 1;
        }
        if (!chips[active].classList.contains('active')) setActive(active);
    }

    let ticking = false;
    window.addEventListener('scroll', () => {
        if (ticking) return;
        ticking = true;
        requestAnimationFrame(() => { update(); ticking = false; });
    }, { passive: true });
    update();
})();


// -------------------------------------
// WebSocket: Pegel L/R vom ESP
// -------------------------------------

(() => {
    const reconnectInterval = 2000; // in ms
    const status = $('connectionStatus');
    const statusText = $('outputSignalLeds');
    let socket;

    function setConnection(state, text) {
        status.dataset.state = state; // 'ok' | 'error' | 'pending'
        statusText.textContent = text;
    }

    function connectWebSocket() {
        setConnection('pending', 'Verbinde …');
        socket = new WebSocket(`ws://${esp32Host}:81/`);

        socket.onopen = () => {
            console.log('WebSocket connected');
            setConnection('ok', 'Verbunden');
        };

        socket.onclose = () => {
            console.warn('WebSocket disconnected');
            setConnection('error', 'Getrennt');
            setTimeout(connectWebSocket, reconnectInterval);
        };

        socket.onerror = (error) => {
            console.error('WebSocket error:', error);
            // Schließen triggert onclose -> reconnect
            socket.close();
        };

        socket.onmessage = (event) => {
            const message = event.data;

            if (message === 'Connected') {
                setConnection('ok', 'Verbunden');
                return;
            }

            try {
                const data = JSON.parse(message);
                handleChannelLevel(data.Left, 'channelSignalLedL', 'signalLedL');
                handleChannelLevel(data.Right, 'channelSignalLedR', 'signalLedR');
            } catch (err) {
                console.error('Ungültige Nachricht vom ESP:', message, err);
            }
        };
    }

    connectWebSocket();
})();
