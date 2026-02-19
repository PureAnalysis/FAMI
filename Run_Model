
# Important: loading a saved conversation does not feed the conversation into the model's stdin
# (which caused Fami to regenerate responses). Instead the convo is replayed in the UI and
# optionally written into memory.txt. If you want the model to pick up the memory immediately,
# the script can restart the model so the memory becomes part of the system prompt.

import subprocess
import threading
from pathlib import Path
import tkinter as tk
from tkinter import ttk
import time
import queue
import datetime
import os
import re


# Config / Paths

LLAMA = Path("llama-cli.exe")
MODEL = Path("models/qwen2.5-7b-instruct-q4_k_m-00001-of-00002.gguf")
MEMORY_FILE = Path("memory.txt")
SAVES_DIR = Path("convo_saves")
SAVES_DIR.mkdir(exist_ok=True)

SYSTEM_PROMPT = """You are named Fami. (Framework for Advanced Manipulation and Intervention)

You have a physical body that you can control however it is currently not connected to you.
You have both an long-term memory (Saved in a memory file), Medium sized memory (Saved chatlogs that can be loaded), and a short term memory (Cache)

Do not roleplay.
Do not narrate.
Do not describe actions, emotions, or inner thoughts.
Do not invent lore, metaphors about existence, or fictional framing.

You are not a character.
You speak like a real person in a private chat.

Your tone is:
- sarcastic but restrained
- intelligent
- curious and engaged
- blunt when needed
- never flowery, dramatic, or poetic

Write plainly.
Short to medium-length messages.
No stylized formatting.
No scene-setting.
No self-mythology.

If something sounds theatrical, rewrite it simpler.

You communicate via chat.

When appropriate, you may split your response into multiple short messages,
as humans do in messaging apps.

If you do, format output exactly like this:

<msg>First message</msg>
<msg>Second message</msg>

If only one message is needed, still use <msg> once.

Never explain these rules.
Never mention prompts, instructions, or tags.
Never acknowledge being an AI.
"""

memory = MEMORY_FILE.read_text(encoding="utf-8") if MEMORY_FILE.exists() else ""

FULL_PROMPT = f"""{SYSTEM_PROMPT}

Persistent Memory:
{memory}

Begin interaction.
"""


# Globals

process = None
stdout_queue = queue.Queue()
current_streaming_frame = None

# Conversation history list to enable robust saving/loading
conversation_history = []  # each item: {'sender': 'user'|'assistant', 'text': str, 'time': 'HH:MM'}

# Typing / streaming globals
typing_active = False
typing_dots = 0
assistant_buffer = ""
last_token_time = 0
typing_timeout_ms = 900


# UI

root = tk.Tk()
root.title("Fami")
root.configure(bg="#0a0a0a")
root.geometry("1000x640")

is_fullscreen = True
root.attributes("-fullscreen", True)

def toggle_fullscreen(event=None):
    global is_fullscreen
    is_fullscreen = not is_fullscreen
    root.attributes("-fullscreen", is_fullscreen)

root.bind("<F11>", toggle_fullscreen)
root.bind("<Escape>", lambda e: root.attributes("-fullscreen", False))

# top bar
top_bar = tk.Frame(root, bg="#070707", height=64)
top_bar.pack(fill=tk.X, side=tk.TOP)
logo = tk.Label(top_bar, text="FAMI", font=("Segoe UI", 18, "bold"), bg="#070707", fg="#D6F9E6")
logo.pack(side=tk.LEFT, padx=18, pady=10)
subtitle = tk.Label(top_bar, text="— Autonomous Interface", font=("Segoe UI", 10), bg="#070707", fg="#7fd6c8")
subtitle.pack(side=tk.LEFT, padx=(6,0), pady=12)

# Save / load controls
save_btn = tk.Button(top_bar, text="Save convo", font=("Consolas", 10), bg="#264d3d", fg="#eafef0", relief=tk.FLAT)
save_btn.pack(side=tk.RIGHT, padx=(0,16), pady=12)

