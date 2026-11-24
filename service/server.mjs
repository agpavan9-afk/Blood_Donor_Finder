// server.mjs – Blood Donor Finder backend (Node.js)
// ------------------------------------------------------
// This server implements a lightweight SQLite‑based API for the Blood Donor Finder
// prototype.  All data is synthetic and stored locally; no external network access
// is required.  It includes the expanded schema, demo donor rows, rate‑limiting,
// request validation, CORS handling, and a simple environment configuration.

import fs from "fs";
import path from "path";
import { fileURLToPath } from "url";
import sqlite3 from "sqlite3";
import { open } from "sqlite";
import express from "express";
import dotenv from "dotenv";

const __dirname = path.dirname(fileURLToPath(import.meta.url));

// ---------------------------------------------------------------------
// Environment
// ---------------------------------------------------------------------
dotenv.config({ path: path.resolve(__dirname, ".env.example") });
const PORT = process.env.PORT || 3000;
const DB_FILE = process.env.DB_FILE || "blood_donor_finder.db";
const MATCHER_BINARY = process.env.MATCHER_BINARY || "../blood_donor_finder.exe";

// ---------------------------------------------------------------------
// Helper: async DB connection (singleton)
// ---------------------------------------------------------------------
let db;
async function getDb() {
  if (db) return db;
  db = await open({ filename: DB_FILE, driver: sqlite3.Database });
  await initialiseSchema();
  return db;
}

// ---------------------------------------------------------------------
// Initialise / migrate the SQLite schema
// ---------------------------------------------------------------------
async function initialiseSchema() {
  // Donor information
  await db.exec(`
    CREATE TABLE IF NOT EXISTS donor (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      ref TEXT UNIQUE NOT NULL,
      name TEXT NOT NULL,
      blood_group TEXT NOT NULL,
      city TEXT NOT NULL,
      latitude REAL NOT NULL,
      longitude REAL NOT NULL,
      last_donation DATE,
      available INTEGER NOT NULL CHECK (available IN (0,1))
    );
  `);

  // Hospital request queue
  await db.exec(`
    CREATE TABLE IF NOT EXISTS hospital_request (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      hospital_name TEXT NOT NULL,
      blood_group TEXT NOT NULL,
      units_needed INTEGER NOT NULL,
      location_city TEXT NOT NULL,
      latitude REAL NOT NULL,
      longitude REAL NOT NULL,
      created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
      status TEXT DEFAULT 'pending' CHECK (status IN ('pending','matched','cancelled'))
    );
  `);

  // Notifications for staff
  await db.exec(`
    CREATE TABLE IF NOT EXISTS notification (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      staff_id INTEGER NOT NULL,
      message TEXT NOT NULL,
      created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
      read INTEGER DEFAULT 0 CHECK (read IN (0,1))
    );
  `);

  // Audit events (for compliance / debugging)
  await db.exec(`
    CREATE TABLE IF NOT EXISTS audit_event (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      event_type TEXT NOT NULL,
      details TEXT,
      ts DATETIME DEFAULT CURRENT_TIMESTAMP
    );
  `);

  // Blood inventory per centre (synthetic example)
  await db.exec(`
    CREATE TABLE IF NOT EXISTS blood_inventory (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      centre_name TEXT NOT NULL,
      blood_group TEXT NOT NULL,
      units INTEGER NOT NULL,
      last_updated DATETIME DEFAULT CURRENT_TIMESTAMP
    );
  `);

  // Daily statistics – used by /api/stats endpoint
  await db.exec(`
    CREATE TABLE IF NOT EXISTS daily_stats (
      date TEXT PRIMARY KEY,
      donors_registered INTEGER DEFAULT 0,
      requests_created INTEGER DEFAULT 0,
      matches_made INTEGER DEFAULT 0
    );
  `);

  // Populate demo donor data (only once)
  const existing = await db.get("SELECT COUNT(*) as cnt FROM donor");
  if (existing.cnt === 0) {
    const demoDonors = [
      // ref, name, blood_group, city, lat, lng, last_donation, available
      ["D001", "Aisha Khan", "A+", "Bengaluru", 12.9716, 77.5946, "2023-07-12", 1],
      ["D002", "Rohan Patel", "O-", "Bengaluru", 12.9352, 77.6245, "2023-05-20", 1],
      ["D003", "Meera Singh", "B+", "Bengaluru", 12.9101, 77.6100, "2023-04-30", 0],
      ["D004", "Kavita Rao", "AB+", "Bengaluru", 12.9500, 77.6200, "2023-06-15", 1],
      ["D005", "Sanjay Kumar", "A-", "Bengaluru", 12.9600, 77.5900, "2023-08-01", 1],
      ["D006", "Lina Liu", "O+", "Bengaluru", 12.9450, 77.6050, "2023-03-22", 1],
      ["D007", "Arun Das", "B-", "Bengaluru", 12.9300, 77.5800, "2023-02-10", 0],
      ["D008", "Priya Nair", "AB-", "Bengaluru", 12.9750, 77.6300, "2023-07-05", 1],
      ["D009", "Vikram Shah", "A+", "Bengaluru", 12.9400, 77.6100, "2023-01-25", 1],
      ["D010", "Neha Joshi", "O-", "Bengaluru", 12.9505, 77.5950, "2023-09-12", 1],
      ["D011", "Rita Patel", "B+", "Bengaluru", 12.9250, 77.5850, "2023-04-01", 1],
      ["D012", "Siddharth Verma", "AB+", "Bengaluru", 12.9605, 77.6105, "2023-06-20", 0],
      ["D013", "Anita Ghosh", "A-", "Bengaluru", 12.9700, 77.6000, "2023-08-15", 1],
      ["D014", "Manoj Kulkarni", "O+", "Bengaluru", 12.9355, 77.6175, "2023-02-28", 1],
      ["D015", "Fatima Begum", "B-", "Bengaluru", 12.9455, 77.5955, "2023-03-10", 1],
      ["D016", "Rahul Singh", "AB-", "Bengaluru", 12.9520, 77.6200, "2023-07-22", 1],
      ["D017", "Shweta Desai", "A+", "Bengaluru", 12.9380, 77.5880, "2023-05-30", 1],
      ["D018", "Kiran Rao", "O-", "Bengaluru", 12.9602, 77.6032, "2023-06-05", 0],
      ["D019", "Anil Mehta", "B+", "Bengaluru", 12.9702, 77.6272, "2023-04-18", 1],
      ["D020", "Jyoti Sharma", "AB+", "Bengaluru", 12.9285, 77.5795, "2023-09-01", 1]
    ];
    const stmt = await db.prepare(`INSERT INTO donor (ref, name, blood_group, city, latitude, longitude, last_donation, available) VALUES (?,?,?,?,?,?,?,?)`);
    for (const d of demoDonors) {
      await stmt.run(...d);
    }
    await stmt.finalize();
    console.log(`Inserted ${demoDonors.length} demo donors`);
  }
}

