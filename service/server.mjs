import path from "node:path";
import { access } from "node:fs/promises";
import { spawn } from "node:child_process";
import { fileURLToPath } from "node:url";

import express from "express";
import sqlite3 from "sqlite3";
import { open } from "sqlite";

const serviceDirectory = path.dirname(fileURLToPath(import.meta.url));
const projectDirectory = path.resolve(serviceDirectory, "..");
const parsedPort = Number(process.env.PORT ?? 3000);
if (!Number.isInteger(parsedPort) || parsedPort < 1 || parsedPort > 65535) {
  throw new Error("PORT must be an integer from 1 to 65535");
}

const PORT = parsedPort;
const HOST = process.env.HOST || "127.0.0.1";
const DB_FILE = path.resolve(projectDirectory, process.env.DB_FILE || "blood_donor_finder.db");
const MATCHER_BINARY = path.resolve(
  projectDirectory,
  process.env.MATCHER_BINARY || (process.platform === "win32" ? "core/bdf_matcher_cli.exe" : "core/bdf_matcher_cli")
);

const BLOOD_GROUPS = Object.freeze(["O-", "O+", "A-", "A+", "B-", "B+", "AB-", "AB+"]);
const BLOOD_GROUP_CODE = new Map(BLOOD_GROUPS.map((group, index) => [group, index]));
const HOSPITALS = Object.freeze([
  { id: 1, name: "Bengaluru Pilot Hospital – Central", city: "Bengaluru", latitude: 12.9634, longitude: 77.5739 },
  { id: 2, name: "Bengaluru Pilot Hospital – South", city: "Bengaluru", latitude: 12.9299, longitude: 77.5908 },
  { id: 3, name: "Bengaluru Pilot Hospital – East", city: "Bengaluru", latitude: 12.9784, longitude: 77.6408 }
]);

const DEMO_DONORS = [
  ["D001", "Aisha Khan", "A+", 12.9716, 77.5946, 18, 1],
  ["D002", "Rohan Patel", "O-", 12.9352, 77.6245, 25, 1],
  ["D003", "Meera Singh", "B+", 12.9101, 77.6100, 30, 0],
  ["D004", "Kavita Rao", "AB+", 12.9500, 77.6200, 22, 1],
  ["D005", "Sanjay Kumar", "A-", 12.9600, 77.5900, 16, 1],
  ["D006", "Lina Liu", "O+", 12.9450, 77.6050, 21, 1],
  ["D007", "Arun Das", "B-", 12.9300, 77.5800, 24, 0],
  ["D008", "Priya Nair", "AB-", 12.9750, 77.6300, 26, 1],
  ["D009", "Vikram Shah", "A+", 12.9400, 77.6100, 23, 1],
  ["D010", "Neha Joshi", "O-", 12.9505, 77.5950, 17, 1],
  ["D011", "Rita Patel", "B+", 12.9250, 77.5850, 29, 1],
  ["D012", "Siddharth Verma", "AB+", 12.9605, 77.6105, 19, 0],
  ["D013", "Anita Ghosh", "A-", 12.9700, 77.6000, 15, 1],
  ["D014", "Manoj Kulkarni", "O+", 12.9355, 77.6175, 27, 1],
  ["D015", "Fatima Begum", "B-", 12.9455, 77.5955, 20, 1],
  ["D016", "Rahul Singh", "AB-", 12.9520, 77.6200, 21, 1],
  ["D017", "Shweta Desai", "A+", 12.9380, 77.5880, 28, 1],
  ["D018", "Kiran Rao", "O-", 12.9602, 77.6032, 18, 0],
  ["D019", "Anil Mehta", "B+", 12.9702, 77.6272, 24, 1],
  ["D020", "Jyoti Sharma", "AB+", 12.9285, 77.5795, 31, 1]
];

let database;

async function addColumnIfMissing(db, table, definition) {
  const column = definition.trim().split(/\s+/)[0];
  const columns = await db.all(`PRAGMA table_info(${table})`);
  if (!columns.some((item) => item.name === column)) {
    await db.exec(`ALTER TABLE ${table} ADD COLUMN ${definition}`);
  }
}

