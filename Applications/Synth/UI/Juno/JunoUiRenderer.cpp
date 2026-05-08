#include "Applications/Synth/UI/Juno/JunoUiRenderer.h"

#include "Applications/Synth/UI/Render/SynthUiRenderPrimitives.h"

#include <algorithm>
#include <array>
#include <string>
#include <utility>

namespace synth::ui_render {

namespace {

float JunoNormalizedContinuous(int value)
{
  return Clamp01(static_cast<float>(std::clamp(value, 0, 127)) / 127.0f);
}

bool IsJunoFocusedParameter(const UiScreenSnapshot& snapshot, const char* parameterId)
{
  return snapshot.focusItemId == parameterId;
}

bool IsJunoDiscreteValuePage(const UiScreenSnapshot& snapshot)
{
  return snapshot.focusItemId == "dco.range" || snapshot.focusItemId == "dco.saw";
}

std::string JunoRangeText(int value)
{
  static const std::array<const char*, 3> kRanges = {"16'", "8'", "4'"};
  return kRanges[static_cast<std::size_t>(std::clamp(value, 0, 2))];
}

std::string JunoChorusText(int value)
{
  static const std::array<const char*, 3> kModes = {"OFF", "I", "II"};
  return kModes[static_cast<std::size_t>(std::clamp(value, 0, 2))];
}

std::string JunoToggleText(int value)
{
  return value > 0 ? "ON" : "OFF";
}

void DrawJunoEnvGraphic(ui::Canvas& canvas,
                        const ui::Rect& rect,
                        const UiScreenSnapshot& snapshot,
                        float alpha,
                        bool showValues);

int JunoOverviewRowHeight(const ui::Canvas& canvas)
{
  return canvas.FontHeight(1) + 2;
}

ui::Rect JunoOverviewEnvRect(const ui::Canvas& canvas, const ui::Rect& rect)
{
  const int gap = 2;
  const int rowHeight = JunoOverviewRowHeight(canvas);
  const int lowerHeight = rowHeight * 3 + gap * 2;
  const int topHeight = std::max(24, rect.height - lowerHeight - 4);
  return MakeRect(rect.x + 2, rect.y, rect.width - 4, topHeight);
}

void DrawJunoEnvValueGrid(ui::Canvas& canvas,
                          const ui::Rect& rect,
                          const UiScreenSnapshot& snapshot,
                          float alpha)
{
  if (rect.width <= 0 || rect.height <= 0)
  {
    return;
  }

  const ui::Color accent = FadeColor(kAccent, alpha);
  const ui::Color text = FadeColor(kText, alpha);
  const std::array<std::pair<const char*, int>, 4> values = {{
      {"ATK", snapshot.junoEnvAttack},
      {"DEC", snapshot.junoEnvDecay},
      {"SUS", snapshot.junoEnvSustain},
      {"REL", snapshot.junoEnvRelease},
  }};
  const std::array<const char*, 4> ids = {"env.attack", "env.decay", "env.sustain", "env.release"};
  const int gap = 2;
  const int cellWidth = std::max(18, (rect.width - gap) / 2);
  const int cellHeight = std::max(12, (rect.height - gap) / 2);

  for (std::size_t index = 0; index < values.size(); ++index)
  {
    const int column = static_cast<int>(index % 2);
    const int row = static_cast<int>(index / 2);
    const ui::Rect cell = MakeRect(rect.x + column * (cellWidth + gap),
                                   rect.y + row * (cellHeight + gap),
                                   cellWidth,
                                   cellHeight);
    const bool focused = IsJunoFocusedParameter(snapshot, ids[index]);
    const ui::Color valueColor = focused ? accent : text;
    const ui::Rect textRect = MakeRect(cell.x + 2, cell.y + 1, std::max(0, cell.width - 4), std::max(0, cell.height - 2));
    const std::string valueText = std::string(values[index].first) + " " + std::to_string(values[index].second);
    const int textY = textRect.y + (textRect.height - canvas.FontHeight(1)) / 2;

    canvas.DrawTextClipped(textRect, textRect.x, textY, valueText, valueColor, 1);
  }
}

void DrawJunoEnvPage(ui::Canvas& canvas, const ui::Rect& rect, const UiScreenSnapshot& snapshot, float alpha)
{
  if (rect.width <= 0 || rect.height <= 0)
  {
    return;
  }

  const int gap = 4;
  const ui::Rect envRect = JunoOverviewEnvRect(canvas, rect);
  DrawJunoEnvGraphic(canvas, envRect, snapshot, alpha, false);

  const int gridTop = envRect.y + envRect.height + gap;
  const ui::Rect gridRect = MakeRect(rect.x,
                                     gridTop,
                                     rect.width,
                                     std::max(0, rect.y + rect.height - gridTop));
  DrawJunoEnvValueGrid(canvas, gridRect, snapshot, alpha);
}

void DrawJunoOverviewStrip(ui::Canvas& canvas,
                           const ui::Rect& rect,
                           const std::string& label,
                           const std::string& value,
                           ui::Color accent,
                           ui::Color divider,
                           ui::Color text,
                           ui::Color fill)
{
  (void)fill;
  (void)divider;
  if (rect.width <= 0 || rect.height <= 0)
  {
    return;
  }

  const int labelScale = 1;
  const int labelGap = 14;
  const int labelWidth = canvas.MeasureTextWidth(label, labelScale);
  const int valueX = rect.x + labelWidth + labelGap;
  const int valueWidth = std::max(0, rect.width - (valueX - rect.x));

  canvas.DrawText(rect.x, rect.y, TruncateText(canvas, label, 24, 1), accent, 1);
  canvas.DrawTextInRect(MakeRect(valueX, rect.y - 1, valueWidth, rect.height + 2),
                        TruncateText(canvas, value, std::max(0, valueWidth - 4), 1),
                        text,
                        1,
                        ui::TextAlign::Left,
                        ui::VerticalAlign::Middle);
}

void DrawJunoOverview(ui::Canvas& canvas, const ui::Rect& rect, const UiScreenSnapshot& snapshot, float alpha)
{
  const ui::Color accent = FadeColor(kAccent, alpha);
  const ui::Color divider = FadeColor(kDivider, alpha);
  const ui::Color text = FadeColor(kText, alpha);
  const ui::Color fill = FadeColor(kAccent, alpha * 0.12f);
  const int gap = 2;
  const int rowHeight = JunoOverviewRowHeight(canvas);
  const ui::Rect envRect = JunoOverviewEnvRect(canvas, rect);
  DrawJunoEnvGraphic(canvas, envRect, snapshot, alpha, false);

  const int lowerTop = envRect.y + envRect.height + gap;

  std::string osc = JunoRangeText(snapshot.junoDcoRange);
  osc += snapshot.junoDcoSaw > 0 ? " SAW" : " PWM";

  std::string filter = "C";
  filter += std::to_string(snapshot.junoVcfCutoff);
  filter += " R";
  filter += std::to_string(snapshot.junoVcfResonance);

  std::string mod = "E";
  mod += std::to_string(snapshot.junoVcfEnv);
  mod += " ";
  mod += snapshot.junoChorusMode == 0 ? "-" : JunoChorusText(snapshot.junoChorusMode);

  DrawJunoOverviewStrip(canvas,
                        MakeRect(rect.x, lowerTop, rect.width, rowHeight),
                        "OSC",
                        osc,
                        accent,
                        divider,
                        text,
                        fill);
  DrawJunoOverviewStrip(canvas,
                        MakeRect(rect.x, lowerTop + rowHeight + gap, rect.width, rowHeight),
                        "FIL",
                        filter,
                        accent,
                        divider,
                        text,
                        fill);
  DrawJunoOverviewStrip(canvas,
                        MakeRect(rect.x, lowerTop + (rowHeight + gap) * 2, rect.width, rowHeight),
                        "MOD",
                        mod,
                        accent,
                        divider,
                        text,
                        fill);
}

void DrawJunoDcoGraphic(ui::Canvas& canvas, const ui::Rect& rect, const UiScreenSnapshot& snapshot, float alpha)
{
  const ui::Color accent = FadeColor(kAccent, alpha);
  const ui::Color divider = FadeColor(kDivider, alpha);
  const ui::Color text = FadeColor(kText, alpha);
  const ui::Color fill = FadeColor(kAccent, alpha * 0.18f);
  const ui::Rect inner = InsetRect(rect, 2, 2);
  if (inner.width <= 0 || inner.height <= 0)
  {
    return;
  }

  const int panelGap = 6;
  const int rangeWidth = std::clamp(inner.width / 4, 34, 46);
  const int detailX = inner.x + rangeWidth + panelGap;
  const int detailWidth = std::max(0, inner.x + inner.width - detailX);
  const int pwmHeight = std::clamp(inner.height / 2, 22, 30);
  const ui::Rect rangeRect = MakeRect(inner.x, inner.y, rangeWidth, inner.height);
  const ui::Rect pwmRect = MakeRect(detailX, inner.y, detailWidth, pwmHeight);
  const ui::Rect statusRect = MakeRect(detailX,
                                       pwmRect.y + pwmRect.height + panelGap,
                                       detailWidth,
                                       std::max(0, inner.y + inner.height - (pwmRect.y + pwmRect.height + panelGap)));

  const auto drawValueCard = [&](const ui::Rect& card,
                                 const char* label,
                                 const std::string& value,
                                 float normalizedValue,
                                 bool focused) {
    if (card.width <= 0 || card.height <= 0)
    {
      return;
    }

    const ui::Color cardColor = focused ? accent : text;
    const ui::Color borderColor = focused ? accent : divider;
    if (focused)
    {
      canvas.FillRect(InsetRect(card, 1, 1), fill);
    }
    canvas.StrokeRect(card, borderColor);
    canvas.DrawTextInRect(MakeRect(card.x, card.y + 2, card.width, 8),
                          label,
                          cardColor,
                          1,
                          ui::TextAlign::Center,
                          ui::VerticalAlign::Middle);
    canvas.DrawTextInRect(MakeRect(card.x + 1, card.y + 10, std::max(0, card.width - 2), std::max(0, card.height - 18)),
                          value,
                          cardColor,
                          value.size() > 2 ? 1 : 2,
                          ui::TextAlign::Center,
                          ui::VerticalAlign::Middle);

    const ui::Rect meterRect = MakeRect(card.x + 3, card.y + card.height - 6, std::max(0, card.width - 6), 3);
    if (meterRect.width > 0)
    {
      canvas.FillRect(meterRect, divider);
      const int fillWidth = std::clamp(static_cast<int>(normalizedValue * static_cast<float>(meterRect.width) + 0.5f),
                                       0,
                                       meterRect.width);
      if (fillWidth > 0)
      {
        canvas.FillRect(MakeRect(meterRect.x, meterRect.y, fillWidth, meterRect.height), focused ? accent : text);
      }
    }
  };

  const bool rangeFocused = IsJunoFocusedParameter(snapshot, "dco.range");
  canvas.StrokeRect(rangeRect, rangeFocused ? accent : divider);
  canvas.DrawTextInRect(MakeRect(rangeRect.x, rangeRect.y + 2, rangeRect.width, 8),
                        "RNG",
                        rangeFocused ? accent : text,
                        1,
                        ui::TextAlign::Center,
                        ui::VerticalAlign::Middle);

  const std::array<const char*, 3> rangeLabels = {"16'", "8'", "4'"};
  const int rangeChipGap = 4;
  const int rangeChipTop = rangeRect.y + 12;
  const int rangeChipHeight = std::max(10, (rangeRect.height - 16 - rangeChipGap * 2) / 3);
  for (std::size_t index = 0; index < rangeLabels.size(); ++index)
  {
    const ui::Rect chip = MakeRect(rangeRect.x + 3,
                                   rangeChipTop + static_cast<int>(index) * (rangeChipHeight + rangeChipGap),
                                   std::max(0, rangeRect.width - 6),
                                   rangeChipHeight);
    const bool selected = snapshot.junoDcoRange == static_cast<int>(index);
    if (selected)
    {
      canvas.FillRect(InsetRect(chip, 1, 1), fill);
    }
    canvas.StrokeRect(chip, selected || rangeFocused ? accent : divider);
    canvas.DrawTextInRect(chip,
                          rangeLabels[index],
                          selected || rangeFocused ? accent : text,
                          1,
                          ui::TextAlign::Center,
                          ui::VerticalAlign::Middle);
  }

  const bool pwmFocused = IsJunoFocusedParameter(snapshot, "dco.pwm");
  if (pwmRect.width > 0 && pwmRect.height > 0)
  {
    if (pwmFocused)
    {
      canvas.FillRect(InsetRect(pwmRect, 1, 1), fill);
    }
    canvas.StrokeRect(pwmRect, pwmFocused ? accent : divider);
    canvas.DrawText(pwmRect.x + 4, pwmRect.y + 3, "PWM", pwmFocused ? accent : text, 1);
    canvas.DrawTextInRect(MakeRect(pwmRect.x, pwmRect.y + 2, pwmRect.width - 4, 10),
                          std::to_string(snapshot.junoDcoPwm),
                          pwmFocused ? accent : text,
                          1,
                          ui::TextAlign::Right,
                          ui::VerticalAlign::Middle);

    const int railLeft = pwmRect.x + 8;
    const int railRight = pwmRect.x + pwmRect.width - 9;
    const int railY = pwmRect.y + pwmRect.height - 10;
    canvas.FillRect(MakeRect(railLeft, railY, std::max(0, railRight - railLeft + 1), 2), divider);

    const float pwmNormalized = JunoNormalizedContinuous(snapshot.junoDcoPwm);
    const int pwmX = railLeft + static_cast<int>(pwmNormalized * static_cast<float>(std::max(1, railRight - railLeft)) + 0.5f);
    const int filledWidth = std::max(0, pwmX - railLeft);
    if (filledWidth > 0)
    {
      canvas.FillRect(MakeRect(railLeft, railY, filledWidth, 2), pwmFocused ? accent : text);
    }
    canvas.DrawLine(pwmX, railY - 5, pwmX, railY + 5, pwmFocused ? accent : text);
  }

  const int statusGap = 4;
  const int statusWidth = std::max(18, (statusRect.width - statusGap * 2) / 3);
  drawValueCard(MakeRect(statusRect.x, statusRect.y, statusWidth, statusRect.height),
                "SAW",
                JunoToggleText(snapshot.junoDcoSaw),
                snapshot.junoDcoSaw > 0 ? 1.0f : 0.0f,
                IsJunoFocusedParameter(snapshot, "dco.saw"));
  drawValueCard(MakeRect(statusRect.x + statusWidth + statusGap, statusRect.y, statusWidth, statusRect.height),
                "SUB",
                std::to_string(snapshot.junoDcoSub),
                JunoNormalizedContinuous(snapshot.junoDcoSub),
                IsJunoFocusedParameter(snapshot, "dco.sub"));
  drawValueCard(MakeRect(statusRect.x + (statusWidth + statusGap) * 2, statusRect.y, statusWidth, statusRect.height),
                "NOI",
                std::to_string(snapshot.junoDcoNoise),
                JunoNormalizedContinuous(snapshot.junoDcoNoise),
                IsJunoFocusedParameter(snapshot, "dco.noise"));
}

void DrawJunoVcfGraphic(ui::Canvas& canvas, const ui::Rect& rect, const UiScreenSnapshot& snapshot, float alpha)
{
  const ui::Color accent = FadeColor(kAccent, alpha);
  const ui::Color divider = FadeColor(kDivider, alpha);
  const ui::Color text = FadeColor(kText, alpha);
  const ui::Color fill = FadeColor(kAccent, alpha * 0.18f);
  const int left = rect.x + 2;
  const int right = rect.x + rect.width - 2;
  const int top = rect.y + 2;
  const int curveBaseY = top + 12;
  const int curveBottomY = top + 22;
  const int cutoffX = left + 8 + static_cast<int>(JunoNormalizedContinuous(snapshot.junoVcfCutoff) * static_cast<float>(std::max(1, right - left - 16)));
  const int peakHeight = static_cast<int>(JunoNormalizedContinuous(snapshot.junoVcfResonance) * 10.0f);
  const int peakY = curveBaseY - peakHeight;

  canvas.DrawLine(left, curveBaseY, cutoffX - 5, curveBaseY, IsJunoFocusedParameter(snapshot, "vcf.cutoff") ? accent : text);
  canvas.DrawLine(cutoffX - 5, curveBaseY, cutoffX, peakY, IsJunoFocusedParameter(snapshot, "vcf.resonance") ? accent : text);
  canvas.DrawLine(cutoffX, peakY, cutoffX + 5, curveBaseY, IsJunoFocusedParameter(snapshot, "vcf.resonance") ? accent : text);
  canvas.DrawLine(cutoffX + 5, curveBaseY, right, curveBottomY, IsJunoFocusedParameter(snapshot, "vcf.cutoff") ? accent : text);

  const int valueY = top + 30;
  const ui::Color cutColor = IsJunoFocusedParameter(snapshot, "vcf.cutoff") ? accent : text;
  const ui::Color resColor = IsJunoFocusedParameter(snapshot, "vcf.resonance") ? accent : text;
  const ui::Color envColor = IsJunoFocusedParameter(snapshot, "vcf.env") ? accent : text;
  const ui::Color lfoColor = IsJunoFocusedParameter(snapshot, "vcf.lfo") ? accent : text;
  canvas.DrawText(left, valueY, "C", cutColor, 1);
  canvas.DrawText(left + 10, valueY, std::to_string(snapshot.junoVcfCutoff), cutColor, 1);
  canvas.DrawText(left + 40, valueY, "R", resColor, 1);
  canvas.DrawText(left + 50, valueY, std::to_string(snapshot.junoVcfResonance), resColor, 1);
  canvas.DrawText(left, valueY + 10, "E", envColor, 1);
  canvas.DrawText(left + 10, valueY + 10, std::to_string(snapshot.junoVcfEnv), envColor, 1);
  canvas.DrawText(left + 40, valueY + 10, "L", lfoColor, 1);
  canvas.DrawText(left + 50, valueY + 10, std::to_string(snapshot.junoVcfLfo), lfoColor, 1);

  const int chipY = valueY + 20;
  canvas.DrawText(left, chipY + 1, "HPF", IsJunoFocusedParameter(snapshot, "hpf.mode") ? accent : text, 1);
  const int chipWidth = 10;
  const int chipGap = 4;
  const int chipStartX = left + 26;
  for (int index = 0; index < 4; ++index)
  {
    const ui::Rect chip = MakeRect(chipStartX + index * (chipWidth + chipGap), chipY, chipWidth, 8);
    if (snapshot.junoHpfMode == index)
    {
      canvas.FillRect(InsetRect(chip, 1, 1), fill);
    }
    canvas.StrokeRect(chip, snapshot.junoHpfMode == index ? accent : divider);
  }
}

void DrawJunoEnvGraphic(ui::Canvas& canvas,
                        const ui::Rect& rect,
                        const UiScreenSnapshot& snapshot,
                        float alpha,
                        bool showValues)
{
  const ui::Color accent = FadeColor(kAccent, alpha);
  const ui::Color divider = FadeColor(kDivider, alpha);
  const ui::Color text = FadeColor(kText, alpha);
  const int left = rect.x + 4;
  const int right = rect.x + rect.width - 4;
  const int top = rect.y + 4;
  const int bottom = rect.y + rect.height - (showValues ? 16 : 6);
  const int width = std::max(20, right - left);
  const int attackWidth = std::max(6, static_cast<int>(JunoNormalizedContinuous(snapshot.junoEnvAttack) * width * 0.22f));
  const int decayWidth = std::max(6, static_cast<int>(JunoNormalizedContinuous(snapshot.junoEnvDecay) * width * 0.24f));
  const int releaseWidth = std::max(6, static_cast<int>(JunoNormalizedContinuous(snapshot.junoEnvRelease) * width * 0.24f));
  const int sustainStartX = std::min(right - releaseWidth - 6, left + attackWidth + decayWidth);
  const int sustainY = bottom - static_cast<int>(JunoNormalizedContinuous(snapshot.junoEnvSustain) * static_cast<float>(bottom - top));

  canvas.DrawLine(left, bottom, left + attackWidth, top, IsJunoFocusedParameter(snapshot, "env.attack") ? accent : divider);
  canvas.DrawLine(left + attackWidth,
                  top,
                  sustainStartX,
                  sustainY,
                  IsJunoFocusedParameter(snapshot, "env.decay") ? accent : divider);
  canvas.DrawLine(sustainStartX,
                  sustainY,
                  right - releaseWidth,
                  sustainY,
                  IsJunoFocusedParameter(snapshot, "env.sustain") ? accent : divider);
  canvas.DrawLine(right - releaseWidth,
                  sustainY,
                  right,
                  bottom,
                  IsJunoFocusedParameter(snapshot, "env.release") ? accent : divider);

  if (showValues)
  {
    const int valueY = rect.y + rect.height - 9;
    const int cellWidth = std::max(1, rect.width / 4);
    const std::array<std::pair<const char*, int>, 4> values = {{
        {"A", snapshot.junoEnvAttack},
        {"D", snapshot.junoEnvDecay},
        {"S", snapshot.junoEnvSustain},
        {"R", snapshot.junoEnvRelease},
    }};
    const std::array<const char*, 4> ids = {"env.attack", "env.decay", "env.sustain", "env.release"};
    for (std::size_t index = 0; index < values.size(); ++index)
    {
      const ui::Color valueColor = IsJunoFocusedParameter(snapshot, ids[index]) ? accent : text;
      const int x = rect.x + static_cast<int>(index) * cellWidth;
      const std::string valueText = std::string(values[index].first) + std::to_string(values[index].second);
      canvas.DrawTextInRect(MakeRect(x, valueY, cellWidth, 8),
                            TruncateText(canvas, valueText, cellWidth - 2, 1),
                            valueColor,
                            1,
                            ui::TextAlign::Center,
                            ui::VerticalAlign::Middle);
    }
  }
}

void DrawJunoLfoGraphic(ui::Canvas& canvas, const ui::Rect& rect, const UiScreenSnapshot& snapshot, float alpha)
{
  const ui::Color accent = FadeColor(kAccent, alpha);
  const ui::Color divider = FadeColor(kDivider, alpha);
  const ui::Color text = FadeColor(kText, alpha);
  const int left = rect.x + 4;
  const int right = rect.x + rect.width - 4;
  const int top = rect.y + 4;
  const int waveBottom = top + rect.height / 2 - 2;
  const int middleY = (top + waveBottom) / 2;
  const int cycleWidth = std::max(16, 18 + static_cast<int>(JunoNormalizedContinuous(snapshot.junoLfoRate) * 18.0f));

  for (int x = left; x + cycleWidth <= right; x += cycleWidth)
  {
    canvas.DrawLine(x, middleY, x + cycleWidth / 2, top, divider);
    canvas.DrawLine(x + cycleWidth / 2, top, x + cycleWidth, middleY, divider);
  }

  const int delayX = left + static_cast<int>(JunoNormalizedContinuous(snapshot.junoLfoDelay) * static_cast<float>(right - left));
  canvas.DrawLine(delayX, top - 1, delayX, waveBottom + 2, IsJunoFocusedParameter(snapshot, "lfo.delay") ? accent : text);

  const int rateY = waveBottom + 10;
  const ui::Color rateColor = IsJunoFocusedParameter(snapshot, "lfo.rate") ? accent : text;
  const ui::Color delayColor = IsJunoFocusedParameter(snapshot, "lfo.delay") ? accent : text;
  canvas.DrawText(left, rateY, "RATE", rateColor, 1);
  canvas.DrawText(left + 30, rateY, std::to_string(snapshot.junoLfoRate), rateColor, 1);
  canvas.DrawText(left, rateY + 10, "DLY", delayColor, 1);
  canvas.DrawText(left + 30, rateY + 10, std::to_string(snapshot.junoLfoDelay), delayColor, 1);
}

void DrawJunoAmpGraphic(ui::Canvas& canvas, const ui::Rect& rect, const UiScreenSnapshot& snapshot, float alpha)
{
  const ui::Color accent = FadeColor(kAccent, alpha);
  const ui::Color divider = FadeColor(kDivider, alpha);
  const ui::Color fill = FadeColor(kAccent, alpha * 0.18f);
  const int meterWidth = 18;
  const int meterHeight = std::max(8, rect.height - 10);
  const int meterX = rect.x + rect.width / 2 - meterWidth / 2;
  const int meterY = rect.y + 4;
  const int fillHeight = static_cast<int>(JunoNormalizedContinuous(snapshot.junoVcaLevel) * static_cast<float>(meterHeight - 2));
  const ui::Rect meterRect = MakeRect(meterX, meterY, meterWidth, meterHeight);
  canvas.StrokeRect(meterRect, divider);
  canvas.FillRect(MakeRect(meterX + 1, meterY + meterHeight - 1 - fillHeight, meterWidth - 2, fillHeight),
                  IsJunoFocusedParameter(snapshot, "vca.level") ? accent : fill);
}

void DrawJunoFxGraphic(ui::Canvas& canvas, const ui::Rect& rect, const UiScreenSnapshot& snapshot, float alpha)
{
  const ui::Color accent = FadeColor(kAccent, alpha);
  const ui::Color divider = FadeColor(kDivider, alpha);
  const ui::Color text = FadeColor(kText, alpha);
  const ui::Color fill = FadeColor(kAccent, alpha * 0.18f);
  const std::array<const char*, 3> labels = {"OFF", "I", "II"};
  const int gap = 6;
  const int boxWidth = std::max(12, (rect.width - gap * 2) / 3);
  const int boxHeight = 18;
  const int startX = rect.x + (rect.width - (boxWidth * 3 + gap * 2)) / 2;
  const int boxY = rect.y + rect.height / 2 - boxHeight / 2;
  for (int index = 0; index < 3; ++index)
  {
    const ui::Rect box = MakeRect(startX + index * (boxWidth + gap), boxY, boxWidth, boxHeight);
    if (snapshot.junoChorusMode == index)
    {
      canvas.FillRect(InsetRect(box, 1, 1), fill);
    }
    canvas.StrokeRect(box, snapshot.junoChorusMode == index ? accent : divider);
    canvas.DrawTextInRect(box,
                          labels[index],
                          snapshot.junoChorusMode == index || IsJunoFocusedParameter(snapshot, "chorus.mode") ? accent : text,
                          1,
                          ui::TextAlign::Center,
                          ui::VerticalAlign::Middle);
  }
}

void DrawJunoSimpleValuePage(ui::Canvas& canvas,
                             const ui::Rect& rect,
                             const std::string& section,
                             const std::string& label,
                             const std::string& value,
                             float alpha)
{
  const ui::Color accent = FadeColor(kAccent, alpha);
  const ui::Color text = FadeColor(kText, alpha);
  const ui::Color divider = FadeColor(kDivider, alpha);
  const int valueScale = value.size() > 14 ? 2 : (value.size() > 8 ? 3 : 5);

  canvas.DrawText(rect.x, rect.y + 10, TruncateText(canvas, section, rect.width, 1), accent, 1);
  canvas.DrawTextInRect(MakeRect(rect.x, rect.y + 30, rect.width, 24),
                        TruncateText(canvas, label, rect.width, 2),
                        text,
                        2,
                        ui::TextAlign::Left,
                        ui::VerticalAlign::Middle);
  canvas.DrawLine(rect.x, rect.y + 58, rect.x + rect.width - 1, rect.y + 58, divider);
  canvas.DrawTextInRect(MakeRect(rect.x, rect.y + 66, rect.width, rect.height - 72),
                        TruncateText(canvas, value, rect.width, valueScale),
                        text,
                        valueScale,
                        ui::TextAlign::Left,
                        ui::VerticalAlign::Middle);
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

  const std::string section = snapshot.headerPrimary.empty() ? std::string{"Juno"} : snapshot.headerPrimary;
  if (snapshot.focusItemId.empty())
  {
    DrawJunoOverview(canvas, rect, snapshot, alpha);
    return;
  }

  if ((section == "VCA" && snapshot.focusItemId == "vca.level") ||
      (section == "CHO" && (snapshot.focusItemId == "chorus.mode" || snapshot.focusItemId == "par.juno.group.fx")) ||
      (section == "HPF" && (snapshot.focusItemId == "hpf.mode" || snapshot.focusItemId == "par.juno.group.hpf")))
  {
    DrawJunoSimpleValuePage(canvas, rect, section, snapshot.primaryText, snapshot.secondaryText, alpha);
    return;
  }

  if (section == "DCO" && IsJunoDiscreteValuePage(snapshot))
  {
    DrawJunoSimpleValuePage(canvas, rect, section, snapshot.primaryText, snapshot.secondaryText, alpha);
    return;
  }

  if (section == "ENV")
  {
    DrawJunoEnvPage(canvas, rect, snapshot, alpha);
    return;
  }

  const ui::Color accent = FadeColor(kAccent, alpha);
  const ui::Color text = FadeColor(kText, alpha);
  const ui::Color divider = FadeColor(kDivider, alpha);
  const bool compactGraphicPage = section == "DCO" || section == "VCF" || section == "LFO";

  canvas.DrawText(rect.x, rect.y + 10, TruncateText(canvas, section, rect.width, 1), accent, 1);
  if (!compactGraphicPage)
  {
    canvas.DrawTextInRect(MakeRect(rect.x, rect.y + 26, rect.width, 18),
                          TruncateText(canvas, snapshot.primaryText, rect.width, 2),
                          text,
                          2,
                          ui::TextAlign::Left,
                          ui::VerticalAlign::Middle);
  }

  const int dividerY = compactGraphicPage ? rect.y + 24 : rect.y + 46;
  canvas.DrawLine(rect.x, dividerY, rect.x + rect.width - 1, dividerY, divider);

  const ui::Rect graphRect = MakeRect(rect.x,
                                      dividerY + 6,
                                      rect.width,
                                      std::max(24, rect.height - (compactGraphicPage ? 36 : 80)));
  if (section == "DCO")
  {
    DrawJunoDcoGraphic(canvas, graphRect, snapshot, alpha);
  }
  else if (section == "VCF")
  {
    DrawJunoVcfGraphic(canvas, graphRect, snapshot, alpha);
  }
  else if (section == "LFO")
  {
    DrawJunoLfoGraphic(canvas, graphRect, snapshot, alpha);
  }
  else if (section == "VCA")
  {
    DrawJunoAmpGraphic(canvas, graphRect, snapshot, alpha);
  }
  else if (section == "CHO")
  {
    DrawJunoFxGraphic(canvas, graphRect, snapshot, alpha);
  }
  else
  {
    DrawJunoOverview(canvas, graphRect, snapshot, alpha);
  }

  if (section != "DCO" && section != "VCF" && section != "LFO")
  {
    const int footerScale = snapshot.secondaryText.size() > 18 ? 1 : (snapshot.secondaryText.size() > 10 ? 2 : 3);
    canvas.DrawTextInRect(MakeRect(rect.x, rect.y + rect.height - 24, rect.width, 18),
                          TruncateText(canvas, snapshot.secondaryText, rect.width, footerScale),
                          text,
                          footerScale,
                          ui::TextAlign::Left,
                          ui::VerticalAlign::Middle);
  }
}

}  // namespace synth::ui_render