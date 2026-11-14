#!/usr/bin/env python3
#
# Copyright 2012 Emilie Gillet.
#
# Author: Emilie Gillet (emilie.o.gillet@gmail.com)
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#
# See http://creativecommons.org/licenses/MIT/ for more information.
#
# -----------------------------------------------------------------------------
#
# Braids' lookup tables, waveforms and wavetables: braids/resources/*.py and
# stmlib/tools/resources_compiler.py of pichenettes/eurorack in one Python 3
# script (CardenzaSynth, 2026-11-14). Changed from the originals:
#
# - the sample rate is an argument (the module runs at 96000 Hz, CardenzaSynth
#   renders at 44100 Hz);
# - only the tables the vendored DSP code reads are written (no strings, no
#   display characters, no lut_vco_detune / lut_env_expo /
#   lut_env_portamento_increments);
# - Python 3: integer divisions and indices are written as such.
#
# At 96000 Hz every table equals the one in upstream's resources.cc
# (--compare checks a resources.cc against what this script computes).
#
#   python3 Library/third_party/braids/resources/generate.py
#
# writes Library/third_party/braids/braids/resources.{cc,h} for 44100 Hz.

import argparse
import os
import re
import sys

import numpy

HEADER = """// Copyright 2012 Emilie Gillet.
//
// Author: Emilie Gillet (emilie.o.gillet@gmail.com)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// See http://creativecommons.org/licenses/MIT/ for more information.
//
// -----------------------------------------------------------------------------
//
// Resources definitions.
//
// Automatically generated for a sample rate of %(sample_rate)d Hz with:
// python3 Library/third_party/braids/resources/generate.py%(arguments)s
"""

HERE = os.path.dirname(os.path.abspath(__file__))


