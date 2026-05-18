#pragma once
#include "types.h"

// PS/2 キーボードドライバ
void keyboard_init(void);

// 1文字取得 (リングバッファから)
// バッファが空の場合は -1 を返す (非ブロッキング)
int  keyboard_getc_nb(void);

// 1文字取得 (ブロッキング: 文字が来るまでプロセスをスリープ)
char keyboard_getc(void);

// バッファに未読データがあるか
bool keyboard_has_data(void);

// 1行読み取り (Enter まで、最大 len-1 文字 + NUL)
// エコーバック付き
int keyboard_readline(char *buf, int len);