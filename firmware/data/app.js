
// -------------------------------------
// Globale Variablen
// -------------------------------------
const esp32Host = 'esp32.local';



// -------------------------------------
// Helper Functions
// -------------------------------------

// Checks if a given string is a valid number (accepts dot or comma as decimal separator)

// Funktion zur Validierung (Zahl mit optional Komma oder Punkt)
function isValidNumber(str) {
    return /^-?\d+(?:[.,]\d+)?$/.test(str.trim());
}

function checkGainValue(str) {
    const num = parseFloat(str);

    return num >= -30 && num <= 30;
}


// Replaces comma with dot and trims whitespace

function normalizeNumber(str) {

    return str.trim().replace(',', '.');
}

// Sends a POST request to the given URL path with provided data --> (Keypair)

function sendPostRequest(path, data, outputElementID) {
    fetch(path, {
        method: 'POST',
        headers: {
            'Content-Type': 'application/x-www-form-urlencoded',
        },
        body: new URLSearchParams(data)
    })
        .then(response => {
            if (!response.ok) {
                throw new Error('Serverantwort war nicht OK: ' + response.statusText);
            }
            return response.text();
        })
        .then(data => {
            console.log(`Antwort vom Server (${path}):`, data);
            document.getElementById(outputElementID).textContent = "Empfangen: " + data;

        })
        .catch(err => {
            console.error(`Fehler (${path}):`, err);
            document.getElementById(outputElementID).textContent = "Fehler: " + err.message;
        });

}

function handleChannelLevel(value, textElementId, signalLEDId) {

    const textElement = document.getElementById(textElementId);
    const LED = document.getElementById(signalLEDId);
    let number;

    if (value === "inf") {
        number = Infinity;
    } else if (value === "-inf") {
        number = -Infinity;
    } else {
        number = parseFloat(value);
    }

    // Textausgabe und LED für den Zahlenwert
    if (Number.isNaN(number)) {
        textElement.textContent = "NaN";
        updateSignalLed(LED, number);
        return;
    }
    else if (!isFinite(number)) {
        textElement.textContent = " -∞ dBFS";
        updateSignalLed(LED, number);
        return;
    }
    else if (isFinite(number)) {
        textElement.textContent = number.toFixed(2) + " dBFS";
        updateSignalLed(LED, number);
        return;
    } else {
        textElement.textContent = "undef";
        updateSignalLed(LED, NaN);

    }
    
}

// Change LED Color
function updateSignalLed(signalLed, level) {

    if (!Number.isFinite(level) || Number.isNaN(level)) {
        signalLed.classList.remove('green', 'yellow', 'red');
        signalLed.classList.add('black');
        return;
    }

    signalLed.classList.remove('green', 'yellow', 'red', 'black');

    if (level <= -70) {
        //LED bleibt grau
    } else if ((level >= -40) && (level < -10)) {
        signalLed.classList.add('green');
    } else if ((level >= -10) && (level < -4)) {
        signalLed.classList.add('yellow');
    } else if (level >= -4) {
        signalLed.classList.add('red');
    }
}


// -------------------------------------
// Event Handlers
// -------------------------------------

// Send pre-gain value
document.getElementById('gainPreSend').addEventListener('click', function () {
    const value = document.getElementById('gainPreInput').value;
    if (!isValidNumber(value)) {
        document.getElementById('outputGainPre').textContent = 'No valid Number (0,5/-3)';
        return;
    }
    const normalized = normalizeNumber(value);

    if (!checkGainValue(normalized)) {
        document.getElementById('outputGainPre').textContent = 'Only Values between -30dB and 30 dB';
        return;
    }
    //Wertepaar erstellen
    sendPostRequest('/preGain', { preGain: normalized }, 'outputGainPre');
    //Eingabefeld leeren
    document.getElementById('gainPreInput').value = '';
});

// Send post-gain value
document.getElementById('gainPostSend').addEventListener('click', function () {
    const value = document.getElementById('gainPostInput').value;
    if (!isValidNumber(value)) {
        document.getElementById('outputGainPost').textContent = 'No valid Number (0,5/-3)';
        return;
    }
    const normalized = normalizeNumber(value);

    if (!checkGainValue(normalized)) {
        document.getElementById('outputGainPost').textContent = 'Only Values between -30dB and 30 dB';
        return;
    }

    //Wertepaar erstellen
    sendPostRequest('/postGain', { postGain: normalized }, 'outputGainPost');
    //Eingabefeld leeren
    document.getElementById('gainPostInput').value = '';
});



// -------------------------------------
// Filter
// -------------------------------------

// Anzahl der Filter, muss zu NUM_FILTERS in der Firmware (include/IO.h) passen
const FILTER_COUNT = 8;

// Grenzen für die Filterparameter
const FILTER_LIMITS = {
    freqMin: 10,
    freqMax: 24000,
    gainMin: -45,
    gainMax: 45
};

// Erzeugt die Karte für Filter n (1-basiert) aus dem Template und vergibt die IDs
// IDs wie bisher: freqN, gainN, qN, typeFilterN, bypassFilterN, filterNsend, outputFilterN
function createFilterCard(template, n) {
    const card = template.content.firstElementChild.cloneNode(true);
    const part = role => card.querySelector(`[data-role="${role}"]`);

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
    // Labels mit den Eingabefeldern verknüpfen
    part('typeLabel').htmlFor = ids.type;
    part('freqLabel').htmlFor = ids.freq;
    part('gainLabel').htmlFor = ids.gain;
    part('qLabel').htmlFor = ids.q;

    return card;
}