def lookup_tables(sample_rate):
  """braids/resources/lookup_tables.py."""
  tables = []
  tables_signed = []
  tables_32 = []

  excursion = 65536 * 65536.0

  # Create table for pitch.
  a4_midi = 69
  a4_pitch = 440.0
  highest_octave = 128
  notes = numpy.arange(
      highest_octave * 128.0,
      (highest_octave + 12) * 128.0 + 16,
      16)
  pitches = a4_pitch * 2 ** ((notes - a4_midi * 128) / (128 * 12))
  increments = excursion / sample_rate * pitches
  delays = sample_rate / pitches * 65536 * 4096

  tables_32.append(('oscillator_increments', increments.astype(numpy.int64)))
  tables_32.append(('oscillator_delays', delays.astype(numpy.int64)))

  # Resonator coefficients.
  cutoff = 440.0 * 2 ** ((numpy.arange(0, 129) - 69) / 12.0)
  f = cutoff / (sample_rate / 2)
  max_resonance = 0.99985
  f[f > 0.25] = 0.25
  bandpass_coeff_1 = -2 * numpy.cos(2 * numpy.pi * f)
  bandpass_coeff_gain = []

  for f in list(f):
    n = numpy.arange(2000)
    response = numpy.sin((n + 1) * 2 * numpy.pi * f) / numpy.sin(2 * numpy.pi * f)
    response *= max_resonance ** n
    response /= (2 * f) ** 0.5
    gain = numpy.abs(response).max()
    bandpass_coeff_gain.append(gain)

  bandpass_coeff_gain = numpy.maximum(
      1, numpy.minimum(256, 16384.0 / numpy.array(bandpass_coeff_gain)))

  tables.append(('resonator_coefficient', -bandpass_coeff_1 * 32768.0))
  tables.append(('resonator_scale', bandpass_coeff_gain))

  # SVF coefficients.
  cutoff = 440.0 * 2 ** ((numpy.arange(0, 257) - 69) / 12.0)
  f = cutoff / sample_rate
  f[f > 1 / 8.0] = 1 / 8.0
  f = 2 * numpy.sin(numpy.pi * f)
  resonance = numpy.arange(0, 257) / 260.0
  damp = numpy.minimum(2 * (1 - resonance ** 0.25),
                       numpy.minimum(2, 2 / f - f * 0.5))

  tables.append(('svf_cutoff', f * 32767.0))
  tables.append(('svf_damp', damp * 32767.0))
  tables.append(('svf_scale', ((damp / 2) ** 0.5) * 32767.0))

  # Envelope for granular synthesis.
  granular_envelope = list(numpy.hanning(257) * 32767.0)
  granular_envelope += [0] * 256
  tables.append(('granular_envelope', granular_envelope))

  granular_envelope_rate = 2 ** (numpy.arange(0, 257) / 64.0) * (1 << 14)
  tables.append(('granular_envelope_rate', granular_envelope_rate / 8))

  # Bowing envelope and friction curve.
  attack = numpy.linspace(0, 1, int(sample_rate * 0.025 / 4)) * 0.2 * 32768
  decay = numpy.linspace(1, 0.8, int(sample_rate * 0.005 / 4)) * 0.2 * 32768
  bowing_envelope = list(attack) + list(decay)
  # Add a guard to factor the border check out of the sample block loop
  bowing_envelope += [decay[-1]] * 32
  tables.append(('bowing_envelope', bowing_envelope))

  delta = numpy.arange(0, 257) / 64.0
  friction = 1 / ((numpy.abs(delta) + 0.75) ** 4)
  friction = numpy.minimum(friction, 1.0)
  tables.append(('bowing_friction', friction * 32768.0))

  attack = numpy.linspace(0, 1, int(sample_rate * 0.005 / 4)) * 1.3 * 16384
  decay = numpy.linspace(1, 0.8, int(sample_rate * 0.01 / 4)) * 1.3 * 16384
  blowing_envelope = list(attack) + list(decay)
  # Add a guard to factor the border check out of the sample block loop
  blowing_envelope += [decay[-1]] * 32
  tables.append(('blowing_envelope', blowing_envelope))

  delta = numpy.arange(0, 257) / 128.0
  jet = delta ** 3 - delta
  jet = numpy.minimum(jet, 1.0)
  tables_signed.append(('blowing_jet', jet * 32767.0))

  flute_body_filter = 4096 * numpy.minimum(
      0.7, 0.4 * 2 ** ((numpy.arange(0, 128) - 69) / 12.0))
  tables.append(('flute_body_filter', flute_body_filter))

  # Quantizer for FM frequencies.
  fm_frequency_ratios = [
      0.125, 0.25, 0.5, 0.5 * 2 ** (16 / 1200.0),
      numpy.sqrt(2) / 2, numpy.pi / 4, 1.0, 1.0 * 2 ** (16 / 1200.0), numpy.sqrt(2),
      numpy.pi / 2, 7.0 / 4, 2, 2 * 2 ** (16 / 1200.0), 9.0 / 4, 11.0 / 4,
      2 * numpy.sqrt(2), 3, numpy.pi, numpy.sqrt(3) * 2, 4, numpy.sqrt(2) * 3,
      numpy.pi * 3 / 2, 5, numpy.sqrt(2) * 4, 8]

  scale = []
  for ratio in fm_frequency_ratios:
    ratio = 256 * 12 * numpy.log2(ratio) + 16384
    scale.extend([ratio, ratio, ratio])

  target_size = int(2 ** numpy.ceil(numpy.log2(len(scale))))
  while len(scale) < target_size:
    gap = numpy.argmax(numpy.diff(scale))
    scale = scale[:gap + 1] + [(scale[gap] + scale[gap + 1]) / 2] + scale[gap + 1:]

  scale.append(scale[-1])
  tables.append(('fm_frequency_quantizer', scale))

  # Bell envelopes for VOSIM and FOF.
  def bell(size, ratio):
    n = size // ratio
    first_half = numpy.hanning(n * 2)[:n] * 65535
    r = size - n
    second_half = numpy.hanning(r * 2)[r:] * 65535
    return list(first_half) + list(second_half) + [0]

  tables.append(('bell', bell(256, 16)))

  return tables, tables_signed, tables_32


