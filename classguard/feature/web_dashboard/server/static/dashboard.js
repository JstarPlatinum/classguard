const state = {
  history: [],
  charts: {},
  latest: null,
  lastSeenAt: null,
};

const maxPoints = 120;
const offlineAfterMs = 10000;

const fields = {
  deviceId: document.getElementById("deviceId"),
  firmware: document.getElementById("firmware"),
  wifiRssi: document.getElementById("wifiRssi"),
  deviceIp: document.getElementById("deviceIp"),
  lastSeen: document.getElementById("lastSeen"),
  onlineState: document.getElementById("onlineState"),
  connectionText: document.getElementById("connectionText"),
  aqScore: document.getElementById("aqScore"),
  aqLevel: document.getElementById("aqLevel"),
  aqAction: document.getElementById("aqAction"),
  aqMessage: document.getElementById("aqMessage"),
  co2: document.getElementById("co2"),
  temperature: document.getElementById("temperature"),
  humidity: document.getElementById("humidity"),
  pm10small: document.getElementById("pm10small"),
  pm25: document.getElementById("pm25"),
  pm10: document.getElementById("pm10"),
  mlxMax: document.getElementById("mlxMax"),
  mlxAvg: document.getElementById("mlxAvg"),
  alerts: document.getElementById("alerts"),
  sampleCount: document.getElementById("sampleCount"),
};

function fmt(value, digits = 1) {
  if (value === null || value === undefined || Number.isNaN(Number(value))) {
    return "--";
  }
  return Number(value).toFixed(digits).replace(/\.0$/, "");
}

function fmtTime(value) {
  if (!value) return "--";
  const date = new Date(value);
  if (Number.isNaN(date.getTime())) return value;
  return date.toLocaleTimeString("zh-CN", { hour12: false });
}

function setText(el, value) {
  el.textContent = value ?? "--";
}

function actionLabel(action) {
  const labels = {
    keep_monitoring: "保持监测",
    ventilation: "加强通风",
    early_ventilation: "提前通风",
    filtered_ventilation: "过滤新风",
    purify_air: "空气净化",
    cooling: "降温",
    dehumidify: "除湿",
    improve_by_reason: "按异常处理",
    no_data: "等待数据",
  };
  return labels[action] || action || "--";
}

function relabelThermalCards() {
  const ratioCard = fields.mlxMax?.closest(".metric-card");
  const stateCard = fields.mlxAvg?.closest(".metric-card");
  if (ratioCard) {
    ratioCard.querySelector("span").textContent = "占有率";
    ratioCard.querySelector("small").textContent = "ratio";
  }
  if (stateCard) {
    stateCard.querySelector("span").textContent = "占用状态";
    stateCard.querySelector("small").textContent = "MLX90640";
  }
}

function updateStatus() {
  const pill = fields.onlineState;
  const latest = state.latest;
  pill.className = "status-pill";

  if (!latest || !state.lastSeenAt) {
    pill.textContent = "未连接";
    fields.connectionText.textContent = "等待数据接入";
    return;
  }

  const isFresh = Date.now() - state.lastSeenAt.getTime() <= offlineAfterMs;
  if (latest.sensor_ok === false) {
    pill.classList.add("error");
    pill.textContent = "传感器异常";
    fields.connectionText.textContent = latest.error_message || "设备上报传感器异常";
    return;
  }

  if (isFresh) {
    pill.classList.add("online");
    pill.textContent = "在线";
    fields.connectionText.textContent = "实时数据正常接入";
  } else {
    pill.classList.add("offline");
    pill.textContent = "离线";
    fields.connectionText.textContent = "超过 10 秒未收到新数据";
  }
}

function renderLatest(item) {
  if (!item) {
    updateStatus();
    renderAlerts();
    return;
  }

  state.latest = item;
  state.lastSeenAt = new Date(item.received_at);

  setText(fields.deviceId, item.device_id);
  setText(fields.firmware, item.firmware);
  setText(fields.wifiRssi, item.wifi_rssi === null ? "--" : `${item.wifi_rssi} dBm`);
  setText(fields.deviceIp, item.wifi_ip);
  setText(fields.lastSeen, fmtTime(item.received_at));
  setText(fields.aqScore, fmt(item.aq_score, 0));
  setText(fields.aqLevel, item.aq_level);
  setText(fields.aqAction, actionLabel(item.aq_action));
  setText(fields.aqMessage, item.aq_message);
  setText(fields.co2, fmt(item.co2_ppm, 0));
  const displayTemperature = item.sht35_temperature_c ?? item.temperature_c;
  const displayHumidity = item.sht35_humidity_percent ?? item.humidity_percent;
  setText(fields.temperature, fmt(displayTemperature, 1));
  setText(fields.humidity, fmt(displayHumidity, 1));
  setText(fields.pm10small, fmt(item.pm1_0, 0));
  setText(fields.pm25, fmt(item.pm2_5, 0));
  setText(fields.pm10, fmt(item.pm10, 0));
  setText(fields.mlxMax, fmt(item.mlx_occupancy_ratio, 3));
  setText(fields.mlxAvg, item.mlx_occupied === null || item.mlx_occupied === undefined ? "--" : item.mlx_occupied ? "有人" : "无人");

  updateStatus();
  renderAlerts();
}

function buildSeries(name, key, color) {
  return {
    name,
    type: "line",
    smooth: true,
    showSymbol: false,
    connectNulls: true,
    lineStyle: { width: 2, color },
    itemStyle: { color },
    data: state.history.map((item) => item[key]),
  };
}

