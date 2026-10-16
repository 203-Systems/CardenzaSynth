#include "Applications/Synth/UI/Juno/JunoUiRenderer.h"

#include "Applications/Synth/UI/Render/SynthUiRenderPrimitives.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace synth::ui_render {

namespace {

float JunoNormalizedContinuous(int value)
{
  return Clamp01(static_cast<float>(std::clamp(value, 0, 127)) / 127.0f);
}

ui::Color GraphColor(const UiScreenSnapshot& snapshot, const char* parameterId, float alpha)
{
  return FadeColor(snapshot.focusItemId == parameterId ? kAccent : kDim, alpha);
}

void DrawJunoEnvGraphic(ui::Canvas& canvas, const ui::Rect& rect, const UiScreenSnapshot& snapshot, float alpha)
{
  const int left = rect.x;
  const int right = rect.x + rect.width - 1;
  const int top = rect.y;
  const int bottom = rect.y + rect.height - 1;
  const int width = std::max(20, right - left);
  const int attackWidth = std::max(2, static_cast<int>(JunoNormalizedContinuous(snapshot.junoEnvAttack) * width * 0.24f));
  const int decayWidth = std::max(2, static_cast<int>(JunoNormalizedContinuous(snapshot.junoEnvDecay) * width * 0.24f));
  const int releaseWidth = std::max(2, static_cast<int>(JunoNormalizedContinuous(snapshot.junoEnvRelease) * width * 0.24f));
  const int sustainStartX = std::min(right - releaseWidth - 4, left + attackWidth + decayWidth);
  const int sustainY = bottom - static_cast<int>(JunoNormalizedContinuous(snapshot.junoEnvSustain) * static_cast<float>(bottom - top));

  canvas.FillRect(MakeRect(left, bottom, rect.width, 1), FadeColor(kRule, alpha));
  canvas.DrawLine(left, bottom, left + attackWidth, top, GraphColor(snapshot, "env.attack", alpha));
  canvas.DrawLine(left + attackWidth, top, sustainStartX, sustainY, GraphColor(snapshot, "env.decay", alpha));
  canvas.DrawLine(sustainStartX, sustainY, right - releaseWidth, sustainY, GraphColor(snapshot, "env.sustain", alpha));
  canvas.DrawLine(right - releaseWidth, sustainY, right, bottom, GraphColor(snapshot, "env.release", alpha));
}

void DrawJunoFilterGraphic(ui::Canvas& canvas, const ui::Rect& rect, const UiScreenSnapshot& snapshot, float alpha)
{
  const int left = rect.x;
  const int right = rect.x + rect.width - 1;
  const int bottom = rect.y + rect.height - 1;
  const int baseY = rect.y + rect.height / 2;
  const int cutoffX = left + 8 + static_cast<int>(JunoNormalizedContinuous(snapshot.junoVcfCutoff) * static_cast<float>(std::max(1, right - left - 16)));
  const int peakY = baseY - static_cast<int>(JunoNormalizedContinuous(snapshot.junoVcfResonance) * static_cast<float>(baseY - rect.y));
  const ui::Color cutoff = GraphColor(snapshot, "vcf.cutoff", alpha);
  const ui::Color resonance = GraphColor(snapshot, "vcf.resonance", alpha);

  canvas.FillRect(MakeRect(left, bottom, rect.width, 1), FadeColor(kRule, alpha));
  canvas.DrawLine(left, baseY, cutoffX - 5, baseY, cutoff);
  canvas.DrawLine(cutoffX - 5, baseY, cutoffX, peakY, resonance);
  canvas.DrawLine(cutoffX, peakY, cutoffX + 5, baseY, resonance);
  canvas.DrawLine(cutoffX + 5, baseY, std::min(right, cutoffX + 5 + (bottom - baseY) * 2), bottom, cutoff);
}

void DrawJunoLfoGraphic(ui::Canvas& canvas, const ui::Rect& rect, const UiScreenSnapshot& snapshot, float alpha)
{
  const int left = rect.x;
  const int right = rect.x + rect.width - 1;
  const int top = rect.y;
  const int bottom = rect.y + rect.height - 1;
  const int middleY = (top + bottom) / 2;
  const float cycleWidth = 34.0f - JunoNormalizedContinuous(snapshot.junoLfoRate) * 22.0f;
  const int delayX = left + static_cast<int>(JunoNormalizedContinuous(snapshot.junoLfoDelay) * static_cast<float>(right - left) * 0.5f);
  const ui::Color wave = GraphColor(snapshot, "lfo.rate", alpha);
  const float maxAmplitude = static_cast<float>(middleY - top);

  canvas.FillRect(MakeRect(left, middleY, std::max(0, delayX - left), 1), FadeColor(kRule, alpha));
  // The sine fades in after the delay point, like the Juno's LFO delay knob.
  int previousY = middleY;
  for (int x = delayX; x <= right; ++x)
  {
    const float phase = static_cast<float>(x - delayX) / cycleWidth;
    const float ramp = Clamp01(phase / 1.5f);
    const int y = middleY - static_cast<int>(std::lround(std::sin(phase * 6.2831853f) * maxAmplitude * ramp));
    canvas.DrawLine(x == delayX ? x : x - 1, previousY, x, y, wave);
    previousY = y;
  }
  canvas.FillRect(MakeRect(delayX, top, 1, bottom - top + 1), GraphColor(snapshot, "lfo.delay", alpha));
}

ui::Rect RowsRect(const ui::Canvas& canvas, const ui::Rect& rect, std::size_t firstRow, std::size_t lastRow)
{
  const ui::Rect first = KeyValueEntryRect(rect, canvas.Height(), 1, firstRow, 0);
  const ui::Rect last = KeyValueEntryRect(rect, canvas.Height(), 1, lastRow, 0);
  return MakeRect(rect.x, first.y + 8, rect.width, last.y + last.height - first.y - 16);
}

}  // namespace