// ---------------------------------------------------------------------
// Rate‑limiting middleware (token‑bucket, per‑IP)
// ---------------------------------------------------------------------
const rateLimitWindowMs = 60 * 1000; // 1 minute
const maxRequestsPerWindow = 60; // generous for demo
const ipBuckets = new Map();
function rateLimiter(req, res, next) {
  const now = Date.now();
  const ip = req.ip;
  let bucket = ipBuckets.get(ip);
  if (!bucket) {
    bucket = { tokens: maxRequestsPerWindow, last: now };
    ipBuckets.set(ip, bucket);
  }
  const elapsed = now - bucket.last;
  const refill = Math.floor(elapsed / rateLimitWindowMs) * maxRequestsPerWindow;
  if (refill > 0) {
    bucket.tokens = Math.min(maxRequestsPerWindow, bucket.tokens + refill);
    bucket.last = now;
  }
  if (bucket.tokens > 0) {
    bucket.tokens--;
    next();
  } else {
    res.status(429).json({ error: "Too many requests – please slow down." });
  }
}

// ---------------------------------------------------------------------
// Request validation helpers (simple sanity checks)
// ---------------------------------------------------------------------
function validateDonorRef(ref) {
  return typeof ref === "string" && /^D\d{3}$/.test(ref);
}
function validateBloodGroup(bg) {
  return /^(A\+|A\-|B\+|B\-|AB\+|AB\-|O\+|O\-)$/i.test(bg);
}

