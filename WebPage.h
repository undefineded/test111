
#ifndef WEB_PAGE_H
#define WEB_PAGE_H

const char* htmlPage = R"rawliteral(
<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
    <title>Pixel Fan Control</title>
    <style>

        :root {
            --bg-main: #6B30E5;
            --bg-checker: #5B27CD;
            --card-bg: #FFFFFF;
            --primary: #75F94C;
            --border: #000000;
            --text-main: #000000;
            --text-sub: #666666;
        }

        body {
            background-color: var(--bg-main);
            background-image: 
                linear-gradient(45deg, var(--bg-checker) 25%, transparent 25%, transparent 75%, var(--bg-checker) 75%, var(--bg-checker)),
                linear-gradient(45deg, var(--bg-checker) 25%, transparent 25%, transparent 75%, var(--bg-checker) 75%, var(--bg-checker));
            background-size: 40px 40px;
            background-position: 0 0, 20px 20px;
            margin: 0;
            padding: 20px 15px;
            color: var(--text-main);
            user-select: none;
            -webkit-tap-highlight-color: transparent;
            font-weight: bold;
        }

        .header {
            text-align: center;
            font-size: 22px;
            color: #fff;
            text-shadow: 4px 4px 0 var(--border), -2px -2px 0 var(--border), 2px -2px 0 var(--border), -2px 2px 0 var(--border), 2px 2px 0 var(--border);
            margin-bottom: 25px;
            letter-spacing: 2px;
            line-height: 1.5;
        }

        .card {
            background: var(--card-bg);
            border: 4px solid var(--border);
            border-radius: 12px;
            box-shadow: 6px 6px 0 var(--border);
            padding: 20px 15px;
            margin-bottom: 25px;
            position: relative;
        }

        .card-title {
            font-size: 14px;
            border-bottom: 4px solid var(--border);
            padding-bottom: 12px;
            margin-bottom: 15px;
            text-transform: uppercase;
            text-align: center;
        }

        /* 实时数据网格 */
        .status-grid { display: flex; justify-content: space-between; text-align: center; }
        .status-item { flex: 1; position: relative; }
        .status-item:not(:last-child)::after {
            content: "";
            position: absolute;
            right: 0;
            top: 10%;
            height: 80%;
            width: 4px;
            background: var(--border);
        }
        .status-val { font-size: 16px; margin-bottom: 8px; color: var(--text-main); }
        .status-unit { font-size: 10px; }
        .status-label { font-size: 10px; color: var(--text-sub); }

        /* 模式选择 */
        select {
            width: 100%;
            padding: 15px;
            background: var(--primary);
            border: 4px solid var(--border);
            border-radius: 8px;
            box-shadow: 4px 4px 0 var(--border);
            font-size: 12px;
            font-family: inherit;
            color: var(--text-main);
            appearance: none;
            outline: none;
            text-align: center;
            cursor: pointer;
        }

        /* 调速滑动条 */
        .slider-wrapper { display: flex; align-items: center; justify-content: space-between; margin-top: 10px; }
        .slider-val { width: 60px; text-align: right; font-size: 14px; }
        input[type=range] {
            -webkit-appearance: none;
            width: calc(100% - 70px);
            height: 20px;
            background: #e0e0e0;
            border: 4px solid var(--border);
            border-radius: 10px;
            outline: none;
            box-shadow: inset 0 4px 0 rgba(0,0,0,0.1);
        }
        input[type=range]::-webkit-slider-thumb {
            -webkit-appearance: none;
            width: 28px;
            height: 28px;
            background: var(--primary);
            border: 4px solid var(--border);
            border-radius: 50%;
            cursor: pointer;
            box-shadow: 2px 2px 0 var(--border);
        }

        .footer {
            text-align: center;
            font-size: 10px;
            color: #fff;
            margin-top: 40px;
            text-shadow: 2px 2px 0 var(--border);
            text-transform: uppercase;
        }
    </style>
</head>
<body>
    <div class="header">FAN<br>CONTROL</div>
    
    <!-- 实时数据卡片 -->
    <div class="card">
        <div class="card-title">STATUS (INA226)</div>
        <div class="status-grid">
            <div class="status-item">
                <div class="status-val" id="val-v">--<span class="status-unit">V</span></div>
                <div class="status-label">VOLT</div>
            </div>
            <div class="status-item">
                <div class="status-val" id="val-i">--<span class="status-unit">A</span></div>
                <div class="status-label">AMP</div>
            </div>
            <div class="status-item">
                <div class="status-val" id="val-p">--<span class="status-unit">W</span></div>
                <div class="status-label">PWR</div>
            </div>
        </div>
    </div>

    <!-- 模式卡片 -->
    <div class="card">
        <div class="card-title">MODE</div>
        <select id="mode" onchange="sendConfig()">
            <option value="0">ESC 50Hz</option>
            <option value="1">FAN 25kHz</option>
        </select>
    </div>

    <!-- 调速卡片 -->
    <div class="card">
        <div class="card-title">PWM PROGRESS</div>
        <div class="slider-wrapper">
            <input type="range" id="progress" min="0" max="100" value="0" oninput="updateUI()" onchange="sendConfig()">
            <div class="slider-val" id="progVal">0%</div>
        </div>
    </div>

    <div class="footer">PIXEL ART STYLE</div>

    <script>
        let isDragging = false;
        let slider = document.getElementById('progress');

        slider.addEventListener('touchstart', () => isDragging = true, {passive: true});
        slider.addEventListener('touchend', () => isDragging = false);
        slider.addEventListener('mousedown', () => isDragging = true);
        slider.addEventListener('mouseup', () => isDragging = false);

        function updateUI() {
            let val = slider.value;
            document.getElementById('progVal').innerText = val + '%';
            slider.style.background = `linear-gradient(to right, var(--primary) ${val}%, #e0e0e0 ${val}%)`;
        }

        function fetchStatus() {
            fetch('/status').then(res => res.json()).then(data => {
                document.getElementById('val-v').innerHTML = data.v.toFixed(2) + '<span class="status-unit">V</span>';
                document.getElementById('val-i').innerHTML = data.i.toFixed(2) + '<span class="status-unit">A</span>';
                document.getElementById('val-p').innerHTML = (data.v * data.i).toFixed(2) + '<span class="status-unit">W</span>';
                
                if (!isDragging && document.activeElement !== slider) {
                    document.getElementById('mode').value = data.mode;
                    slider.value = data.progress;
                    updateUI();
                }
            }).catch(e => console.log("Fetch Err", e));
        }

        function sendConfig() {
            let m = document.getElementById('mode').value;
            let p = slider.value;
            fetch(`/set?mode=${m}&progress=${p}`);
        }

        window.onload = () => {
            updateUI();
            fetchStatus();
            setInterval(fetchStatus, 1000);
        };
    </script>
</body>
</html>
)rawliteral";

#endif // WEB_PAGE_H