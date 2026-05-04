use std::env;
use std::fs;
use std::process::Command;
use std::time::{SystemTime, UNIX_EPOCH};

pub fn run_setup() -> Result<String, String> {
    let home = match env::var("HOME") {
        Ok(h) => h,
        Err(_) => match env::var("USERPROFILE") {
            Ok(h) => h,
            Err(_) => return Err("Cannot determine home directory".into()),
        },
    };

    // Reject home paths with single quotes (would break shell commands)
    if home.contains('\'') {
        return Err("Home path contains illegal character (single quote)".into());
    }

    Command::new("flatpak")
        .args(["override", "--user", "--reset", "org.vinegarhq.Sober"])
        .output()
        .map_err(|e| format!("flatpak reset failed: {}", e))?;

    // Grant read-only access to just the .so — not the entire home
    let so_path = format!("{}/libdeltoid.so", home);
    Command::new("flatpak")
        .args([
            "override",
            "--user",
            "--filesystem",
            &format!("{}:ro", so_path),
            "org.vinegarhq.Sober",
        ])
        .output()
        .map_err(|e| format!("flatpak filesystem override failed: {}", e))?;

    let preload_path = format!("{}/libdeltoid.so", home);
    Command::new("flatpak")
        .args([
            "override",
            "--user",
            "--env",
            &format!("LD_PRELOAD={}", preload_path),
            "org.vinegarhq.Sober",
        ])
        .output()
        .map_err(|e| format!("flatpak LD_PRELOAD override failed: {}", e))?;

    // Verify overrides took effect
    let out = Command::new("flatpak")
        .args(["override", "--user", "--show", "org.vinegarhq.Sober"])
        .output()
        .map_err(|e| format!("flatpak verify failed: {}", e))?;

    let show = String::from_utf8_lossy(&out.stdout);
    if !show.contains("LD_PRELOAD") {
        return Err("Flatpak override missing LD_PRELOAD — check permissions".into());
    }
    if !show.contains(&so_path) {
        return Err("Flatpak override missing filesystem .so — check permissions".into());
    }

    Ok("Stealth hook set. Launch Sober now.".into())
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum InjectStatus {
    Unknown,
    NotRunning,
    RunningNotInjected,
    Injected,
}

pub fn check_inject_status() -> InjectStatus {
    // 1. Find Sober PID
    let pid = match find_sober_pid() {
        Some(p) => p,
        None => return InjectStatus::NotRunning,
    };

    // 2. Check /proc/<pid>/maps for libdeltoid.so
    let maps_path = format!("/proc/{}/maps", pid);
    if let Ok(maps) = fs::read_to_string(&maps_path) {
        if !maps.contains("libdeltoid.so") {
            return InjectStatus::RunningNotInjected;
        }
    } else {
        return InjectStatus::RunningNotInjected;
    }

    // 3. Check heartbeat file matches PID and is recent (< 60s)
    if let Ok(content) = fs::read_to_string("/tmp/deltoid_active") {
        let parts: Vec<&str> = content.trim().split_whitespace().collect();
        if parts.len() == 2 {
            if let (Ok(file_pid), Ok(file_ts)) = (parts[0].parse::<u32>(), parts[1].parse::<u64>()) {
                if file_pid == pid {
                    let now = SystemTime::now()
                        .duration_since(UNIX_EPOCH)
                        .unwrap_or_default()
                        .as_secs();
                    if now.saturating_sub(file_ts) < 60 {
                        return InjectStatus::Injected;
                    }
                }
            }
        }
    }

    // libdeltoid.so is in maps but no recent heartbeat yet — likely still initializing
    InjectStatus::RunningNotInjected
}

fn find_sober_pid() -> Option<u32> {
    // Try pgrep first
    if let Ok(out) = Command::new("pgrep").args(["-f", "sober"]).output() {
        let stdout = String::from_utf8_lossy(&out.stdout);
        for line in stdout.lines() {
            if let Ok(pid) = line.trim().parse::<u32>() {
                // Verify it's actually the flatpak sober process by checking cmdline
                if let Ok(cmdline) = fs::read_to_string(format!("/proc/{}/cmdline", pid)) {
                    if cmdline.contains("sober") {
                        return Some(pid);
                    }
                }
            }
        }
    }

    // Fallback: iterate /proc
    if let Ok(entries) = fs::read_dir("/proc") {
        for entry in entries.flatten() {
            let name = entry.file_name();
            if let Ok(pid) = name.to_string_lossy().parse::<u32>() {
                if let Ok(cmdline) = fs::read_to_string(format!("/proc/{}/cmdline", pid)) {
                    if cmdline.contains("sober") && cmdline.contains("org.vinegarhq") {
                        return Some(pid);
                    }
                }
            }
        }
    }

    None
}
