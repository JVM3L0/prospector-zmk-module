#pragma once

/*
 * Gruvbox Suave do Velksync (palettes.dart, gruvboxSoftDark). Os destaques
 * passam pelo soften() do app na intensidade "Pastel" (velk_ui.c).
 */

#define VELK_SURFACE 0x282828
#define VELK_CARD    0x32302F
#define VELK_HIGH    0x45403D
#define VELK_CRUST   0x1B1B1B
#define VELK_TEXT    0xD4BE98
#define VELK_SUB     0xA89984

#define VELK_PRIMARY 0xA9B665 /* green */
#define VELK_RED     0xEA6962
#define VELK_YELLOW  0xD8A657
/* Azul pastel fora da paleta: o "blue" do Gruvbox (0x7DAEA3) é verde-azulado
 * e o cartão de saída se confundia com as baterias. */
#ifndef VELK_BLUE
#define VELK_BLUE    0x9DB8DD
#endif
#define VELK_GREEN   0xA9B665

/* Quanto o cartão de saída puxa para a cor dele (56/255 = 22%). */
#ifndef VELK_HERO_TINT
#define VELK_HERO_TINT 56
#endif

/* ThemeIntensity.pastel no escuro: 22% em direção à cor do texto. */
#define VELK_SOFTEN 56 /* 0,22 × 255 */

/* Bateria abaixo disto fica na cor de erro. */
#define VELK_LOW_BATTERY 20
