use eframe::egui;
use std::fs;
use std::time::SystemTime;

mod setup;
use setup::{check_inject_status, InjectStatus};

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum Tab {
    Editor,
    Settings,
}

pub struct DeltoidApp {
    active_tab: Tab,
    script: String,
    status: String,
    show_toast: bool,
    toast_timer: f32,
    inject_status: InjectStatus,
    status_check_timer: f32,
}

impl Default for DeltoidApp {
    fn default() -> Self {
        Self {
            active_tab: Tab::Editor,
            script: String::from("print(\"Hello from Deltoid!\")"),
            status: String::from("Ready"),
            show_toast: false,
            toast_timer: 0.0,
            inject_status: InjectStatus::Unknown,
            status_check_timer: 0.0,
        }
    }
}

fn dark_theme(ctx: &egui::Context) {
    let mut style = (*ctx.style()).clone();
    style.visuals = egui::Visuals::dark();
    style.visuals.extreme_bg_color = egui::Color32::from_rgb(18, 18, 24);
    style.visuals.panel_fill = egui::Color32::from_rgb(28, 28, 36);
    style.visuals.window_fill = egui::Color32::from_rgb(28, 28, 36);
    style.visuals.widgets.inactive.weak_bg_fill = egui::Color32::from_rgb(40, 40, 52);
    style.visuals.widgets.inactive.bg_fill = egui::Color32::from_rgb(40, 40, 52);
    style.visuals.widgets.hovered.weak_bg_fill = egui::Color32::from_rgb(55, 55, 72);
    style.visuals.widgets.hovered.bg_fill = egui::Color32::from_rgb(55, 55, 72);
    style.visuals.widgets.active.bg_fill = egui::Color32::from_rgb(70, 70, 90);
    style.visuals.selection.bg_fill = egui::Color32::from_rgb(88, 101, 242);
    style.visuals.widgets.noninteractive.bg_fill = egui::Color32::from_rgb(28, 28, 36);
    style.visuals.widgets.noninteractive.weak_bg_fill = egui::Color32::from_rgb(28, 28, 36);
    style.visuals.override_text_color = Some(egui::Color32::from_rgb(220, 220, 230));
    style.visuals.window_rounding = egui::Rounding::same(8.0);
    style.visuals.menu_rounding = egui::Rounding::same(6.0);
    style.spacing.item_spacing = egui::vec2(8.0, 8.0);
    style.spacing.window_margin = egui::Margin::same(12.0);
    ctx.set_style(style);
}