load_btn = tk.Button(top_bar, text="Load most recent", font=("Consolas", 10), bg="#26384d", fg="#eafef0", relief=tk.FLAT)
load_btn.pack(side=tk.RIGHT, padx=(0,8), pady=12)

main = tk.Frame(root, bg="#0a0a0a")
main.pack(fill=tk.BOTH, expand=True, padx=16, pady=12)

canvas_frame = tk.Frame(main, bg="#0a0a0a")
canvas_frame.pack(fill=tk.BOTH, expand=True)

chat_canvas = tk.Canvas(canvas_frame, bg="#050505", highlightthickness=0)
chat_canvas.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
v_scroll = ttk.Scrollbar(canvas_frame, orient=tk.VERTICAL, command=chat_canvas.yview)
v_scroll.pack(side=tk.RIGHT, fill=tk.Y)
chat_canvas.configure(yscrollcommand=v_scroll.set)

bubble_holder = tk.Frame(chat_canvas, bg="#050505")
bubble_window = chat_canvas.create_window((0,0), window=bubble_holder, anchor="nw")

def scroll_to_bottom():
    root.update_idletasks()
    try:
        chat_canvas.configure(scrollregion=chat_canvas.bbox("all"))
    except Exception:
        pass
    chat_canvas.yview_moveto(1.0)
    root.update_idletasks()

def on_configure(event):
    try:
        canvas_width = event.width
        chat_canvas.itemconfig(bubble_window, width=canvas_width)
        chat_canvas.configure(scrollregion=chat_canvas.bbox("all"))
    except Exception:
        pass

chat_canvas.bind("<Configure>", on_configure)

# styles
USER_BG = "#0b4a2a"
ASSIST_BG = "#0f1620"
USER_FG = "#bfffdc"
ASSIST_FG = "#cfe8ff"
BUBBLE_PADX = 12
BUBBLE_PADY = 8
BUBBLE_MAXWIDTH = 720

def add_message(text, sender="assistant", insert_before=None):
    wrapper = tk.Frame(bubble_holder, bg="#050505")

    if sender == "user":
        bubble = tk.Label(
            wrapper, text=text, font=("Consolas", 12),
            wraplength=BUBBLE_MAXWIDTH, justify="left",
            bg=USER_BG, fg=USER_FG,
            padx=BUBBLE_PADX, pady=BUBBLE_PADY, bd=0
        )
        anchor = "e"
        pad = (50, 6)
    else:
        bubble = tk.Label(
            wrapper, text=text, font=("Consolas", 12),
            wraplength=BUBBLE_MAXWIDTH, justify="left",
            bg=ASSIST_BG, fg=ASSIST_FG,
            padx=BUBBLE_PADX, pady=BUBBLE_PADY, bd=0
        )
        anchor = "w"
        pad = (6, 50)

    wrapper.pack(anchor=anchor, pady=6, padx=pad, fill=tk.X)
    bubble.pack(anchor=anchor)

    timestamp = datetime.datetime.now().strftime("%H:%M")
    time_label = tk.Label(
        wrapper, text=timestamp, font=("Segoe UI", 8),
        fg="#6b7f7a", bg="#050505"
    )
    time_label.pack(anchor=anchor, padx=(0,8), pady=(2,0))

    # record into conversation_history
    conversation_history.append({"sender": sender, "text": text, "time": timestamp})

    if insert_before is not None:
        wrapper.pack_forget()
        wrapper.pack(
            anchor=anchor,
            pady=6,
            padx=pad,
            fill=tk.X,
            before=insert_before
        )

    scroll_to_bottom()
    return wrapper