async function initialiseSchema(db) {
  await db.exec(`
    PRAGMA foreign_keys = ON;
    PRAGMA journal_mode = WAL;

    CREATE TABLE IF NOT EXISTS donor (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      ref TEXT UNIQUE NOT NULL,
      name TEXT NOT NULL,
      blood_group TEXT NOT NULL,
      city TEXT NOT NULL,
      latitude REAL NOT NULL,
      longitude REAL NOT NULL,
      last_donation DATE,
      available INTEGER NOT NULL DEFAULT 0 CHECK (available IN (0, 1))
    );

    CREATE TABLE IF NOT EXISTS hospital_request (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      hospital_name TEXT NOT NULL,
      blood_group TEXT NOT NULL,
      units_needed INTEGER NOT NULL,
      location_city TEXT NOT NULL,
      latitude REAL NOT NULL,
      longitude REAL NOT NULL,
      created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
      status TEXT DEFAULT 'pending' CHECK (status IN ('pending', 'matched', 'cancelled'))
    );

    CREATE TABLE IF NOT EXISTS notification (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      staff_id INTEGER NOT NULL,
      message TEXT NOT NULL,
      created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
      read INTEGER DEFAULT 0 CHECK (read IN (0, 1))
    );

    CREATE TABLE IF NOT EXISTS audit_event (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      event_type TEXT NOT NULL,
      details TEXT,
      ts DATETIME DEFAULT CURRENT_TIMESTAMP
    );

    CREATE TABLE IF NOT EXISTS daily_stats (
      date TEXT PRIMARY KEY,
      donors_registered INTEGER DEFAULT 0,
      requests_created INTEGER DEFAULT 0,
      matches_made INTEGER DEFAULT 0
    );
  `);

  await addColumnIfMissing(db, "donor", "consent_current INTEGER NOT NULL DEFAULT 1");
  await addColumnIfMissing(db, "donor", "blood_group_verified INTEGER NOT NULL DEFAULT 1");
  await addColumnIfMissing(db, "donor", "profile_active INTEGER NOT NULL DEFAULT 1");
  await addColumnIfMissing(db, "donor", "eta_minutes REAL NOT NULL DEFAULT 30");
  await addColumnIfMissing(db, "donor", "consent_checked_at INTEGER NOT NULL DEFAULT 0");
  await addColumnIfMissing(db, "donor", "availability_checked_at INTEGER NOT NULL DEFAULT 0");
  await addColumnIfMissing(db, "donor", "eta_checked_at INTEGER NOT NULL DEFAULT 0");

  const currentCount = await db.get("SELECT COUNT(*) AS count FROM donor");
  const now = Math.floor(Date.now() / 1000);
  if (currentCount.count === 0) {
    const statement = await db.prepare(`
      INSERT INTO donor (
        ref, name, blood_group, city, latitude, longitude, last_donation, available,
        consent_current, blood_group_verified, profile_active, eta_minutes,
        consent_checked_at, availability_checked_at, eta_checked_at
      ) VALUES (?, ?, ?, 'Bengaluru', ?, ?, NULL, ?, 1, 1, 1, ?, ?, ?, ?)
    `);
    try {
      for (const [ref, name, group, latitude, longitude, etaMinutes, available] of DEMO_DONORS) {
        await statement.run(ref, name, group, latitude, longitude, available, etaMinutes, now, now, now);
      }
    } finally {
      await statement.finalize();
    }
  }

  await db.run(
    `UPDATE donor
       SET consent_checked_at = CASE WHEN consent_checked_at <= 0 THEN ? ELSE consent_checked_at END,
           availability_checked_at = CASE WHEN availability_checked_at <= 0 THEN ? ELSE availability_checked_at END,
           eta_checked_at = CASE WHEN eta_checked_at <= 0 THEN ? ELSE eta_checked_at END`,
    now,
    now,
    now
  );
}

async function getDatabase() {
  if (!database) {
    database = await open({ filename: DB_FILE, driver: sqlite3.Database });
    await initialiseSchema(database);
  }
  return database;
}

function asyncRoute(handler) {
  return (request, response, next) => Promise.resolve(handler(request, response, next)).catch(next);
}

function validDonorRef(ref) {
  return typeof ref === "string" && /^D\d{3}$/.test(ref);
}

function parsePositiveId(value) {
  return /^\d+$/.test(String(value)) && Number(value) > 0 ? Number(value) : null;
}