impl eframe::App for DeltoidApp {
    fn update(&mut self, ctx: &egui::Context, _frame: &mut eframe::Frame) {
        dark_theme(ctx);

        // Toast logic
        if self.show_toast {
            self.toast_timer -= ctx.input(|i| i.stable_dt);
            if self.toast_timer <= 0.0 {
                self.show_toast = false;
            }
        }

        // Periodic inject status check
        self.status_check_timer -= ctx.input(|i| i.stable_dt);
        if self.status_check_timer <= 0.0 {
            self.inject_status = check_inject_status();
            self.status_check_timer = 2.0;
        }

        // Top bar
        egui::TopBottomPanel::top("top_bar")
            .exact_height(52.0)
            .show(ctx, |ui| {
                ui.horizontal_centered(|ui| {
                    ui.add_space(12.0);
                    ui.label(
                        egui::RichText::new("◆ DELTOID")
                            .size(20.0)
                            .color(egui::Color32::from_rgb(88, 101, 242))
                            .strong(),
                    );
                    ui.label(
                        egui::RichText::new("Executor")
                            .size(14.0)
                            .color(egui::Color32::from_rgb(150, 150, 170)),
                    );

                    ui.with_layout(egui::Layout::right_to_left(egui::Align::Center), |ui| {
                        ui.add_space(12.0);
                        if self.show_toast {
                            ui.label(
                                egui::RichText::new(&self.status)
                                    .color(egui::Color32::from_rgb(100, 255, 100))
                                    .size(13.0),
                            );
                        }
                    });
                });
            });

        // Tab bar
        egui::TopBottomPanel::top("tabs")
            .exact_height(40.0)
            .show(ctx, |ui| {
                ui.horizontal(|ui| {
                    ui.add_space(12.0);
                    for tab in [Tab::Editor, Tab::Settings] {
                        let label = match tab {
                            Tab::Editor => "📝 Editor",
                            Tab::Settings => "⚙ Settings",
                        };
                        let active = self.active_tab == tab;
                        let color = if active {
                            egui::Color32::from_rgb(88, 101, 242)
                        } else {
                            egui::Color32::from_rgb(150, 150, 170)
                        };
                        let bg = if active {
                            egui::Color32::from_rgb(40, 40, 55)
                        } else {
                            egui::Color32::TRANSPARENT
                        };

                        let response = ui
                            .add_sized(
                                egui::vec2(110.0, 32.0),
                                egui::Button::new(
                                    egui::RichText::new(label)
                                        .color(color)
                                        .size(14.0)
                                        .strong(),
                                )
                                .fill(bg)
                                .rounding(egui::Rounding::same(6.0)),
                            )
                            .on_hover_cursor(egui::CursorIcon::PointingHand);

                        if response.clicked() {
                            self.active_tab = tab;
                        }
                    }
                });
            });

        // Main content
        egui::CentralPanel::default().show(ctx, |ui| {
            match self.active_tab {
                Tab::Editor => self.render_editor(ui, ctx),
                Tab::Settings => self.render_settings(ui),
            }
        });

        // Bottom status bar
        egui::TopBottomPanel::bottom("status")
            .exact_height(28.0)
            .show(ctx, |ui| {
                ui.horizontal(|ui| {
                    ui.add_space(8.0);

                    // Inject status indicator
                    let (inject_label, inject_color) = match self.inject_status {
                        InjectStatus::Injected => ("● Injected", egui::Color32::from_rgb(80, 200, 120)),
                        InjectStatus::RunningNotInjected => ("● Sober running", egui::Color32::from_rgb(255, 200, 80)),
                        InjectStatus::NotRunning => ("● Sober not running", egui::Color32::from_rgb(240, 80, 80)),
                        InjectStatus::Unknown => ("● Checking…", egui::Color32::from_rgb(200, 200, 200)),
                    };
                    ui.label(egui::RichText::new(inject_label).color(inject_color).size(12.0));

                    ui.separator();

                    let (indicator, color) = if self.status.starts_with('✅') || self.status.starts_with("Script sent") {
                        ("●", egui::Color32::from_rgb(80, 200, 120))
                    } else if self.status.starts_with('❌') {
                        ("●", egui::Color32::from_rgb(240, 80, 80))
                    } else {
                        ("●", egui::Color32::from_rgb(200, 200, 200))
                    };
                    ui.label(egui::RichText::new(indicator).color(color).size(12.0));
                    ui.label(
                        egui::RichText::new(&self.status)
                            .size(12.0)
                            .color(egui::Color32::from_rgb(180, 180, 200)),
                    );
                });
            });
    }
}

