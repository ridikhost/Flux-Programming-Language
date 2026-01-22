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
}

fn main() -> ExitCode {
    let args: Vec<String> = env::args().collect();
    let exe = args.get(0).map(|s| s.as_str()).unwrap_or("flux");

    if args.len() < 2 || args[1] == "--help" || args[1] == "-h" {
        print_help(exe);
        return ExitCode::from(3);
    }
    if args[1] == "--version" {
        println!("Flux 1.0.0 (rust-cli + c-engine + ui)");
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
        event::{self, Event, KeyCode, KeyEventKind},
        execute,
        terminal::{disable_raw_mode, enable_raw_mode, EnterAlternateScreen, LeaveAlternateScreen},
    };
    use ratatui::{prelude::*, widgets::*};

    enable_raw_mode()?;
    let mut stdout = std::io::stdout();
    execute!(stdout, EnterAlternateScreen)?;
    let backend = ratatui::backend::CrosstermBackend::new(stdout);
    let mut terminal = Terminal::new(backend)?;

    // Extract inputs for editing
    let mut inputs: Vec<(usize, String)> = vec![]; // (item index, current value)
    for (i, it) in doc.items.iter().enumerate() {
        if let UiItem::Input { value, .. } = it {
            inputs.push((i, value.clone()));
        }
    }
    let mut focus_input: Option<usize> = if inputs.is_empty() { None } else { Some(0) };
    let mut status = String::from("Press Tab to change focus, Enter on buttons, q to quit.");

    loop {
        terminal.draw(|f| {
            let size = f.area();
            let chunks = Layout::default()
                .direction(Direction::Vertical)
                .constraints([
                    Constraint::Length(3),
                    Constraint::Min(1),
                    Constraint::Length(2),
                ])
                .split(size);

            let title = Paragraph::new(doc.title.clone())
                .block(Block::default().borders(Borders::ALL).title("Flux UI"));
            f.render_widget(title, chunks[0]);

            let mut lines: Vec<Line> = Vec::new();

            // Build display lines
            let mut input_cursor = 0usize;
            for (i, it) in doc.items.iter().enumerate() {
                match it {
                    UiItem::Text { text } => {
                        lines.push(Line::from(text.clone()));
                    }
                    UiItem::Button { label } => {
                        lines.push(Line::from(format!("[ {label} ]")));
                    }
                    UiItem::Input { label, .. } => {
                        let current_val = inputs.get(input_cursor).map(|x| x.1.clone()).unwrap_or_default();
                        let focused = focus_input == Some(input_cursor);
                        let prefix = if focused { "> " } else { "  " };
                        lines.push(Line::from(format!("{prefix}{label}: {current_val}")));
                        input_cursor += 1;
                    }
                }
            }

            let list = Paragraph::new(Text::from(lines))
                .block(Block::default().borders(Borders::ALL).title("Screen"));
            f.render_widget(list, chunks[1]);

            let status_p = Paragraph::new(status.clone())
                .block(Block::default().borders(Borders::ALL).title("Status"));
            f.render_widget(status_p, chunks[2]);
        })?;

        if event::poll(std::time::Duration::from_millis(16))? {
            if let Event::Key(k) = event::read()? {
                if k.kind != KeyEventKind::Press { continue; }

                match k.code {
                    KeyCode::Char('q') | KeyCode::Esc => break,
                    KeyCode::Tab => {
                        if let Some(fi) = focus_input {
                            if inputs.is_empty() { focus_input = None; }
                            else { focus_input = Some((fi + 1) % inputs.len()); }
                        } else if !inputs.is_empty() {
                            focus_input = Some(0);
                        }
                    }
                    KeyCode::Backspace => {
                        if let Some(fi) = focus_input {
                            if let Some((_idx, val)) = inputs.get_mut(fi) {
                                val.pop();
                            }
                        }
                    }
                    KeyCode::Enter => {
                        status = "Enter pressed (callbacks not implemented yet).".to_string();
                    }
                    KeyCode::Char(c) => {
                        if let Some(fi) = focus_input {
                            if let Some((_idx, val)) = inputs.get_mut(fi) {
                                val.push(c);
                            }
                        }
                    }
                    _ => {}
                }
            }
        }
    }

    // Restore terminal
    disable_raw_mode()?;
    execute!(terminal.backend_mut(), LeaveAlternateScreen)?;
    terminal.show_cursor()?;
    Ok(())
}

// -------------------- Window UI (egui) --------------------

fn run_window(doc: UiDoc) -> Result<(), Box<dyn std::error::Error>> {
    #[derive(Default)]
    struct AppState {
        doc: Option<UiDoc>,
        // store current editable values for inputs by index
        input_values: std::collections::HashMap<usize, String>,
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
