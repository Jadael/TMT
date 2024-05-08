import mido
from collections import defaultdict
import tkinter as tk
from tkinter import filedialog, ttk, scrolledtext

def find_free_voice(columns, start):
    # Find a column that is free from the step before the new note if needed
    for i in sorted(columns.keys()):
        if all(step < start for step, _, _, _ in columns[i]):
            return i
    return max(columns.keys(), default=-1) + 1

def midi_to_rhythml(midi_file_path, resolution=1, include_velocity=True):
    mid = mido.MidiFile(midi_file_path)
    results = []
    
    for track_idx, track in enumerate(mid.tracks):
        steps_per_beat = resolution
        ticks_per_step = mid.ticks_per_beat // steps_per_beat
        
        events = []
        current_time = 0
        for msg in track:
            current_time += msg.time
            if msg.type in ['note_on', 'note_off']:
                step = current_time // ticks_per_step
                events.append((step, msg.type, msg.note, msg.velocity))
        
        events.sort()
        active_notes = defaultdict(lambda: {'end_step': None, 'velocity': 0, 'last_pitch': None})
        columns = defaultdict(list)
        max_step = 0
        
        for step, msg_type, note, velocity in events:
            if msg_type == 'note_on' and velocity > 0:
                voice_col = find_free_voice(columns, step)
                active_notes[note] = {'start_step': step, 'end_step': None, 'velocity': velocity, 'voice_col': voice_col}
                note_pitch = f"m{note}"
                velocity_percent = f"{velocity/127 * 100:.2f}%"
                columns[voice_col].append((step, note_pitch, 'R', velocity_percent if include_velocity else ""))
            elif (msg_type == 'note_off' or (msg_type == 'note_on' and velocity == 0)) and note in active_notes:
                start_step = active_notes[note]['start_step']
                voice_col = active_notes[note]['voice_col']
                duration = step - start_step
                for i in range(1, duration + 1):
                    current_step = start_step + i
                    if current_step > max_step:
                        max_step = current_step
                    columns[voice_col].append((current_step, '', 'X', ''))
                active_notes[note]['end_step'] = step
        
        rhythml_output = []
        for step in range(max_step + 1):
            row = []
            for col in range(len(columns)):
                entries = [entry for entry in columns[col] if entry[0] == step]
                if entries:
                    _, pitch, gate, vel = entries[0]
                else:
                    pitch, gate, vel = '', '', ''
                row.append(f"{pitch}, {gate}, {vel}")
            rhythml_output.append(" , ".join(row))
        results.append((track.name if track.name else f"Track {track_idx}", "\n".join(rhythml_output)))
    
    return results

def open_file():
    filepath = filedialog.askopenfilename(filetypes=(("MIDI Files", "*.mid *.midi"), ("All Files", "*.*")))
    if filepath:
        try:
            resolution = int(resolution_entry.get()) if resolution_entry.get().isdigit() else 1
            include_velocity = velocity_var.get()
            results = midi_to_rhythml(filepath, resolution, include_velocity)
            # Clear existing tabs
            for tab in notebook.tabs():
                notebook.forget(tab)
            # Create a tab for each track
            for name, content in results:
                tab = ttk.Frame(notebook)
                notebook.add(tab, text=name[:30])  # Limit name length
                text_widget = scrolledtext.ScrolledText(tab, width=100, height=30)
                text_widget.pack(expand=True, fill=tk.BOTH)
                text_widget.insert(tk.END, content)
        except Exception as e:
            print(str(e))

app = tk.Tk()
app.title("MIDI to RhythML Converter")

open_button = tk.Button(app, text="Open MIDI File", command=open_file)
open_button.pack(pady=10)

settings_frame = tk.Frame(app)
settings_frame.pack(pady=10)

tk.Label(settings_frame, text="Resolution (steps per beat):").pack(side=tk.LEFT)
resolution_entry = tk.Entry(settings_frame, width=5)
resolution_entry.pack(side=tk.LEFT)
resolution_entry.insert(0, "1")  # Default resolution set to 1

velocity_var = tk.BooleanVar(value=True)
velocity_check = tk.Checkbutton(settings_frame, text="Include Velocity", variable=velocity_var)
velocity_check.pack(side=tk.LEFT)

notebook = ttk.Notebook(app)
notebook.pack(expand=True, fill=tk.BOTH, padx=10, pady=10)

app.mainloop()