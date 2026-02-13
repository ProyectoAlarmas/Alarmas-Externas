
/*************************************************
 * BACKEND ALARMAS – POSTGRESQL (SUPABASE)
 *************************************************/

const express = require('express');
const cors = require('cors');
const mqtt = require('mqtt');
const path = require('path');
const { Pool } = require('pg');

/* ================================
   CONFIGURACIÓN POSTGRESQL
================================ */
const pool = new Pool({
  connectionString: process.env.DATABASE_URL,
  ssl: { rejectUnauthorized: false },
  family: 4
});


pool
  .query('SELECT 1')
  .then(() => console.log('🗄️ Conectado a PostgreSQL (Supabase)'))
  .catch(err => console.error('❌ Error PostgreSQL', err));

/* ================================
   APP EXPRESS
================================ */
const app = express();
app.use(cors());
app.use(express.static(path.join(__dirname, 'public')));

const PORT = process.env.PORT || 3000;

/* ================================
   SSE
================================ */
let sseClients = [];

function emitEvent(data) {
  const payload = `data: ${JSON.stringify(data)}\n\n`;
  sseClients.forEach(c => c.write(payload));
}

/* ================================
   ALARMAS
================================ */

const alarmNames = [
  "FALLA ALIM COM","GPO ELEC OPER","FALLA FUS DIST",
  "FALLA RECT","ALTA TEMP","FALLA FUS VOLTA",
  "BAT EN OPER","BAJO VOLTAJE",
  "LIBRE 1","LIBRE 2","LIBRE 3","LIBRE 4",
  "LIBRE 5","LIBRE 6","LIBRE 7","LIBRE 8"
];

let lastStateOTY = 0;
let currentValueOTY = 0;

/* ================================
   MQTT
================================ */
const mqttUrl = 'wss://d128f885.ala.us-east-1.emqxsl.com:8084/mqtt';

const mqttOptions = {
  clientId: 'backend_' + Math.random().toString(16).substr(2, 8),
  username: 'Hola',
  password: '12345'
};

const client = mqtt.connect(mqttUrl, mqttOptions);

client.on('connect', () => {
  console.log('✅ Conectado a MQTT');
  client.subscribe('esp32/External_Alarms_2');
});

client.on('message', async (topic, message) => {
  const value = (message[0] << 8) | message[1];
  currentValueOTY = value;
  const now = new Date();

  console.log('📥 Estado recibido:', value);

  for (let i = 15; i >= 0; i--) {
    const bit = (value >> i) & 1;
    const prev = (lastStateOTY >> i) & 1;

    if (bit !== prev) {
      const event = bit ? 'ACTIVADA' : 'DESACTIVADA';
	const alarmIndex = 15 - i; // 0 a 15

      const alarmName = alarmNames[15 - i];

      console.log(`⚠️ ${alarmName} ${event} @ ${now.toLocaleString('es-MX')}`);

      // 👉 GUARDAR EN POSTGRESQL
      try {
        await pool.query(
  `INSERT INTO alarm_events (station, alarm, alarm_name, state)
   VALUES ($1, $2, $3, $4)`,
  ['OTY', alarmIndex, alarmName, event]
);

      } catch (err) {
        console.error('❌ Error INSERT PostgreSQL', err);
      }

      // 🔴 SSE → HISTORIAL
      emitEvent({
        type: 'history',
        station: 'OTY',
        alarm: alarmName,
        state: event,
        timestamp: now.toISOString()
      });
    }
  }

  lastStateOTY = value;

  // 🔵 SSE → ESTADO COMPLETO
  emitEvent({
    type: 'state',
    station: 'OTY',
    value
  });
});

/* ================================
   API HISTORIAL
================================ */
app.get('/api/history/:station', async (req, res) => {
  const station = req.params.station;

  try {
    const result = await pool.query(
      `SELECT station, alarm_name, state, timestamp
       FROM alarm_events
       WHERE station = $1
       ORDER BY timestamp DESC
       LIMIT 200`,
      [station]
    );

    res.json(result.rows);
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
});

app.delete('/api/history/:station', async (req, res) => {
  const station = req.params.station;

  try {
    const result = await pool.query(
      `DELETE FROM alarm_events WHERE station = $1`,
      [station]
    );

    res.json({
      ok: true,
      deleted: result.rowCount
    });
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
});

/* ================================
   SSE ENDPOINT
================================ */
app.get('/api/events', (req, res) => {
  res.setHeader('Content-Type', 'text/event-stream');
  res.setHeader('Cache-Control', 'no-cache');
  res.setHeader('Connection', 'keep-alive');
  res.flushHeaders();

  // Estado inicial
  res.write(`data: ${JSON.stringify({
    type: 'state',
    station: 'OTY',
    value: currentValueOTY,
    initial: true
  })}\n\n`);

  sseClients.push(res);
  console.log('🌐 Cliente SSE conectado');

  req.on('close', () => {
    sseClients = sseClients.filter(c => c !== res);
    console.log('❌ Cliente SSE desconectado');
  });
});

/* ================================
   START SERVER
================================ */
app.listen(PORT, () => {
  console.log(`🚀 Servidor corriendo en puerto ${PORT}`);
});

