import time
import psutil
from collections import deque
import numpy as np
from scipy.stats import entropy

# Configuration
WINDOW_SIZE = 100  # Sliding window for sequence (adjust for sensitivity)
SAMPLE_INTERVAL = 0.1  # Seconds between samples
BASELINE_ENTROPY = 1.0  # Initial threshold from benign runs; auto-tunes
TUNE_FACTOR = 1.2  # Threshold = baseline * TUNE_FACTOR

# Global counters for previous values (aggregate, perdisk=False for consistency)
prev_disk = psutil.disk_io_counters()  # Aggregate disk I/O
prev_swap = psutil.swap_memory()  # For RAM approximation via swap in/out

# Sliding window deque for I/O sequence: 0=disk read, 1=disk write, 2=RAM read, 3=RAM write
io_window = deque(maxlen=WINDOW_SIZE)
threshold = BASELINE_ENTROPY

def categorize_io(disk_curr, swap_curr):
    """
    Categorize dominant I/O type from current counters (aggregate).
    Deltas: read_bytes/write_bytes for disk; sin/sout for swap (RAM proxy).
    Bias to writes for instrumentation targeting (e.g., log tampering).
    """
    # Disk deltas
    disk_reads_delta = disk_curr.read_bytes - prev_disk.read_bytes
    disk_writes_delta = disk_curr.write_bytes - prev_disk.write_bytes
    
    # Swap deltas (approx RAM: swap in=RAM read from disk, swap out=RAM write to disk)
    ram_read_delta = swap_curr.sin - prev_swap.sin  # Swap in (load to RAM)
    ram_write_delta = swap_curr.sout - prev_swap.sout  # Swap out (evict from RAM)
    
    # Categorize based on dominant delta (prioritize disk, then RAM)
    if disk_writes_delta > disk_reads_delta * 2 and disk_writes_delta > 0:
        return 1  # Disk write (potential ETW log targeting)
    elif disk_reads_delta > 0:
        return 0  # Disk read
    elif ram_write_delta > 0:
        return 3  # RAM write
    else:
        return 2  # RAM read (default, including swap in)

def compute_io_entropy(sequence):
    """Shannon entropy on I/O sequence; high = anomalous randomness."""
    if len(sequence) < 2:
        return 0.0
    arr = np.array(sequence)
    unique, counts = np.unique(arr, return_counts=True)
    probs = counts / len(arr)
    return entropy(probs)

# Main realtime loop
print("Starting realtime I/O monitoring... Press Ctrl+C to stop.")
print("Monitoring for high-entropy patterns indicating Windows instrumentation targeting.")

try:
    while True:
        # Sample current counters (aggregate for system-wide)
        curr_disk = psutil.disk_io_counters()  # No perdisk=True to avoid dict issues
        curr_swap = psutil.swap_memory()
        
        # Categorize based on deltas
        io_type = categorize_io(curr_disk, curr_swap)
        
        # Append to window
        io_window.append(io_type)
        
        # Compute entropy if window full
        if len(io_window) == WINDOW_SIZE:
            current_entropy = compute_io_entropy(io_window)
            
            # Auto-tune threshold (rolling average of recent entropies)
            if len(io_window) > WINDOW_SIZE * 2:
                recent_window = list(io_window)[-WINDOW_SIZE // 2:]
                recent_entropy = compute_io_entropy(recent_window)
                threshold = recent_entropy * TUNE_FACTOR
            else:
                threshold = BASELINE_ENTROPY
            
            # Detection: High entropy + high disk write ratio
            arr = np.array(io_window)
            disk_ratio = np.sum(arr <= 1) / len(io_window) * 100  # % disk ops
            
            if current_entropy > threshold and disk_ratio > 60:
                print(f"[ALERT] Anomalous pattern at {time.strftime('%H:%M:%S')}: Entropy={current_entropy:.4f} (> {threshold:.4f}), Disk Ratio={disk_ratio:.2f}% - Possible ETW/WMI targeting!")
            else:
                print(f"Normal: Entropy={current_entropy:.4f}, Threshold={threshold:.4f}, Disk Ratio={disk_ratio:.2f}%")
        
        # Update prev for next iteration
        prev_disk = curr_disk
        prev_swap = curr_swap
        
        time.sleep(SAMPLE_INTERVAL)
        
except KeyboardInterrupt:
    print("\nMonitoring stopped.")
    if io_window:
        final_entropy = compute_io_entropy(io_window)
        print(f"Final window entropy: {final_entropy:.4f}")

except Exception as e:
    print(f"Error during monitoring: {e}")