function publicDonor(row) {
  return { ref: row.ref, name: row.name, blood_group: row.blood_group, city: row.city, available: row.available };
}

function hospitalFromBody(body) {
  const id = Number(body?.hospital_id);
  if (Number.isInteger(id)) return HOSPITALS.find((hospital) => hospital.id === id);
  if (typeof body?.hospital_name === "string") {
    return HOSPITALS.find((hospital) => hospital.name === body.hospital_name);
  }
  return undefined;
}

function numericDonorReference(ref) {
  return Number(ref.slice(1));
}

function parseMatcherOutput(stdout, donorRefs) {
  const trimmed = stdout.trim();
  if (!trimmed) return [];
  return trimmed.split(/\r?\n/).map((line) => {
    const fields = line.split("\t");
    if (fields.length !== 4) throw new Error("The C matcher returned a malformed row");
    const [numericRef, distanceKm, etaMinutes, score] = fields;
    const ref = donorRefs.get(numericRef);
    const parsed = { donorRef: ref, distanceKm: Number(distanceKm), etaMinutes: Number(etaMinutes), score: Number(score) };
    if (!ref || !Number.isFinite(parsed.distanceKm) || !Number.isFinite(parsed.etaMinutes) || !Number.isFinite(parsed.score)) {
      throw new Error("The C matcher returned invalid data");
    }
    return parsed;
  });
}

async function runMatcher(requestRecord, donors) {
  await access(MATCHER_BINARY);
  const now = Math.floor(Date.now() / 1000);
  const donorRefs = new Map();
  const lines = [[
    BLOOD_GROUP_CODE.get(requestRecord.blood_group),
    requestRecord.latitude,
    requestRecord.longitude,
    75,
    90,
    now,
    90 * 24 * 60 * 60,
    30 * 24 * 60 * 60,
    15 * 60,
    0.6
  ].join("\t")];

  for (const donor of donors) {
    const numericRef = numericDonorReference(donor.ref);
    donorRefs.set(String(numericRef), donor.ref);
    lines.push([
      numericRef,
      BLOOD_GROUP_CODE.get(donor.blood_group),
      donor.latitude,
      donor.longitude,
      donor.eta_minutes,
      donor.consent_current,
      donor.available,
      donor.blood_group_verified,
      donor.profile_active,
      donor.consent_checked_at,
      donor.availability_checked_at,
      // The demo has no routing provider, so its clearly labelled fixture ETA is refreshed here.
      now
    ].join("\t"));
  }

  return new Promise((resolve, reject) => {
    const child = spawn(MATCHER_BINARY, [], { windowsHide: true, stdio: ["pipe", "pipe", "pipe"] });
    let stdout = "";
    let stderr = "";
    let settled = false;
    const timeout = setTimeout(() => {
      child.kill();
      if (!settled) {
        settled = true;
        reject(new Error("The C matcher timed out"));
      }
    }, 5000);

    child.stdout.on("data", (chunk) => {
      stdout += chunk.toString();
      if (stdout.length > 1_000_000) child.kill();
    });
    child.stderr.on("data", (chunk) => {
      stderr += chunk.toString();
      if (stderr.length > 32_000) child.kill();
    });
    child.on("error", (error) => {
      clearTimeout(timeout);
      if (!settled) {
        settled = true;
        reject(error);
      }
    });
    child.on("close", (code) => {
      clearTimeout(timeout);
      if (settled) return;
      settled = true;
      if (code !== 0) {
        reject(new Error(`The C matcher failed${stderr.trim() ? `: ${stderr.trim()}` : ""}`));
        return;
      }
      try {
        resolve(parseMatcherOutput(stdout, donorRefs));
      } catch (error) {
        reject(error);
      }
    });
    child.stdin.end(`${lines.join("\n")}\n`);
  });
}

const app = express();
app.disable("x-powered-by");
app.use(express.json({ limit: "32kb", strict: true }));
app.use((request, response, next) => {
  response.setHeader("X-Content-Type-Options", "nosniff");
  response.setHeader("X-Frame-Options", "DENY");
  response.setHeader("Referrer-Policy", "no-referrer");
  response.setHeader("Permissions-Policy", "geolocation=(), camera=(), microphone=()");
  response.setHeader(
    "Content-Security-Policy",
    "default-src 'self'; img-src 'self' data:; style-src 'self' 'unsafe-inline'; script-src 'self' 'unsafe-inline'; connect-src 'self' http://127.0.0.1:* http://localhost:*"
  );
  next();
});