impl DeltoidApp {
    fn render_editor(&mut self, ui: &mut egui::Ui, ctx: &egui::Context) {
        ui.add_space(8.0);

        // Toolbar
        ui.horizontal(|ui| {
            ui.add_space(8.0);

            let btn = |ui: &mut egui::Ui, icon: &str, text: &str| -> bool {
                ui.add_sized(
                    egui::vec2(90.0, 32.0),
                    egui::Button::new(
                        egui::RichText::new(format!("{} {}", icon, text))
                            .size(13.0)
                            .color(egui::Color32::WHITE),
                    )
                    .fill(egui::Color32::from_rgb(50, 50, 65))
                    .rounding(egui::Rounding::same(6.0)),
                )
                .on_hover_cursor(egui::CursorIcon::PointingHand)
                .clicked()
            };

            if btn(ui, "📂", "Open") {
                if let Some(path) = rfd::FileDialog::new().add_filter("Lua", &["lua"]).pick_file() {
                    if let Ok(content) = fs::read_to_string(&path) {
                        self.script = content;
                        self.toast("Script loaded");
                    }
                }
            }

            if btn(ui, "💾", "Save") {
                if let Some(path) = rfd::FileDialog::new().add_filter("Lua", &["lua"]).save_file() {
                    if fs::write(&path, &self.script).is_ok() {
                        self.toast("Script saved");
                    }
                }
            }

            if btn(ui, "🗑", "Clear") {
                self.script.clear();
                self.toast("Cleared");
            }

            ui.with_layout(egui::Layout::right_to_left(egui::Align::Center), |ui| {
                let execute = ui
                    .add_sized(
                        egui::vec2(120.0, 36.0),
                        egui::Button::new(
                            egui::RichText::new("▶ Execute")
                                .size(14.0)
                                .strong()
                                .color(egui::Color32::WHITE),
                        )
                        .fill(egui::Color32::from_rgb(88, 101, 242))
                        .rounding(egui::Rounding::same(8.0)),
                    )
                    .on_hover_cursor(egui::CursorIcon::PointingHand)
                    .clicked();

                if execute || ctx.input(|i| i.key_pressed(egui::Key::Enter) && i.modifiers.ctrl) {
                    self.execute_script();
                }
                ui.add_space(8.0);
            });
        });

        ui.add_space(4.0);

        // Script editor
        ui.vertical_centered(|ui| {
            let available = ui.available_size();
            egui::Frame::none()
                .fill(egui::Color32::from_rgb(18, 18, 24))
                .rounding(egui::Rounding::same(8.0))
                .inner_margin(egui::Margin::same(10.0))
                .show(ui, |ui| {
                    ui.set_min_size(available - egui::vec2(16.0, 16.0));
                    egui::ScrollArea::vertical()
                        .auto_shrink([false; 2])
                        .show(ui, |ui| {
                            ui.add(
                                egui::TextEdit::multiline(&mut self.script)
                                    .code_editor()
                                    .desired_rows(25)
                                    .desired_width(f32::INFINITY)
                                    .font(egui::TextStyle::Monospace),
                            );
                        });
                });
        });
    }

