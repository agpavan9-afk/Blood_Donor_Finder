import assert from "node:assert/strict";
import { spawn, spawnSync } from "node:child_process";
import { mkdtemp, readFile, rm } from "node:fs/promises";
import { createServer } from "node:net";
import os from "node:os";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { Script } from "node:vm";

const projectDirectory = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const executableSuffix = process.platform === "win32" ? ".exe" : "";
const matcherPath = path.join(projectDirectory, "core", `bdf_matcher_cli${executableSuffix}`);
const demoPath = path.join(projectDirectory, `blood_donor_finder${executableSuffix}`);

function runExecutable(executable, argumentsList = [], input = "") {
  return spawnSync(executable, argumentsList, {
    cwd: projectDirectory,
    input,
    encoding: "utf8",
    windowsHide: true
  });
}

async function availablePort() {
  const probe = createServer();
  await new Promise((resolve, reject) => {
    probe.once("error", reject);
    probe.listen(0, "127.0.0.1", resolve);
  });
  const address = probe.address();
  const port = typeof address === "object" && address ? address.port : 0;
  await new Promise((resolve) => probe.close(resolve));
  assert.ok(port > 0, "Could not reserve a local test port");
  return port;
}

async function waitForHealth(baseUrl, serverProcess, logs) {
  for (let attempt = 0; attempt < 100; attempt += 1) {
    if (serverProcess.exitCode !== null) {
      throw new Error(`Test service exited early.\n${logs.join("")}`);
    }
    try {
      const response = await fetch(`${baseUrl}/api/health`);
      if (response.ok) return response.json();
    } catch {
      // The service may still be opening its SQLite database.
    }
    await new Promise((resolve) => setTimeout(resolve, 50));
  }
  throw new Error(`Timed out waiting for the test service.\n${logs.join("")}`);
}

async function jsonRequest(baseUrl, endpoint, options = {}) {
  const response = await fetch(`${baseUrl}${endpoint}`, options);
  const body = await response.json();
  return { response, body };
}

function testMatcherCli() {
  const now = Math.floor(Date.now() / 1000);
  const request = `3\t12.9600\t77.5900\t75\t90\t${now}\t86400\t3600\t1800\t0.6`;
  const donors = [
    `101\t0\t12.9610\t77.5910\t18\t1\t1\t1\t1\t${now}\t${now}\t${now}`,
    `102\t4\t12.9620\t77.5920\t10\t1\t1\t1\t1\t${now}\t${now}\t${now}`,
    `103\t3\t12.9630\t77.5930\t25\t1\t1\t1\t1\t${now}\t${now - 7200}\t${now}`,
    `104\t2\t12.9700\t77.6000\t15\t1\t1\t1\t1\t${now}\t${now}\t${now}`
  ];
  const ranked = runExecutable(matcherPath, [], `${request}\n${donors.join("\n")}\n`);
  assert.equal(ranked.status, 0, ranked.stderr);
  assert.deepEqual(ranked.stdout.trim().split(/\r?\n/).map((line) => line.split("\t")[0]), ["104", "101"]);

  const priority = runExecutable(matcherPath, ["--priority"], "3\t1\t2\t4\t5\t6\n");
  assert.equal(priority.status, 0, priority.stderr);
  assert.equal(priority.stdout.trim(), "29.000000");

  const antimeridian = runExecutable(
    matcherPath,
    [],
    `0\t0\t179.9\t50\t90\t${now}\t86400\t3600\t1800\t0.5\n201\t0\t0\t-179.9\t15\t1\t1\t1\t1\t${now}\t${now}\t${now}\n`
  );
  assert.equal(antimeridian.status, 0, antimeridian.stderr);
  assert.match(antimeridian.stdout, /^201\t/);

  const invalid = runExecutable(
    matcherPath,
    [],
    `0\t0\t0\t50\t90\t${now}\t86400\t3600\t1800\t0.5\n-1\t0\t0\t0\t15\t1\t1\t1\t1\t${now}\t${now}\t${now}\n`
  );
  assert.equal(invalid.status, 2);
  assert.match(invalid.stderr, /invalid candidate input/);

  const demo = runExecutable(demoPath, [], "0\n");
  assert.equal(demo.status, 0, demo.stderr);
  assert.match(demo.stdout, /Goodbye/);
}

