use std::{env, fs, process::ExitCode, time::Instant};
use std::ffi::CString;
use std::os::raw::c_char;

use serde::Deserialize;
use eframe::egui;

#[repr(C)]
struct FluxProgram;

unsafe extern "C" {
    fn flux_compile(filename: *const c_char, source: *const c_char) -> *mut FluxProgram;
    fn flux_run(prog: *mut FluxProgram) -> i32;
    fn flux_ui_take_request(prog: *mut FluxProgram, out_json: *mut *const c_char, out_backend: *mut *const c_char) -> i32;
    fn flux_free_cstr(s: *const c_char);
    fn flux_free_program(prog: *mut FluxProgram);
}

fn print_help(exe: &str) {
    eprintln!("Usage:\n  {exe} run <file.fl>\n  {exe} --version\n  {exe} --help");
}

#[derive(Debug, Deserialize, Clone)]
struct UiDoc {
    title: String,
    backend: String,
    items: Vec<UiItem>,
}

#[derive(Debug, Deserialize, Clone)]
#[serde(tag = "kind")]
enum UiItem {
    #[serde(rename = "text")]
    Text { text: String },

    #[serde(rename = "button")]
    Button { label: String },

    #[serde(rename = "input")]
    Input { label: String, value: String },

    #[serde(rename = "select")]
    Select { label: String, options: Vec<String> },

    #[serde(rename = "checkbox")]
    Checkbox { label: String, checked: bool },
}


fn main() -> ExitCode {
    let args: Vec<String> = env::args().collect();
    let exe = args.get(0).map(|s| s.as_str()).unwrap_or("flux");

    if args.len() < 2 || args[1] == "--help" || args[1] == "-h" {
        print_help(exe);
        return ExitCode::from(3);
    }
    if args[1] == "--version" {
        println!("Flux 1.0.1 (rust-cli + c-engine + ui)");
        return ExitCode::SUCCESS;
    }
    if args[1] != "run" || args.len() < 3 {
        print_help(exe);
        return ExitCode::from(3);
    }

    let path = &args[2];
    let src = match fs::read_to_string(path) {
        Ok(s) => s,
        Err(_) => {
            eprintln!("could not read file: {path}");
            return ExitCode::from(3);
        }
    };

    // Keep CStrings alive across compile+run (C borrows pointers).
    let c_filename = CString::new(path.as_str()).unwrap();
    let c_source = CString::new(src).unwrap();

    let t0 = Instant::now();
    let prog = unsafe { flux_compile(c_filename.as_ptr(), c_source.as_ptr()) };
    let dt = t0.elapsed();

    if prog.is_null() {
        return ExitCode::from(2);
    }

    println!("[FLUX]: Compiled in {:.3} ms", dt.as_secs_f64() * 1000.0);

    let ok = unsafe { flux_run(prog) };

    // If program requested UI, run it (TUI or window).
    unsafe {
        let mut json_ptr: *const c_char = std::ptr::null();
        let mut backend_ptr: *const c_char = std::ptr::null();
        let has_ui = flux_ui_take_request(prog, &mut json_ptr as *mut _, &mut backend_ptr as *mut _);

        if has_ui == 1 && !json_ptr.is_null() {
            let json = std::ffi::CStr::from_ptr(json_ptr).to_string_lossy().to_string();
            let backend = if backend_ptr.is_null() {
                "tui".to_string()
            } else {
                std::ffi::CStr::from_ptr(backend_ptr).to_string_lossy().to_string()
            };

            flux_free_cstr(json_ptr);
            if !backend_ptr.is_null() { flux_free_cstr(backend_ptr); }

            let doc: UiDoc = match serde_json::from_str(&json) {
                Ok(d) => d,
                Err(e) => {
                    eprintln!("UI JSON parse error: {e}\n{json}");
                    flux_free_program(prog);
                    return ExitCode::from(1);
                }
            };

            // backend can be requested by ui.run("window") etc
            let chosen = backend.to_lowercase();
            if chosen == "window" {
                if let Err(e) = run_window(doc) {
                    eprintln!("window ui error: {e}");
                    flux_free_program(prog);
                    return ExitCode::from(1);
                }
            } else {
                if let Err(e) = run_tui(doc) {
                    eprintln!("tui error: {e}");
                    flux_free_program(prog);
                    return ExitCode::from(1);
                }
            }
        }
    }

    unsafe { flux_free_program(prog) };

    if ok == 1 { ExitCode::SUCCESS } else { ExitCode::from(1) }
}