// ---------------------------------------------------------------------
// Express app configuration
// ---------------------------------------------------------------------
const app = express();
app.use(express.json());
app.use(rateLimiter);
// CORS – allow any origin for demo purposes (in production restrict to UI domain)
app.use((req, res, next) => {
  res.setHeader("Access-Control-Allow-Origin", "*");
  res.setHeader("Access-Control-Allow-Methods", "GET,POST,PUT,DELETE,OPTIONS");
  res.setHeader("Access-Control-Allow-Headers", "Content-Type,Authorization");
  if (req.method === "OPTIONS") return res.sendStatus(204);
  next();
});

// Serve the local staff interface from the same origin as the API.
app.get("/", (req, res) => res.sendFile(path.join(__dirname, "index.html")));

// ---------------------------------------------------------------------
// API endpoints
// ---------------------------------------------------------------------
// Health check
app.get("/api/health", (req, res) => res.json({ status: "ok", uptime: process.uptime() }));

// -------------------------------------------------
// Stats – aggregated daily numbers
// -------------------------------------------------
app.get("/api/stats", async (req, res) => {
  const rows = await (await getDb()).all(`SELECT * FROM daily_stats ORDER BY date DESC LIMIT 30`);
  res.json(rows);
});

// -------------------------------------------------
// Hospital request queue
app.get("/api/requests", async (req, res) => {
  const rows = await (await getDb()).all(
    "SELECT id, hospital_name, blood_group, units_needed, location_city, created_at, status " +
    "FROM hospital_request ORDER BY created_at DESC, id DESC LIMIT 100"
  );
  res.json(rows);
});

// Donor list (public) – limited fields
// -------------------------------------------------
app.get("/api/donors", async (req, res) => {
  const rows = await (await getDb()).all(`SELECT ref, name, blood_group, city, latitude, longitude, available FROM donor`);
  res.json(rows);
});

// -------------------------------------------------
// Single donor detail
// -------------------------------------------------
app.get("/api/donors/:ref", async (req, res) => {
  const { ref } = req.params;
  if (!validateDonorRef(ref)) return res.status(400).json({ error: "Invalid donor reference" });
  const row = await (await getDb()).get(`SELECT * FROM donor WHERE ref = ?`, ref);
  if (!row) return res.status(404).json({ error: "Donor not found" });
  res.json(row);
});

// -------------------------------------------------
// Update donor availability (staff action)
// -------------------------------------------------
app.put("/api/donors/:ref/availability", async (req, res) => {
  const { ref } = req.params;
  const { available } = req.body;
  if (!validateDonorRef(ref)) return res.status(400).json({ error: "Invalid donor reference" });
  if (typeof available !== "number" || ![0, 1].includes(available))
    return res.status(400).json({ error: "Availability must be 0 or 1" });
  const dbh = await getDb();
  const result = await dbh.run(`UPDATE donor SET available = ? WHERE ref = ?`, available, ref);
  if (result.changes === 0) return res.status(404).json({ error: "Donor not found" });
  res.json({ message: "Availability updated" });
});

// -------------------------------------------------
// Blood‑group catalogue (static list)
// -------------------------------------------------
app.get("/api/blood-groups", (req, res) => {
  const groups = ["A+", "A-", "B+", "B-", "AB+", "AB-", "O+", "O-"];
  res.json(groups);
});

// -------------------------------------------------
// Hospital data – placeholder for demo (static array)
// -------------------------------------------------
app.get("/api/hospitals", (req, res) => {
  const hospitals = [
    { id: 1, name: "City Hospital", city: "Bengaluru", latitude: 12.9718, longitude: 77.5939 },
    { id: 2, name: "Green Valley Clinic", city: "Bengaluru", latitude: 12.9355, longitude: 77.6240 }
  ];
  res.json(hospitals);
});

// -------------------------------------------------
// Staff notifications (simple pull)
// -------------------------------------------------
app.get("/api/staff/notifications/:staffId", async (req, res) => {
  const { staffId } = req.params;
  const rows = await (await getDb()).all(`SELECT * FROM notification WHERE staff_id = ? ORDER BY created_at DESC LIMIT 20`, staffId);
  res.json(rows);
});

