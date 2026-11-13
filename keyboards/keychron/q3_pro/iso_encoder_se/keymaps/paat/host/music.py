"""Music view: the spectrum of what the PC plays, as bars, bass at the left,
with the beat on top: every beat lifts all the bars and pulses the other RGB
devices, which otherwise glow in the keyboard's colours.

Records the default sound output (`parec`, PipeWire or PulseAudio) and sends
one bar per frequency band about 30 times a second. Needs NumPy.
"""

import os
import select
import subprocess

import views

RATE = 24000             # Hz; covers the bands below
WINDOW = 1024            # samples per spectrum
HOP = 800                # new samples per update: 30 updates a second
BANDS = 9
LOW_HZ, HIGH_HZ = 45, 9000
RANGE_DB = 40            # a band this far below the recent peak shows nothing
SILENCE_DB = -70         # never treat anything quieter than this as the peak
PEAK_FALL_DB = 0.05      # per update: how fast the scale adapts to quieter music
BAR_FALL = 0.07          # per update: how fast a bar drops (of full height)
TILT_DB_PER_OCTAVE = 3   # music has less energy higher up; lift it to match

BASS_BANDS = 2           # the lowest bands carry the beat
BEAT_DB = 6              # bass rising this much in one update is a full beat
BEAT_AVERAGE = 0.96      # per update: memory of the bass average (about a second)
BEAT_FALL = 0.09         # per update: how fast a beat fades
BEAT_LIFT = 0.45         # how far a full beat lifts every bar (of full height)
DEVICE_FLOOR = 0.1       # brightness of the other devices between beats

class Analyser:
    """Turns successive blocks of samples into bar heights, 0..1 per band."""

    def __init__(self, np):
        self.np = np
        self.window = np.hanning(WINDOW)
        edges = np.geomspace(LOW_HZ, HIGH_HZ, BANDS + 1)
        bins = np.fft.rfftfreq(WINDOW, 1 / RATE)
        self.slices = []
        for low, high in zip(edges[:-1], edges[1:]):
            first = int(np.searchsorted(bins, low))
            last = max(int(np.searchsorted(bins, high)), first + 1)
            self.slices.append(slice(first, last))
        centres = np.sqrt(edges[:-1] * edges[1:])
        self.tilt = TILT_DB_PER_OCTAVE * np.log2(centres / centres[0])
        self.samples = np.zeros(WINDOW)
        self.peak = SILENCE_DB + RANGE_DB
        self.bars = np.zeros(BANDS)
        self.bass_average = SILENCE_DB
        self.bass_before = SILENCE_DB
        self.beat = 0.0  # 1 on a beat, fading to 0

    def feed(self, block):
        """`block`: HOP new samples, -1..1. Returns the bar heights."""
        np = self.np
        self.samples = np.concatenate((self.samples[len(block):], block))
        power = np.abs(np.fft.rfft(self.samples * self.window) / (WINDOW / 4)) ** 2
        db = 10 * np.log10(np.array([power[s].mean() for s in self.slices]) + 1e-12) + self.tilt
        # The loudest band of the recent past is full height.
        self.peak = max(self.peak - PEAK_FALL_DB, float(db.max()), SILENCE_DB + RANGE_DB)
        heights = np.clip((db - (self.peak - RANGE_DB)) / RANGE_DB, 0, 1)
        self.bars = np.maximum(heights, self.bars - BAR_FALL)

        bass = float(db[:BASS_BANDS].max())
        audible = bass > self.peak - RANGE_DB
        # A beat is the bass jumping up, while louder than it has been lately.
        punch = (bass - self.bass_before) / BEAT_DB if audible and bass > self.bass_average else 0
        self.bass_before = bass
        self.bass_average = BEAT_AVERAGE * self.bass_average + (1 - BEAT_AVERAGE) * max(bass, SILENCE_DB)
        self.beat = max(min(punch, 1), self.beat - BEAT_FALL, 0)
        return self.bars


def run(link, wanted):
    import numpy as np

    recorder = subprocess.Popen(
        ["parec", "--device=@DEFAULT_MONITOR@", "--format=s16le", f"--rate={RATE}", "--channels=1",
         "--latency-msec=30", "--client-name=keyboard rgb_sync"],
        stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, bufsize=0)
    try:
        analyser = Analyser(np)
        audio = recorder.stdout.fileno()
        pending = b""
        while wanted():
            # Never wait long for sound: the view may have been switched away.
            if not select.select([audio], [], [], 0.2)[0]:
                continue
            data = os.read(audio, 65536)
            if not data:
                raise OSError("parec stopped")
            pending += data
            bars = None
            while len(pending) >= HOP * 2:
                block, pending = pending[:HOP * 2], pending[HOP * 2:]
                bars = analyser.feed(np.frombuffer(block, dtype="<i2") / 32768)
            if bars is not None:
                beat = analyser.beat
                link.bars(np.minimum(bars + BEAT_LIFT * beat, 1))
                views.modulation = [DEVICE_FLOOR + (1 - DEVICE_FLOOR) * max(beat, 0.6 * float(b)) for b in bars]
    finally:
        views.modulation = None
        recorder.kill()
        recorder.wait()
