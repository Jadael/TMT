import wave
import os
import sys

def normalize_samples(samples):
    # Find the peak value in the samples to determine the scaling factor
    max_peak = max(abs(sample) for sample in samples)
    if max_peak == 0:
        return samples  # Prevent division by zero if the file is silent
    scale_factor = 4 / max_peak
    return [sample * scale_factor for sample in samples]
    
def normalize_and_center_samples(samples):
    # Calculate DC offset as the average of samples
    dc_offset = sum(samples) / len(samples)
    # Adjust samples to remove DC offset
    adjusted_samples = [sample - dc_offset for sample in samples]
    
    # Find the peak value in the adjusted samples for scaling
    max_peak = max(abs(sample) for sample in adjusted_samples)
    if max_peak == 0:
        return adjusted_samples  # Prevent division by zero if the file is silent
    scale_factor = 5 / max_peak  # Scale to fit -5V to +5V range
    return [sample * scale_factor for sample in adjusted_samples]

def get_samples(wav_file):
    # Determine the sample width (in bytes) and the number of frames
    sample_width = wav_file.getsampwidth()
    num_frames = wav_file.getnframes()
    
    # Read all frames
    frames = wav_file.readframes(num_frames)
    
    # Convert frames to sample values based on sample width
    if sample_width == 1:  # 8-bit unsigned samples
        samples = [int.from_bytes(frames[i:i+1], 'unsigned char') - 128 for i in range(len(frames))]
    elif sample_width == 2:  # 16-bit signed samples
        samples = [int.from_bytes(frames[i:i+2], 'little', signed=True) for i in range(0, len(frames), 2)]
    elif sample_width == 3:  # 24-bit signed samples
        samples = [int.from_bytes(b'\x00' + frames[i:i+3], 'little', signed=True) for i in range(0, len(frames), 3)]
    elif sample_width == 4:  # 32-bit signed samples
        samples = [int.from_bytes(frames[i:i+4], 'little', signed=True) for i in range(0, len(frames), 4)]
    else:
        raise ValueError("Unsupported sample width")
    
    return samples

def wav_to_txt(input_wav, output_txt):
    with wave.open(input_wav, 'r') as wav_file:
        samples = get_samples(wav_file)
        normalized_samples = normalize_and_center_samples(samples)
        with open(output_txt, 'w') as f:
            for sample in normalized_samples:
                f.write(f"{sample:.6f}\n")

def process_directory(directory):
    # Loop through all files in the directory
    for filename in os.listdir(directory):
        if filename.lower().endswith('.wav'):
            input_wav = os.path.join(directory, filename)
            output_txt = os.path.join(directory, f"{os.path.splitext(filename)[0]}.txt")
            wav_to_txt(input_wav, output_txt)
            print(f"Converted {input_wav} to {output_txt}")

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python script.py <directory>")
    else:
        process_directory(sys.argv[1])