// -------------------------------------------------
// Create a new hospital request (queue entry)
// -------------------------------------------------
app.post("/api/requests", async (req, res) => {
  const { hospital_name, blood_group, units_needed, location_city, latitude, longitude } = req.body;
  if (!hospital_name || !validateBloodGroup(blood_group) || typeof units_needed !== "number" || !location_city || typeof latitude !== "number" || typeof longitude !== "number") {
    return res.status(400).json({ error: "Missing or invalid fields" });
  }
  const dbh = await getDb();
  const result = await dbh.run(
    `INSERT INTO hospital_request (hospital_name, blood_group, units_needed, location_city, latitude, longitude) VALUES (?,?,?,?,?,?)`,
    hospital_name, blood_group, units_needed, location_city, latitude, longitude
  );
  // Record audit event
  await dbh.run(`INSERT INTO audit_event (event_type, details) VALUES ('request_created', ?)`,
    JSON.stringify({ requestId: result.lastID, hospital_name, blood_group, units_needed })
  );
  res.status(201).json({ requestId: result.lastID, status: "pending" });
});

// -------------------------------------------------
// Run the C matching engine (wrapper)
// -------------------------------------------------
import { spawn } from "child_process";
app.get("/api/match/:requestId", async (req, res) => {
  const { requestId } = req.params;
  const dbh = await getDb();
  const request = await dbh.get(`SELECT * FROM hospital_request WHERE id = ?`, requestId);
  if (!request) return res.status(404).json({ error: "Request not found" });

  // Build command line for the C binary – radius 75km, include city in response
  const args = [
    "-r", request.blood_group,
    "-lat", request.latitude,
    "-lng", request.longitude,
    "-radius", "75"
  ];
  const proc = spawn(MATCHER_BINARY, args, { windowsHide: true });

  let stdout = "", stderr = "";
  proc.stdout.on("data", (data) => (stdout += data.toString()));
  proc.stderr.on("data", (data) => (stderr += data.toString()));
  proc.on("close", async (code) => {
    if (code !== 0) {
      console.error(`Matcher exited ${code}: ${stderr}`);
      return res.status(500).json({ error: "Matcher failed", details: stderr });
    }
    // Expected output: JSON array of donor refs with scores
    let matches;
    try {
      matches = JSON.parse(stdout);
    } catch (e) {
      return res.status(500).json({ error: "Invalid matcher output", raw: stdout });
    }
    // Update request status
    await dbh.run(`UPDATE hospital_request SET status = 'matched' WHERE id = ?`, requestId);
    // Log audit event
    await dbh.run(`INSERT INTO audit_event (event_type, details) VALUES ('match_performed', ?)`,
      JSON.stringify({ requestId, matches })
    );
    res.json({ requestId, matches, matcherEngine: "c_binary" });
  });
});

// -------------------------------------------------
// Fallback JS matcher (if binary missing)
// -------------------------------------------------
app.get("/api/match-js/:requestId", async (req, res) => {
  const { requestId } = req.params;
  const dbh = await getDb();
  const request = await dbh.get(`SELECT * FROM hospital_request WHERE id = ?`, requestId);
  if (!request) return res.status(404).json({ error: "Request not found" });

  // Simple JS proximity filter using Haversine (radius 75 km)
  function haversine(lat1, lon1, lat2, lon2) {
    const toRad = (deg) => (deg * Math.PI) / 180;
    const R = 6371; // km
    const dLat = toRad(lat2 - lat1);
    const dLon = toRad(lon2 - lon1);
    const a = Math.sin(dLat / 2) ** 2 + Math.cos(toRad(lat1)) * Math.cos(toRad(lat2)) * Math.sin(dLon / 2) ** 2;
    return 2 * R * Math.asin(Math.sqrt(a));
  }
  const donors = await dbh.all(`SELECT ref, blood_group, latitude, longitude, available FROM donor WHERE available = 1`);
  const matches = donors
    .filter((d) => d.blood_group === request.blood_group && haversine(request.latitude, request.longitude, d.latitude, d.longitude) <= 75)
    .map((d) => ({ ref: d.ref, distanceKm: haversine(request.latitude, request.longitude, d.latitude, d.longitude).toFixed(1) }));

  await dbh.run(`UPDATE hospital_request SET status = 'matched' WHERE id = ?`, requestId);
  await dbh.run(`INSERT INTO audit_event (event_type, details) VALUES ('match_js_performed', ?)`,
    JSON.stringify({ requestId, matches })
  );
  res.json({ requestId, matches, matcherEngine: "js_fallback" });
});

// -------------------------------------------------
// Start server
// -------------------------------------------------
app.listen(PORT, () => {
  console.log(`Blood Donor Finder backend listening on http://localhost:${PORT}`);
});
