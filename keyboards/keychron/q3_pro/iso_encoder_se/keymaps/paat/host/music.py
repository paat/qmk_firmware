"""Music view: the keyboard is a spectrum analyser of what the PC plays, one
bar per frequency band, bass at the left. The other RGB devices show the bass
beat only: a dim glow in the keyboard's colours that flashes on every beat.

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

BASS_HZ = 40, 150        # where the beat is looked for
BASS_WINDOW = 2048       # samples: a longer look, or a steady bass note flickers
BASS_NEAR_DB = 12        # bass this far below the loudest band is no beat
BEAT_LOOK = 5            # updates (1/6 s) back in which the bass must have been lower
BEAT_DB = 8              # ... by this much: the bass coming in after a gap is a beat
BEAT_FULL_DB = 16        # ... and by this much it is a full one
BEAT_QUIET_DB = -24      # the loudest band counts as at least this loud
LOUDEST_FALL_DB = 0.02   # per update: how fast "the loudest band" forgets
BEAT_GAP = 8             # updates after a beat in which no new one starts (1/4 s)
BEAT_FALL = 0.07         # per update: how fast a beat fades (1/2 s from full)
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
        bass_bins = np.fft.rfftfreq(BASS_WINDOW, 1 / RATE)
        self.bass_window = np.hanning(BASS_WINDOW)
        self.bass = slice(int(np.searchsorted(bass_bins, BASS_HZ[0])), int(np.searchsorted(bass_bins, BASS_HZ[1])) + 1)
        self.bass_samples = np.zeros(BASS_WINDOW)
        self.bass_before = []      # bass level of the last BEAT_LOOK updates
        self.loudest = BEAT_QUIET_DB  # recent peak over all bands, as heard (no tilt)
        self.beat = 0.0  # 1 on a beat, fading to 0
        self.since_beat = BEAT_GAP

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

        self.bass_samples = np.concatenate((self.bass_samples[len(block):], block))
        bass_power = np.abs(np.fft.rfft(self.bass_samples * self.bass_window) / (BASS_WINDOW / 4)) ** 2
        bass = 10 * float(np.log10(bass_power[self.bass].mean() + 1e-12))
        self.loudest = max(self.loudest - LOUDEST_FALL_DB, float((db - self.tilt).max()), bass, BEAT_QUIET_DB)
        # A beat is the bass coming in: well above where it was a moment ago,
        # and among the loudest things playing. A bass that just stays is none.
        rise = bass - min(self.bass_before) if self.bass_before else 0
        self.bass_before = (self.bass_before + [bass])[-BEAT_LOOK:]
        self.since_beat += 1
        if rise >= BEAT_DB and bass > self.loudest - BASS_NEAR_DB and self.since_beat >= BEAT_GAP:
            strength = 0.6 + 0.4 * min((rise - BEAT_DB) / (BEAT_FULL_DB - BEAT_DB), 1)
            self.beat = max(strength, self.beat)
            self.since_beat = 0
        else:
            self.beat = max(self.beat - BEAT_FALL, 0)
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
                link.bars(bars)
                views.modulation = [DEVICE_FLOOR + (1 - DEVICE_FLOOR) * analyser.beat] * BANDS
    finally:
        views.modulation = None
        recorder.kill()
        recorder.wait()