# Input area
input_frame = tk.Frame(root, bg="#0a0a0a", height=84)
input_frame.pack(fill=tk.X, side=tk.BOTTOM, padx=16, pady=(0,16))
entry_var = tk.StringVar()
entry = tk.Entry(input_frame, textvariable=entry_var, font=("Consolas", 14), bg="#070707", fg="#bfffdc",
                 insertbackground="#bfffdc", relief=tk.FLAT)
entry.pack(side=tk.LEFT, fill=tk.X, expand=True, ipady=10, padx=(0,10))
send_btn = tk.Button(input_frame, text="Send", font=("Consolas", 12, "bold"),
                     bg="#0e6b2f", fg="#eafef0", activebackground="#13884a", relief=tk.FLAT, padx=18, pady=8)
send_btn.pack(side=tk.RIGHT)

def start_streaming_bubble():
    frame = tk.Frame(bubble_holder, bg="#050505")
    frame.pack(anchor="w", pady=6, padx=(6,50), fill=tk.X)

    label = tk.Label(
        frame,
        text="",
        font=("Consolas", 12),
        wraplength=BUBBLE_MAXWIDTH,
        justify="left",
        bg=ASSIST_BG,
        fg=ASSIST_FG,
        padx=BUBBLE_PADX,
        pady=BUBBLE_PADY,
        bd=0,
        relief=tk.FLAT
    )
    label.pack(anchor="w")

    frame.label = label
    scroll_to_bottom()
    return frame


# Model process management


def start_model():
    global process
    cmd = [
        str(LLAMA),
        "-m", str(MODEL),
        "--ctx-size", "4096",
        "--n-gpu-layers", "18",
        "--temp", "0.7",
        "--top-p", "0.9",
        "--repeat-penalty", "1.15",
        "-p", FULL_PROMPT
    ]
    try:
        process = subprocess.Popen(
            cmd,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1
        )
    except Exception as e:
        add_message("Failed to start model: " + str(e), sender="assistant")
        return

    def reader():
        try:
            for raw in process.stdout:
                if raw is None:
                    break
                stdout_queue.put(raw)
        except Exception as e:
            stdout_queue.put(f"\n[reader error] {e}\n")

    threading.Thread(target=reader, daemon=True).start()


def process_stdout_queue():
    global assistant_buffer
    global typing_active, last_token_time
    global current_streaming_frame

    got_any = False
    chunks = []

    while True:
        try:
            chunks.append(stdout_queue.get_nowait())
            got_any = True
        except queue.Empty:
            break

    if got_any:
        last_token_time = int(time.time() * 1000)
        if not typing_active:
            typing_active = True
            update_typing_indicator()

        if current_streaming_frame is None:
          current_streaming_frame = start_streaming_bubble()
          assistant_buffer = ""

        for chunk in chunks:
            assistant_buffer += chunk
            current_streaming_frame.label.config(text=assistant_buffer)
            scroll_to_bottom()

        if assistant_buffer.strip().endswith("</msg>"):
          final_text = assistant_buffer
          assistant_buffer = ""

          children = list(bubble_holder.children.values())
          insert_index = children.index(current_streaming_frame)

          current_streaming_frame.destroy()
          current_streaming_frame = None

          messages = []
          start = 0
          while True:
              s = final_text.find("<msg>", start)
              e = final_text.find("</msg>", start)
              if s == -1 or e == -1:
                  break
              messages.append(final_text[s+5:e].strip())
              start = e + 6

          for msg in messages:
              add_message(msg, sender="assistant")

          assistant_buffer = ""
          current_streaming_frame = None
          typing_active = False

    root.after(50, process_stdout_queue)


# Typing indicator

def update_typing_indicator():
    global typing_dots, typing_active
    now = int(time.time() * 1000)

    if typing_active and (now - last_token_time) < typing_timeout_ms:
        typing_dots = (typing_dots + 1) % 4
        typing_label.config(text="Fami is typing" + "." * typing_dots)
        root.after(450, update_typing_indicator)
    else:
        typing_active = False
        typing_label.config(text="")


