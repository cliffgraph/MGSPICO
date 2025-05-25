#pragma once
/**
 * Copyright (c) 2025 Harumakkin.
 * SPDX-License-Identifier: MIT
 */
// https://spdx.org/licenses/

#include "stdint.h"

extern const uint8_t _binary_player_bin_start[];
extern const uint8_t _binary_player_bin_end[];

// 参考：普通のやつらの下を行け: objcopy で実行ファイルにデータを埋め込
// http://0xcc.net/blog/archives/000076.html