bool IsJunoParameterSnapshot(const UiScreenSnapshot& snapshot)
{
  return snapshot.mode == UiMode::ContextMenu && snapshot.modifier == UiModifier::Parameter &&
         snapshot.engineKind == SynthEngineKind::Juno;
}

void DrawJunoParameterContent(ui::Canvas& canvas, const ui::Rect& rect, const UiScreenSnapshot& snapshot, float alpha)
{
  if (alpha <= 0.01f || rect.width <= 0 || rect.height <= 0)
  {
    return;
  }

  const int top = DrawPageHeader(canvas, rect, snapshot.title, snapshot.meta, kAccent, kDim, alpha);
  if (snapshot.contentStyle == UiContentStyle::Adjust)
  {
    DrawAdjustValue(canvas, MakeRect(rect.x, top, rect.width, rect.y + rect.height - top), snapshot.primaryText,
                    snapshot.secondaryText, snapshot.valueLevel, alpha);
    return;
  }

  DrawKeyValues(canvas, rect, snapshot.rightBarColumns, snapshot.rightBarItems, alpha);
  const std::string& group = snapshot.focusItemId;
  if (group.empty())
  {
    // Section overview: the ENV key previews its shape instead of four numbers.
    const ui::Rect entry = KeyValueEntryRect(rect, canvas.Height(), 2, 2, 1);
    const int inkTop = entry.y + (entry.height - InkHeight(1)) / 2;
    DrawInk(canvas, entry.x, inkTop, "ENV", FadeColor(kDim, alpha), 1);
    DrawJunoEnvGraphic(canvas, MakeRect(entry.x + 24, inkTop - 1, entry.width - 24, InkHeight(1) + 2), snapshot, alpha);
  }
  else if (group == "par.juno.group.env")
  {
    DrawJunoEnvGraphic(canvas, RowsRect(canvas, rect, 2, 3), snapshot, alpha);
  }
  else if (group == "par.juno.group.vcf")
  {
    DrawJunoFilterGraphic(canvas, RowsRect(canvas, rect, 2, 3), snapshot, alpha);
  }
  else if (group == "par.juno.group.lfo")
  {
    DrawJunoLfoGraphic(canvas, RowsRect(canvas, rect, 2, 3), snapshot, alpha);
  }
}

}  // namespace synth::ui_render