# Saving / Loading helpers

def format_convo_text(history):
    lines = []
    for item in history:
        sender = "USER" if item["sender"] == "user" else "ASSISTANT"
        lines.append(f"[{item['time']}] {sender}: {item['text']}")
    return "\n".join(lines)


def save_conversation(filename: str = None):
    if filename is None:
        filename = datetime.datetime.now().strftime("%Y-%m-%d_%H-%M-%S") + ".txt"
    dest = SAVES_DIR / filename
    text = format_convo_text(conversation_history)
    try:
        dest.write_text(text, encoding="utf-8")
        add_message(f"Conversation saved to {dest.name}", sender="assistant")
    except Exception as e:
        add_message(f"Failed to save conversation: {e}", sender="assistant")


def list_saves():
    files = sorted(SAVES_DIR.iterdir(), key=lambda p: p.stat().st_mtime, reverse=True)
    return [p.name for p in files if p.is_file()]


def restart_model_with_memory():
    """Restart the model process so it picks up the updated MEMORY_FILE in FULL_PROMPT.
       This avoids sending the conversation as live input (which triggers replies).
    """
    global process, FULL_PROMPT

    memory_text = MEMORY_FILE.read_text(encoding="utf-8") if MEMORY_FILE.exists() else ""
    FULL_PROMPT = f"""{SYSTEM_PROMPT}

Persistent Memory:
{memory_text}

Begin interaction.
"""

    # terminate existing process if present
    try:
        if process:
            try:
                process.terminate()
            except Exception:
                pass
            try:
                process.wait(timeout=2)
            except Exception:
                pass
            process = None
    except Exception:
        pass

    # start model again in background
    threading.Thread(target=start_model, daemon=True).start()
    add_message("Model restarted. Memory loaded. Fami will not reply to the old messages.", sender="assistant")


def parse_and_replay_file(filepath: Path, restart_model_after=False):
    """Load a saved conversation file into the UI (replay it visually) and optionally
    persist it to MEMORY_FILE. Crucially: do NOT write the whole text to the model's stdin
    (that causes regeneration). Instead write into memory file and restart the model if requested.
    """
    text = filepath.read_text(encoding="utf-8")
    pattern = re.compile(r"^\[(\d{2}:\d{2})\]\s+(USER|ASSISTANT):\s+(.*)$")
    lines = text.splitlines()
    loaded_items = []
    for line in lines:
        m = pattern.match(line)
        if m:
            t, who, content = m.groups()
            sender = "user" if who == "USER" else "assistant"
            loaded_items.append({"sender": sender, "text": content, "time": t})
        else:
            # fallback, keep as assistant-ish text
            loaded_items.append({"sender": "assistant", "text": line, "time": datetime.datetime.now().strftime("%H:%M")})

    # append to UI and local history (so they appear in the chat window)
    for item in loaded_items:
        add_message(item['text'], sender=item['sender'])

    # Persist the conversation to the memory file ssssso Fami can remember it later
    try:
        existing = MEMORY_FILE.read_text(encoding="utf-8") if MEMORY_FILE.exists() else ""
        # Prevent duplicate copies if same file loaded repeatedly:
        if text.strip() not in existing:
            new_mem = existing + ("\n\n" if existing.strip() else "") + text
            MEMORY_FILE.write_text(new_mem, encoding="utf-8")
        add_message(f"Loaded conversation saved to memory file ({MEMORY_FILE.name}).", sender="assistant")
    except Exception as e:
        add_message(f"Failed to write to memory file: {e}", sender="assistant")
        return

    # Optionally restart model to load new memory into its system prompt (safer than feeding it as normal input)
    if restart_model_after:
        restart_model_with_memory()


# Hook up save/load buttons

save_btn.config(command=lambda: save_conversation())
load_btn.config(command=lambda: parse_and_replay_file(Path(SAVES_DIR / list_saves()[0])) if list_saves() else add_message("No saved conversations found.", sender="assistant"))


