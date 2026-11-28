import { spawnSync } from "node:child_process";
import path from "node:path";
import { fileURLToPath } from "node:url";

const scriptDirectory = path.dirname(fileURLToPath(import.meta.url));
const projectDirectory = path.resolve(scriptDirectory, "..");
const compiler = process.env.CC?.trim() || "gcc";
const executableSuffix = process.platform === "win32" ? ".exe" : "";
const commonFlags = ["-std=c11", "-Wall", "-Wextra", "-Wpedantic", "-Werror", "-O2"];

const targets = [
  {
    name: "standalone data-structures demo",
    sources: ["main.c", "core/rbtree.c", "core/skiplist.c"],
    output: `blood_donor_finder${executableSuffix}`
  },
  {
    name: "C donor matching engine",
    sources: ["core/matcher.c", "core/matcher_cli.c"],
    output: path.join("core", `bdf_matcher_cli${executableSuffix}`)
  }
];

for (const target of targets) {
  const result = spawnSync(
    compiler,
    [...commonFlags, ...target.sources, "-lm", "-o", target.output],
    { cwd: projectDirectory, stdio: "inherit", windowsHide: true }
  );

  if (result.error?.code === "ENOENT") {
    console.error(`C compiler '${compiler}' was not found. Install GCC and ensure it is on PATH, or set CC.`);
    process.exit(1);
  }
  if (result.error) {
    console.error(`Could not start the C compiler: ${result.error.message}`);
    process.exit(1);
  }
  if (result.status !== 0) {
    console.error(`Failed to build ${target.name}.`);
    process.exit(result.status || 1);
  }
  console.log(`Built ${target.name}: ${target.output}`);
}