// -------------------- TUI --------------------


fn run_tui(doc: UiDoc) -> Result<(), Box<dyn std::error::Error>> {
    use crossterm::{
        event::{self, Event, KeyCode, KeyEventKind, KeyModifiers},
        execute,
        terminal::{disable_raw_mode, enable_raw_mode, EnterAlternateScreen, LeaveAlternateScreen},
    };
    use ratatui::{prelude::*, widgets::*};

    // State for interactive widgets
    #[derive(Clone)]
    enum ItemState {
        Static,
        Button,
        Input { value: String },
        Select { options: Vec<String>, selected: usize },
        Checkbox { checked: bool },
    }

    let mut states: Vec<ItemState> = Vec::with_capacity(doc.items.len());
    let mut focusables: Vec<usize> = vec![];

    for (i, it) in doc.items.iter().enumerate() {
        let st = match it {
            UiItem::Text { .. } => ItemState::Static,
            UiItem::Button { .. } => {
                focusables.push(i);
                ItemState::Button
            }
            UiItem::Input { value, .. } => {
                focusables.push(i);
                ItemState::Input { value: value.clone() }
            }
            UiItem::Select { options, .. } => {
                focusables.push(i);
                ItemState::Select { options: options.clone(), selected: 0 }
            }
            UiItem::Checkbox { checked, .. } => {
                focusables.push(i);
                ItemState::Checkbox { checked: *checked }
            }
        };
        states.push(st);
    }

    let mut focus_pos: usize = 0;
    let mut status = String::from("Tab: focus | Enter: activate | Space: toggle | q: quit");

    enable_raw_mode()?;
    let mut stdout = std::io::stdout();
    execute!(stdout, EnterAlternateScreen)?;
    let backend = ratatui::backend::CrosstermBackend::new(stdout);
    let mut terminal = Terminal::new(backend)?;

    let cleanup = || -> Result<(), Box<dyn std::error::Error>> {
        disable_raw_mode()?;
        execute!(std::io::stdout(), LeaveAlternateScreen)?;
        Ok(())
    };

    loop {
        terminal.draw(|f| {
            let area = f.area();
            let chunks = Layout::default()
                .direction(Direction::Vertical)
                .margin(1)
                .constraints([Constraint::Length(2), Constraint::Min(1), Constraint::Length(2)].as_ref())
                .split(area);

            let title = Paragraph::new(doc.title.as_str())
                .style(Style::default().add_modifier(Modifier::BOLD));
            f.render_widget(title, chunks[0]);

            let mut lines: Vec<Line> = vec![];

            for (i, item) in doc.items.iter().enumerate() {
                let focused = focusables.get(focus_pos).copied() == Some(i);
                let prefix = if focused { "➤ " } else { "  " };

                let line = match (item, &states[i]) {
                    (UiItem::Text { text }, _) => Line::from(format!("{prefix}{text}")),
                    (UiItem::Button { label }, _) => {
                        let mut s = format!("{prefix}[ {label} ]");
                        if focused { s.push_str("  (Enter)"); }
                        Line::from(s)
                    }
                    (UiItem::Input { label, .. }, ItemState::Input { value }) => {
                        let mut s = format!("{prefix}{label}: {value}");
                        if focused { s.push_str("  (type)"); }
                        Line::from(s)
                    }
                    (UiItem::Select { label, .. }, ItemState::Select { options, selected }) => {
                        let cur = options.get(*selected).cloned().unwrap_or_else(|| "".to_string());
                        let mut s = format!("{prefix}{label}: <{cur}>");
                        if focused { s.push_str("  (←/→)"); }
                        Line::from(s)
                    }
                    (UiItem::Checkbox { label, .. }, ItemState::Checkbox { checked }) => {
                        let mark = if *checked { "[x]" } else { "[ ]" };
                        let mut s = format!("{prefix}{mark} {label}");
                        if focused { s.push_str("  (Space)"); }
                        Line::from(s)
                    }
                    _ => Line::from(format!("{prefix}<unknown>")),
                };

                lines.push(line);
            }

            let list = Paragraph::new(lines)
                .wrap(Wrap { trim: false })
                .block(Block::default().borders(Borders::ALL).title("UI"));
            f.render_widget(list, chunks[1]);

            let status_p = Paragraph::new(status.as_str())
                .block(Block::default().borders(Borders::ALL).title("Status"));
            f.render_widget(status_p, chunks[2]);
        })?;

        if event::poll(std::time::Duration::from_millis(50))? {
            if let Event::Key(key) = event::read()? {
                if key.kind != KeyEventKind::Press {
                    continue;
                }

                match key.code {
                    KeyCode::Char('q') => { cleanup()?; return Ok(()); }
                    KeyCode::Tab => {
                        if !focusables.is_empty() {
                            focus_pos = (focus_pos + 1) % focusables.len();
                        }
                    }
                    KeyCode::BackTab => {
                        if !focusables.is_empty() {
                            focus_pos = (focus_pos + focusables.len() - 1) % focusables.len();
                        }
                    }
                    _ => {}
                }

                if focusables.is_empty() { continue; }
                let idx = focusables[focus_pos];

                match (&doc.items[idx], &mut states[idx], key.code) {
                    (UiItem::Button { label }, ItemState::Button, KeyCode::Enter) => {
                        status = format!("Clicked '{label}' (callbacks not implemented).");
                    }
                    (UiItem::Checkbox { label, .. }, ItemState::Checkbox { checked }, KeyCode::Char(' ')) => {
                        *checked = !*checked;
                        status = format!("{label}: {}", if *checked { "true" } else { "false" });
                    }
                    (UiItem::Select { label, .. }, ItemState::Select { options, selected }, KeyCode::Left) => {
                        if !options.is_empty() {
                            *selected = (*selected + options.len() - 1) % options.len();
                            status = format!("{label}: {}", options[*selected]);
                        }
                    }
                    (UiItem::Select { label, .. }, ItemState::Select { options, selected }, KeyCode::Right) => {
                        if !options.is_empty() {
                            *selected = (*selected + 1) % options.len();
                            status = format!("{label}: {}", options[*selected]);
                        }
                    }
                    (UiItem::Input { .. }, ItemState::Input { value }, KeyCode::Backspace) => {
                        value.pop();
                    }
                    (UiItem::Input { .. }, ItemState::Input { value }, KeyCode::Char(c)) => {
                        // basic text entry (ignore ctrl shortcuts)
                        if !key.modifiers.contains(KeyModifiers::CONTROL) {
                            value.push(c);
                        }
                    }
                    _ => {}
                }
            }
        }
    }
}