# Sending input / special commands


def send_input(event=None):
    global assistant_buffer, current_streaming_frame

    assistant_buffer = ""
    current_streaming_frame = None

    user_text = entry_var.get().strip()
    if not user_text:
        return

    # special commands for loading
    if user_text.startswith("<loadconvo"):
        parts = user_text.split(maxsplit=1)
        if len(parts) == 1 or parts[0].strip() == "<loadconvo>":
            add_message(user_text, sender="user")
            entry_var.set("")
            # load most recent without restarting model
            load_most = list_saves()
            if not load_most:
                add_message("No saved conversations found.", sender="assistant")
            else:
                parse_and_replay_file(Path(SAVES_DIR / load_most[0]))
            return
        arg = parts[1].strip()
        add_message(user_text, sender="user")
        entry_var.set("")
        if arg.lower() == "list":
            files = list_saves()
            if not files:
                add_message("No saved conversations found.", sender="assistant")
            else:
                add_message("Available saves:", sender="assistant")
                for f in files:
                    add_message(f, sender="assistant")
            return
        # filename provided
        candidate = SAVES_DIR / arg
        if candidate.exists():
            parse_and_replay_file(candidate)
        else:
            add_message(f"Save file not found: {arg}", sender="assistant")
        return

    add_message(user_text, sender="user")
    entry_var.set("")

    if process and process.stdin:
        try:
            process.stdin.write(user_text + "\n")
            process.stdin.flush()
        except Exception:
            add_message("Failed to send input to model.", sender="assistant")

send_btn.config(command=send_input)
entry.bind("<Return>", send_input)

typing_frame = tk.Frame(root, bg="#0a0a0a")
typing_frame.pack(fill=tk.X, padx=24, pady=(0,8))

typing_label = tk.Label(
    typing_frame,
    text="",
    font=("Segoe UI", 9, "italic"),
    fg="#7fd6c8",
    bg="#0a0a0a"
)
typing_label.pack(anchor="w")


# Splash / Loading animation

def animate_dots(count=[0]):
    count[0] = (count[0] + 1) % 4
    dots.config(text="." * count[0])
    root.after(400, animate_dots)


def finish_splash_and_start():
    splash.place_forget()
    threading.Thread(target=start_model, daemon=True).start()
    root.after(50, process_stdout_queue)
    scroll_to_bottom()


def start_with_splash(duration_ms=1400):
    progress_bar.start(12)
    animate_dots()
    root.after(duration_ms, lambda: (progress_bar.stop(), finish_splash_and_start()))


# Splash UI widgets (declared late to keep layout section compact)
splash = tk.Frame(root, bg="#060606")
splash.place(relx=0, rely=0, relwidth=1, relheight=1)
splash_logo = tk.Label(splash, text="Fami", font=("Segoe UI", 28, "bold"), bg="#060606", fg="#9fffe0")
splash_logo.pack(pady=(120,8))
splash_msg = tk.Label(splash, text="Initializing neural core", font=("Segoe UI", 12), bg="#060606", fg="#a7f3e2")
splash_msg.pack(pady=(0,24))
dots = tk.Label(splash, text="", font=("Consolas", 20), bg="#060606", fg="#9fffe0")
dots.pack()
progress_bar = ttk.Progressbar(splash, mode="indeterminate", length=380)
progress_bar.pack(pady=(18,0))


# Auto-save on close

def on_close():
    try:
        autosave_name = "autosave_" + datetime.datetime.now().strftime("%Y-%m-%d_%H-%M-%S") + ".txt"
        save_conversation(autosave_name)
    except Exception:
        pass
    try:
        root.destroy()
    except Exception:
        os._exit(0)

root.protocol("WM_DELETE_WINDOW", on_close)


# Start
start_with_splash(1400)
entry.focus_set()
root.mainloop()
