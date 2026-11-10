// Smart LED WS2812B - Mobile-First Controller Web App
document.addEventListener('DOMContentLoaded', () => {
    // State management
    const state = {
        power: true,
        brightness: 150,
        color: { r: 255, g: 0, b: 150 },
        color_temp: 4000,
        effect_mode: 0,
        speed: 50,
        intensity: 50,
        num_leds: 30,
        max_ma: 1500,
        gradient: [
            { r: 255, g: 0, b: 128 },
            { r: 0, g: 240, b: 255 }
        ],
        timer: {
            active: false,
            remaining_sec: 0,
            gradual_dim: true
        },
        schedule: {
            enabled: false,
            on_hour: 18,
            on_min: 0,
            off_hour: 6,
            off_min: 0
        }
    };

    // WebSocket connection
    let ws = null;
    let wsReconnectTimer = null;
    let timerCountdownInterval = null;
    let throttleTimeout = null;

    // DOM Elements
    const connStatus = document.getElementById('connStatus');
    const powerBtn = document.getElementById('powerBtn');
    const powerLabel = document.getElementById('powerLabel');
    const colorBadge = document.getElementById('colorBadge');
    const hexInput = document.getElementById('hexInput');
    const rgbDisplay = document.getElementById('rgbDisplay');
    const brightSlider = document.getElementById('brightSlider');
    const brightVal = document.getElementById('brightVal');
    const tempSlider = document.getElementById('tempSlider');
    const tempVal = document.getElementById('tempVal');
    const speedSlider = document.getElementById('speedSlider');
    const speedVal = document.getElementById('speedVal');
    const intensitySlider = document.getElementById('intensitySlider');
    const intensityVal = document.getElementById('intensityVal');
    const effectsGrid = document.getElementById('effectsGrid');
    const gradientPreviewBar = document.getElementById('gradientPreviewBar');
    const gradientStopsList = document.getElementById('gradientStopsList');
    const gradientStopsCount = document.getElementById('gradientStopsCount');
    const addStopBtn = document.getElementById('addStopBtn');
    const applyGradientBtn = document.getElementById('applyGradientBtn');
    const gradualDimCheck = document.getElementById('gradualDimCheck');
    const timerBadge = document.getElementById('timerBadge');
    const timerCountdownBox = document.getElementById('timerCountdownBox');
    const countdownVal = document.getElementById('countdownVal');
    const cancelTimerBtn = document.getElementById('cancelTimerBtn');
    const clockDisplay = document.getElementById('clockDisplay');
    const scheduleEnableCheck = document.getElementById('scheduleEnableCheck');
    const onTimeInput = document.getElementById('onTimeInput');
    const offTimeInput = document.getElementById('offTimeInput');
    const saveScheduleBtn = document.getElementById('saveScheduleBtn');
    const numLedsSlider = document.getElementById('numLedsSlider');
    const numLedsVal = document.getElementById('numLedsVal');
    const maxMaSlider = document.getElementById('maxMaSlider');
    const maxMaVal = document.getElementById('maxMaVal');
    const saveSettingsBtn = document.getElementById('saveSettingsBtn');
    const resetWifiBtn = document.getElementById('resetWifiBtn');
    const toast = document.getElementById('toast');

    // Diagnostic elements
    const diagIp = document.getElementById('diagIp');
    const diagHeap = document.getElementById('diagHeap');
    const diagRssi = document.getElementById('diagRssi');
    const diagUptime = document.getElementById('diagUptime');
    const diagClients = document.getElementById('diagClients');

    // 21 Effects Catalog
    const effectsList = [
        { id: 0, name: "Solid Color", desc: "Warna Statis Penuh" },
        { id: 1, name: "Rainbow Cycle", desc: "Pelangi Mengalir Halus" },
        { id: 2, name: "Rainbow Wave", desc: "Gelombang Spektrum Warna" },
        { id: 3, name: "Breathing", desc: "Denyut Bernapas Halus" },
        { id: 4, name: "Comet / Meteor", desc: "Bintang Jatuh Berekor" },
        { id: 5, name: "Fire 2012", desc: "Simulasi Api Unggun Nyata" },
        { id: 6, name: "Twinkle / Sparkle", desc: "Kilauan Bintang Malam" },
        { id: 7, name: "Police Strobe", desc: "Lampu Strobo Polisi" },
        { id: 8, name: "Color Wipe", desc: "Sapu Warna Bergantian" },
        { id: 9, name: "Theater Chase", desc: "Lampu Sorot Panggung" },
        { id: 10, name: "Gradient Flow", desc: "Aliran Multi-Warna Dinamis" },
        { id: 11, name: "Pacman Chaser", desc: "Pacman Memakan Butir Titik" },
        { id: 12, name: "Sinelon", desc: "Ayunan Gelombang Sinus" },
        { id: 13, name: "Juggle", desc: "Titik Warna Memantul Acak" },
        { id: 14, name: "Matrix Rain", desc: "Hujan Kode Cyberpunk Hijau" },
        { id: 15, name: "Strobe Flash", desc: "Kilatan Blitz Cepat" },
        { id: 16, name: "BPM Pulse", desc: "Denyut Irama Beat Musik" },
        { id: 17, name: "Larson Scanner", desc: "Mata Cylon / Knight Rider" },
        { id: 18, name: "Ripple", desc: "Riak Air Melebar ke Luar" },
        { id: 19, name: "Heartbeat", desc: "Denyut Jantung Ganda" },
        { id: 20, name: "Confetti", desc: "Taburan Warna Warni Pesta" }
    ];

    // Initialize Effects Grid
    function renderEffectsList() {
        effectsGrid.innerHTML = '';
        effectsList.forEach(eff => {
            const card = document.createElement('div');
            card.className = `effect-card ${state.effect_mode === eff.id ? 'active' : ''}`;
            card.dataset.id = eff.id;
            card.innerHTML = `
                <div class="effect-icon">
                    <svg viewBox="0 0 24 24" width="20" height="20" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round">
                        <polygon points="12 2 15.09 8.26 22 9.27 17 14.14 18.18 21.02 12 17.77 5.82 21.02 7 14.14 2 9.27 8.91 8.26 12 2"></polygon>
                    </svg>
                </div>
                <div class="effect-info">
                    <h3>${eff.name}</h3>
                    <span>${eff.desc}</span>
                </div>
            `;
            card.addEventListener('click', () => {
                selectEffect(eff.id);
            });
            effectsGrid.appendChild(card);
        });
    }

    function selectEffect(id) {
        state.effect_mode = id;
        document.querySelectorAll('.effect-card').forEach(c => {
            c.classList.toggle('active', parseInt(c.dataset.id) === id);
        });
        sendJson({
            cmd: "effect",
            id: id,
            speed: state.speed,
            intensity: state.intensity
        });
        showToast(`Efek: ${effectsList.find(e => e.id === id)?.name || 'Aktif'}`);
    }

    // Dynamic Neon Accent update
    function updateThemeAccent(hex) {
        document.documentElement.style.setProperty('--accent-color', hex);
        const rgb = hexToRgb(hex);
        if (rgb) {
            document.documentElement.style.setProperty('--accent-glow', `rgba(${rgb.r}, ${rgb.g}, ${rgb.b}, 0.45)`);
        }
    }

    // Toast feedback
    function showToast(msg) {
        toast.textContent = msg;
        toast.classList.add('show');
        setTimeout(() => toast.classList.remove('show'), 2200);
    }

    // Color Helpers
    function rgbToHex(r, g, b) {
        return "#" + ((1 << 24) + (r << 16) + (g << 8) + b).toString(16).slice(1).toUpperCase();
    }

    function hexToRgb(hex) {
        const clean = hex.replace('#', '');
        if (clean.length === 3) {
            return {
                r: parseInt(clean[0] + clean[0], 16),
                g: parseInt(clean[1] + clean[1], 16),
                b: parseInt(clean[2] + clean[2], 16)
            };
        }
        if (clean.length === 6) {
            return {
                r: parseInt(clean.substring(0, 2), 16),
                g: parseInt(clean.substring(2, 4), 16),
                b: parseInt(clean.substring(4, 6), 16)
            };
        }
        return null;
    }

    // HTML5 Canvas Color Wheel
    const canvas = document.getElementById('colorWheel');
    const ctx = canvas.getContext('2d');
    const wheelCursor = document.getElementById('wheelCursor');
    const radius = canvas.width / 2;
    let isDraggingWheel = false;

    function drawColorWheel() {
        const image = ctx.createImageData(canvas.width, canvas.height);
        const data = image.data;

        for (let x = -radius; x < radius; x++) {
            for (let y = -radius; y < radius; y++) {
                const dist = Math.sqrt(x * x + y * y);
                if (dist <= radius) {
                    let angle = Math.atan2(y, x);
                    if (angle < 0) angle += 2 * Math.PI;

                    const hue = (angle / (2 * Math.PI)) * 360;
                    const sat = dist / radius;
                    const rgb = hsvToRgb(hue, sat, 1.0);

                    const pixelX = x + radius;
                    const pixelY = y + radius;
                    const idx = (pixelY * canvas.width + pixelX) * 4;

                    data[idx] = rgb.r;
                    data[idx + 1] = rgb.g;
                    data[idx + 2] = rgb.b;
                    data[idx + 3] = 255;
                }
            }
        }
        ctx.putImageData(image, 0, 0);
    }

    function hsvToRgb(h, s, v) {
        let f = (n, k = (n + h / 60) % 6) => v - v * s * Math.max(Math.min(k, 4 - k, 1), 0);
        return {
            r: Math.round(f(5) * 255),
            g: Math.round(f(3) * 255),
            b: Math.round(f(1) * 255)
        };
    }

    function updateWheelCursor(x, y) {
        wheelCursor.style.left = `${x}px`;
        wheelCursor.style.top = `${y}px`;
    }

    function handleWheelInput(clientX, clientY, sendNow = false) {
        const rect = canvas.getBoundingClientRect();
        const x = clientX - rect.left - radius;
        const y = clientY - rect.top - radius;
        let dist = Math.sqrt(x * x + y * y);

        let clampedX = x;
        let clampedY = y;
        if (dist > radius - 2) {
            clampedX = (x / dist) * (radius - 2);
            clampedY = (y / dist) * (radius - 2);
            dist = radius - 2;
        }

        updateWheelCursor(clampedX + radius, clampedY + radius);

        let angle = Math.atan2(clampedY, clampedX);
        if (angle < 0) angle += 2 * Math.PI;

        const hue = (angle / (2 * Math.PI)) * 360;
        const sat = Math.min(dist / (radius - 2), 1.0);
        const rgb = hsvToRgb(hue, sat, 1.0);

        state.color = rgb;
        const hex = rgbToHex(rgb.r, rgb.g, rgb.b);
        colorBadge.textContent = hex;
        hexInput.value = hex;
        rgbDisplay.textContent = `RGB: ${rgb.r}, ${rgb.g}, ${rgb.b}`;
        updateThemeAccent(hex);

        sendThrottled({
            cmd: "color",
            r: rgb.r,
            g: rgb.g,
            b: rgb.b
        }, sendNow);
    }

    // Color Wheel Event Listeners
    canvas.addEventListener('mousedown', (e) => {
        isDraggingWheel = true;
        handleWheelInput(e.clientX, e.clientY, true);
    });

    window.addEventListener('mousemove', (e) => {
        if (isDraggingWheel) {
            handleWheelInput(e.clientX, e.clientY, false);
        }
    });

    window.addEventListener('mouseup', () => {
        if (isDraggingWheel) {
            isDraggingWheel = false;
        }
    });

    canvas.addEventListener('touchstart', (e) => {
        isDraggingWheel = true;
        const touch = e.touches[0];
        handleWheelInput(touch.clientX, touch.clientY, true);
    }, { passive: false });

    canvas.addEventListener('touchmove', (e) => {
        if (isDraggingWheel) {
            e.preventDefault();
            const touch = e.touches[0];
            handleWheelInput(touch.clientX, touch.clientY, false);
        }
    }, { passive: false });

    canvas.addEventListener('touchend', () => {
        isDraggingWheel = false;
    });

    // Hex Input Event Listener
    hexInput.addEventListener('change', (e) => {
        let val = e.target.value.trim();
        if (!val.startsWith('#')) val = '#' + val;
        const rgb = hexToRgb(val);
        if (rgb) {
            state.color = rgb;
            colorBadge.textContent = val.toUpperCase();
            rgbDisplay.textContent = `RGB: ${rgb.r}, ${rgb.g}, ${rgb.b}`;
            updateThemeAccent(val);
            sendJson({ cmd: "color", r: rgb.r, g: rgb.g, b: rgb.b });
        }
    });

    // Power Button
    powerBtn.addEventListener('click', () => {
        state.power = !state.power;
        updatePowerUI(state.power);
        sendJson({ cmd: "power", state: state.power });
        showToast(state.power ? "LED Dinyalakan" : "LED Dimatikan");
    });

    function updatePowerUI(power) {
        powerBtn.classList.toggle('active', power);
        powerLabel.classList.toggle('active', power);
        powerLabel.textContent = power ? "MENYALA" : "MATI";
    }

    // Brightness Slider
    brightSlider.addEventListener('input', (e) => {
        const val = parseInt(e.target.value);
        state.brightness = val;
        const pct = Math.round((val / 255) * 100);
        brightVal.textContent = `${pct}%`;
        sendThrottled({ cmd: "brightness", val: val });
    });

    // Brightness Quick Chips
    document.querySelectorAll('.chip[data-bright]').forEach(chip => {
        chip.addEventListener('click', () => {
            const pct = parseInt(chip.dataset.bright);
            const val = Math.round((pct / 100) * 255);
            brightSlider.value = val;
            brightVal.textContent = `${pct}%`;
            state.brightness = val;
            sendJson({ cmd: "brightness", val: val });
        });
    });

    // Color Temperature Slider
    tempSlider.addEventListener('input', (e) => {
        const val = parseInt(e.target.value);
        state.color_temp = val;
        tempVal.textContent = `${val}K`;
        sendThrottled({ cmd: "temp", val: val });
    });

    // Quick Presets
    document.querySelectorAll('.preset-btn').forEach(btn => {
        btn.addEventListener('click', () => {
            const hex = btn.dataset.color;
            const temp = btn.dataset.temp;
            if (temp) {
                const k = parseInt(temp);
                tempSlider.value = k;
                tempVal.textContent = `${k}K`;
                sendJson({ cmd: "temp", val: k });
            } else if (hex) {
                const rgb = hexToRgb(hex);
                if (rgb) {
                    state.color = rgb;
                    hexInput.value = hex;
                    colorBadge.textContent = hex;
                    rgbDisplay.textContent = `RGB: ${rgb.r}, ${rgb.g}, ${rgb.b}`;
                    updateThemeAccent(hex);
                    sendJson({ cmd: "color", r: rgb.r, g: rgb.g, b: rgb.b });
                }
            }
            showToast(`Preset: ${btn.dataset.name}`);
        });
    });

    // Speed & Intensity Sliders
    speedSlider.addEventListener('input', (e) => {
        const val = parseInt(e.target.value);
        state.speed = val;
        speedVal.textContent = val;
        sendThrottled({ cmd: "speed", val: val });
    });

    intensitySlider.addEventListener('input', (e) => {
        const val = parseInt(e.target.value);
        state.intensity = val;
        intensityVal.textContent = val;
        sendThrottled({ cmd: "intensity", val: val });
    });

    // Custom Gradient Management
    function renderGradientUI() {
        gradientStopsList.innerHTML = '';
        gradientStopsCount.textContent = `${state.gradient.length} Titik`;

        // Update preview bar gradient CSS
        const cssStops = state.gradient.map(s => rgbToHex(s.r, s.g, s.b)).join(', ');
        gradientPreviewBar.style.background = `linear-gradient(to right, ${cssStops})`;

        state.gradient.forEach((stop, idx) => {
            const item = document.createElement('div');
            item.className = 'gradient-stop-item';
            const hex = rgbToHex(stop.r, stop.g, stop.b);

            item.innerHTML = `
                <div class="stop-label">
                    <span>Titik #${idx + 1}</span>
                    <input type="color" class="stop-color-input" value="${hex}" data-idx="${idx}">
                </div>
                ${state.gradient.length > 2 ? `<button class="btn btn-danger btn-sm remove-stop-btn" data-idx="${idx}">Hapus</button>` : ''}
            `;
            gradientStopsList.appendChild(item);
        });

        // Attach stop listeners
        document.querySelectorAll('.stop-color-input').forEach(input => {
            input.addEventListener('input', (e) => {
                const idx = parseInt(e.target.dataset.idx);
                const rgb = hexToRgb(e.target.value);
                if (rgb) {
                    state.gradient[idx] = rgb;
                    const updatedCss = state.gradient.map(s => rgbToHex(s.r, s.g, s.b)).join(', ');
                    gradientPreviewBar.style.background = `linear-gradient(to right, ${updatedCss})`;
                }
            });
        });

        document.querySelectorAll('.remove-stop-btn').forEach(btn => {
            btn.addEventListener('click', (e) => {
                const idx = parseInt(e.target.dataset.idx);
                if (state.gradient.length > 2) {
                    state.gradient.splice(idx, 1);
                    renderGradientUI();
                }
            });
        });

        addStopBtn.style.display = state.gradient.length >= 4 ? 'none' : 'block';
    }

    addStopBtn.addEventListener('click', () => {
        if (state.gradient.length < 4) {
            // Default new color
            state.gradient.push({ r: 255, g: 180, b: 0 });
            renderGradientUI();
        }
    });

    applyGradientBtn.addEventListener('click', () => {
        sendJson({
            cmd: "gradient",
            stops: state.gradient
        });
        showToast("Gradasi Diterapkan ke Strip LED!");
    });

    // Sleep Timer
    document.querySelectorAll('.timer-chip').forEach(chip => {
        chip.addEventListener('click', () => {
            const mins = parseInt(chip.dataset.mins);
            const gradual = gradualDimCheck.checked;
            sendJson({
                cmd: "timer",
                enable: true,
                minutes: mins,
                gradual: gradual
            });
            showToast(`Sleep Timer: ${mins} Menit Aktif`);
        });
    });

    cancelTimerBtn.addEventListener('click', () => {
        sendJson({ cmd: "timer", enable: false });
        showToast("Sleep Timer Dibatalkan");
    });

    function updateTimerUI(timer) {
        if (timer && timer.active && timer.remaining_sec > 0) {
            timerBadge.textContent = "Aktif";
            timerBadge.style.color = "var(--accent-color)";
            timerCountdownBox.style.display = "block";

            let rem = timer.remaining_sec;
            if (timerCountdownInterval) clearInterval(timerCountdownInterval);

            const tick = () => {
                if (rem <= 0) {
                    countdownVal.textContent = "00:00";
                    timerBadge.textContent = "Tidak Aktif";
                    timerBadge.style.color = "";
                    timerCountdownBox.style.display = "none";
                    clearInterval(timerCountdownInterval);
                    return;
                }
                const m = Math.floor(rem / 60);
                const s = rem % 60;
                countdownVal.textContent = `${String(m).padStart(2, '0')}:${String(s).padStart(2, '0')}`;
                rem--;
            };
            tick();
            timerCountdownInterval = setInterval(tick, 1000);
        } else {
            timerBadge.textContent = "Tidak Aktif";
            timerBadge.style.color = "";
            timerCountdownBox.style.display = "none";
            if (timerCountdownInterval) clearInterval(timerCountdownInterval);
        }
    }

    // Schedule
    saveScheduleBtn.addEventListener('click', () => {
        const enabled = scheduleEnableCheck.checked;
        const on = onTimeInput.value;
        const off = offTimeInput.value;
        sendJson({
            cmd: "schedule",
            enabled: enabled,
            on: on,
            off: off
        });
        showToast("Jadwal Harian Berhasil Disimpan!");
    });

    // Hardware Settings
    numLedsSlider.addEventListener('input', (e) => {
        numLedsVal.textContent = `${e.target.value} LED`;
    });

    maxMaSlider.addEventListener('input', (e) => {
        maxMaVal.textContent = `${e.target.value} mA`;
    });

    saveSettingsBtn.addEventListener('click', () => {
        const count = parseInt(numLedsSlider.value);
        const ma = parseInt(maxMaSlider.value);
        sendJson({
            cmd: "settings",
            num_leds: count,
            max_ma: ma
        });
        showToast("Pengaturan Hardware Berhasil Disimpan!");
    });

    // WiFi Reset
    resetWifiBtn.addEventListener('click', () => {
        if (confirm("Reset seluruh kredensial WiFi? ESP8266 akan restart dan membuka Access Point SmartLED-Setup.")) {
            sendJson({ cmd: "reset_wifi" });
            showToast("Mereset WiFi... Hubungkan ke SmartLED-Setup");
        }
    });

    // Navigation Tabs
    const navItems = document.querySelectorAll('.nav-item');
    const tabPanes = document.querySelectorAll('.tab-pane');

    navItems.forEach(item => {
        item.addEventListener('click', () => {
            const targetId = item.dataset.tab;
            navItems.forEach(n => n.classList.remove('active'));
            tabPanes.forEach(p => p.classList.remove('active'));

            item.classList.add('active');
            const targetPane = document.getElementById(targetId);
            if (targetPane) targetPane.classList.add('active');
            window.scrollTo({ top: 0, behavior: 'smooth' });
        });
    });

    // WebSocket Management
    function initWebSocket() {
        const proto = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
        const host = window.location.host || 'smartled.local';
        const wsUrl = `${proto}//${host}/ws`;

        connStatus.className = 'connection-status';
        connStatus.querySelector('.status-text').textContent = 'Connecting';

        try {
            ws = new WebSocket(wsUrl);

            ws.onopen = () => {
                connStatus.className = 'connection-status connected';
                connStatus.querySelector('.status-text').textContent = 'Online';
                if (wsReconnectTimer) {
                    clearTimeout(wsReconnectTimer);
                    wsReconnectTimer = null;
                }
            };

            ws.onmessage = (event) => {
                try {
                    const data = JSON.parse(event.data);
                    if (data.type === 'state') {
                        applyServerState(data);
                    } else if (data.type === 'diag') {
                        applyServerDiag(data);
                    }
                } catch (e) {
                    console.error("WS Parse Error:", e);
                }
            };

            ws.onclose = () => {
                connStatus.className = 'connection-status disconnected';
                connStatus.querySelector('.status-text').textContent = 'Offline';
                scheduleReconnect();
            };

            ws.onerror = () => {
                connStatus.className = 'connection-status disconnected';
                ws.close();
            };
        } catch (err) {
            scheduleReconnect();
        }
    }

    function scheduleReconnect() {
        if (!wsReconnectTimer) {
            wsReconnectTimer = setTimeout(() => {
                initWebSocket();
            }, 2500);
        }
    }

    function sendJson(payload) {
        if (ws && ws.readyState === WebSocket.OPEN) {
            ws.send(JSON.stringify(payload));
        }
    }

    function sendThrottled(payload, immediate = false) {
        if (immediate) {
            if (throttleTimeout) clearTimeout(throttleTimeout);
            sendJson(payload);
            return;
        }
        if (!throttleTimeout) {
            throttleTimeout = setTimeout(() => {
                sendJson(payload);
                throttleTimeout = null;
            }, 60);
        }
    }

    // Apply Incoming State from ESP8266
    function applyServerState(s) {
        // Power
        if (s.power !== undefined) {
            state.power = s.power;
            updatePowerUI(s.power);
        }

        // Color
        if (s.color) {
            state.color = s.color;
            const hex = rgbToHex(s.color.r, s.color.g, s.color.b);
            colorBadge.textContent = hex;
            hexInput.value = hex;
            rgbDisplay.textContent = `RGB: ${s.color.r}, ${s.color.g}, ${s.color.b}`;
            updateThemeAccent(hex);
        }

        // Brightness
        if (s.brightness !== undefined) {
            state.brightness = s.brightness;
            brightSlider.value = s.brightness;
            brightVal.textContent = `${Math.round((s.brightness / 255) * 100)}%`;
        }

        // Temp
        if (s.color_temp !== undefined) {
            state.color_temp = s.color_temp;
            tempSlider.value = s.color_temp;
            tempVal.textContent = `${s.color_temp}K`;
        }

        // Effect Mode
        if (s.effect_mode !== undefined) {
            state.effect_mode = s.effect_mode;
            document.querySelectorAll('.effect-card').forEach(c => {
                c.classList.toggle('active', parseInt(c.dataset.id) === s.effect_mode);
            });
        }

        // Speed & Intensity
        if (s.speed !== undefined) {
            state.speed = s.speed;
            speedSlider.value = s.speed;
            speedVal.textContent = s.speed;
        }
        if (s.intensity !== undefined) {
            state.intensity = s.intensity;
            intensitySlider.value = s.intensity;
            intensityVal.textContent = s.intensity;
        }

        // Hardware
        if (s.num_leds !== undefined) {
            state.num_leds = s.num_leds;
            numLedsSlider.value = s.num_leds;
            numLedsVal.textContent = `${s.num_leds} LED`;
        }
        if (s.max_ma !== undefined) {
            state.max_ma = s.max_ma;
            maxMaSlider.value = s.max_ma;
            maxMaVal.textContent = `${s.max_ma} mA`;
        }

        // Gradient
        if (s.gradient && Array.isArray(s.gradient)) {
            state.gradient = s.gradient;
            renderGradientUI();
        }

        // Timer
        if (s.timer) {
            state.timer = s.timer;
            updateTimerUI(s.timer);
        }

        // Schedule
        if (s.schedule) {
            state.schedule = s.schedule;
            scheduleEnableCheck.checked = s.schedule.enabled;
            onTimeInput.value = `${String(s.schedule.on_hour).padStart(2, '0')}:${String(s.schedule.on_min).padStart(2, '0')}`;
            offTimeInput.value = `${String(s.schedule.off_hour).padStart(2, '0')}:${String(s.schedule.off_min).padStart(2, '0')}`;
        }
    }

    // Apply Telemetry Diagnostics from ESP8266
    function applyServerDiag(d) {
        if (d.ip && diagIp) diagIp.textContent = d.ip;
        if (d.free_heap !== undefined && diagHeap) diagHeap.textContent = `${(d.free_heap / 1024).toFixed(1)} KB`;
        if (d.rssi !== undefined && diagRssi) diagRssi.textContent = `${d.rssi} dBm`;
        if (d.uptime !== undefined && diagUptime) {
            const h = Math.floor(d.uptime / 3600);
            const m = Math.floor((d.uptime % 3600) / 60);
            const s = d.uptime % 60;
            diagUptime.textContent = `${h}j ${m}m ${s}d`;
        }
        if (d.time && clockDisplay) clockDisplay.textContent = `WIB ${d.time}`;
        if (d.clients !== undefined && diagClients) diagClients.textContent = d.clients;
    }

    // Initialization Call
    drawColorWheel();
    renderEffectsList();
    renderGradientUI();
    initWebSocket();
});
