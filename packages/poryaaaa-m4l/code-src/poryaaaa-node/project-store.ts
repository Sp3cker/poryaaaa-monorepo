import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { homedir } from "node:os";


export type ProjectStateFile = Record<string, unknown>;

export interface StatePathOptions {
  platform?: NodeJS.Platform | string;
  env?: Record<string, string | undefined>;
  homeDir?: string;
}

export interface ProjectStoreOptions extends StatePathOptions {
  statePath?: string;
}

export function resolveProjectsJsonPath(opts: StatePathOptions = {}): string {
  const platform = opts.platform ?? process.platform;
  const env = opts.env ?? process.env;
  const home = opts.homeDir ?? homedir();

  if (platform === "darwin") {
    return join(home, "Library", "Application Support", "poryaaaa", "projects.json");
  }
  if (platform === "win32") {
    const base = env.APPDATA || join(home, "AppData", "Roaming");
    return join(base, "poryaaaa", "projects.json");
  }
  const configHome = env.XDG_CONFIG_HOME || join(home, ".config");
  return join(configHome, "poryaaaa", "projects.json");
}

export class ProjectStore {
  readonly statePath: string;

  constructor(opts: ProjectStoreOptions = {}) {
    this.statePath = opts.statePath ?? resolveProjectsJsonPath(opts);
  }

  readVoicegroupState(): Record<string, string> | null {
    const all = this.readAllStates();
    const root = typeof all.root === "string" ? all.root : "";
    const bank = typeof all.bank === "string" ? all.bank : "";
    if (root && bank) return { root, bank };

    if (!root && !bank) return null;
    return { root, bank };
  }

  writeVoicegroupState(state: { root: string, bank: string }): void {
    const all = this.readAllStates();
    all.root = state.root;
    all.bank = state.bank;
    this.writeAllStates(all);
  }

  readAllStates(): ProjectStateFile {
    let raw: string;
    try {
      raw = readFileSync(this.statePath, "utf8");
    } catch (_) {
      return {};
    }
    try {
      const parsed = JSON.parse(raw);
      if (!parsed || typeof parsed !== "object" || Array.isArray(parsed)) {
        return {};
      }
      return parsed as ProjectStateFile;
    } catch (_) {
      this.backupMalformedState(raw);
      return {};
    }
  }

  private backupMalformedState(raw: string): void {
    const backupPath = `${this.statePath}.bad`;
    if (existsSync(backupPath)) return;
    mkdirSync(dirname(backupPath), { recursive: true });
    writeFileSync(backupPath, raw);
  }



  private writeAllStates(states: ProjectStateFile): void {
    mkdirSync(dirname(this.statePath), { recursive: true });
    writeFileSync(this.statePath, `${JSON.stringify(states, null, 2)}\n`);
  }
}