def waveforms(sample_rate):
  """braids/resources/waveforms.py."""
  result = []

  # Create amplitude modulated sine/square tables for formants.
  sine_samples = []
  square_samples = []
  sine = numpy.sin(numpy.arange(16.0) / 16.0 * 2 * numpy.pi)
  for i, x in enumerate(sine):
    gains = numpy.exp(0.184 * numpy.arange(16.0))
    gains[0] = 0
    values = gains * x * 4
    values = numpy.round(values).astype(int)
    amps = numpy.round(gains)
    if i >= 8:
      amps = -amps
    square_samples.extend(amps.astype(int))
    sine_samples.extend(values)

  result.extend([
      ('formant_sine', sine_samples),
      ('formant_square', square_samples)
  ])

  # Band-limited waveforms.
  wavetable_size = 256

  def dither(x, order=0, dtype=numpy.int16):
    for _ in range(order):
      x = numpy.hstack((numpy.zeros(1,), numpy.cumsum(x)))
    x = numpy.round(x)
    for _ in range(order):
      x = numpy.diff(x)
    if any(x < numpy.iinfo(dtype).min) or any(x > numpy.iinfo(dtype).max):
      print('Clipping occurred!', file=sys.stderr)
    x[x < numpy.iinfo(dtype).min] = numpy.iinfo(dtype).min
    x[x > numpy.iinfo(dtype).max] = numpy.iinfo(dtype).max
    return x.astype(dtype)

  def scale(array, low=-32766, high=32766, center=True, dither_level=2):
    array = numpy.array(array, dtype=float)
    if center:
      array -= array.mean()
    mx = numpy.abs(array).max()
    array = (array + mx) / (2 * mx)
    array = array * (high - low) + low
    return dither(array, order=dither_level)

  # Sine wave.
  sine = -numpy.sin(numpy.arange(wavetable_size + 1) / float(wavetable_size) *
                    2 * numpy.pi) * 127.5 + 127.5

  # Band limited waveforms.
  num_zones = 15
  quadrature = numpy.fmod(
      numpy.arange(wavetable_size + 1) + wavetable_size // 4,
      wavetable_size).astype(int)
  fill = numpy.fmod(
      numpy.arange(wavetable_size + 1),
      wavetable_size).astype(int)

  result.append(('sine', scale(sine[quadrature])))

  for zone in range(num_zones):
    f0 = 440.0 * 2.0 ** ((18 + 8 * zone - 69) / 12.0)
    if zone == num_zones - 1:
      f0 = sample_rate / 2.0 - 1
    else:
      f0 = min(f0, sample_rate / 2.0)
    period = sample_rate / f0
    m = 2 * numpy.floor(period / 2) + 1.0
    i = numpy.arange(-wavetable_size // 2, wavetable_size // 2) / float(wavetable_size)
    pulse = numpy.sin(numpy.pi * i * m) / (m * numpy.sin(numpy.pi * i) + 1e-9)
    pulse[wavetable_size // 2] = 1.0
    pulse = pulse[fill]
    result.append(('bandlimited_comb_%d' % zone, scale(pulse[quadrature])))

  return result


def waveshapers():
  """braids/resources/waveshapers.py."""

  def scale(x, low=-32766, high=32766, center=True):
    x = numpy.array(x, dtype=float)
    if center:
      x -= x.mean()
    mx = numpy.abs(x).max()
    x = (x + mx) / (2 * mx)
    x = x * (high - low) + low
    x = numpy.round(x)
    return x.astype(int)

  x = numpy.arange(0, 257) / 128.0 - 1.0
  x[-1] = x[-2]
  violent_overdrive = numpy.tanh(8.0 * x)
  moderate_overdrive = numpy.tanh(2.0 * x)

  tri_fold = numpy.sin(numpy.pi * (3 * x + (2 * x) ** 3))

  # In v1.4 RC
  window = numpy.exp(-x * x * 4) ** 1.5
  sine = numpy.sin(8 * numpy.pi * x)
  sine_fold = sine * window + numpy.arctan(3 * x) * (1 - window)
  sine_fold /= numpy.abs(sine_fold).max()

  return [
      ('moderate_overdrive', scale(moderate_overdrive)),
      ('violent_overdrive', scale(violent_overdrive)),
      ('sine_fold', scale(sine_fold, center=False)),
      ('tri_fold', scale(tri_fold)),
  ]


def wavetables():
  """The wavetable data of braids/resources/waveforms.py (braids/data/*.bin
  and the table it calls random_data)."""
  with open(os.path.join(HERE, 'data', 'waves.bin'), 'rb') as f:
    waves = list(f.read())
  with open(os.path.join(HERE, 'data', 'map.bin'), 'rb') as f:
    wave_map = list(f.read())
  with open(os.path.join(HERE, 'data', 'code.bin'), 'rb') as f:
    code = list(f.read())
  return [('waves', waves), ('map', wave_map), ('code', code)]


def resources(sample_rate):
  """Every table: (entries, table name, prefix, C type)."""
  tables, tables_signed, tables_32 = lookup_tables(sample_rate)
  return [
      (tables, 'lookup_table', 'LUT', 'uint16_t'),
      (tables_signed, 'lookup_table_signed', 'LUT', 'int16_t'),
      (tables_32, 'lookup_table_hr', 'LUT', 'uint32_t'),
      (waveforms(sample_rate), 'waveform', 'WAV', 'int16_t'),
      (waveshapers(), 'waveshaper', 'WS', 'int16_t'),
      (wavetables(), 'wt', 'WT', 'uint8_t'),
  ]


def integers(values):
  # As '%6d' % value did in resources_compiler.py: towards zero.
  return [int(value) for value in values]


def write_header(path, sample_rate, arguments, library):
  with open(path, 'w') as f:
    f.write(HEADER % {'sample_rate': sample_rate, 'arguments': arguments})
    f.write('\n\n#ifndef BRAIDS_RESOURCES_H_\n#define BRAIDS_RESOURCES_H_\n\n\n')
    f.write('#include "stmlib/stmlib.h"\n\n\n\nnamespace braids {\n\n')
    f.write('typedef uint8_t ResourceId;\n\n')
    for _, name, _, c_type in library:
      f.write('extern const %s* const %s_table[];\n\n' % (c_type, name))
    for entries, _, prefix, c_type in library:
      for key, _ in entries:
        f.write('extern const %s %s_%s[];\n' % (c_type, prefix.lower(), key))
    for entries, _, prefix, _ in library:
      for index, (key, values) in enumerate(entries):
        f.write('#define %s_%s %d\n' % (prefix, key.upper(), index))
        f.write('#define %s_%s_SIZE %d\n' % (prefix, key.upper(), len(values)))
    f.write('\n}  // namespace braids\n\n#endif  // BRAIDS_RESOURCES_H_\n')


def write_cc(path, sample_rate, arguments, library):
  with open(path, 'w') as f:
    f.write(HEADER % {'sample_rate': sample_rate, 'arguments': arguments})
    f.write('\n\n#include "braids/resources.h"\n\nnamespace braids {\n\n')
    for entries, name, prefix, c_type in library:
      for key, values in entries:
        values = integers(values)
        f.write('const %s %s_%s[] = {\n' % (c_type, prefix.lower(), key))
        for i in range(0, len(values), 4):
          f.write('  ')
          f.write(', '.join(
              '%6d' % value if value < 1 << 31 else '%6dUL' % value
              for value in values[i:i + 4]))
          f.write(',\n')
        f.write('};\n')
      f.write('\n\nconst %s* const %s_table[] = {\n' % (c_type, name))
      for key, _ in entries:
        f.write('  %s_%s,\n' % (prefix.lower(), key))
      f.write('};\n\n')
    f.write('\n}  // namespace braids\n')


def read_cc(path):
  """The arrays of a resources.cc, by name."""
  with open(path) as f:
    text = f.read()
  arrays = {}
  for match in re.finditer(r'const \w+ (\w+)\[\][^=]*= \{([^}]*)\};', text):
    numbers = re.findall(r'-?\d+', match.group(2).replace('UL', ''))
    if numbers and not match.group(1).endswith('_table'):
      arrays[match.group(1)] = [int(number) for number in numbers]
  return arrays


def compare(path, library):
  """Compares a resources.cc with the tables computed here; the number that
  differ."""
  theirs = read_cc(path)
  different = 0
  for entries, _, prefix, _ in library:
    for key, values in entries:
      name = '%s_%s' % (prefix.lower(), key)
      ours = integers(values)
      if name not in theirs:
        print('%-32s missing in %s' % (name, path))
        different += 1
      elif theirs[name] != ours:
        count = sum(1 for a, b in zip(ours, theirs[name]) if a != b)
        worst = max([abs(a - b) for a, b in zip(ours, theirs[name])] or [0])
        print('%-32s differs: %d / %d values, largest difference %d, sizes %d / %d' % (
            name, count, len(ours), worst, len(ours), len(theirs[name])))
        different += 1
      else:
        print('%-32s equal (%d values)' % (name, len(ours)))
  return different


def main():
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('--sample-rate', type=int, default=44100)
  parser.add_argument('--output', default=os.path.join(HERE, '..', 'braids'),
                      help='directory for resources.cc and resources.h')
  parser.add_argument('--compare', metavar='RESOURCES_CC',
                      help='compare with a resources.cc instead of writing')
  options = parser.parse_args()
  library = resources(options.sample_rate)
  if options.compare:
    return 1 if compare(options.compare, library) else 0
  arguments = '' if options.sample_rate == 44100 else ' --sample-rate %d' % options.sample_rate
  write_header(os.path.join(options.output, 'resources.h'), options.sample_rate, arguments, library)
  write_cc(os.path.join(options.output, 'resources.cc'), options.sample_rate, arguments, library)
  return 0


if __name__ == '__main__':
  sys.exit(main())
