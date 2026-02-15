import mido
import sounddevice as sd
import numpy as np
import time

def midi_note_to_frequency(note):
    """Convert MIDI note number to frequency in Hz"""
    return 440.0 * (2.0 ** ((note - 69) / 12.0))

def midi_note_to_name(note):
    """Convert MIDI note number to note name"""
    note_names = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B']
    octave = (note // 12) - 1
    note_name = note_names[note % 12]
    return f"{note_name}{octave}"

def midi_to_c_hex(midi_file, tempo_override=None):
    """Convert MIDI file to C hex array format"""
    mid = mido.MidiFile(midi_file)
    
    notes = []
    tempo = 500000  # Default tempo (microseconds per beat)
    ticks_per_beat = mid.ticks_per_beat
    
    # Extract notes and timing with proper duration tracking
    for track in mid.tracks:
        current_time = 0
        active_notes = {}  # Track note_on events with velocity
        
        for msg in track:
            current_time += msg.time
            
            # Update tempo if found
            if msg.type == 'set_tempo':
                tempo = msg.tempo
            
            # Track note_on events with velocity
            elif msg.type == 'note_on' and msg.velocity > 0:
                active_notes[msg.note] = (current_time, msg.velocity)
            
            # Calculate duration on note_off
            elif (msg.type == 'note_off') or (msg.type == 'note_on' and msg.velocity == 0):
                if msg.note in active_notes:
                    start_time, velocity = active_notes[msg.note]
                    duration_ticks = current_time - start_time
                    
                    # Apply tempo override if specified
                    effective_tempo = tempo
                    if tempo_override:
                        # Convert BPM to microseconds per beat
                        effective_tempo = int(60000000 / tempo_override)
                    
                    # Convert ticks to milliseconds using tempo
                    duration_ms = int((duration_ticks * effective_tempo) / (ticks_per_beat * 1000))
                    start_ms = int((start_time * effective_tempo) / (ticks_per_beat * 1000))
                    
                    freq = int(midi_note_to_frequency(msg.note))
                    note_name = midi_note_to_name(msg.note)
                    notes.append((start_ms, freq, duration_ms, note_name, velocity))
                    
                    del active_notes[msg.note]
    
    # Sort by start time
    notes.sort(key=lambda x: x[0])
    
    # Generate C code
    c_code = "// Generated MIDI data\n"
    c_code += f"// Tempo: {tempo} microseconds per beat\n"
    c_code += f"// Ticks per beat: {ticks_per_beat}\n\n"
    c_code += "struct Note {\n"
    c_code += "    unsigned int start_time;  // milliseconds\n"
    c_code += "    unsigned int frequency;   // Hz\n"
    c_code += "    unsigned int duration;    // milliseconds\n"
    c_code += "};\n\n"
    c_code += f"const struct Note melody[] = {{\n"
    
    for start, freq, dur, note_name, velocity in notes:
        c_code += f"    {{0x{start:04X}, 0x{freq:04X}, 0x{dur:04X}}},  // {note_name}: {start}ms, {freq}Hz, {dur}ms, vel={velocity}\n"
    
    c_code += "};\n\n"
    c_code += f"const int melody_length = {len(notes)};\n"
    c_code += f"const int tempo = {tempo};\n"
    
    return c_code, notes

def generate_nes_pluck(frequency, duration, sample_rate=44100, amplitude=0.4, velocity=100):
    """Generate a pluck square lead with release tail"""
    # Add release tail to total duration
    release_time = 0.15   # 150ms release tail
    total_duration = duration + release_time
    
    t = np.linspace(0, total_duration, int(sample_rate * total_duration), False)
    
    # Square wave (50% duty cycle)
    square = np.sign(np.sin(2 * np.pi * frequency * t))
    
    # Velocity scaling (MIDI velocity 0-127)
    velocity_scale = velocity / 127.0
    
    # Pluck envelope with release tail
    attack_time = 0.0   # 3ms very fast attack
    decay_time = 0.4    # 80ms decay
    sustain_level = 0.0   # Sustain at 40%
    
    attack_samples = int(attack_time * sample_rate)
    decay_samples = int(decay_time * sample_rate)
    sustain_samples = int(duration * sample_rate) - attack_samples - decay_samples
    release_samples = int(release_time * sample_rate)
    total_samples = len(square)
    
    envelope = np.ones(total_samples)
    idx = 0
    
    # Fast attack
    if attack_samples > 0:
        envelope[idx:idx+attack_samples] = np.linspace(0, 1, attack_samples)
        idx += attack_samples
    
    # Exponential decay to sustain
    if decay_samples > 0 and idx + decay_samples <= total_samples:
        decay_curve = np.exp(-4 * np.linspace(0, 1, decay_samples))
        decay_curve = sustain_level + (1 - sustain_level) * decay_curve
        envelope[idx:idx+decay_samples] = decay_curve
        idx += decay_samples
    
    # Sustain phase (hold at sustain level)
    if sustain_samples > 0 and idx + sustain_samples <= total_samples:
        envelope[idx:idx+sustain_samples] = sustain_level
        idx += sustain_samples
    
    # Release tail (exponential fade out from sustain level)
    if idx < total_samples:
        remaining = total_samples - idx
        release_curve = np.exp(-3 * np.linspace(0, 1, remaining))
        envelope[idx:] = sustain_level * release_curve
    
    return (square * envelope * amplitude * velocity_scale).astype(np.float32)

def play_notes(notes):
    """Play notes using sounddevice with pluck square lead synth"""
    if not notes:
        print("No notes to play")
        return
    
    print("Playing MIDI with Pluck Square Lead synth (streaming)...")
    
    sample_rate = 44100
    
    # Find total duration
    if notes:
        total_duration_ms = max(note[0] + note[2] for note in notes)
        total_samples = int((total_duration_ms / 1000.0) * sample_rate)
    else:
        return
    
    # Create audio buffer
    audio_buffer = np.zeros(total_samples, dtype=np.float32)
    
    # Generate all notes into buffer with velocity
    for start_ms, freq, duration, note_name, velocity in notes:
        if freq > 20 and freq < 20000:
            start_sample = int((start_ms / 1000.0) * sample_rate)
            wave = generate_nes_pluck(freq, duration / 1000.0, sample_rate, velocity=velocity)
            
            # Mix into buffer
            end_sample = min(start_sample + len(wave), total_samples)
            wave_len = end_sample - start_sample
            audio_buffer[start_sample:end_sample] += wave[:wave_len]
    
    # Normalize to prevent clipping
    max_val = np.max(np.abs(audio_buffer))
    if max_val > 1.0:
        audio_buffer = audio_buffer / max_val
    
    # Stream play
    print(f"Streaming {total_duration_ms/1000.0:.2f} seconds of audio...")
    sd.play(audio_buffer, sample_rate)
    sd.wait()
    
    print(f"Playback completed!")

if __name__ == "__main__":
    import sys
    import argparse
    
    parser = argparse.ArgumentParser(description='Convert MIDI to C hex code and play')
    parser.add_argument('midi_file', help='MIDI file to convert')
    parser.add_argument('--tempo', type=int, help='Override tempo in BPM (e.g., 120)')
    
    args = parser.parse_args()
    
    midi_file = args.midi_file
    
    print(f"Converting {midi_file}...")
    if args.tempo:
        print(f"Using custom tempo: {args.tempo} BPM")
        c_code, notes = midi_to_c_hex(midi_file, args.tempo)
    else:
        c_code, notes = midi_to_c_hex(midi_file)
    
    # Save C code
    output_file = midi_file.replace('.mid', '.h')
    with open(output_file, 'w') as f:
        f.write(c_code)
    
    print(f"C header saved to {output_file}")
    print(f"Total notes: {len(notes)}")
    
    # Play the melody
    play_notes(notes)