async function testBrowserSyntax() {
  const html = await readFile(path.join(projectDirectory, "service", "index.html"), "utf8");
  const inlineScript = html.match(/<script>([\s\S]*?)<\/script>/);
  assert.ok(inlineScript, "The UI inline script could not be found");
  new Script(inlineScript[1], { filename: "service/index.html" });
}

async function testService() {
  const temporaryDirectory = await mkdtemp(path.join(os.tmpdir(), "blood-donor-finder-test-"));
  const databasePath = path.join(temporaryDirectory, "integration.db");
  const port = await availablePort();
  const baseUrl = `http://127.0.0.1:${port}`;
  const logs = [];
  const serverProcess = spawn(process.execPath, [path.join(projectDirectory, "service", "server.mjs")], {
    cwd: projectDirectory,
    env: {
      ...process.env,
      HOST: "127.0.0.1",
      PORT: String(port),
      DB_FILE: databasePath,
      MATCHER_BINARY: matcherPath
    },
    windowsHide: true,
    stdio: ["ignore", "pipe", "pipe"]
  });
  serverProcess.stdout.on("data", (chunk) => logs.push(chunk.toString()));
  serverProcess.stderr.on("data", (chunk) => logs.push(chunk.toString()));

  try {
    const health = await waitForHealth(baseUrl, serverProcess, logs);
    assert.deepEqual(health, { status: "ok", database: "sqlite", matcher: "c", syntheticData: true });

    const hospitals = await jsonRequest(baseUrl, "/api/hospitals");
    const groups = await jsonRequest(baseUrl, "/api/blood-groups");
    const donors = await jsonRequest(baseUrl, "/api/donors");
    assert.equal(hospitals.body.length, 3);
    assert.equal(groups.body.length, 8);
    assert.equal(donors.body.length, 20);
    assert.ok(donors.body.every((donor) => !("latitude" in donor) && !("longitude" in donor) && !("eta_minutes" in donor)));

    const availability = await jsonRequest(baseUrl, "/api/donors/D001/availability", {
      method: "PUT",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ available: 0 })
    });
    assert.equal(availability.response.status, 200);
    assert.equal(availability.body.available, 0);

    const invalidHospital = await jsonRequest(baseUrl, "/api/requests", {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ hospital_id: 999, blood_group: "A+", units_needed: 2 })
    });
    assert.equal(invalidHospital.response.status, 400);

    const created = await jsonRequest(baseUrl, "/api/requests", {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ hospital_id: 1, blood_group: "A+", units_needed: 2, latitude: 0, longitude: 0 })
    });
    assert.equal(created.response.status, 201);
    assert.equal(created.body.status, "pending");

    const matched = await jsonRequest(baseUrl, `/api/match/${created.body.requestId}`);
    assert.equal(matched.response.status, 200);
    assert.equal(matched.body.matcherEngine, "c-kd-tree-bounded-heap");
    assert.ok(matched.body.matches.length > 0);
    assert.ok(matched.body.matches.every((match) => !("name" in match) && !("latitude" in match) && !("longitude" in match)));

    const statsBeforeRepeat = await jsonRequest(baseUrl, "/api/stats");
    assert.equal(statsBeforeRepeat.body[0].matches_made, 1);
    await jsonRequest(baseUrl, `/api/match/${created.body.requestId}`);
    const statsAfterRepeat = await jsonRequest(baseUrl, "/api/stats");
    assert.equal(statsAfterRepeat.body[0].matches_made, 1);

    const malformed = await fetch(`${baseUrl}/api/requests`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: "{bad json"
    });
    assert.equal(malformed.status, 400);
    const page = await fetch(baseUrl);
    assert.equal(page.status, 200);
    assert.match(await page.text(), /Blood Donor Finder/);
  } finally {
    if (serverProcess.exitCode === null) serverProcess.kill();
    await Promise.race([
      new Promise((resolve) => serverProcess.once("exit", resolve)),
      new Promise((resolve) => setTimeout(resolve, 2000))
    ]);
    await rm(temporaryDirectory, { recursive: true, force: true });
  }
}

await testBrowserSyntax();
testMatcherCli();
await testService();
console.log("All C, browser syntax, API integration, privacy, and persistence tests passed.");