const requestBuckets = new Map();
app.use("/api", (request, response, next) => {
  const now = Date.now();
  const key = request.ip;
  const prior = requestBuckets.get(key);
  const bucket = !prior || now - prior.startedAt >= 60_000 ? { startedAt: now, count: 0 } : prior;
  bucket.count += 1;
  requestBuckets.set(key, bucket);
  if (bucket.count > 120) {
    response.status(429).json({ error: "Too many requests. Please wait and try again." });
    return;
  }
  next();
});

app.get("/", (request, response) => response.sendFile(path.join(serviceDirectory, "index.html")));
app.get("/api/health", (request, response) => {
  response.json({ status: "ok", database: "sqlite", matcher: "c", syntheticData: true });
});
app.get("/api/blood-groups", (request, response) => response.json(BLOOD_GROUPS));
app.get("/api/hospitals", (request, response) => response.json(HOSPITALS));

app.get("/api/donors", asyncRoute(async (request, response) => {
  const db = await getDatabase();
  const rows = await db.all("SELECT ref, name, blood_group, city, available FROM donor ORDER BY ref");
  response.json(rows.map(publicDonor));
}));

app.get("/api/donors/:ref", asyncRoute(async (request, response) => {
  if (!validDonorRef(request.params.ref)) {
    response.status(400).json({ error: "Invalid donor reference." });
    return;
  }
  const db = await getDatabase();
  const row = await db.get("SELECT ref, name, blood_group, city, available FROM donor WHERE ref = ?", request.params.ref);
  if (!row) {
    response.status(404).json({ error: "Donor not found." });
    return;
  }
  response.json(publicDonor(row));
}));

app.put("/api/donors/:ref/availability", asyncRoute(async (request, response) => {
  if (!validDonorRef(request.params.ref)) {
    response.status(400).json({ error: "Invalid donor reference." });
    return;
  }
  if (request.body?.available !== 0 && request.body?.available !== 1) {
    response.status(400).json({ error: "Availability must be 0 or 1." });
    return;
  }
  const db = await getDatabase();
  const now = Math.floor(Date.now() / 1000);
  const result = await db.run(
    "UPDATE donor SET available = ?, availability_checked_at = ? WHERE ref = ?",
    request.body.available,
    now,
    request.params.ref
  );
  if (result.changes === 0) {
    response.status(404).json({ error: "Donor not found." });
    return;
  }
  await db.run(
    "INSERT INTO audit_event (event_type, details) VALUES ('availability_updated', ?)",
    JSON.stringify({ donorRef: request.params.ref, available: request.body.available })
  );
  response.json({ donorRef: request.params.ref, available: request.body.available });
}));

app.get("/api/requests", asyncRoute(async (request, response) => {
  const db = await getDatabase();
  const rows = await db.all(
    `SELECT id, hospital_name, blood_group, units_needed, location_city, created_at, status
       FROM hospital_request ORDER BY id DESC LIMIT 100`
  );
  response.json(rows);
}));

app.post("/api/requests", asyncRoute(async (request, response) => {
  const hospital = hospitalFromBody(request.body);
  const bloodGroup = typeof request.body?.blood_group === "string" ? request.body.blood_group.toUpperCase() : "";
  const units = Number(request.body?.units_needed);
  if (!hospital) {
    response.status(400).json({ error: "Select a configured hospital." });
    return;
  }
  if (!BLOOD_GROUP_CODE.has(bloodGroup)) {
    response.status(400).json({ error: "Select a valid blood group." });
    return;
  }
  if (!Number.isInteger(units) || units < 1 || units > 100) {
    response.status(400).json({ error: "Units requested must be a whole number from 1 to 100." });
    return;
  }

  const db = await getDatabase();
  const result = await db.run(
    `INSERT INTO hospital_request
      (hospital_name, blood_group, units_needed, location_city, latitude, longitude)
     VALUES (?, ?, ?, ?, ?, ?)`,
    hospital.name,
    bloodGroup,
    units,
    hospital.city,
    hospital.latitude,
    hospital.longitude
  );
  await db.run(
    "INSERT INTO audit_event (event_type, details) VALUES ('request_created', ?)",
    JSON.stringify({ requestId: result.lastID, hospitalId: hospital.id, bloodGroup, units })
  );
  await db.run(`
    INSERT INTO daily_stats (date, requests_created) VALUES (date('now'), 1)
    ON CONFLICT(date) DO UPDATE SET requests_created = requests_created + 1
  `);
  response.status(201).json({ requestId: result.lastID, status: "pending" });
}));