// Setzt den Bypass-Button eines Filters auf aktiv/inaktiv
function setBypassButton(button, isBypassed) {
    button.classList.toggle('active', isBypassed);
    button.textContent = isBypassed ? "Bypassed" : "Bypass";
}

// Liest, prüft und sendet die Parameter von Filter n
function sendFilter(n) {
    const output = document.getElementById(`outputFilter${n}`);
    const inputs = ['freq', 'gain', 'q'].map(name => document.getElementById(`${name}${n}`));
    const rawValues = inputs.map(input => input.value);

    if (rawValues.some(val => !isValidNumber(val))) {
        output.textContent = 'No valid Number (0,5/-3)';
        return;
    }

    // Erst normalisieren (Komma -> Punkt), dann als Zahl prüfen
    const normalized = rawValues.map(normalizeNumber);
    const [freq, gain, q] = normalized.map(parseFloat);

    if (freq < FILTER_LIMITS.freqMin || freq > FILTER_LIMITS.freqMax ||
        gain < FILTER_LIMITS.gainMin || gain > FILTER_LIMITS.gainMax || q <= 0) {
        output.textContent = `Frequency range ${FILTER_LIMITS.freqMin} to ${FILTER_LIMITS.freqMax}, ` +
            `Gain range from ${FILTER_LIMITS.gainMin} to ${FILTER_LIMITS.gainMax} and Q greater than 0`;
        return;
    }

    sendPostRequest(`/filter${n}`, {
        [`freq${n}`]: normalized[0],
        [`gain${n}`]: normalized[1],
        [`q${n}`]: normalized[2],
        [`type${n}`]: document.getElementById(`typeFilter${n}`).value
    }, `outputFilter${n}`);

    // Felder leeren
    inputs.forEach(input => input.value = '');

    // Neue Werte heben den Bypass auf
    setBypassButton(document.getElementById(`bypassFilter${n}`), false);
}

// Bypass von Filter n umschalten (1 = Bypass aktiv, 0 = Bypass aus)
function toggleBypass(n) {
    const button = document.getElementById(`bypassFilter${n}`);
    const isBypassed = !button.classList.contains('active');
    setBypassButton(button, isBypassed);

    sendPostRequest(`/bypassFilter${n}`, { [`bypass${n}`]: isBypassed ? 1 : 0 }, `outputFilter${n}`);
}

// Alle Filter-Karten erzeugen und vor dem Post-Filter einfügen
(() => {
    const template = document.getElementById('filterTemplate');
    const postFilterContainer = document.getElementById('postFilterContainer');

    for (let n = 1; n <= FILTER_COUNT; n++) {
        const card = createFilterCard(template, n);
        postFilterContainer.before(card);

        document.getElementById(`filter${n}send`).addEventListener('click', () => sendFilter(n));
        document.getElementById(`bypassFilter${n}`).addEventListener('click', () => toggleBypass(n));
    }
})();



// Handle Mute Toggle

document.getElementById('muteCheckbox').addEventListener('click', function () {
    const value = document.getElementById('muteCheckbox').checked ? 0 : 1; // 0 for checked, 1 for uncheck oder lieber anderherum ? 1 : 0;
    sendPostRequest('/mute', { mute: value }, 'outputMute');
});






// Handel Signal detection 
// 
// mit Selbstaufrufender funktionm
(() => {
    const signalLedL = document.getElementById('signalLedL');
    const signalLedR = document.getElementById('signalLedR');
    const reconnectInterval = 2000; // in ms

    let socket;

    function connectWebSocket() {
        socket = new WebSocket(`ws://${esp32Host}:81/`);

        socket.onopen = () => {
            console.log('WebSocket connected');
            document.getElementById('outputSignalLeds').textContent = "WebSocket connected";
        };

        socket.onclose = () => {
            console.warn('WebSocket disconnected');
            document.getElementById('outputSignalLeds').textContent = "WebSocket disconnected";

            // Reconnect nach einer Pause
            setTimeout(connectWebSocket, reconnectInterval);
        };

        socket.onerror = (error) => {
            console.error('WebSocket error:', error);
            document.getElementById('outputSignalLeds').textContent = "Fehler: " + error.message;

            // Verbindung explizit schließen → triggert onclose → reconnect
            socket.close();
        };

        socket.onmessage = (event) => {
            const message = event.data;

            if (message === "Connected") {
                document.getElementById('outputSignalLeds').textContent = "ESP: " + message;
                return;
            }

            try {
                const data = JSON.parse(message);
                handleChannelLevel(data.Left, 'channelSignalLedL', 'signalLedL');
                handleChannelLevel(data.Right, 'channelSignalLedR', 'signalLedR');
            } catch (err) {
                document.getElementById('outputSignalLeds').textContent = "Ungültige Nachricht vom ESP: " + message;
                console.error("Fehler beim Parsen:", err);
                console.log("Raw Message: ", event.data);
            }
        };
    }

    // WebSocket-Verbindung starten
    connectWebSocket();
})();