    fn render_settings(&mut self, ui: &mut egui::Ui) {
        ui.add_space(12.0);
        ui.horizontal(|ui| {
            ui.add_space(8.0);
            ui.heading(
                egui::RichText::new("Settings")
                    .size(18.0)
                    .color(egui::Color32::from_rgb(220, 220, 230)),
            );
        });
        ui.add_space(16.0);

        egui::Frame::none()
            .fill(egui::Color32::from_rgb(35, 35, 48))
            .rounding(egui::Rounding::same(8.0))
            .inner_margin(egui::Margin::same(16.0))
            .outer_margin(egui::Margin::same(8.0))
            .show(ui, |ui| {
                ui.label(
                    egui::RichText::new("Flatpak Configuration")
                        .size(15.0)
                        .strong()
                        .color(egui::Color32::WHITE),
                );
                ui.add_space(8.0);
                ui.label(
                    egui::RichText::new(
                        "This configures the Sober flatpak to allow filesystem access and LD_PRELOAD injection.",
                    )
                    .size(13.0)
                    .color(egui::Color32::from_rgb(160, 160, 180)),
                );
                ui.add_space(12.0);

                if ui
                    .add_sized(
                        egui::vec2(160.0, 36.0),
                        egui::Button::new(
                            egui::RichText::new("🔧 Run Setup")
                                .size(14.0)
                                .color(egui::Color32::WHITE),
                        )
                        .fill(egui::Color32::from_rgb(88, 101, 242))
                        .rounding(egui::Rounding::same(8.0)),
                    )
                    .on_hover_cursor(egui::CursorIcon::PointingHand)
                    .clicked()
                {
                    match setup::run_setup() {
                        Ok(msg) => self.toast(&msg),
                        Err(e) => {
                            self.status = format!("❌ {}", e);
                        }
                    }
                }
            });

        ui.add_space(8.0);

        egui::Frame::none()
            .fill(egui::Color32::from_rgb(35, 35, 48))
            .rounding(egui::Rounding::same(8.0))
            .inner_margin(egui::Margin::same(16.0))
            .outer_margin(egui::Margin::same(8.0))
            .show(ui, |ui| {
                ui.label(
                    egui::RichText::new("Injection Status")
                        .size(15.0)
                        .strong()
                        .color(egui::Color32::WHITE),
                );
                ui.add_space(8.0);

                let status_text = match self.inject_status {
                    InjectStatus::Injected => "✅ Injected — ready to execute scripts",
                    InjectStatus::RunningNotInjected => "⏳ Sober is running but injection not active yet.\n   Launch Sober through the launcher and wait a few seconds.",
                    InjectStatus::NotRunning => "❌ Sober is not running.\n   Launch Sober after running setup.",
                    InjectStatus::Unknown => "🔍 Checking injection status…",
                };
                ui.label(
                    egui::RichText::new(status_text)
                        .size(13.0)
                        .color(egui::Color32::from_rgb(160, 160, 180)),
                );
                ui.add_space(12.0);

                if ui
                    .add_sized(
                        egui::vec2(140.0, 32.0),
                        egui::Button::new(
                            egui::RichText::new("🔄 Refresh")
                                .size(13.0)
                                .color(egui::Color32::WHITE),
                        )
                        .fill(egui::Color32::from_rgb(60, 60, 80))
                        .rounding(egui::Rounding::same(6.0)),
                    )
                    .on_hover_cursor(egui::CursorIcon::PointingHand)
                    .clicked()
                {
                    self.inject_status = check_inject_status();
                }
            });

        ui.add_space(8.0);

        egui::Frame::none()
            .fill(egui::Color32::from_rgb(35, 35, 48))
            .rounding(egui::Rounding::same(8.0))
            .inner_margin(egui::Margin::same(16.0))
            .outer_margin(egui::Margin::same(8.0))
            .show(ui, |ui| {
                ui.label(
                    egui::RichText::new("About")
                        .size(15.0)
                        .strong()
                        .color(egui::Color32::WHITE),
                );
                ui.add_space(8.0);
                ui.label(
                    egui::RichText::new("Deltoid Executor v0.1.0")
                        .size(13.0)
                        .color(egui::Color32::from_rgb(160, 160, 180)),
                );
                ui.label(
                    egui::RichText::new("External UI + Internal executor for Sober (Roblox)")
                        .size(13.0)
                        .color(egui::Color32::from_rgb(160, 160, 180)),
                );
                ui.hyperlink_to("GitHub", "https://github.com/21341414/deltoid");
            });
    }

    fn execute_script(&mut self) {
        let path = "/tmp/deltoid_exec.lua";
        match fs::write(path, &self.script) {
            Ok(_) => {
                let duration = SystemTime::now()
                    .duration_since(SystemTime::UNIX_EPOCH)
                    .unwrap_or_default();
                let secs = duration.as_secs();
                let hh = (secs / 3600) % 24;
                let mm = (secs / 60) % 60;
                let ss = secs % 60;
                self.status = format!(
                    "✅ Script sent ({:02}:{:02}:{:02})",
                    hh, mm, ss
                );
                self.toast("Script executed");
            }
            Err(e) => {
                self.status = format!("❌ Failed to write script: {}", e);
            }
        }
    }

    fn toast(&mut self, msg: &str) {
        self.status = msg.to_string();
        self.show_toast = true;
        self.toast_timer = 3.0;
    }
}

fn main() {
    let options = eframe::NativeOptions {
        viewport: egui::ViewportBuilder::default()
            .with_inner_size([900.0, 650.0])
            .with_min_inner_size([700.0, 500.0]),
        ..Default::default()
    };
    eframe::run_native(
        "Deltoid",
        options,
        Box::new(|_cc| Box::new(DeltoidApp::default())),
    )
    .unwrap();
}