app.get("/api/match/:requestId", asyncRoute(async (request, response) => {
  const requestId = parsePositiveId(request.params.requestId);
  if (!requestId) {
    response.status(400).json({ error: "Invalid request reference." });
    return;
  }
  const db = await getDatabase();
  const requestRecord = await db.get("SELECT * FROM hospital_request WHERE id = ?", requestId);
  if (!requestRecord) {
    response.status(404).json({ error: "Request not found." });
    return;
  }
  if (requestRecord.status === "cancelled") {
    response.status(409).json({ error: "A cancelled request cannot be matched." });
    return;
  }

  const donors = await db.all(`
    SELECT ref, blood_group, latitude, longitude, eta_minutes, consent_current, available,
           blood_group_verified, profile_active, consent_checked_at, availability_checked_at,
           eta_checked_at
      FROM donor
  `);
  const matches = await runMatcher(requestRecord, donors);
  const limit = Math.min(matches.length, Math.max(requestRecord.units_needed * 3, 5));
  const selected = matches.slice(0, limit);
  const changedToMatched = selected.length > 0 && requestRecord.status !== "matched";
  if (selected.length > 0) await db.run("UPDATE hospital_request SET status = 'matched' WHERE id = ?", requestId);
  if (changedToMatched) {
    await db.run(`
      INSERT INTO daily_stats (date, matches_made) VALUES (date('now'), 1)
      ON CONFLICT(date) DO UPDATE SET matches_made = matches_made + 1
    `);
  }
  await db.run(
    "INSERT INTO audit_event (event_type, details) VALUES ('match_performed', ?)",
    JSON.stringify({ requestId, candidateRefs: selected.map((match) => match.donorRef) })
  );
  response.json({
    requestId,
    matches: selected,
    matcherEngine: "c-kd-tree-bounded-heap",
    status: selected.length > 0 ? "matched" : requestRecord.status,
    notice: "Candidate ranking only. Synthetic travel times are used; clinical eligibility and crossmatch are required."
  });
}));

app.get("/api/stats", asyncRoute(async (request, response) => {
  const db = await getDatabase();
  const rows = await db.all("SELECT * FROM daily_stats ORDER BY date DESC LIMIT 30");
  response.json(rows);
}));

app.get("/api/staff/notifications/:staffId", asyncRoute(async (request, response) => {
  const staffId = parsePositiveId(request.params.staffId);
  if (!staffId) {
    response.status(400).json({ error: "Invalid staff reference." });
    return;
  }
  const db = await getDatabase();
  const rows = await db.all(
    "SELECT id, message, created_at, read FROM notification WHERE staff_id = ? ORDER BY id DESC LIMIT 20",
    staffId
  );
  response.json(rows);
}));

app.use("/api", (request, response) => response.status(404).json({ error: "API route not found." }));
app.use((error, request, response, next) => {
  if (response.headersSent) return next(error);
  if (error?.type === "entity.parse.failed") {
    response.status(400).json({ error: "Request body must contain valid JSON." });
    return;
  }
  const matcherUnavailable = error?.code === "ENOENT" && error?.path === MATCHER_BINARY;
  if (matcherUnavailable) {
    response.status(503).json({ error: "The C matching engine is not built. Run npm run build:c, then try again." });
    return;
  }
  console.error(error);
  response.status(500).json({ error: "The service could not complete the request." });
});

await getDatabase();
const server = app.listen(PORT, HOST, () => {
  console.log(`Blood Donor Finder listening on http://${HOST}:${PORT}`);
  console.log("Synthetic demonstration data only — do not enter real donor or patient information.");
});

async function shutdown(signal) {
  console.log(`\n${signal} received; closing the local service.`);
  server.close(async () => {
    if (database) await database.close();
    process.exit(0);
  });
}

process.on("SIGINT", () => shutdown("SIGINT"));
process.on("SIGTERM", () => shutdown("SIGTERM"));
