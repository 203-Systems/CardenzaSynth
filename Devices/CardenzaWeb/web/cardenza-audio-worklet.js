// Cardenza output: plays mono frames that the page renders on the main thread (the wasm module
// lives there) and posts here. A ring buffer absorbs main-thread jitter; frames are resampled
// linearly from the synth rate (44.1 kHz) when the context runs at another rate.
//
// Messages in:  { samples: Float32Array }    frames at the source rate, in order
//               { reset: true }               drop everything buffered
// Messages out: { consumed, underruns }      totals in source frames, every few render quanta

class CardenzaOutputProcessor extends AudioWorkletProcessor {
  constructor(options) {
    super();
    const opts = (options && options.processorOptions) || {};
    this.step = (opts.sourceRate || sampleRate) / sampleRate;
    this.prefill = opts.prefillFrames || 1024;
    this.reportEvery = opts.reportEveryQuanta || 2;
    this.capacity = 1 << 16; // 65536 frames, about 1.5 s
    this.mask = this.capacity - 1;
    this.ring = new Float32Array(this.capacity);
    this.written = 0; // total source frames received
    this.position = 0; // fractional read position in total source frames
    this.playing = false;
    this.underruns = 0;
    this.quanta = 0;
    this.last = 0;
    this.port.onmessage = (event) => this.receive(event.data);
  }

  receive(data) {
    if (data.reset) {
      this.position = this.written;
      this.playing = false;
      return;
    }
    const samples = data.samples;
    if (!samples) return;
    const count = samples.length;
    for (let index = 0; index < count; index++) {
      this.ring[(this.written + index) & this.mask] = samples[index];
    }
    this.written += count;
    // Never let the writer lap the reader (only after a very long stall).
    if (this.written - this.position > this.capacity - 2) {
      this.position = this.written - this.prefill;
    }
  }

  process(inputs, outputs) {
    const output = outputs[0];
    const first = output[0];
    const frames = first.length;
    let index = 0;

    if (!this.playing && this.written - this.position >= this.prefill) this.playing = true;

    if (this.playing) {
      const ring = this.ring;
      const mask = this.mask;
      const step = this.step;
      let position = this.position;
      for (; index < frames; index++) {
        const base = Math.floor(position);
        if (base + 1 >= this.written) break;
        const frac = position - base;
        const a = ring[base & mask];
        first[index] = frac === 0 ? a : a + (ring[(base + 1) & mask] - a) * frac;
        position += step;
      }
      this.position = position;
      if (index > 0) this.last = first[index - 1];
      if (index < frames) {
        // Underrun: fade the last sample out instead of clicking, then refill before resuming.
        this.underruns++;
        this.playing = false;
      }
    }

    let tail = this.last;
    for (; index < frames; index++) {
      tail *= 0.96;
      first[index] = tail;
    }
    this.last = tail;

    for (let channel = 1; channel < output.length; channel++) output[channel].set(first);

    if (++this.quanta >= this.reportEvery) {
      this.quanta = 0;
      this.port.postMessage({ consumed: Math.floor(this.position), underruns: this.underruns });
    }
    return true;
  }
}

registerProcessor('cardenza-output', CardenzaOutputProcessor);