fn run_window(doc: UiDoc) -> Result<(), Box<dyn std::error::Error>> {
    #[derive(Default)]
    struct AppState {
        doc: Option<UiDoc>,
        // store current editable values for inputs by index
        input_values: std::collections::HashMap<usize, String>,
        select_values: std::collections::HashMap<usize, usize>,
        checkbox_values: std::collections::HashMap<usize, bool>,
        status: String,
    }

    impl eframe::App for AppState {
        fn update(&mut self, ctx: &egui::Context, _frame: &mut eframe::Frame) {
            let Some(doc) = self.doc.clone() else { return; };

            egui::CentralPanel::default().show(ctx, |ui| {
                ui.heading(&doc.title);
                ui.separator();

                for (i, item) in doc.items.iter().enumerate() {
                    match item {
                        UiItem::Text { text } => { ui.label(text); }
                        UiItem::Button { label } => {
                            if ui.button(label).clicked() {
                                self.status = format!("Clicked '{label}' (callbacks not implemented yet).");
                            }
                        }
                        UiItem::Input { label, value } => {
                            let entry = self.input_values.entry(i).or_insert_with(|| value.clone());
                            ui.horizontal(|ui| {
                                ui.label(label);
                                ui.text_edit_singleline(entry);
                            });
                        }
                        UiItem::Select { label, options } => {
                            let sel = self.select_values.entry(i).or_insert(0usize);
                            ui.horizontal(|ui| {
                                ui.label(label);
                                egui::ComboBox::from_id_source(i)
                                    .selected_text(options.get(*sel).cloned().unwrap_or_else(|| "<none>".to_string()))
                                    .show_ui(ui, |ui| {
                                        for (j, opt) in options.iter().enumerate() {
                                            ui.selectable_value(sel, j, opt);
                                        }
                                    });
                            });
                        }
                        UiItem::Checkbox { label, checked } => {
                            let v = self.checkbox_values.entry(i).or_insert(*checked);
                            ui.checkbox(v, label);
                        }
                    }
                }

                ui.separator();
                ui.label(&self.status);
            });
        }
    }

    let mut app = AppState::default();
    app.doc = Some(doc);

    let native_options = eframe::NativeOptions::default();
    eframe::run_native(
        "Flux UI",
        native_options,
        Box::new(|_cc| Ok(Box::new(app))),
    )?;

    Ok(())
}
