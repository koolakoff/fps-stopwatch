# Промпты макетов

## Уточнения версии 2

Встроенный image_gen, редактирование первого макета: английский интерфейс (`REFRESH RATE`, `frame interval`, английские подсказки), уменьшение шрифта и шага списка частот на 20%, точные строки `144 Hz (d)` и `240 Hz (m)`, правый отступ секундомера 5% ширины, отступ кнопок и интервала 3%. Тёмная палитра сохраняется; светлая версия повторяет геометрию тёмной с заменой фона на #F2F0E8, текста на #101418 и акцента на #00695C. После первой генерации отдельно уточнены обязательность пометок и уменьшение таймера на 7% с привязкой слева для увеличения правого отступа.

Созданы встроенным image_gen; тип ui-mockup. Светлый макет использует тёмный как визуальный референс.

## Тёмный

```text
Use case: ui-mockup. Create a precise flat fullscreen Windows stopwatch application screenshot, landscape 16:9, no monitor bezel, no desktop, no gradients, no shadows. Minimal functional measurement instrument. Huge heavy tabular sans serif stopwatch centered in the remaining area to the right of narrow left frequency list: exact text "02:17:483". The "483" milliseconds are 1.5 times the font size of "02:17:", all on one baseline, the complete time occupies most of screen width without overlapping sidebar. Left aligned narrow frequency list, small heading "ЧАСТОТА", entries "60 Hz", "120 Hz", "144 Hz", "165 Hz", "240 Hz"; 144 Hz is selected with thin rectangular border and turquoise text plus small "ИСХОДНАЯ". Top right two small unfilled mouse buttons: half-filled circle theme icon and close ×. Bottom right "6.94 ms" with small label "интервал кадра". Bottom left discreet shortcut hint "Пробел — старт / пауза    ↑↓ — частота    M — максимум    D — исходная    I — тема". No other decoration. Dark theme: uniform graphite background #101418, warm off-white digits and text #F2F0E8, turquoise original frequency #80CBC4. Crisp typography.
```

## Светлый

```text
undefined
```