function chartOption(series, yName) {
  const labels = state.history.map((item) => fmtTime(item.received_at));
  return {
    animation: false,
    grid: { top: 36, right: 18, bottom: 34, left: 48 },
    tooltip: { trigger: "axis" },
    legend: { top: 8, textStyle: { color: "#657282" } },
    xAxis: {
      type: "category",
      boundaryGap: false,
      data: labels,
      axisLabel: { color: "#657282" },
      axisLine: { lineStyle: { color: "#d8dee6" } },
    },
    yAxis: {
      type: "value",
      name: yName,
      nameTextStyle: { color: "#657282" },
      axisLabel: { color: "#657282" },
      splitLine: { lineStyle: { color: "#edf1f5" } },
    },
    series,
  };
}

function initCharts() {
  if (!window.echarts) {
    document.querySelectorAll(".chart").forEach((el) => {
      el.textContent = "ECharts 未加载";
      el.classList.add("chart-empty");
    });
    return;
  }

  state.charts.air = echarts.init(document.getElementById("airChart"));
  state.charts.env = echarts.init(document.getElementById("envChart"));
  state.charts.pm = echarts.init(document.getElementById("pmChart"));
  state.charts.thermal = echarts.init(document.getElementById("thermalChart"));
  window.addEventListener("resize", () => {
    Object.values(state.charts).forEach((chart) => chart.resize());
  });
}

function renderCharts() {
  if (!window.echarts || !state.charts.air) return;

  state.charts.air.setOption(
    chartOption([
      buildSeries("综合评分", "aq_score", "#138a5b"),
      buildSeries("CO2", "co2_ppm", "#2563eb"),
      buildSeries("PM2.5", "pm2_5", "#b7791f"),
    ], "score / ppm / ug/m3")
  );
  state.charts.env.setOption(
    chartOption([
      buildSeries("SHT35 温度", "sht35_temperature_c", "#c24135"),
      buildSeries("SHT35 湿度", "sht35_humidity_percent", "#138a5b"),
      buildSeries("SCD41 温度", "temperature_c", "#7c3aed"),
      buildSeries("SCD41 湿度", "humidity_percent", "#2563eb"),
    ], "deg C / %RH")
  );
  state.charts.pm.setOption(
    chartOption([
      buildSeries("PM1.0", "pm1_0", "#64748b"),
      buildSeries("PM2.5", "pm2_5", "#b7791f"),
      buildSeries("PM10", "pm10", "#7c3aed"),
    ], "ug/m3")
  );
  state.charts.thermal.setOption(
    chartOption([
      buildSeries("占有率", "mlx_occupancy_ratio", "#2563eb"),
      buildSeries("热强度", "mlx_occupancy_heat_score", "#138a5b"),
      buildSeries("综合分", "mlx_occupancy_score", "#c24135"),
    ], "score")
  );
}

function renderAlerts() {
  const item = state.latest;
  const alerts = [];

  if (!item) {
    alerts.push({ level: "warn", text: "暂无上传数据" });
  } else {
    if (Date.now() - state.lastSeenAt.getTime() > offlineAfterMs) {
      alerts.push({ level: "error", text: "设备离线：超过 10 秒未收到新数据" });
    }
    if (item.sensor_ok === false) {
      alerts.push({ level: "error", text: item.error_message || "传感器状态异常" });
    }
    if (Array.isArray(item.aq_redlines) && item.aq_redlines.length > 0) {
      item.aq_redlines.forEach((redline) => {
        alerts.push({ level: "error", text: `综合评价红线：${redline}` });
      });
    }
    if (item.aq_message) {
      alerts.push({ level: item.aq_score !== null && item.aq_score < 55 ? "warn" : "", text: item.aq_message });
    }
    if (!item.aq_message && Number(item.co2_ppm) > 1000) {
      alerts.push({ level: "warn", text: `CO2 偏高：${fmt(item.co2_ppm, 0)} ppm` });
    }
    if (!item.aq_message && Number(item.pm2_5) > 35) {
      alerts.push({ level: "warn", text: `PM2.5 偏高：${fmt(item.pm2_5, 0)} ug/m3` });
    }
    if (alerts.length === 0) {
      alerts.push({ level: "", text: "当前无报警" });
    }
  }

  fields.alerts.innerHTML = alerts
    .map((alert) => `<li class="${alert.level}">${alert.text}</li>`)
    .join("");
  fields.sampleCount.textContent = `${state.history.length} 条记录`;
}

function appendHistory(item) {
  state.history.push(item);
  if (state.history.length > maxPoints) {
    state.history = state.history.slice(-maxPoints);
  }
}

async function loadInitialData() {
  const [latestResp, historyResp] = await Promise.all([
    fetch("/api/latest"),
    fetch("/api/history?limit=120"),
  ]);
  const latestJson = await latestResp.json();
  const historyJson = await historyResp.json();
  state.history = historyJson.data || [];
  renderLatest(latestJson.data);
  renderCharts();
}

function connectEvents() {
  const source = new EventSource("/events");
  source.onmessage = (event) => {
    const item = JSON.parse(event.data);
    appendHistory(item);
    renderLatest(item);
    renderCharts();
  };
  source.onerror = () => {
    fields.connectionText.textContent = "实时连接中断，正在等待浏览器重连";
  };
}

async function boot() {
  relabelThermalCards();
  initCharts();
  try {
    await loadInitialData();
    connectEvents();
  } catch (error) {
    fields.connectionText.textContent = "看板初始化失败";
    fields.alerts.innerHTML = `<li class="error">${error.message}</li>`;
  }
  setInterval(() => {
    updateStatus();
    renderAlerts();
  }, 1000);
}

boot();
