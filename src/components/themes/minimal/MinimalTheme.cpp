#include "MinimalTheme.h"

#include <GfxRenderer.h>

#include "fontIds.h"

void MinimalTheme::drawButtonMenu(GfxRenderer& renderer, Rect rect, int buttonCount, int selectedIndex,
                                  const std::function<std::string(int index)>& buttonLabel,
                                  const std::function<UIIcon(int index)>& /*rowIcon*/, int rowSpacing,
                                  int rowHeight) const {
  const int height = rowHeight > 0 ? rowHeight : MinimalMetrics::values.menuRowHeight;
  const int spacing = rowSpacing >= 0 ? rowSpacing : MinimalMetrics::values.menuSpacing;
  const int sidePadding = MinimalMetrics::values.contentSidePadding;

  for (int i = 0; i < buttonCount; i++) {
    const int tileY = rect.y + i * (height + spacing);
    const int tileX = rect.x + sidePadding;
    const int tileW = rect.width - sidePadding * 2;

    if (selectedIndex == i) {
      renderer.fillRoundedRect(tileX, tileY, tileW, height, 4, Color::Black);
    } else {
      renderer.drawRoundedRect(tileX, tileY, tileW, height, 1, 4, true);
    }

    const std::string labelStr = buttonLabel(i);
    const char* label = labelStr.c_str();
    const int textWidth = renderer.getTextWidth(UI_10_FONT_ID, label);
    const int textX = tileX + (tileW - textWidth) / 2;
    const int lineHeight = renderer.getLineHeight(UI_10_FONT_ID);
    const int textY = tileY + (height - lineHeight) / 2;

    renderer.drawText(UI_10_FONT_ID, textX, textY, label, selectedIndex != i);
  }
}
