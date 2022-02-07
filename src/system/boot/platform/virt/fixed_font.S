/*
 * Copyright 2021-2022, Haiku, Inc.
 * Distributed under the terms of the MIT License.
 */


.global gFixedFont
.data
gFixedFont:
.incbin "fixed_font.bin"
.size	gFixedFont, .-gFixedFont
