"""Music view: the keyboard is a spectrum analyser of what the PC plays, one
bar per frequency band, bass at the left. The other RGB devices show the beat
only: a dim glow in the keyboard's colours that flashes on every beat.

The beat comes from the bass. While the bass hits at a steady tempo the
flashes are locked to it: they come on the beat, also where a single kick is
missing or weak. Without a steady tempo there are no flashes.

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
BEAT_QUIET_DB = -24      # the loudest band counts as at least this loud
LOUDEST_FALL_DB = 0.02   # per update: how fast "the loudest band" forgets
DEVICE_FLOOR = 0.1       # brightness of the other devices between beats

UPDATES_PER_SECOND = RATE / HOP
TEMPO_BPM = 70, 180      # tempi looked for; outside it, half or double is found
TEMPO_USUAL_BPM = 125    # of two tempi that fit equally, the one nearer this wins
TEMPO_HISTORY = 256      # updates of bass hits the tempo is found in (8.5 s)
TEMPO_EVERY = 8          # updates between two looks at the tempo
TEMPO_LOCK = 0.3         # how regular the hits must be (0..1) to lock on
TEMPO_KEEP = 0.2         # ... and to stay locked
TEMPO_LOOKS = 4          # looks in a row that must agree to lock on, or to let go
TEMPO_SAME = 0.04        # tempi this close (4 %) are the same tempo
TEMPO_PHASE = 120        # updates (4 s) back in which the place of the beat is looked for
TEMPO_SILENT = 60        # updates (2 s) without a bass hit that end a lock
TEMPO_LEAD = 2.5         # updates a locked flash is sent early: the time from the
                         # sound being played to a light changing (about 80 ms)
TEMPO_FADE = 0.7         # part of a beat after which a flash is dark
TEMPO_BLUR = 0.25, 0.5, 1, 0.5, 0.25  # a hit counts a little before and after, too


class Tempo:
    """Finds a steady tempo in the bass hits and says when its beats are due."""

    def __init__(self, np):
        self.np = np
        self.hits = np.zeros(TEMPO_HISTORY)  # strength of the bass hit per update, newest last
        self.lags = np.arange(TEMPO_HISTORY)
        low, high = (60 * UPDATES_PER_SECOND / bpm for bpm in TEMPO_BPM[::-1])
        self.periods = np.arange(low, high, 0.05)  # updates per beat
        bpm = 60 * UPDATES_PER_SECOND / self.periods
        self.usual = np.exp(-0.5 * (np.log2(bpm / TEMPO_USUAL_BPM) / 1.0) ** 2)
        self.period = None  # updates per beat while locked
        self.due = 0.0      # updates until the next beat
        self.since = 0      # updates since the last one
        self.count = 0
        self.other = None   # a tempo heard that is not the locked one ...
        self.other_looks = 0  # ... and for how many looks in a row
        self.lost_looks = 0   # looks in a row the locked tempo was not heard
        self.regular = 0.0    # how well the hits fit the tempo, at the last look

    @property
    def bpm(self):
        return 60 * UPDATES_PER_SECOND / self.period if self.period else None

    def feed(self, hit):
        """`hit`: how much the bass rose in this update. True when a beat is due."""
        np = self.np
        self.hits = np.concatenate((self.hits[1:], [hit]))
        self.count += 1
        if self.count % TEMPO_EVERY == 0:
            self._look()
        if self.period is None:
            return False
        self.due -= 1
        self.since += 1
        if self.due > 0.5:
            return False
        self.due += self.period
        if self.since < 0.6 * self.period:
            return False  # the beat moved: no second flash right after the last
        self.since = 0
        return True

    def _look(self):
        np = self.np
        if not self.hits[-TEMPO_SILENT:].any():
            self.period, self.other, self.regular = None, None, 0.0
            return
        hits = np.convolve(self.hits, TEMPO_BLUR, "same")
        centred = hits - hits.mean()
        spectrum = np.fft.rfft(centred, 2 * TEMPO_HISTORY)
        alike = np.fft.irfft(np.abs(spectrum) ** 2)[:TEMPO_HISTORY]
        if alike[0] <= 0:
            return
        alike = alike / alike[0] * TEMPO_HISTORY / (TEMPO_HISTORY - self.lags)  # 1 = the same hits again

        def fit(periods, beats=4):
            """A tempo fits when the hits repeat after one, two, three ... beats."""
            return sum(np.interp(k * periods, self.lags, alike) for k in range(1, beats + 1)) / beats

        fits = fit(self.periods)
        best = int(np.argmax(fits * self.usual))
        heard, regular = float(self.periods[best]), float(fits[best])
        # More beats apart, the same hits tell the tempo more exactly.
        near = np.arange(0.97 * heard, 1.03 * heard, 0.01)
        heard = float(near[int(np.argmax(fit(near, 8)))])

        # A tempo heard for several looks in a row is one to lock on to.
        if regular >= TEMPO_LOCK and self.other and abs(heard - self.other) < TEMPO_SAME * self.other:
            self.other_looks += 1
        else:
            self.other, self.other_looks = (heard, 1) if regular >= TEMPO_LOCK else (None, 0)

        if self.period:
            self.regular = float(fit(np.array([self.period]))[0])
            self.lost_looks = 0 if self.regular >= TEMPO_KEEP else self.lost_looks + 1
            if self.lost_looks == 0 and (self.other_looks < TEMPO_LOOKS or
                                         abs(self.other - self.period) < TEMPO_SAME * self.period):
                # Still the same tempo: steer gently towards what is heard now.
                if self.other and abs(self.other - self.period) < TEMPO_SAME * self.period:
                    self.period += 0.3 * (heard - self.period)
                off = (self._due(hits, self.period) - self.due + self.period / 2) % self.period - self.period / 2
                self.due += 0.5 * off
                return
            if self.other_looks < TEMPO_LOOKS:
                if self.lost_looks >= TEMPO_LOOKS:
                    self.period = None
                return
        elif self.other_looks < TEMPO_LOOKS:
            return
        self.period, self.regular, self.lost_looks = self.other, regular, 0
        self.due = self._due(hits, self.period) % self.period

    def _due(self, hits, period):
        """Updates until the next beat of this tempo, from where its hits fall."""
        np = self.np
        agos = np.arange(0, period, 0.25)  # the latest beat was this many updates ago
        beats = np.arange(int(TEMPO_PHASE / period))
        places = TEMPO_HISTORY - 1 - agos[:, None] - beats[None, :] * period
        ago = float(agos[int(np.argmax(np.interp(places, self.lags, hits).sum(axis=1)))])
        return period - ago - TEMPO_LEAD


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
        self.bass_before = None    # bass level of the update before
        self.loudest = BEAT_QUIET_DB  # recent peak over all bands, as heard (no tilt)
        self.beat = 0.0  # 1 on a beat, fading to 0
        self.tempo = Tempo(np)

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
        # A hit is the bass rising while it is among the loudest things playing.
        near = bass > self.loudest - BASS_NEAR_DB
        before = bass if self.bass_before is None else self.bass_before
        self.bass_before = bass
        if self.tempo.feed(max(bass - before, 0) if near else 0):
            self.beat = 1.0
        else:
            beat = self.tempo.period or UPDATES_PER_SECOND / 2
            self.beat = max(self.beat - 1 / (TEMPO_FADE * beat), 0)
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
        shown_bpm = None
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
                bpm = analyser.tempo.bpm
                if (bpm is None) != (shown_bpm is None) or (bpm and abs(bpm - shown_bpm) >= 3):
                    views.log(f"music: {round(bpm)} BPM" if bpm else "music: no steady tempo")
                    shown_bpm = bpm
    finally:
        views.modulation = None
        recorder.kill()
        recorder.wait()
