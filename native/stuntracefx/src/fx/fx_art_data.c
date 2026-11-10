/*
 * fx_art_data.c: the HUD's pictures as clean geometry (fx_art.c). Each
 * picture's fill is loops of corners, x, y and the corner's radius in the
 * picture's texels, the edge straight from corner to corner and each corner
 * rounded by the arc of its radius tangent to both its edges (0 sharp); the
 * shape lies on the right going round. The pieces of each were read from
 * its texels (straights, the stairs' slants, curves where the art turns,
 * sharp notches and cusps), some glyphs built one by one by hand, and every
 * number fitted to the texels by a tool outside the game. "off" is how many
 * of the picture's texels the clean shapes leave on the other side; the
 * first use checks that count.
 */
#include "fx/fx_art.h"

/* ---- the 16 x 32 glyphs (bank $15 from $8000) ---- */

/* the 16 x 32 glyph '0' ($15:$8000): 2 loops, 8 corners (0 sharp), radii 1.6, 6.5; 0 off */
static const FxArtCorner big16_0_0[] = { { 13.98f, 1.09f, 6.49f }, { 13.98f, 26.88f, 6.48f }, { 1.01f, 26.88f, 6.49f }, { 1.01f, 1.09f, 6.49f } };
static const FxArtCorner big16_0_1[] = { { 5.0f, 21.01f, 1.58f }, { 10.0f, 21.01f, 1.57f }, { 10.0f, 6.99f, 1.58f }, { 5.0f, 6.99f, 1.58f } };
static const FxArtLoop big16_0[] = { { 4, big16_0_0 }, { 4, big16_0_1 } };
static const FxArtLayer big16_0_fill[] = { { 3, 2, big16_0 } };

/* the 16 x 32 glyph '1' ($15:$8004): 1 loop, 10 corners (2 sharp), radii 0, 2.8, 2.9, 3.1, 3.2; 1 off */
static const FxArtCorner big16_1_0[] = { { 10.0f, 1.0f, 3.06f }, { 10.0f, 21.04f, 0.0f }, { 13.97f, 20.03f, 3.19f }, { 13.97f, 26.97f, 2.84f }, { 2.0f, 26.97f, 2.87f }, { 2.0f, 19.96f, 3.17f }, { 6.0f, 21.04f, 0.0f }, { 6.0f, 6.95f, 0.01f }, { 2.01f, 8.01f, 3.18f }, { 2.01f, 1.0f, 2.88f } };
static const FxArtLoop big16_1[] = { { 10, big16_1_0 } };
static const FxArtLayer big16_1_fill[] = { { 3, 1, big16_1 } };

/* the 16 x 32 glyph '2' ($15:$8008): 1 loop, 12 corners (1 sharp), radii 1.5, 1.6, 3, 3.1, 6.8, 6.9; 0 off */
static const FxArtCorner big16_2_0[] = { { 14.02f, 0.98f, 6.88f }, { 14.02f, 17.02f, 6.91f }, { 5.0f, 17.02f, 1.54f }, { 5.0f, 20.99f, 0.0f }, { 13.98f, 20.99f, 3.01f }, { 13.98f, 27.0f, 3.0f }, { 0.98f, 27.0f, 3.13f }, { 0.98f, 10.98f, 6.85f }, { 10.01f, 10.98f, 1.52f }, { 10.01f, 6.99f, 1.62f }, { 1.0f, 6.99f, 3.0f }, { 1.0f, 0.98f, 3.01f } };
static const FxArtLoop big16_2[] = { { 12, big16_2_0 } };
static const FxArtLayer big16_2_fill[] = { { 3, 1, big16_2 } };

/* the 16 x 32 glyph '3' ($15:$800C): 1 loop, 15 corners (1 sharp), radii 1.5, 1.6, 3, 6.9, 7.1; 2 off */
static const FxArtCorner big16_3_0[] = { { 14.02f, 0.98f, 6.89f }, { 14.02f, 11.98f, 7.05f }, { 11.5f, 14.5f, 0.0f }, { 14.03f, 17.03f, 7.12f }, { 14.03f, 27.01f, 6.9f }, { 1.0f, 27.01f, 3.03f }, { 1.0f, 21.0f, 2.98f }, { 10.0f, 21.0f, 1.54f }, { 10.0f, 16.99f, 1.58f }, { 2.0f, 16.99f, 3.01f }, { 2.0f, 11.0f, 2.98f }, { 10.0f, 11.0f, 1.54f }, { 10.0f, 6.99f, 1.57f }, { 1.0f, 6.99f, 3.0f }, { 1.0f, 0.98f, 3.01f } };
static const FxArtLoop big16_3[] = { { 15, big16_3_0 } };
static const FxArtLayer big16_3_fill[] = { { 3, 1, big16_3 } };

/* the 16 x 32 glyph '4' ($15:$8010): 2 loops, 14 corners (4 sharp), radii 0.2, 0.4, 1.6, 2.9, 3.1; 1 off, built by hand */
static const FxArtCorner big16_4_0[] = { { 5.97f, 0.96f, 2.94f }, { 13.0f, 0.96f, 3.14f }, { 13.0f, 17.25f, 0.0f }, { 15.0f, 17.92f, 2.9f }, { 15.0f, 22.07f, 2.89f }, { 13.0f, 22.75f, 0.0f }, { 13.0f, 27.01f, 1.61f }, { 8.99f, 27.01f, 1.57f }, { 8.99f, 23.0f, 0.0f }, { 0.99f, 23.0f, 3.11f }, { 0.99f, 15.93f, 0.25f } };
static const FxArtCorner big16_4_1[] = { { 4.96f, 17.0f, 0.0f }, { 9.01f, 17.0f, 0.25f }, { 9.01f, 5.1f, 0.45f } };
static const FxArtLoop big16_4[] = { { 11, big16_4_0 }, { 3, big16_4_1 } };
static const FxArtLayer big16_4_fill[] = { { 3, 2, big16_4 } };

/* the 16 x 32 glyph '5' ($15:$8014): 1 loop, 12 corners (1 sharp), radii 0, 1.6, 3, 3.1, 6.9; 0 off */
static const FxArtCorner big16_5_0[] = { { 13.97f, 0.99f, 3.01f }, { 13.97f, 7.0f, 3.0f }, { 5.0f, 7.0f, 0.0f }, { 5.0f, 10.97f, 0.04f }, { 14.02f, 10.97f, 6.9f }, { 14.02f, 27.0f, 6.88f }, { 1.0f, 27.0f, 3.03f }, { 1.0f, 21.0f, 2.98f }, { 10.01f, 21.0f, 1.57f }, { 10.01f, 17.0f, 1.59f }, { 0.99f, 17.0f, 3.11f }, { 0.99f, 0.99f, 3.07f } };
static const FxArtLoop big16_5[] = { { 12, big16_5_0 } };
static const FxArtLayer big16_5_fill[] = { { 3, 1, big16_5 } };

/* the 16 x 32 glyph '6' ($15:$8018): 2 loops, 12 corners (1 sharp), radii 0.2, 1.6, 3, 6.5, 6.8; 0 off */
static const FxArtCorner big16_6_0[] = { { 13.97f, 0.98f, 3.03f }, { 13.97f, 7.0f, 2.99f }, { 5.0f, 7.0f, 1.58f }, { 5.0f, 9.99f, 0.18f }, { 13.98f, 9.99f, 6.81f }, { 13.98f, 26.88f, 6.48f }, { 1.0f, 26.88f, 6.51f }, { 1.0f, 0.98f, 6.81f } };
static const FxArtCorner big16_6_1[] = { { 5.0f, 21.01f, 1.59f }, { 10.01f, 21.01f, 1.58f }, { 10.01f, 16.0f, 1.59f }, { 5.0f, 16.0f, 0.0f } };
static const FxArtLoop big16_6[] = { { 8, big16_6_0 }, { 4, big16_6_1 } };
static const FxArtLayer big16_6_fill[] = { { 3, 2, big16_6 } };

/* the 16 x 32 glyph '7' ($15:$801C): 1 loop, 8 corners (0 sharp), radii 0, 0.9, 1.2, 1.6, 2.8, 3, 3.1; 0 off */
static const FxArtCorner big16_7_0[] = { { 14.01f, 0.99f, 3.11f }, { 14.01f, 10.47f, 1.24f }, { 5.71f, 27.02f, 2.84f }, { 1.97f, 27.02f, 1.61f }, { 1.97f, 24.46f, 0.01f }, { 10.76f, 6.99f, 0.93f }, { 1.0f, 6.99f, 3.0f }, { 1.0f, 0.99f, 3.0f } };
static const FxArtLoop big16_7[] = { { 8, big16_7_0 } };
static const FxArtLayer big16_7_fill[] = { { 3, 1, big16_7 } };

/* the 16 x 32 glyph '8' ($15:$8400): 3 loops, 18 corners (2 sharp), radii 1.5, 6.5, 7; 3 off */
static const FxArtCorner big16_8_0[] = { { 13.98f, 1.09f, 6.5f }, { 13.98f, 10.89f, 6.98f }, { 11.5f, 13.5f, 0.0f }, { 13.99f, 15.99f, 7.0f }, { 13.99f, 26.88f, 6.51f }, { 1.0f, 26.88f, 6.47f }, { 1.0f, 16.0f, 7.0f }, { 3.5f, 13.5f, 0.0f }, { 1.01f, 11.01f, 7.0f }, { 1.01f, 1.09f, 6.47f } };
static const FxArtCorner big16_8_1[] = { { 5.0f, 11.0f, 1.52f }, { 10.0f, 11.0f, 1.52f }, { 10.0f, 7.0f, 1.52f }, { 5.0f, 7.0f, 1.52f } };
static const FxArtCorner big16_8_2[] = { { 5.0f, 21.0f, 1.52f }, { 10.0f, 21.0f, 1.52f }, { 10.0f, 17.0f, 1.52f }, { 5.0f, 17.0f, 1.52f } };
static const FxArtLoop big16_8[] = { { 10, big16_8_0 }, { 4, big16_8_1 }, { 4, big16_8_2 } };
static const FxArtLayer big16_8_fill[] = { { 3, 3, big16_8 } };

/* the 16 x 32 glyph '9' ($15:$8404): 2 loops, 12 corners (1 sharp), radii 0.2, 1.6, 3, 6.5, 6.8; 1 off */
static const FxArtCorner big16_9_0[] = { { 13.99f, 1.09f, 6.49f }, { 13.99f, 27.0f, 6.81f }, { 1.0f, 27.0f, 3.03f }, { 1.0f, 21.0f, 2.98f }, { 10.0f, 21.0f, 1.56f }, { 10.0f, 17.01f, 0.0f }, { 1.0f, 17.01f, 6.82f }, { 1.0f, 1.09f, 6.5f } };
static const FxArtCorner big16_9_1[] = { { 4.99f, 11.0f, 1.59f }, { 10.0f, 11.0f, 0.17f }, { 10.0f, 6.99f, 1.59f }, { 4.99f, 6.99f, 1.6f } };
static const FxArtLoop big16_9[] = { { 8, big16_9_0 }, { 4, big16_9_1 } };
static const FxArtLayer big16_9_fill[] = { { 3, 2, big16_9 } };

/* the 16 x 32 glyph 'A' ($15:$8408): 2 loops, 12 corners (4 sharp), radii 1.5, 1.6, 6.5; 0 off */
static const FxArtCorner big16_A_0[] = { { 13.99f, 1.09f, 6.5f }, { 13.99f, 27.0f, 1.55f }, { 10.0f, 27.0f, 1.53f }, { 10.0f, 18.0f, 0.0f }, { 5.0f, 18.0f, 0.0f }, { 5.0f, 27.0f, 1.57f }, { 1.0f, 27.0f, 1.52f }, { 1.0f, 1.09f, 6.49f } };
static const FxArtCorner big16_A_1[] = { { 5.0f, 12.0f, 0.0f }, { 10.0f, 12.0f, 0.0f }, { 10.0f, 6.99f, 1.59f }, { 5.0f, 6.99f, 1.6f } };
static const FxArtLoop big16_A[] = { { 8, big16_A_0 }, { 4, big16_A_1 } };
static const FxArtLayer big16_A_fill[] = { { 3, 2, big16_A } };

/* the 16 x 32 glyph 'B' ($15:$840C): 3 loops, 15 corners (3 sharp), radii 0, 1.5, 1.6, 4.6, 6.9, 7.1; 4 off */
static const FxArtCorner big16_B_0[] = { { 14.04f, 0.98f, 6.92f }, { 14.04f, 12.11f, 4.62f }, { 11.14f, 13.27f, 0.0f }, { 14.03f, 15.56f, 7.06f }, { 14.03f, 27.01f, 6.92f }, { 1.0f, 27.01f, 1.55f }, { 1.0f, 0.98f, 1.51f } };
static const FxArtCorner big16_B_1[] = { { 5.0f, 11.0f, 0.02f }, { 10.01f, 11.0f, 1.58f }, { 10.01f, 7.0f, 1.6f }, { 5.0f, 7.0f, 0.01f } };
static const FxArtCorner big16_B_2[] = { { 5.0f, 21.0f, 0.0f }, { 10.01f, 21.0f, 1.58f }, { 10.01f, 17.0f, 1.6f }, { 5.0f, 17.0f, 0.0f } };
static const FxArtLoop big16_B[] = { { 7, big16_B_0 }, { 4, big16_B_1 }, { 4, big16_B_2 } };
static const FxArtLayer big16_B_fill[] = { { 3, 3, big16_B } };

/* the 16 x 32 glyph 'C' ($15:$8410): 1 loop, 8 corners (0 sharp), radii 1.6, 3, 6.8; 0 off */
static const FxArtCorner big16_C_0[] = { { 13.97f, 0.98f, 3.03f }, { 13.97f, 7.0f, 2.99f }, { 5.0f, 7.0f, 1.58f }, { 5.0f, 21.0f, 1.56f }, { 13.98f, 21.0f, 3.0f }, { 13.98f, 27.0f, 3.01f }, { 0.99f, 27.0f, 6.84f }, { 0.99f, 0.98f, 6.83f } };
static const FxArtLoop big16_C[] = { { 8, big16_C_0 } };
static const FxArtLayer big16_C_fill[] = { { 3, 1, big16_C } };

/* the 16 x 32 glyph 'D' ($15:$8414): 2 loops, 8 corners (0 sharp), radii 0, 1.6, 3.1, 6.9; 0 off */
static const FxArtCorner big16_D_0[] = { { 14.01f, 0.97f, 6.88f }, { 14.01f, 27.01f, 6.87f }, { 0.99f, 27.01f, 3.12f }, { 0.99f, 0.97f, 3.1f } };
static const FxArtCorner big16_D_1[] = { { 5.0f, 21.0f, 0.04f }, { 10.0f, 21.0f, 1.56f }, { 10.0f, 7.0f, 1.58f }, { 5.0f, 7.0f, 0.04f } };
static const FxArtLoop big16_D[] = { { 4, big16_D_0 }, { 4, big16_D_1 } };
static const FxArtLayer big16_D_fill[] = { { 3, 2, big16_D } };

/* the 16 x 32 glyph 'E' ($15:$8418): 1 loop, 12 corners (4 sharp), radii 3, 3.1; 0 off */
static const FxArtCorner big16_E_0[] = { { 13.98f, 0.99f, 3.01f }, { 13.98f, 7.0f, 3.0f }, { 5.0f, 7.0f, 0.0f }, { 5.0f, 10.99f, 0.0f }, { 12.98f, 10.99f, 3.01f }, { 12.98f, 17.0f, 3.0f }, { 5.0f, 17.0f, 0.0f }, { 5.0f, 20.99f, 0.0f }, { 13.98f, 20.99f, 3.0f }, { 13.98f, 27.0f, 3.0f }, { 0.99f, 27.0f, 3.1f }, { 0.99f, 0.99f, 3.06f } };
static const FxArtLoop big16_E[] = { { 12, big16_E_0 } };
static const FxArtLayer big16_E_fill[] = { { 3, 1, big16_E } };

/* the 16 x 32 glyph 'F' ($15:$841C): 1 loop, 10 corners (3 sharp), radii 1.5, 1.6, 3, 3.1; 0 off */
static const FxArtCorner big16_F_0[] = { { 13.98f, 0.99f, 3.01f }, { 13.98f, 7.0f, 3.0f }, { 5.0f, 7.0f, 0.0f }, { 5.0f, 10.99f, 0.0f }, { 12.98f, 10.99f, 3.01f }, { 12.98f, 17.0f, 3.0f }, { 5.0f, 17.0f, 0.0f }, { 5.0f, 27.0f, 1.57f }, { 1.0f, 27.0f, 1.53f }, { 1.0f, 0.99f, 3.06f } };
static const FxArtLoop big16_F[] = { { 10, big16_F_0 } };
static const FxArtLayer big16_F_fill[] = { { 3, 1, big16_F } };

/* the 16 x 32 glyph 'G' ($15:$8800): 1 loop, 12 corners (1 sharp), radii 1.5, 2.9, 3, 3.1, 6.5, 6.8; 1 off */
static const FxArtCorner big16_G_0[] = { { 13.97f, 0.98f, 3.02f }, { 13.97f, 7.0f, 2.99f }, { 5.0f, 7.0f, 1.55f }, { 5.0f, 21.0f, 1.55f }, { 10.0f, 21.0f, 1.54f }, { 10.0f, 15.92f, 0.0f }, { 6.03f, 17.04f, 3.13f }, { 6.03f, 10.01f, 2.9f }, { 13.98f, 10.01f, 3.07f }, { 13.98f, 26.88f, 6.48f }, { 1.0f, 26.88f, 6.5f }, { 1.0f, 0.98f, 6.81f } };
static const FxArtLoop big16_G[] = { { 12, big16_G_0 } };
static const FxArtLayer big16_G_fill[] = { { 3, 1, big16_G } };

/* the 16 x 32 glyph 'H' ($15:$8804): 1 loop, 12 corners (4 sharp), radii 1.5, 1.6; 0 off */
static const FxArtCorner big16_H_0[] = { { 5.0f, 0.96f, 1.59f }, { 5.0f, 11.0f, 0.0f }, { 10.0f, 11.0f, 0.0f }, { 10.0f, 0.96f, 1.54f }, { 14.0f, 0.96f, 1.59f }, { 14.0f, 27.0f, 1.57f }, { 10.0f, 27.0f, 1.53f }, { 10.0f, 17.0f, 0.0f }, { 5.0f, 17.0f, 0.0f }, { 5.0f, 27.0f, 1.57f }, { 1.0f, 27.0f, 1.53f }, { 1.0f, 0.96f, 1.54f } };
static const FxArtLoop big16_H[] = { { 12, big16_H_0 } };
static const FxArtLayer big16_H_fill[] = { { 3, 1, big16_H } };

/* the 16 x 32 glyph 'I' ($15:$8808): 1 loop, 12 corners (4 sharp), radii 2.8, 2.9, 3.1, 3.2; 0 off */
static const FxArtCorner big16_I_0[] = { { 13.95f, 1.01f, 2.84f }, { 13.95f, 8.03f, 3.12f }, { 10.0f, 6.94f, 0.0f }, { 10.0f, 21.05f, 0.0f }, { 13.97f, 20.03f, 3.17f }, { 13.97f, 26.97f, 2.84f }, { 2.01f, 26.97f, 2.88f }, { 2.01f, 19.94f, 3.15f }, { 6.0f, 21.05f, 0.0f }, { 6.0f, 6.93f, 0.0f }, { 2.0f, 8.03f, 3.16f }, { 2.0f, 1.01f, 2.87f } };
static const FxArtLoop big16_I[] = { { 12, big16_I_0 } };
static const FxArtLayer big16_I_fill[] = { { 3, 1, big16_I } };

/* the 16 x 32 glyph 'L' ($15:$8814): 1 loop, 6 corners (0 sharp), radii 0, 1.5, 1.6, 3, 3.1; 0 off */
static const FxArtCorner big16_L_0[] = { { 5.0f, 0.96f, 1.59f }, { 5.0f, 20.99f, 0.04f }, { 13.98f, 20.99f, 3.0f }, { 13.98f, 27.0f, 3.0f }, { 1.0f, 27.0f, 3.09f }, { 1.0f, 0.96f, 1.54f } };
static const FxArtLoop big16_L[] = { { 6, big16_L_0 } };
static const FxArtLayer big16_L_fill[] = { { 3, 1, big16_L } };

/* the 16 x 32 glyph 'M' ($15:$8818): 1 loop, 12 corners (4 sharp), radii 1, 1.5, 2.8; 1 off, built by hand */
static const FxArtCorner big16_M_0[] = { { 1.0f, 1.07f, 2.77f }, { 4.2f, 1.07f, 0.96f }, { 7.49f, 4.68f, 0.0f }, { 10.78f, 1.07f, 0.96f }, { 14.0f, 1.07f, 2.8f }, { 14.0f, 27.0f, 1.53f }, { 10.0f, 27.0f, 1.52f }, { 10.0f, 8.91f, 0.0f }, { 7.51f, 11.72f, 0.0f }, { 5.0f, 8.91f, 0.0f }, { 5.0f, 27.0f, 1.55f }, { 1.0f, 27.0f, 1.51f } };
static const FxArtLoop big16_M[] = { { 12, big16_M_0 } };
static const FxArtLayer big16_M_fill[] = { { 3, 1, big16_M } };

/* the 16 x 32 glyph 'N' ($15:$881C): 1 loop, 10 corners (0 sharp), radii 0.2, 1.5, 1.6, 1.9, 2.7; 6 off */
static const FxArtCorner big16_N_0[] = { { 4.94f, 1.13f, 1.85f }, { 10.0f, 13.92f, 0.24f }, { 10.0f, 0.97f, 1.52f }, { 14.0f, 0.97f, 1.58f }, { 14.0f, 26.82f, 2.69f }, { 10.04f, 26.82f, 1.87f }, { 5.0f, 14.08f, 0.24f }, { 5.0f, 27.0f, 1.56f }, { 1.0f, 27.0f, 1.52f }, { 1.0f, 1.13f, 2.68f } };
static const FxArtLoop big16_N[] = { { 10, big16_N_0 } };
static const FxArtLayer big16_N_fill[] = { { 3, 1, big16_N } };

/* the 16 x 32 glyph 'O' ($15:$8C00): 2 loops, 8 corners (0 sharp), radii 1.6, 6.5; 0 off */
static const FxArtCorner big16_O_0[] = { { 13.98f, 1.09f, 6.49f }, { 13.98f, 26.88f, 6.48f }, { 1.01f, 26.88f, 6.49f }, { 1.01f, 1.09f, 6.49f } };
static const FxArtCorner big16_O_1[] = { { 5.0f, 21.01f, 1.58f }, { 10.0f, 21.01f, 1.57f }, { 10.0f, 6.99f, 1.58f }, { 5.0f, 6.99f, 1.58f } };
static const FxArtLoop big16_O[] = { { 4, big16_O_0 }, { 4, big16_O_1 } };
static const FxArtLayer big16_O_fill[] = { { 3, 2, big16_O } };

/* the 16 x 32 glyph 'P' ($15:$8C04): 2 loops, 10 corners (3 sharp), radii 1.5, 1.6, 3.1, 6.9; 0 off */
static const FxArtCorner big16_P_0[] = { { 14.02f, 0.97f, 6.9f }, { 14.02f, 18.02f, 6.9f }, { 5.0f, 18.02f, 0.0f }, { 5.0f, 27.0f, 1.57f }, { 1.0f, 27.0f, 1.53f }, { 1.0f, 0.97f, 3.1f } };
static const FxArtCorner big16_P_1[] = { { 5.0f, 12.0f, 0.0f }, { 10.01f, 12.0f, 1.57f }, { 10.01f, 7.0f, 1.59f }, { 5.0f, 7.0f, 0.0f } };
static const FxArtLoop big16_P[] = { { 6, big16_P_0 }, { 4, big16_P_1 } };
static const FxArtLayer big16_P_fill[] = { { 3, 2, big16_P } };

/* the 16 x 32 glyph 'R' ($15:$8C0C): 2 loops, 14 corners (5 sharp), radii 0.6, 1.5, 1.6, 2.9, 3.1, 6.9, 7.1; 1 off, built by hand */
static const FxArtCorner big16_R_0[] = { { 1.0f, 0.97f, 3.1f }, { 14.03f, 0.97f, 6.92f }, { 14.03f, 12.29f, 7.05f }, { 10.4f, 15.4f, 0.0f }, { 14.0f, 22.43f, 0.0f }, { 14.0f, 26.94f, 2.93f }, { 10.67f, 26.94f, 0.64f }, { 5.0f, 15.47f, 0.0f }, { 5.0f, 27.0f, 1.58f }, { 1.0f, 27.0f, 1.53f } };
static const FxArtCorner big16_R_1[] = { { 10.01f, 7.0f, 1.61f }, { 5.0f, 7.0f, 0.0f }, { 5.0f, 11.0f, 0.0f }, { 10.01f, 11.0f, 1.59f } };
static const FxArtLoop big16_R[] = { { 10, big16_R_0 }, { 4, big16_R_1 } };
static const FxArtLayer big16_R_fill[] = { { 3, 2, big16_R } };

/* the 16 x 32 glyph 'S' ($15:$8C10): 1 loop, 12 corners (0 sharp), radii 1.5, 1.6, 3, 6.9; 0 off */
static const FxArtCorner big16_S_0[] = { { 13.97f, 0.98f, 3.03f }, { 13.97f, 7.0f, 2.99f }, { 4.99f, 7.0f, 1.59f }, { 4.99f, 10.96f, 1.51f }, { 14.02f, 10.96f, 6.93f }, { 14.02f, 27.01f, 6.89f }, { 1.0f, 27.01f, 3.03f }, { 1.0f, 21.0f, 2.98f }, { 10.01f, 21.0f, 1.58f }, { 10.01f, 17.01f, 1.57f }, { 0.96f, 17.01f, 6.91f }, { 0.96f, 0.98f, 6.89f } };
static const FxArtLoop big16_S[] = { { 12, big16_S_0 } };
static const FxArtLayer big16_S_fill[] = { { 3, 1, big16_S } };

/* the 16 x 32 glyph 'T' ($15:$8C14): 1 loop, 8 corners (2 sharp), radii 1.5, 3; 0 off */
static const FxArtCorner big16_T_0[] = { { 14.98f, 0.99f, 3.01f }, { 14.98f, 7.0f, 3.0f }, { 10.0f, 7.0f, 0.0f }, { 10.0f, 26.99f, 1.51f }, { 6.0f, 26.99f, 1.5f }, { 6.0f, 6.99f, 0.0f }, { 1.0f, 6.99f, 3.01f }, { 1.0f, 0.99f, 2.99f } };
static const FxArtLoop big16_T[] = { { 8, big16_T_0 } };
static const FxArtLayer big16_T_fill[] = { { 3, 1, big16_T } };

/* the 16 x 32 glyph 'U' ($15:$8C18): 1 loop, 8 corners (0 sharp), radii 1.5, 1.6, 6.5; 0 off */
static const FxArtCorner big16_U_0[] = { { 5.0f, 0.96f, 1.59f }, { 5.0f, 21.01f, 1.58f }, { 10.0f, 21.01f, 1.56f }, { 10.0f, 0.97f, 1.53f }, { 13.99f, 0.97f, 1.58f }, { 13.99f, 26.88f, 6.48f }, { 1.0f, 26.88f, 6.51f }, { 1.0f, 0.96f, 1.54f } };
static const FxArtLoop big16_U[] = { { 8, big16_U_0 } };
static const FxArtLayer big16_U_fill[] = { { 3, 1, big16_U } };

/* the 16 x 32 glyph 'V' ($15:$8C1C): 1 loop, 15 corners (3 sharp), radii 0, 0.1, 1.5, 1.6, 2.7, 2.8, 7.5, 10.6, 11.8, 11.9; 5 off */
static const FxArtCorner big16_V_0[] = { { 5.0f, 0.98f, 1.55f }, { 5.0f, 17.18f, 0.09f }, { 4.83f, 16.9f, 0.0f }, { 7.17f, 19.15f, 10.63f }, { 7.86f, 22.34f, 0.0f }, { 7.92f, 19.03f, 7.51f }, { 10.06f, 16.89f, 0.0f }, { 10.0f, 16.99f, 0.03f }, { 10.0f, 0.98f, 1.51f }, { 14.01f, 0.98f, 1.56f }, { 14.01f, 18.01f, 11.93f }, { 9.72f, 27.02f, 2.71f }, { 5.28f, 27.02f, 2.83f }, { 0.99f, 18.0f, 11.77f }, { 0.99f, 0.98f, 1.53f } };
static const FxArtLoop big16_V[] = { { 15, big16_V_0 } };
static const FxArtLayer big16_V_fill[] = { { 3, 1, big16_V } };

/* the 16 x 32 glyph 'W' ($15:$9000): 1 loop, 12 corners (4 sharp), radii 0.5, 0.8, 1.9, 2.9; 0 off, built by hand */
static const FxArtCorner big16_W_0[] = { { 0.99f, 0.84f, 1.87f }, { 5.01f, 0.84f, 1.94f }, { 5.01f, 17.86f, 0.0f }, { 7.44f, 15.39f, 0.0f }, { 10.0f, 17.8f, 0.0f }, { 10.0f, 0.84f, 1.87f }, { 14.0f, 0.84f, 1.93f }, { 14.0f, 26.93f, 2.9f }, { 10.84f, 26.93f, 0.5f }, { 7.49f, 23.33f, 0.0f }, { 4.22f, 26.91f, 0.8f }, { 0.99f, 26.91f, 2.87f } };
static const FxArtLoop big16_W[] = { { 12, big16_W_0 } };
static const FxArtLayer big16_W_fill[] = { { 3, 1, big16_W } };

/* the 16 x 32 glyph 'X' ($15:$9004): 1 loop, 16 corners (6 sharp), radii 0.8, 0.9, 1, 1.5, 2.6, 2.7, 2.8; 3 off, built by hand */
static const FxArtCorner big16_X_0[] = { { 1.01f, 1.05f, 2.8f }, { 4.39f, 1.05f, 0.91f }, { 7.5f, 7.89f, 0.0f }, { 10.55f, 1.06f, 0.95f }, { 13.96f, 1.06f, 2.79f }, { 13.96f, 5.63f, 0.0f }, { 9.74f, 14.0f, 1.5f }, { 13.96f, 22.39f, 0.0f }, { 13.96f, 26.87f, 2.64f }, { 10.55f, 26.87f, 0.97f }, { 7.51f, 20.17f, 0.0f }, { 4.43f, 26.87f, 0.75f }, { 1.01f, 26.87f, 2.71f }, { 1.01f, 22.34f, 0.0f }, { 5.25f, 14.0f, 1.5f }, { 1.01f, 5.67f, 0.0f } };
static const FxArtLoop big16_X[] = { { 16, big16_X_0 } };
static const FxArtLayer big16_X_fill[] = { { 3, 1, big16_X } };

/* the 16 x 32 glyph 'Y' ($15:$9008): 1 loop, 17 corners (4 sharp), radii 0, 0.1, 0.2, 1.5, 1.6, 1.7, 1.8, 4.9, 7, 10.6, 19.5; 9 off */
static const FxArtCorner big16_Y_0[] = { { 5.0f, 0.96f, 1.6f }, { 5.0f, 8.15f, 0.03f }, { 4.95f, 8.07f, 0.0f }, { 7.96f, 11.0f, 10.55f }, { 8.14f, 15.59f, 0.0f }, { 7.29f, 10.47f, 7.02f }, { 10.17f, 7.9f, 0.0f }, { 10.0f, 8.19f, 0.09f }, { 10.0f, 0.97f, 1.53f }, { 14.04f, 0.97f, 1.65f }, { 14.04f, 11.61f, 19.47f }, { 10.03f, 16.85f, 0.0f }, { 10.03f, 27.05f, 1.82f }, { 5.0f, 27.05f, 1.66f }, { 5.0f, 17.85f, 0.25f }, { 0.99f, 8.73f, 4.94f }, { 0.99f, 0.96f, 1.56f } };
static const FxArtLoop big16_Y[] = { { 17, big16_Y_0 } };
static const FxArtLayer big16_Y_fill[] = { { 3, 1, big16_Y } };

/* the 16 x 32 glyph 'Z' ($15:$900C): 1 loop, 10 corners (2 sharp), radii 0.2, 3; 1 off */
static const FxArtCorner big16_Z_0[] = { { 14.0f, 1.0f, 3.0f }, { 14.0f, 6.5f, 0.0f }, { 6.75f, 21.0f, 0.25f }, { 14.0f, 21.0f, 3.0f }, { 14.0f, 27.0f, 3.0f }, { 1.0f, 27.0f, 3.0f }, { 1.0f, 21.5f, 0.0f }, { 8.25f, 7.0f, 0.25f }, { 1.0f, 7.0f, 3.0f }, { 1.0f, 1.0f, 3.0f } };
static const FxArtLoop big16_Z[] = { { 10, big16_Z_0 } };
static const FxArtLayer big16_Z_fill[] = { { 3, 1, big16_Z } };

/* the 16 x 32 glyph '.' ($15:$9010): 1 loop, 4 corners (0 sharp), radii 3; 0 off */
static const FxArtCorner big16_dot_0[] = { { 10.0f, 21.0f, 3.0f }, { 10.0f, 27.0f, 3.0f }, { 4.0f, 27.0f, 3.0f }, { 4.0f, 21.0f, 3.0f } };
static const FxArtLoop big16_dot[] = { { 4, big16_dot_0 } };
static const FxArtLayer big16_dot_fill[] = { { 3, 1, big16_dot } };

/* the 16 x 32 glyph ''' ($15:$9014): 1 loop, 10 corners (2 sharp), radii 0.1, 1.1, 1.6, 2.9, 3.2, 4.1, 7.3; 0 off */
static const FxArtCorner big16_quote_0[] = { { 11.02f, 0.94f, 3.25f }, { 11.02f, 10.03f, 7.29f }, { 5.99f, 15.0f, 0.06f }, { 3.97f, 15.0f, 1.58f }, { 3.97f, 11.31f, 1.14f }, { 5.03f, 11.23f, 0.0f }, { 5.92f, 9.24f, 4.13f }, { 8.16f, 8.83f, 0.0f }, { 3.95f, 6.73f, 2.92f }, { 3.95f, 0.94f, 3.24f } };
static const FxArtLoop big16_quote[] = { { 10, big16_quote_0 } };
static const FxArtLayer big16_quote_fill[] = { { 3, 1, big16_quote } };

/* the 16 x 32 glyph '"' ($15:$9018): 2 loops, 16 corners (2 sharp), radii 1, 2.5, 3, 5, 5.9, 6, 6.1; 8 off */
static const FxArtCorner big16_quotes_0[] = { { 7.0f, 1.0f, 3.01f }, { 7.0f, 10.21f, 5.0f }, { 3.39f, 14.97f, 2.5f }, { 1.01f, 14.97f, 1.02f }, { 1.01f, 10.79f, 5.99f }, { 3.59f, 9.0f, 0.0f }, { 0.98f, 7.18f, 6.09f }, { 0.98f, 1.0f, 3.0f } };
static const FxArtCorner big16_quotes_1[] = { { 14.0f, 1.0f, 3.0f }, { 14.0f, 10.2f, 5.0f }, { 10.37f, 14.97f, 2.5f }, { 8.02f, 14.97f, 1.02f }, { 8.02f, 10.77f, 5.95f }, { 10.58f, 9.0f, 0.0f }, { 8.0f, 7.2f, 6.03f }, { 8.0f, 1.0f, 3.0f } };
static const FxArtLoop big16_quotes[] = { { 8, big16_quotes_0 }, { 8, big16_quotes_1 } };
static const FxArtLayer big16_quotes_fill[] = { { 3, 2, big16_quotes } };

/* the 16 x 32 glyph '!' ($15:$901C): 2 loops, 8 corners (0 sharp), radii 1.5, 1.6; 0 off */
static const FxArtCorner big16_bang_0[] = { { 10.0f, 0.98f, 1.56f }, { 10.0f, 19.0f, 1.57f }, { 5.0f, 19.0f, 1.53f }, { 5.0f, 0.98f, 1.51f } };
static const FxArtCorner big16_bang_1[] = { { 10.01f, 21.99f, 1.63f }, { 10.01f, 27.0f, 1.59f }, { 4.98f, 27.0f, 1.56f }, { 4.98f, 21.99f, 1.52f } };
static const FxArtLoop big16_bang[] = { { 4, big16_bang_0 }, { 4, big16_bang_1 } };
static const FxArtLayer big16_bang_fill[] = { { 3, 2, big16_bang } };

/* ---- the 8 x 16 glyphs (bank $15 from $9400) ---- */

/* the 8 x 16 glyph '0' ($15:$9400): 2 loops, 8 corners (4 sharp), radii 2.1; 0 off */
static const FxArtCorner small8_0_0[] = { { 7.01f, 2.97f, 2.07f }, { 7.01f, 14.03f, 2.07f }, { 0.99f, 14.03f, 2.07f }, { 0.99f, 2.97f, 2.07f } };
static const FxArtCorner small8_0_1[] = { { 3.0f, 11.0f, 0.0f }, { 5.0f, 11.0f, 0.0f }, { 5.0f, 6.0f, 0.0f }, { 3.0f, 6.0f, 0.0f } };
static const FxArtLoop small8_0[] = { { 4, small8_0_0 }, { 4, small8_0_1 } };
static const FxArtLayer small8_0_fill[] = { { 3, 2, small8_0 } };

/* the 8 x 16 glyph '1' ($15:$9402): 1 loop, 10 corners (10 sharp), radii none; 0 off, built by hand */
static const FxArtCorner small8_1_0[] = { { 1.98f, 2.98f, 0.0f }, { 5.0f, 2.98f, 0.0f }, { 5.0f, 10.97f, 0.0f }, { 6.02f, 10.97f, 0.0f }, { 6.02f, 14.02f, 0.0f }, { 1.98f, 14.02f, 0.0f }, { 1.98f, 10.97f, 0.0f }, { 3.0f, 10.97f, 0.0f }, { 3.0f, 6.03f, 0.0f }, { 1.98f, 6.03f, 0.0f } };
static const FxArtLoop small8_1[] = { { 10, small8_1_0 } };
static const FxArtLayer small8_1_fill[] = { { 3, 1, small8_1 } };

/* the 8 x 16 glyph '2' ($15:$9404): 1 loop, 12 corners (9 sharp), radii 2.1, 2.2; 0 off */
static const FxArtCorner small8_2_0[] = { { 7.03f, 2.98f, 2.08f }, { 7.03f, 10.01f, 2.15f }, { 3.0f, 10.01f, 0.0f }, { 3.0f, 10.99f, 0.0f }, { 7.02f, 10.99f, 0.0f }, { 7.02f, 14.01f, 0.0f }, { 0.98f, 14.01f, 0.0f }, { 0.98f, 7.0f, 2.13f }, { 5.0f, 7.0f, 0.0f }, { 5.0f, 6.01f, 0.0f }, { 0.98f, 6.01f, 0.0f }, { 0.98f, 2.98f, 0.0f } };
static const FxArtLoop small8_2[] = { { 12, small8_2_0 } };
static const FxArtLayer small8_2_fill[] = { { 3, 1, small8_2 } };

/* the 8 x 16 glyph '3' ($15:$9406): 1 loop, 16 corners (12 sharp), radii 0.7, 2; 0 off */
static const FxArtCorner small8_3_0[] = { { 7.0f, 3.0f, 2.0f }, { 7.0f, 8.46f, 0.71f }, { 6.0f, 8.0f, 0.0f }, { 6.0f, 9.0f, 0.0f }, { 7.0f, 8.54f, 0.71f }, { 7.0f, 14.0f, 2.0f }, { 1.0f, 14.0f, 0.0f }, { 1.0f, 11.0f, 0.0f }, { 5.0f, 11.0f, 0.0f }, { 5.0f, 10.0f, 0.0f }, { 2.0f, 10.0f, 0.0f }, { 2.0f, 7.0f, 0.0f }, { 5.0f, 7.0f, 0.0f }, { 5.0f, 6.0f, 0.0f }, { 1.0f, 6.0f, 0.0f }, { 1.0f, 3.0f, 0.0f } };
static const FxArtLoop small8_3[] = { { 16, small8_3_0 } };
static const FxArtLayer small8_3_fill[] = { { 3, 1, small8_3 } };

/* the 8 x 16 glyph '4' ($15:$9408): 2 loops, 10 corners (10 sharp), radii none; 11 off, built by hand */
static const FxArtCorner small8_4_0[] = { { 3.34f, 2.98f, 0.0f }, { 7.01f, 2.98f, 0.0f }, { 7.01f, 14.03f, 0.0f }, { 4.99f, 14.03f, 0.0f }, { 4.99f, 12.01f, 0.0f }, { 0.99f, 12.01f, 0.0f }, { 0.99f, 7.88f, 0.0f } };
static const FxArtCorner small8_4_1[] = { { 2.8f, 9.02f, 0.0f }, { 5.02f, 9.02f, 0.0f }, { 5.02f, 5.26f, 0.0f } };
static const FxArtLoop small8_4[] = { { 7, small8_4_0 }, { 3, small8_4_1 } };
static const FxArtLayer small8_4_fill[] = { { 3, 2, small8_4 } };

/* the 8 x 16 glyph '5' ($15:$940A): 1 loop, 12 corners (6 sharp), radii 0, 0.4, 2.1, 2.2; 0 off */
static const FxArtCorner small8_5_0[] = { { 7.02f, 2.99f, 0.01f }, { 7.02f, 6.01f, 0.02f }, { 3.0f, 6.01f, 0.0f }, { 3.0f, 6.99f, 0.0f }, { 7.03f, 6.99f, 2.15f }, { 7.03f, 14.02f, 2.08f }, { 0.98f, 14.02f, 0.01f }, { 0.98f, 11.0f, 0.42f }, { 5.0f, 11.0f, 0.0f }, { 5.0f, 10.0f, 0.0f }, { 0.99f, 10.0f, 0.0f }, { 0.99f, 2.99f, 0.0f } };
static const FxArtLoop small8_5[] = { { 12, small8_5_0 } };
static const FxArtLayer small8_5_fill[] = { { 3, 1, small8_5 } };

/* the 8 x 16 glyph '6' ($15:$940C): 2 loops, 12 corners (6 sharp), radii 0.1, 2, 2.1, 2.2; 0 off */
static const FxArtCorner small8_6_0[] = { { 7.03f, 2.98f, 0.1f }, { 7.03f, 6.01f, 0.1f }, { 3.0f, 6.01f, 0.0f }, { 3.0f, 6.99f, 0.0f }, { 7.03f, 6.99f, 2.15f }, { 7.03f, 14.03f, 2.1f }, { 0.99f, 14.03f, 2.07f }, { 0.99f, 2.98f, 2.05f } };
static const FxArtCorner small8_6_1[] = { { 3.0f, 11.0f, 0.0f }, { 5.0f, 11.0f, 0.0f }, { 5.0f, 10.0f, 0.0f }, { 3.0f, 10.0f, 0.0f } };
static const FxArtLoop small8_6[] = { { 8, small8_6_0 }, { 4, small8_6_1 } };
static const FxArtLayer small8_6_fill[] = { { 3, 2, small8_6 } };

/* the 8 x 16 glyph '7' ($15:$940E): 1 loop, 8 corners (8 sharp), radii none; 13 off, built by hand */
static const FxArtCorner small8_7_0[] = { { 0.98f, 2.99f, 0.0f }, { 7.01f, 2.99f, 0.0f }, { 7.01f, 7.12f, 0.0f }, { 3.61f, 14.04f, 0.0f }, { 1.99f, 14.04f, 0.0f }, { 1.99f, 10.77f, 0.0f }, { 4.44f, 6.06f, 0.0f }, { 0.98f, 6.06f, 0.0f } };
static const FxArtLoop small8_7[] = { { 8, small8_7_0 } };
static const FxArtLayer small8_7_fill[] = { { 3, 1, small8_7 } };

/* the 8 x 16 glyph '8' ($15:$9410): 3 loops, 20 corners (12 sharp), radii 0.7, 2; 0 off */
static const FxArtCorner small8_8_0[] = { { 7.0f, 3.0f, 2.0f }, { 7.0f, 8.46f, 0.71f }, { 6.0f, 8.0f, 0.0f }, { 6.0f, 9.0f, 0.0f }, { 7.0f, 8.54f, 0.71f }, { 7.0f, 14.0f, 2.0f }, { 1.0f, 14.0f, 2.0f }, { 1.0f, 8.54f, 0.71f }, { 2.0f, 9.0f, 0.0f }, { 2.0f, 8.0f, 0.0f }, { 1.0f, 8.46f, 0.71f }, { 1.0f, 3.0f, 2.0f } };
static const FxArtCorner small8_8_1[] = { { 3.0f, 7.0f, 0.0f }, { 5.0f, 7.0f, 0.0f }, { 5.0f, 6.0f, 0.0f }, { 3.0f, 6.0f, 0.0f } };
static const FxArtCorner small8_8_2[] = { { 3.0f, 11.0f, 0.0f }, { 5.0f, 11.0f, 0.0f }, { 5.0f, 10.0f, 0.0f }, { 3.0f, 10.0f, 0.0f } };
static const FxArtLoop small8_8[] = { { 12, small8_8_0 }, { 4, small8_8_1 }, { 4, small8_8_2 } };
static const FxArtLayer small8_8_fill[] = { { 3, 3, small8_8 } };

/* the 8 x 16 glyph '9' ($15:$9412): 2 loops, 12 corners (6 sharp), radii 0, 0.1, 2, 2.1, 2.2; 0 off */
static const FxArtCorner small8_9_0[] = { { 7.01f, 2.97f, 2.07f }, { 7.01f, 14.02f, 2.05f }, { 0.98f, 14.02f, 0.0f }, { 0.98f, 10.99f, 0.0f }, { 5.0f, 10.99f, 0.0f }, { 5.0f, 10.01f, 0.0f }, { 0.97f, 10.01f, 2.15f }, { 0.97f, 2.97f, 2.11f } };
static const FxArtCorner small8_9_1[] = { { 3.0f, 7.0f, 0.05f }, { 5.0f, 7.0f, 0.0f }, { 5.0f, 6.0f, 0.03f }, { 3.0f, 6.0f, 0.0f } };
static const FxArtLoop small8_9[] = { { 8, small8_9_0 }, { 4, small8_9_1 } };
static const FxArtLayer small8_9_fill[] = { { 3, 2, small8_9 } };

/* the 8 x 16 glyph 'A' ($15:$9414): 2 loops, 12 corners (5 sharp), radii 0, 0.1, 0.2, 2.1; 0 off */
static const FxArtCorner small8_A_0[] = { { 7.01f, 2.97f, 2.06f }, { 7.01f, 14.03f, 0.0f }, { 4.99f, 14.03f, 0.0f }, { 4.99f, 10.0f, 0.01f }, { 3.01f, 10.0f, 0.0f }, { 3.01f, 14.03f, 0.0f }, { 0.99f, 14.03f, 0.0f }, { 0.99f, 2.97f, 2.06f } };
static const FxArtCorner small8_A_1[] = { { 3.0f, 7.0f, 0.17f }, { 5.0f, 7.0f, 0.14f }, { 5.0f, 6.0f, 0.14f }, { 3.0f, 6.0f, 0.08f } };
static const FxArtLoop small8_A[] = { { 8, small8_A_0 }, { 4, small8_A_1 } };
static const FxArtLayer small8_A_fill[] = { { 3, 2, small8_A } };

/* the 8 x 16 glyph 'B' ($15:$9416): 3 loops, 16 corners (10 sharp), radii 0.2, 0.7, 2.5; 5 off */
static const FxArtCorner small8_B_0[] = { { 7.03f, 2.98f, 2.5f }, { 7.03f, 8.52f, 0.7f }, { 6.0f, 7.98f, 0.0f }, { 6.0f, 9.02f, 0.0f }, { 7.03f, 8.48f, 0.71f }, { 7.03f, 14.02f, 2.5f }, { 0.99f, 14.02f, 0.25f }, { 0.99f, 2.98f, 0.25f } };
static const FxArtCorner small8_B_1[] = { { 3.0f, 7.0f, 0.0f }, { 5.0f, 7.0f, 0.0f }, { 5.0f, 6.0f, 0.0f }, { 3.0f, 6.0f, 0.0f } };
static const FxArtCorner small8_B_2[] = { { 3.0f, 11.0f, 0.0f }, { 5.0f, 11.0f, 0.0f }, { 5.0f, 10.0f, 0.0f }, { 3.0f, 10.0f, 0.0f } };
static const FxArtLoop small8_B[] = { { 8, small8_B_0 }, { 4, small8_B_1 }, { 4, small8_B_2 } };
static const FxArtLayer small8_B_fill[] = { { 3, 3, small8_B } };

/* the 8 x 16 glyph 'C' ($15:$9418): 1 loop, 8 corners (4 sharp), radii 0, 2; 2 off */
static const FxArtCorner small8_C_0[] = { { 7.02f, 2.98f, 0.0f }, { 7.02f, 6.01f, 0.0f }, { 3.0f, 6.01f, 0.04f }, { 3.0f, 10.99f, 0.03f }, { 7.02f, 10.99f, 0.0f }, { 7.02f, 14.02f, 0.0f }, { 0.99f, 14.02f, 2.05f }, { 0.99f, 2.98f, 2.05f } };
static const FxArtLoop small8_C[] = { { 8, small8_C_0 } };
static const FxArtLayer small8_C_fill[] = { { 3, 1, small8_C } };

/* the 8 x 16 glyph 'D' ($15:$941A): 2 loops, 8 corners (6 sharp), radii 2; 0 off */
static const FxArtCorner small8_D_0[] = { { 7.01f, 2.98f, 2.05f }, { 7.01f, 14.02f, 2.05f }, { 0.99f, 14.02f, 0.0f }, { 0.99f, 2.98f, 0.0f } };
static const FxArtCorner small8_D_1[] = { { 3.0f, 11.0f, 0.0f }, { 5.0f, 11.0f, 0.0f }, { 5.0f, 6.0f, 0.0f }, { 3.0f, 6.0f, 0.0f } };
static const FxArtLoop small8_D[] = { { 4, small8_D_0 }, { 4, small8_D_1 } };
static const FxArtLayer small8_D_fill[] = { { 3, 2, small8_D } };

/* the 8 x 16 glyph 'E' ($15:$941C): 1 loop, 12 corners (5 sharp), radii 0, 0.1; 0 off */
static const FxArtCorner small8_E_0[] = { { 7.02f, 2.99f, 0.01f }, { 7.02f, 6.01f, 0.01f }, { 3.0f, 6.01f, 0.0f }, { 3.0f, 7.0f, 0.1f }, { 6.0f, 7.0f, 0.0f }, { 6.0f, 10.0f, 0.03f }, { 3.0f, 10.0f, 0.0f }, { 3.0f, 10.99f, 0.0f }, { 7.02f, 10.99f, 0.01f }, { 7.02f, 14.01f, 0.01f }, { 0.99f, 14.01f, 0.01f }, { 0.99f, 2.99f, 0.0f } };
static const FxArtLoop small8_E[] = { { 12, small8_E_0 } };
static const FxArtLayer small8_E_fill[] = { { 3, 1, small8_E } };

/* the 8 x 16 glyph 'F' ($15:$941E): 1 loop, 10 corners (10 sharp), radii none; 0 off */
static const FxArtCorner small8_F_0[] = { { 7.02f, 2.99f, 0.0f }, { 7.02f, 6.01f, 0.0f }, { 3.0f, 6.01f, 0.0f }, { 3.0f, 7.0f, 0.0f }, { 6.01f, 7.0f, 0.0f }, { 6.01f, 10.01f, 0.0f }, { 3.01f, 10.01f, 0.0f }, { 3.01f, 14.03f, 0.0f }, { 0.99f, 14.03f, 0.0f }, { 0.99f, 2.99f, 0.0f } };
static const FxArtLoop small8_F[] = { { 10, small8_F_0 } };
static const FxArtLayer small8_F_fill[] = { { 3, 1, small8_F } };

/* the 8 x 16 glyph 'G' ($15:$9600): 1 loop, 12 corners (7 sharp), radii 0.2, 0.6, 2.1; 2 off */
static const FxArtCorner small8_G_0[] = { { 7.02f, 2.98f, 0.23f }, { 7.02f, 6.0f, 0.0f }, { 3.0f, 6.0f, 0.0f }, { 3.0f, 11.0f, 0.0f }, { 5.0f, 11.0f, 0.0f }, { 5.0f, 9.92f, 0.0f }, { 3.99f, 10.86f, 0.6f }, { 3.99f, 7.0f, 0.0f }, { 7.01f, 7.0f, 0.0f }, { 7.01f, 14.02f, 0.23f }, { 0.99f, 14.02f, 2.06f }, { 0.99f, 2.98f, 2.06f } };
static const FxArtLoop small8_G[] = { { 12, small8_G_0 } };
static const FxArtLayer small8_G_fill[] = { { 3, 1, small8_G } };

/* the 8 x 16 glyph 'H' ($15:$9602): 1 loop, 12 corners (10 sharp), radii 0, 0.1; 0 off */
static const FxArtCorner small8_H_0[] = { { 3.01f, 2.97f, 0.0f }, { 3.01f, 7.0f, 0.04f }, { 4.99f, 7.0f, 0.0f }, { 4.99f, 2.97f, 0.0f }, { 7.01f, 2.97f, 0.0f }, { 7.01f, 14.03f, 0.0f }, { 4.99f, 14.03f, 0.0f }, { 4.99f, 10.0f, 0.06f }, { 3.01f, 10.0f, 0.0f }, { 3.01f, 14.03f, 0.0f }, { 0.99f, 14.03f, 0.0f }, { 0.99f, 2.97f, 0.0f } };
static const FxArtLoop small8_H[] = { { 12, small8_H_0 } };
static const FxArtLayer small8_H_fill[] = { { 3, 1, small8_H } };

/* the 8 x 16 glyph 'I' ($15:$9604): 1 loop, 12 corners (12 sharp), radii none; 0 off, built by hand */
static const FxArtCorner small8_I_0[] = { { 1.98f, 2.98f, 0.0f }, { 6.02f, 2.98f, 0.0f }, { 6.02f, 6.03f, 0.0f }, { 5.0f, 6.03f, 0.0f }, { 5.0f, 10.97f, 0.0f }, { 6.02f, 10.97f, 0.0f }, { 6.02f, 14.02f, 0.0f }, { 1.98f, 14.02f, 0.0f }, { 1.98f, 10.97f, 0.0f }, { 3.0f, 10.97f, 0.0f }, { 3.0f, 6.03f, 0.0f }, { 1.98f, 6.03f, 0.0f } };
static const FxArtLoop small8_I[] = { { 12, small8_I_0 } };
static const FxArtLayer small8_I_fill[] = { { 3, 1, small8_I } };

/* the 8 x 16 glyph 'J' ($15:$9606): 1 loop, 6 corners (5 sharp), radii 2; 0 off */
static const FxArtCorner small8_J_0[] = { { 5.0f, 10.99f, 0.0f }, { 5.0f, 2.97f, 0.0f }, { 7.01f, 2.97f, 0.0f }, { 7.01f, 14.01f, 2.04f }, { 0.98f, 14.01f, 0.0f }, { 0.98f, 10.99f, 0.0f } };
static const FxArtLoop small8_J[] = { { 6, small8_J_0 } };
static const FxArtLayer small8_J_fill[] = { { 3, 1, small8_J } };

/* the 8 x 16 glyph 'K' ($15:$9608): 1 loop, 11 corners (8 sharp), radii 0.8, 1.7; 8 off, built by hand */
static const FxArtCorner small8_K_0[] = { { 0.99f, 2.97f, 0.0f }, { 3.0f, 2.97f, 0.0f }, { 3.0f, 8.02f, 0.0f }, { 5.39f, 2.93f, 0.0f }, { 7.77f, 2.93f, 0.79f }, { 4.67f, 8.5f, 1.68f }, { 7.77f, 14.07f, 0.79f }, { 5.39f, 14.07f, 0.0f }, { 3.0f, 8.98f, 0.0f }, { 3.0f, 14.03f, 0.0f }, { 0.99f, 14.03f, 0.0f } };
static const FxArtLoop small8_K[] = { { 11, small8_K_0 } };
static const FxArtLayer small8_K_fill[] = { { 3, 1, small8_K } };

/* the 8 x 16 glyph 'L' ($15:$960A): 1 loop, 6 corners (5 sharp), radii 2; 0 off */
static const FxArtCorner small8_L_0[] = { { 3.0f, 2.97f, 0.0f }, { 3.0f, 10.99f, 0.0f }, { 7.02f, 10.99f, 0.0f }, { 7.02f, 14.01f, 0.0f }, { 0.99f, 14.01f, 2.04f }, { 0.99f, 2.97f, 0.0f } };
static const FxArtLoop small8_L[] = { { 6, small8_L_0 } };
static const FxArtLayer small8_L_fill[] = { { 3, 1, small8_L } };

/* the 8 x 16 glyph 'M' ($15:$960C): 1 loop, 12 corners (11 sharp), radii 0.2; 10 off, built by hand */
static const FxArtCorner small8_M_0[] = { { 0.99f, 2.94f, 0.0f }, { 1.78f, 2.94f, 0.0f }, { 3.51f, 5.29f, 0.0f }, { 5.18f, 2.97f, 0.0f }, { 7.01f, 2.97f, 0.0f }, { 7.01f, 14.03f, 0.0f }, { 5.0f, 14.03f, 0.0f }, { 5.0f, 7.54f, 0.0f }, { 3.49f, 9.16f, 0.25f }, { 2.0f, 7.51f, 0.0f }, { 2.0f, 14.05f, 0.0f }, { 0.99f, 14.05f, 0.0f } };
static const FxArtLoop small8_M[] = { { 12, small8_M_0 } };
static const FxArtLayer small8_M_fill[] = { { 3, 1, small8_M } };

/* the 8 x 16 glyph 'N' ($15:$960E): 1 loop, 11 corners (3 sharp), radii 0, 0.1; 4 off, built by hand */
static const FxArtCorner small8_N_0[] = { { 0.99f, 2.95f, 0.09f }, { 2.62f, 2.95f, 0.01f }, { 5.0f, 8.01f, 0.0f }, { 5.0f, 2.96f, 0.09f }, { 7.01f, 2.96f, 0.09f }, { 7.01f, 14.04f, 0.09f }, { 5.0f, 14.04f, 0.06f }, { 5.0f, 12.79f, 0.0f }, { 3.0f, 9.26f, 0.0f }, { 3.0f, 14.04f, 0.1f }, { 0.99f, 14.04f, 0.09f } };
static const FxArtLoop small8_N[] = { { 11, small8_N_0 } };
static const FxArtLayer small8_N_fill[] = { { 3, 1, small8_N } };

/* the 8 x 16 glyph 'O' ($15:$9610): 2 loops, 8 corners (4 sharp), radii 2.1; 0 off */
static const FxArtCorner small8_O_0[] = { { 7.01f, 2.97f, 2.07f }, { 7.01f, 14.03f, 2.07f }, { 0.99f, 14.03f, 2.07f }, { 0.99f, 2.97f, 2.07f } };
static const FxArtCorner small8_O_1[] = { { 3.0f, 11.0f, 0.0f }, { 5.0f, 11.0f, 0.0f }, { 5.0f, 6.0f, 0.0f }, { 3.0f, 6.0f, 0.0f } };
static const FxArtLoop small8_O[] = { { 4, small8_O_0 }, { 4, small8_O_1 } };
static const FxArtLayer small8_O_fill[] = { { 3, 2, small8_O } };

/* the 8 x 16 glyph 'P' ($15:$9612): 2 loops, 10 corners (6 sharp), radii 0, 2.1; 0 off */
static const FxArtCorner small8_P_0[] = { { 7.03f, 2.98f, 2.09f }, { 7.03f, 10.02f, 2.09f }, { 3.0f, 10.02f, 0.0f }, { 3.0f, 14.03f, 0.01f }, { 0.99f, 14.03f, 0.01f }, { 0.99f, 2.98f, 0.0f } };
static const FxArtCorner small8_P_1[] = { { 3.0f, 7.0f, 0.0f }, { 5.0f, 7.0f, 0.0f }, { 5.0f, 6.0f, 0.0f }, { 3.0f, 6.0f, 0.0f } };
static const FxArtLoop small8_P[] = { { 6, small8_P_0 }, { 4, small8_P_1 } };
static const FxArtLayer small8_P_fill[] = { { 3, 2, small8_P } };

/* the 8 x 16 glyph 'Q' ($15:$9614): 2 loops, 11 corners (4 sharp), radii 0, 0.1, 1.9, 2.1; 3 off, built by hand */
static const FxArtCorner small8_Q_0[] = { { 0.99f, 2.98f, 2.07f }, { 6.93f, 2.98f, 1.9f }, { 6.93f, 14.03f, 0.0f }, { 3.98f, 14.03f, 0.06f }, { 3.98f, 13.02f, 0.0f }, { 0.99f, 13.02f, 2.06f } };
static const FxArtCorner small8_Q_1[] = { { 5.0f, 6.0f, 0.0f }, { 3.0f, 6.0f, 0.0f }, { 3.0f, 10.13f, 0.01f }, { 3.37f, 10.13f, 0.01f }, { 5.0f, 8.57f, 0.02f } };
static const FxArtLoop small8_Q[] = { { 6, small8_Q_0 }, { 5, small8_Q_1 } };
static const FxArtLayer small8_Q_fill[] = { { 3, 2, small8_Q } };

/* the 8 x 16 glyph 'R' ($15:$9616): 2 loops, 16 corners (13 sharp), radii 0.7, 2; 0 off */
static const FxArtCorner small8_R_0[] = { { 7.0f, 3.0f, 2.0f }, { 7.0f, 8.46f, 0.71f }, { 6.0f, 8.0f, 0.0f }, { 6.0f, 9.0f, 0.0f }, { 7.0f, 8.54f, 0.71f }, { 7.0f, 14.0f, 0.0f }, { 5.0f, 14.0f, 0.0f }, { 5.0f, 10.0f, 0.0f }, { 3.0f, 10.0f, 0.0f }, { 3.0f, 14.0f, 0.0f }, { 1.0f, 14.0f, 0.0f }, { 1.0f, 3.0f, 0.0f } };
static const FxArtCorner small8_R_1[] = { { 3.0f, 7.0f, 0.0f }, { 5.0f, 7.0f, 0.0f }, { 5.0f, 6.0f, 0.0f }, { 3.0f, 6.0f, 0.0f } };
static const FxArtLoop small8_R[] = { { 12, small8_R_0 }, { 4, small8_R_1 } };
static const FxArtLayer small8_R_fill[] = { { 3, 2, small8_R } };

/* the 8 x 16 glyph 'S' ($15:$9618): 1 loop, 12 corners (6 sharp), radii 0, 2.1, 2.2; 0 off */
static const FxArtCorner small8_S_0[] = { { 7.02f, 2.98f, 0.0f }, { 7.02f, 6.01f, 0.0f }, { 3.0f, 6.01f, 0.01f }, { 3.0f, 6.99f, 0.0f }, { 7.03f, 6.99f, 2.15f }, { 7.03f, 14.02f, 2.08f }, { 0.98f, 14.02f, 0.0f }, { 0.98f, 10.99f, 0.0f }, { 5.0f, 10.99f, 0.01f }, { 5.0f, 10.01f, 0.0f }, { 0.97f, 10.01f, 2.15f }, { 0.97f, 2.98f, 2.08f } };
static const FxArtLoop small8_S[] = { { 12, small8_S_0 } };
static const FxArtLayer small8_S_fill[] = { { 3, 1, small8_S } };

/* the 8 x 16 glyph 'T' ($15:$961A): 1 loop, 8 corners (3 sharp), radii 0, 0.1, 0.2, 1.1; 4 off */
static const FxArtCorner small8_T_0[] = { { 7.06f, 2.99f, 0.14f }, { 7.06f, 9.35f, 1.1f }, { 5.0f, 5.68f, 0.0f }, { 5.0f, 14.03f, 0.0f }, { 3.0f, 14.03f, 0.02f }, { 3.0f, 5.68f, 0.0f }, { 0.93f, 9.27f, 1.11f }, { 0.93f, 2.99f, 0.15f } };
static const FxArtLoop small8_T[] = { { 8, small8_T_0 } };
static const FxArtLayer small8_T_fill[] = { { 3, 1, small8_T } };

/* the 8 x 16 glyph 'U' ($15:$961C): 1 loop, 8 corners (5 sharp), radii 0, 2.1; 0 off */
static const FxArtCorner small8_U_0[] = { { 3.01f, 2.97f, 0.0f }, { 3.01f, 11.0f, 0.03f }, { 4.99f, 11.0f, 0.0f }, { 4.99f, 2.97f, 0.0f }, { 7.01f, 2.97f, 0.0f }, { 7.01f, 14.03f, 2.06f }, { 0.99f, 14.03f, 2.06f }, { 0.99f, 2.97f, 0.0f } };
static const FxArtLoop small8_U[] = { { 8, small8_U_0 } };
static const FxArtLayer small8_U_fill[] = { { 3, 1, small8_U } };

/* the 8 x 16 glyph 'V' ($15:$961E): 1 loop, 8 corners (2 sharp), radii 0.1, 3; 3 off */
static const FxArtCorner small8_V_0[] = { { 3.01f, 2.95f, 0.07f }, { 3.01f, 10.0f, 0.0f }, { 4.99f, 10.0f, 0.0f }, { 4.99f, 2.95f, 0.07f }, { 7.0f, 2.95f, 0.06f }, { 7.0f, 13.75f, 2.99f }, { 1.0f, 13.75f, 3.01f }, { 1.0f, 2.95f, 0.07f } };
static const FxArtLoop small8_V[] = { { 8, small8_V_0 } };
static const FxArtLayer small8_V_fill[] = { { 3, 1, small8_V } };

/* the 8 x 16 glyph 'W' ($15:$9800): 1 loop, 12 corners (11 sharp), radii 0.3; 11 off, built by hand */
static const FxArtCorner small8_W_0[] = { { 0.99f, 2.95f, 0.0f }, { 2.0f, 2.95f, 0.0f }, { 2.0f, 10.49f, 0.0f }, { 3.48f, 8.84f, 0.26f }, { 5.0f, 10.46f, 0.0f }, { 5.0f, 2.97f, 0.0f }, { 7.01f, 2.97f, 0.0f }, { 7.01f, 14.03f, 0.0f }, { 5.18f, 14.03f, 0.0f }, { 3.53f, 11.72f, 0.0f }, { 1.77f, 14.06f, 0.0f }, { 0.99f, 14.06f, 0.0f } };
static const FxArtLoop small8_W[] = { { 12, small8_W_0 } };
static const FxArtLayer small8_W_fill[] = { { 3, 1, small8_W } };

/* the 8 x 16 glyph 'X' ($15:$9802): 1 loop, 18 corners (15 sharp), radii 0, 0.4; 11 off, built by hand */
static const FxArtCorner small8_X_0[] = { { 0.98f, 2.97f, 0.0f }, { 3.02f, 2.97f, 0.0f }, { 3.02f, 5.0f, 0.0f }, { 4.98f, 5.0f, 0.0f }, { 4.98f, 2.97f, 0.0f }, { 7.02f, 2.97f, 0.0f }, { 7.02f, 6.84f, 0.0f }, { 4.65f, 8.49f, 0.38f }, { 7.02f, 10.17f, 0.01f }, { 7.02f, 14.03f, 0.0f }, { 4.98f, 14.03f, 0.0f }, { 4.98f, 12.0f, 0.0f }, { 3.02f, 12.0f, 0.0f }, { 3.02f, 14.03f, 0.0f }, { 0.98f, 14.03f, 0.0f }, { 0.98f, 10.16f, 0.0f }, { 3.34f, 8.5f, 0.37f }, { 0.98f, 6.84f, 0.0f } };
static const FxArtLoop small8_X[] = { { 18, small8_X_0 } };
static const FxArtLayer small8_X_fill[] = { { 3, 1, small8_X } };

/* the 8 x 16 glyph 'Y' ($15:$9804): 1 loop, 13 corners (4 sharp), radii 0.1, 0.2, 2.2, 2.5, 3; 5 off */
static const FxArtCorner small8_Y_0[] = { { 3.01f, 2.97f, 0.1f }, { 3.01f, 7.0f, 0.0f }, { 4.99f, 7.0f, 0.0f }, { 4.99f, 2.96f, 0.1f }, { 7.02f, 2.96f, 0.11f }, { 7.02f, 8.88f, 2.21f }, { 5.01f, 9.07f, 0.0f }, { 5.01f, 14.04f, 0.22f }, { 2.99f, 14.04f, 0.11f }, { 2.99f, 9.82f, 2.99f }, { 3.26f, 9.22f, 0.0f }, { 0.97f, 9.02f, 2.52f }, { 0.97f, 2.97f, 0.1f } };
static const FxArtLoop small8_Y[] = { { 13, small8_Y_0 } };
static const FxArtLayer small8_Y_fill[] = { { 3, 1, small8_Y } };

/* the 8 x 16 glyph 'Z' ($15:$9806): 1 loop, 13 corners (6 sharp), radii 0.1, 0.2, 2.3, 2.5; 6 off */
static const FxArtCorner small8_Z_0[] = { { 7.02f, 2.99f, 0.0f }, { 7.02f, 9.42f, 2.51f }, { 4.39f, 9.3f, 0.0f }, { 4.4f, 9.31f, 0.12f }, { 4.4f, 11.0f, 0.0f }, { 7.04f, 11.0f, 0.18f }, { 7.04f, 14.01f, 0.19f }, { 0.99f, 14.01f, 0.0f }, { 0.99f, 7.56f, 2.26f }, { 3.5f, 7.85f, 0.0f }, { 3.5f, 6.0f, 0.0f }, { 0.96f, 6.0f, 0.19f }, { 0.96f, 2.99f, 0.19f } };
static const FxArtLoop small8_Z[] = { { 13, small8_Z_0 } };
static const FxArtLayer small8_Z_fill[] = { { 3, 1, small8_Z } };

/* the 8 x 16 glyph '.' ($15:$9808): 1 loop, 4 corners (4 sharp), radii none; 0 off */
static const FxArtCorner small8_dot_0[] = { { 5.02f, 10.98f, 0.0f }, { 5.02f, 14.02f, 0.0f }, { 1.98f, 14.02f, 0.0f }, { 1.98f, 10.98f, 0.0f } };
static const FxArtLoop small8_dot[] = { { 4, small8_dot_0 } };
static const FxArtLayer small8_dot_fill[] = { { 3, 1, small8_dot } };

/* the 8 x 16 glyph ''' ($15:$980A): 1 loop, 8 corners (8 sharp), radii none; 6 off, built by hand */
static const FxArtCorner small8_quote_0[] = { { 1.98f, 0.98f, 0.0f }, { 5.01f, 0.98f, 0.0f }, { 5.01f, 4.7f, 0.0f }, { 3.69f, 6.03f, 0.0f }, { 1.98f, 6.03f, 0.0f }, { 1.98f, 4.27f, 0.0f }, { 3.43f, 3.45f, 0.0f }, { 1.98f, 2.76f, 0.0f } };
static const FxArtLoop small8_quote[] = { { 8, small8_quote_0 } };
static const FxArtLayer small8_quote_fill[] = { { 3, 1, small8_quote } };

/* the 8 x 16 glyph '"' ($15:$980C): 2 loops, 19 corners (9 sharp), radii 0, 0.1, 0.2; 11 off, built by hand */
static const FxArtCorner small8_quotes_0[] = { { 0.97f, 0.98f, 0.01f }, { 3.0f, 0.98f, 0.24f }, { 3.0f, 3.0f, 0.0f }, { 4.0f, 3.0f, 0.0f }, { 4.0f, 4.0f, 0.0f }, { 3.0f, 4.0f, 0.0f }, { 3.0f, 6.02f, 0.24f }, { 0.97f, 6.02f, 0.01f }, { 0.97f, 4.27f, 0.09f }, { 2.43f, 3.44f, 0.0f }, { 0.97f, 2.75f, 0.08f } };
static const FxArtCorner small8_quotes_1[] = { { 4.0f, 0.98f, 0.23f }, { 7.02f, 0.98f, 0.01f }, { 7.02f, 4.7f, 0.09f }, { 5.71f, 6.01f, 0.0f }, { 4.0f, 6.01f, 0.22f }, { 4.0f, 4.27f, 0.0f }, { 5.41f, 3.5f, 0.0f }, { 4.0f, 2.72f, 0.0f } };
static const FxArtLoop small8_quotes[] = { { 11, small8_quotes_0 }, { 8, small8_quotes_1 } };
static const FxArtLayer small8_quotes_fill[] = { { 3, 2, small8_quotes } };

/* ---- the big sprites' 24 x 40 letters (bank $15 from $A000) ---- */

/* the 24 x 40 letter 'A' ($15:$A000): 2 loops, 14 corners (4 sharp), radii 1.3, 3, 3.5, 5.1, 7.7, 8.9, 12.6; 10 off */
static const FxArtCorner banner24_A_0[] = { { 6.25f, 1.94f, 7.66f }, { 13.13f, 1.94f, 5.1f }, { 20.0f, 7.18f, 12.6f }, { 20.0f, 35.86f, 3.5f }, { 14.0f, 35.86f, 1.27f }, { 14.0f, 25.0f, 0.0f }, { 8.02f, 25.0f, 0.0f }, { 8.02f, 35.86f, 3.5f }, { 2.0f, 35.86f, 1.28f }, { 2.0f, 11.13f, 8.93f } };
static const FxArtCorner banner24_A_1[] = { { 8.0f, 17.0f, 0.0f }, { 14.0f, 17.0f, 0.0f }, { 14.0f, 10.02f, 2.99f }, { 8.0f, 10.02f, 3.0f } };
static const FxArtLoop banner24_A[] = { { 10, banner24_A_0 }, { 4, banner24_A_1 } };
static const FxArtLayer banner24_A_fill[] = { { 3, 2, banner24_A } };

/* the 24 x 40 letter 'C' ($15:$A006): 1 loop, 8 corners (0 sharp), radii 1.5, 1.6, 2.6, 2.7, 4.9, 8.3, 8.4; 12 off */
static const FxArtCorner banner24_C_0[] = { { 19.87f, 1.98f, 2.68f }, { 19.87f, 10.03f, 4.88f }, { 8.0f, 10.03f, 1.5f }, { 8.0f, 28.0f, 1.57f }, { 19.87f, 28.0f, 2.64f }, { 19.87f, 36.05f, 4.9f }, { 1.98f, 36.05f, 8.38f }, { 1.98f, 1.98f, 8.28f } };
static const FxArtLoop banner24_C[] = { { 8, banner24_C_0 } };
static const FxArtLayer banner24_C_fill[] = { { 3, 1, banner24_C } };

/* the 24 x 40 letter 'E' ($15:$A00C): 1 loop, 12 corners (1 sharp), radii 0.1, 0.2, 2.6, 2.9, 3.6, 3.7, 4.8, 4.9; 24 off */
static const FxArtCorner banner24_E_0[] = { { 19.87f, 2.01f, 2.59f }, { 19.87f, 10.03f, 4.86f }, { 8.0f, 10.03f, 0.22f }, { 8.0f, 15.0f, 0.18f }, { 17.89f, 15.0f, 2.9f }, { 17.89f, 23.04f, 4.94f }, { 8.0f, 23.04f, 0.0f }, { 8.0f, 28.0f, 0.06f }, { 19.89f, 28.0f, 2.88f }, { 19.89f, 36.0f, 4.85f }, { 1.98f, 36.0f, 3.66f }, { 1.98f, 2.01f, 3.64f } };
static const FxArtLoop banner24_E[] = { { 12, banner24_E_0 } };
static const FxArtLayer banner24_E_fill[] = { { 3, 1, banner24_E } };

/* the 24 x 40 letter 'F' ($15:$A012): 1 loop, 10 corners (2 sharp), radii 0.2, 1.3, 2.6, 2.9, 3.5, 3.6, 4.9; 16 off */
static const FxArtCorner banner24_F_0[] = { { 19.87f, 2.01f, 2.59f }, { 19.87f, 10.03f, 4.86f }, { 8.0f, 10.03f, 0.0f }, { 8.0f, 15.0f, 0.19f }, { 17.89f, 15.0f, 2.9f }, { 17.89f, 23.04f, 4.94f }, { 8.02f, 23.04f, 0.0f }, { 8.02f, 35.88f, 3.52f }, { 1.99f, 35.88f, 1.32f }, { 1.99f, 2.01f, 3.63f } };
static const FxArtLoop banner24_F[] = { { 10, banner24_F_0 } };
static const FxArtLayer banner24_F_fill[] = { { 3, 1, banner24_F } };

/* the 24 x 40 letter 'G' ($15:$A018): 1 loop, 12 corners (0 sharp), radii 0.1, 1.5, 1.6, 2.7, 2.9, 3.2, 4.9, 5, 8.3, 8.4; 13 off */
static const FxArtCorner banner24_G_0[] = { { 19.87f, 1.98f, 2.67f }, { 19.87f, 10.03f, 4.88f }, { 8.0f, 10.03f, 1.5f }, { 8.0f, 28.0f, 1.58f }, { 14.0f, 28.0f, 1.57f }, { 14.0f, 23.01f, 0.05f }, { 9.1f, 23.01f, 2.92f }, { 9.1f, 14.95f, 4.97f }, { 20.02f, 14.95f, 3.25f }, { 20.02f, 36.05f, 8.41f }, { 1.98f, 36.05f, 8.38f }, { 1.98f, 1.98f, 8.28f } };
static const FxArtLoop banner24_G[] = { { 12, banner24_G_0 } };
static const FxArtLayer banner24_G_fill[] = { { 3, 1, banner24_G } };

/* the 24 x 40 letter 'H' ($15:$A500): 1 loop, 12 corners (2 sharp), radii 0.1, 0.2, 0.9, 1.3, 3.4, 3.5; 11 off */
static const FxArtCorner banner24_H_0[] = { { 8.0f, 2.12f, 0.91f }, { 8.0f, 15.0f, 0.06f }, { 13.98f, 15.0f, 0.0f }, { 13.98f, 2.11f, 3.43f }, { 20.0f, 2.11f, 0.95f }, { 20.0f, 35.88f, 3.5f }, { 14.0f, 35.88f, 1.3f }, { 14.0f, 23.0f, 0.15f }, { 8.02f, 23.0f, 0.0f }, { 8.02f, 35.88f, 3.53f }, { 1.99f, 35.88f, 1.32f }, { 1.99f, 2.12f, 3.4f } };
static const FxArtLoop banner24_H[] = { { 12, banner24_H_0 } };
static const FxArtLayer banner24_H_fill[] = { { 3, 1, banner24_H } };

/* the 24 x 40 letter 'I' ($15:$A506): 1 loop, 12 corners (4 sharp), radii 3, 4.9, 5; 21 off */
static const FxArtCorner banner24_I_0[] = { { 19.0f, 1.99f, 3.0f }, { 19.0f, 10.0f, 4.99f }, { 14.0f, 9.99f, 0.0f }, { 14.0f, 28.0f, 0.0f }, { 18.99f, 28.0f, 3.0f }, { 18.99f, 36.0f, 5.0f }, { 3.01f, 36.0f, 3.0f }, { 3.01f, 27.92f, 4.92f }, { 8.0f, 28.0f, 0.0f }, { 8.0f, 10.0f, 0.0f }, { 3.01f, 10.0f, 3.0f }, { 3.01f, 1.99f, 5.0f } };
static const FxArtLoop banner24_I[] = { { 12, banner24_I_0 } };
static const FxArtLayer banner24_I_fill[] = { { 3, 1, banner24_I } };

/* the 24 x 40 letter 'K' ($15:$A50C): 1 loop, 13 corners (3 sharp), radii 0.5, 0.9, 1.1, 1.3, 1.6, 2, 3.5, 3.7, 3.9; 26 off, built by hand */
static const FxArtCorner banner24_K_0[] = { { 1.99f, 2.09f, 3.46f }, { 8.0f, 2.09f, 1.15f }, { 8.0f, 15.52f, 0.0f }, { 14.68f, 2.05f, 0.49f }, { 20.14f, 2.05f, 3.72f }, { 20.14f, 6.24f, 2.0f }, { 13.74f, 19.0f, 1.65f }, { 20.17f, 31.82f, 0.86f }, { 20.17f, 35.92f, 3.9f }, { 14.69f, 35.92f, 0.0f }, { 8.02f, 22.51f, 0.0f }, { 8.02f, 35.88f, 3.52f }, { 1.99f, 35.88f, 1.32f } };
static const FxArtLoop banner24_K[] = { { 13, banner24_K_0 } };
static const FxArtLayer banner24_K_fill[] = { { 3, 1, banner24_K } };

/* the 24 x 40 letter 'L' ($15:$A512): 1 loop, 6 corners (1 sharp), radii 0.9, 2.6, 3.4, 4.8, 5.1; 15 off */
static const FxArtCorner banner24_L_0[] = { { 8.0f, 2.12f, 0.92f }, { 8.0f, 28.0f, 0.0f }, { 19.86f, 28.0f, 2.62f }, { 19.86f, 36.0f, 4.8f }, { 1.98f, 36.0f, 5.05f }, { 1.98f, 2.12f, 3.42f } };
static const FxArtLoop banner24_L[] = { { 6, banner24_L_0 } };
static const FxArtLayer banner24_L_fill[] = { { 3, 1, banner24_L } };

/* the 24 x 40 letter 'M' ($15:$A518): 1 loop, 13 corners (4 sharp), radii 1.1, 1.3, 3, 3.1, 3.5; 13 off */
static const FxArtCorner banner24_M_0[] = { { 7.73f, 1.95f, 3.0f }, { 11.0f, 8.5f, 0.0f }, { 14.27f, 1.96f, 2.99f }, { 20.01f, 1.96f, 3.03f }, { 20.01f, 35.87f, 3.49f }, { 14.0f, 35.87f, 1.27f }, { 14.0f, 15.62f, 0.0f }, { 12.54f, 15.62f, 0.0f }, { 13.3f, 17.24f, 1.14f }, { 8.0f, 17.24f, 0.0f }, { 8.0f, 35.87f, 3.48f }, { 2.0f, 35.87f, 1.27f }, { 2.0f, 1.95f, 3.13f } };
static const FxArtLoop banner24_M[] = { { 13, banner24_M_0 } };
static const FxArtLayer banner24_M_fill[] = { { 3, 1, banner24_M } };

/* the 24 x 40 letter 'N' ($15:$AA00): 1 loop, 10 corners (0 sharp), radii 0.2, 0.9, 1.3, 3.1, 3.3, 3.4, 3.5; 14 off */
static const FxArtCorner banner24_N_0[] = { { 8.31f, 2.08f, 3.31f }, { 13.99f, 18.95f, 0.18f }, { 13.99f, 2.11f, 3.4f }, { 20.01f, 2.11f, 0.95f }, { 20.01f, 35.9f, 3.54f }, { 13.78f, 35.9f, 3.12f }, { 8.0f, 18.98f, 0.21f }, { 8.0f, 35.88f, 3.49f }, { 1.99f, 35.88f, 1.31f }, { 1.99f, 2.08f, 3.48f } };
static const FxArtLoop banner24_N[] = { { 10, banner24_N_0 } };
static const FxArtLayer banner24_N_fill[] = { { 3, 1, banner24_N } };

/* the 24 x 40 letter 'O' ($15:$AA06): 2 loops, 8 corners (0 sharp), radii 1.6, 8.3, 8.4; 8 off */
static const FxArtCorner banner24_O_0[] = { { 20.01f, 1.92f, 8.33f }, { 20.01f, 36.05f, 8.4f }, { 1.98f, 36.05f, 8.38f }, { 1.98f, 1.92f, 8.4f } };
static const FxArtCorner banner24_O_1[] = { { 8.0f, 28.0f, 1.58f }, { 14.0f, 28.0f, 1.56f }, { 14.0f, 10.0f, 1.58f }, { 8.0f, 10.0f, 1.58f } };
static const FxArtLoop banner24_O[] = { { 4, banner24_O_0 }, { 4, banner24_O_1 } };
static const FxArtLayer banner24_O_fill[] = { { 3, 2, banner24_O } };

/* the 24 x 40 letter 'P' ($15:$AA0C): 2 loops, 10 corners (1 sharp), radii 0, 1.3, 1.6, 3.5, 3.7, 8.2, 8.4; 12 off */
static const FxArtCorner banner24_P_0[] = { { 20.02f, 2.0f, 8.19f }, { 20.02f, 23.02f, 8.36f }, { 8.02f, 23.02f, 0.0f }, { 8.02f, 35.88f, 3.52f }, { 1.99f, 35.88f, 1.32f }, { 1.99f, 2.0f, 3.66f } };
static const FxArtCorner banner24_P_1[] = { { 8.0f, 15.0f, 0.03f }, { 14.01f, 15.0f, 1.57f }, { 14.01f, 10.0f, 1.59f }, { 8.0f, 10.0f, 0.03f } };
static const FxArtLoop banner24_P[] = { { 6, banner24_P_0 }, { 4, banner24_P_1 } };
static const FxArtLayer banner24_P_fill[] = { { 3, 2, banner24_P } };

/* the 24 x 40 letter 'R' ($15:$AA12): 2 loops, 15 corners (3 sharp), radii 0, 1.3, 1.6, 3.1, 3.5, 3.7, 7.4, 8.3; 8 off */
static const FxArtCorner banner24_R_0[] = { { 20.03f, 2.0f, 8.31f }, { 20.03f, 16.37f, 7.45f }, { 16.94f, 19.0f, 0.0f }, { 20.03f, 21.63f, 7.45f }, { 20.03f, 35.89f, 3.55f }, { 14.0f, 35.89f, 1.3f }, { 14.0f, 22.99f, 3.13f }, { 8.02f, 22.99f, 0.04f }, { 8.02f, 35.88f, 3.53f }, { 1.99f, 35.88f, 1.32f }, { 1.99f, 2.0f, 3.65f } };
static const FxArtCorner banner24_R_1[] = { { 8.0f, 15.0f, 0.0f }, { 14.01f, 15.0f, 1.57f }, { 14.01f, 10.0f, 1.59f }, { 8.0f, 10.0f, 0.0f } };
static const FxArtLoop banner24_R[] = { { 11, banner24_R_0 }, { 4, banner24_R_1 } };
static const FxArtLayer banner24_R_fill[] = { { 3, 2, banner24_R } };

/* the 24 x 40 letter 'S' ($15:$AA18): 1 loop, 12 corners (0 sharp), radii 1.5, 2.7, 3, 4.9, 8.3, 8.4; 17 off */
static const FxArtCorner banner24_S_0[] = { { 19.87f, 1.98f, 2.68f }, { 19.87f, 10.03f, 4.88f }, { 8.0f, 10.03f, 1.51f }, { 8.0f, 14.97f, 1.52f }, { 20.03f, 14.97f, 8.36f }, { 20.03f, 36.02f, 8.37f }, { 2.08f, 36.02f, 2.95f }, { 2.08f, 27.96f, 4.92f }, { 14.0f, 27.96f, 1.49f }, { 14.0f, 23.02f, 1.53f }, { 1.96f, 23.02f, 8.39f }, { 1.96f, 1.98f, 8.33f } };
static const FxArtLoop banner24_S[] = { { 12, banner24_S_0 } };
static const FxArtLayer banner24_S_fill[] = { { 3, 1, banner24_S } };

/* the 24 x 40 letter 'T' ($15:$B000): 1 loop, 8 corners (1 sharp), radii 0.7, 1.3, 2.8, 2.9, 3.5, 4.9, 5; 9 off */
static const FxArtCorner banner24_T_0[] = { { 20.89f, 1.98f, 2.75f }, { 20.89f, 10.08f, 5.0f }, { 14.01f, 10.08f, 0.0f }, { 14.01f, 35.88f, 3.5f }, { 8.0f, 35.88f, 1.3f }, { 8.0f, 9.99f, 0.72f }, { 1.09f, 9.99f, 2.91f }, { 1.09f, 1.98f, 4.88f } };
static const FxArtLoop banner24_T[] = { { 8, banner24_T_0 } };
static const FxArtLayer banner24_T_fill[] = { { 3, 1, banner24_T } };

/* the 24 x 40 letter 'U' ($15:$B006): 1 loop, 8 corners (0 sharp), radii 0.9, 1.5, 1.6, 3.4, 8.4; 9 off */
static const FxArtCorner banner24_U_0[] = { { 8.0f, 2.12f, 0.9f }, { 8.0f, 28.0f, 1.58f }, { 13.99f, 28.0f, 1.54f }, { 13.99f, 2.12f, 3.4f }, { 20.0f, 2.12f, 0.93f }, { 20.0f, 36.05f, 8.38f }, { 1.98f, 36.05f, 8.38f }, { 1.98f, 2.12f, 3.42f } };
static const FxArtLoop banner24_U[] = { { 8, banner24_U_0 } };
static const FxArtLayer banner24_U_fill[] = { { 3, 1, banner24_U } };

/* the 24 x 40 letter 'V' ($15:$B00C): 1 loop, 16 corners (3 sharp), radii 0.5, 1, 1.2, 1.8, 2.7, 3.4, 3.5, 12.1, 14.7, 15; 19 off */
static const FxArtCorner banner24_V_0[] = { { 7.99f, 2.1f, 1.01f }, { 7.99f, 21.48f, 15.0f }, { 11.0f, 27.5f, 0.0f }, { 13.99f, 21.51f, 14.7f }, { 13.99f, 2.1f, 3.43f }, { 20.0f, 2.1f, 1.02f }, { 20.0f, 21.49f, 1.76f }, { 18.96f, 21.95f, 0.0f }, { 19.04f, 22.08f, 0.49f }, { 19.04f, 25.03f, 12.06f }, { 13.73f, 36.01f, 2.68f }, { 8.27f, 36.01f, 2.74f }, { 2.95f, 25.0f, 12.12f }, { 2.95f, 21.76f, 0.0f }, { 1.99f, 21.58f, 1.19f }, { 1.99f, 2.1f, 3.45f } };
static const FxArtLoop banner24_V[] = { { 16, banner24_V_0 } };
static const FxArtLayer banner24_V_fill[] = { { 3, 1, banner24_V } };

/* ---- the finishing places' 24 x 32 glyphs (bank $04 from $8000) ---- */

/* the places' 24 x 32 glyph '1' ($04:$8000): 1 loop, 10 corners (3 sharp), radii 2.9, 3, 3.3; 0 off, built by hand */
static const FxArtCorner places_1_0[] = { { 9.0f, 0.98f, 3.02f }, { 20.37f, 0.98f, 3.3f }, { 12.97f, 23.02f, 0.0f }, { 16.95f, 23.02f, 3.0f }, { 16.95f, 28.99f, 2.93f }, { 3.0f, 28.99f, 3.02f }, { 3.0f, 23.0f, 2.98f }, { 7.64f, 23.0f, 0.0f }, { 13.02f, 6.97f, 0.0f }, { 9.0f, 6.97f, 2.96f } };
static const FxArtLoop places_1[] = { { 10, places_1_0 } };
static const FxArtLayer places_1_fill[] = { { 3, 1, places_1 } };

/* the places' 24 x 32 glyph '2' ($04:$8006): 1 loop, 11 corners (0 sharp), radii 0.2, 1.4, 2.9, 3, 5, 6.4, 7.1, 12.3; 13 off */
static const FxArtCorner places_2_0[] = { { 19.58f, 7.01f, 1.42f }, { 6.0f, 7.01f, 3.04f }, { 6.0f, 1.0f, 2.97f }, { 20.77f, 1.0f, 6.36f }, { 20.77f, 12.53f, 7.12f }, { 6.94f, 18.64f, 5.02f }, { 6.94f, 23.0f, 0.25f }, { 16.98f, 23.0f, 3.0f }, { 16.98f, 29.0f, 3.01f }, { 1.09f, 29.0f, 2.93f }, { 1.09f, 16.29f, 12.31f } };
static const FxArtLoop places_2[] = { { 11, places_2_0 } };
static const FxArtLayer places_2_fill[] = { { 3, 1, places_2 } };

/* the places' 24 x 32 glyph '3' ($04:$800C): 1 loop, 18 corners (3 sharp), radii 1.3, 1.5, 1.9, 3, 6.6, 8.1, 23.7; 20 off */
static const FxArtCorner places_3_0[] = { { 12.99f, 22.99f, 3.01f }, { 12.99f, 18.0f, 1.51f }, { 4.99f, 18.0f, 3.0f }, { 4.99f, 11.99f, 3.0f }, { 14.99f, 11.99f, 3.01f }, { 14.99f, 7.0f, 1.5f }, { 6.03f, 7.0f, 3.0f }, { 6.03f, 0.89f, 3.05f }, { 20.69f, 0.89f, 6.57f }, { 20.69f, 14.71f, 1.34f }, { 18.21f, 10.58f, 0.0f }, { 18.2f, 10.58f, 0.0f }, { 18.2f, 14.4f, 23.74f }, { 17.01f, 18.03f, 0.0f }, { 17.97f, 18.57f, 1.89f }, { 17.97f, 28.98f, 8.05f }, { 0.99f, 28.98f, 2.99f }, { 0.99f, 22.99f, 2.99f } };
static const FxArtLoop places_3[] = { { 18, places_3_0 } };
static const FxArtLayer places_3_fill[] = { { 3, 1, places_3 } };

/* the places' 24 x 32 glyph 't' ($04:$8012): 1 loop, 16 corners (5 sharp), radii 0.8, 1.2, 2.4, 2.5, 3.5; 21 off, built by hand */
static const FxArtCorner places_t_0[] = { { 9.43f, 3.22f, 2.51f }, { 14.78f, 3.22f, 2.43f }, { 13.18f, 8.01f, 0.0f }, { 16.81f, 8.01f, 2.5f }, { 16.81f, 13.0f, 2.49f }, { 11.04f, 13.0f, 0.0f }, { 7.32f, 23.98f, 0.0f }, { 10.83f, 23.98f, 2.48f }, { 10.83f, 28.86f, 2.4f }, { 1.43f, 28.86f, 3.52f }, { 1.43f, 23.9f, 1.21f }, { 3.29f, 23.9f, 0.83f }, { 6.03f, 12.96f, 0.0f }, { 3.16f, 12.96f, 2.46f }, { 3.16f, 8.0f, 2.49f }, { 7.94f, 8.0f, 0.0f } };
static const FxArtLoop places_t[] = { { 16, places_t_0 } };
static const FxArtLayer places_t_fill[] = { { 3, 1, places_t } };

/* the places' 24 x 32 glyph 'd' ($04:$8018): 2 loops, 18 corners (5 sharp), radii 0.1, 0.5, 1.5, 2.5, 4.8, 5, 6, 6.9, 7.2, 19.4, 21.5; 33 off */
static const FxArtCorner places_d_0[] = { { 7.54f, 10.99f, 7.16f }, { 14.52f, 10.99f, 0.0f }, { 14.5f, 11.0f, 0.0f }, { 11.62f, 4.24f, 4.85f }, { 19.55f, 4.24f, 0.5f }, { 19.55f, 11.94f, 21.52f }, { 15.21f, 17.69f, 0.0f }, { 15.22f, 17.7f, 0.1f }, { 15.22f, 21.57f, 19.43f }, { 13.94f, 24.92f, 0.0f }, { 16.72f, 20.87f, 1.52f }, { 16.72f, 29.01f, 2.52f }, { 1.95f, 29.01f, 6.95f }, { 1.95f, 17.69f, 5.99f } };
static const FxArtCorner places_d_1[] = { { 5.91f, 24.0f, 1.51f }, { 9.57f, 24.0f, 2.48f }, { 12.0f, 15.91f, 0.0f }, { 5.91f, 15.91f, 5.01f } };
static const FxArtLoop places_d[] = { { 14, places_d_0 }, { 4, places_d_1 } };
static const FxArtLayer places_d_fill[] = { { 3, 2, places_d } };

/* the places' 24 x 32 glyph 's' ($04:$8400): 1 loop, 10 corners (0 sharp), radii 0.5, 1, 2.3, 2.5, 2.6, 4, 5.1, 6; 9 off */
static const FxArtCorner places_s_0[] = { { 10.64f, 24.03f, 0.49f }, { 4.28f, 16.79f, 4.0f }, { 4.28f, 9.04f, 6.0f }, { 18.78f, 9.04f, 2.57f }, { 18.78f, 13.97f, 2.34f }, { 7.83f, 13.97f, 1.01f }, { 15.47f, 21.44f, 4.0f }, { 15.47f, 28.87f, 5.12f }, { 1.18f, 28.87f, 2.5f }, { 1.18f, 24.03f, 2.35f } };
static const FxArtLoop places_s[] = { { 10, places_s_0 } };
static const FxArtLayer places_s_fill[] = { { 3, 1, places_s } };

/* the places' 24 x 32 glyph 'n' ($04:$8406): 1 loop, 15 corners (3 sharp), radii 0, 1, 1.5, 1.6, 1.9, 2.4, 2.5, 2.7, 2.8, 4; 16 off, built by hand */
static const FxArtCorner places_n_0[] = { { 2.2f, 9.99f, 2.48f }, { 7.31f, 9.99f, 2.37f }, { 8.55f, 11.79f, 0.0f }, { 9.68f, 10.07f, 1.45f }, { 21.14f, 10.07f, 3.96f }, { 16.31f, 23.99f, 0.01f }, { 18.79f, 23.99f, 0.0f }, { 21.27f, 28.96f, 2.42f }, { 9.93f, 28.96f, 2.84f }, { 14.72f, 15.0f, 1.64f }, { 10.05f, 15.0f, 1.0f }, { 5.27f, 28.85f, 2.66f }, { 0.67f, 28.85f, 1.93f }, { 5.33f, 14.98f, 0.0f }, { 2.2f, 14.98f, 2.51f } };
static const FxArtLoop places_n[] = { { 15, places_n_0 } };
static const FxArtLayer places_n_fill[] = { { 3, 1, places_n } };

/* the places' 24 x 32 glyph 'r' ($04:$840C): 1 loop, 14 corners (3 sharp), radii 0, 0.5, 2, 2.2, 2.8, 3.5, 8.2; 6 off */
static const FxArtCorner places_r_0[] = { { 7.72f, 9.0f, 3.5f }, { 11.49f, 9.0f, 0.01f }, { 11.49f, 11.29f, 0.5f }, { 10.78f, 10.96f, 0.0f }, { 10.83f, 10.88f, 0.03f }, { 10.83f, 11.53f, 0.0f }, { 13.52f, 11.53f, 0.0f }, { 14.28f, 9.9f, 2.83f }, { 19.76f, 9.9f, 2.83f }, { 19.76f, 14.92f, 2.19f }, { 12.05f, 14.92f, 8.22f }, { 5.83f, 28.78f, 2.0f }, { 2.09f, 28.78f, 2.23f }, { 2.09f, 25.54f, 0.02f } };
static const FxArtLoop places_r[] = { { 14, places_r_0 } };
static const FxArtLayer places_r_fill[] = { { 3, 1, places_r } };

const FxArtPicture fx_art_pictures[] = {
    { "the 16 x 32 glyph '0'", 0x15, 0x8000, 16, 32, FX_ART_OUTLINED, 1, 1, big16_0_fill, 0 },
    { "the 16 x 32 glyph '1'", 0x15, 0x8004, 16, 32, FX_ART_OUTLINED, 1, 1, big16_1_fill, 1 },
    { "the 16 x 32 glyph '2'", 0x15, 0x8008, 16, 32, FX_ART_OUTLINED, 1, 1, big16_2_fill, 0 },
    { "the 16 x 32 glyph '3'", 0x15, 0x800C, 16, 32, FX_ART_OUTLINED, 1, 1, big16_3_fill, 2 },
    { "the 16 x 32 glyph '4'", 0x15, 0x8010, 16, 32, FX_ART_OUTLINED, 1, 1, big16_4_fill, 1 },
    { "the 16 x 32 glyph '5'", 0x15, 0x8014, 16, 32, FX_ART_OUTLINED, 1, 1, big16_5_fill, 0 },
    { "the 16 x 32 glyph '6'", 0x15, 0x8018, 16, 32, FX_ART_OUTLINED, 1, 1, big16_6_fill, 0 },
    { "the 16 x 32 glyph '7'", 0x15, 0x801C, 16, 32, FX_ART_OUTLINED, 1, 1, big16_7_fill, 0 },
    { "the 16 x 32 glyph '8'", 0x15, 0x8400, 16, 32, FX_ART_OUTLINED, 1, 1, big16_8_fill, 3 },
    { "the 16 x 32 glyph '9'", 0x15, 0x8404, 16, 32, FX_ART_OUTLINED, 1, 1, big16_9_fill, 1 },
    { "the 16 x 32 glyph 'A'", 0x15, 0x8408, 16, 32, FX_ART_OUTLINED, 1, 1, big16_A_fill, 0 },
    { "the 16 x 32 glyph 'B'", 0x15, 0x840C, 16, 32, FX_ART_OUTLINED, 1, 1, big16_B_fill, 4 },
    { "the 16 x 32 glyph 'C'", 0x15, 0x8410, 16, 32, FX_ART_OUTLINED, 1, 1, big16_C_fill, 0 },
    { "the 16 x 32 glyph 'D'", 0x15, 0x8414, 16, 32, FX_ART_OUTLINED, 1, 1, big16_D_fill, 0 },
    { "the 16 x 32 glyph 'E'", 0x15, 0x8418, 16, 32, FX_ART_OUTLINED, 1, 1, big16_E_fill, 0 },
    { "the 16 x 32 glyph 'F'", 0x15, 0x841C, 16, 32, FX_ART_OUTLINED, 1, 1, big16_F_fill, 0 },
    { "the 16 x 32 glyph 'G'", 0x15, 0x8800, 16, 32, FX_ART_OUTLINED, 1, 1, big16_G_fill, 1 },
    { "the 16 x 32 glyph 'H'", 0x15, 0x8804, 16, 32, FX_ART_OUTLINED, 1, 1, big16_H_fill, 0 },
    { "the 16 x 32 glyph 'I'", 0x15, 0x8808, 16, 32, FX_ART_OUTLINED, 1, 1, big16_I_fill, 0 },
    { "the 16 x 32 glyph 'L'", 0x15, 0x8814, 16, 32, FX_ART_OUTLINED, 1, 1, big16_L_fill, 0 },
    { "the 16 x 32 glyph 'M'", 0x15, 0x8818, 16, 32, FX_ART_OUTLINED, 1, 1, big16_M_fill, 1 },
    { "the 16 x 32 glyph 'N'", 0x15, 0x881C, 16, 32, FX_ART_OUTLINED, 1, 1, big16_N_fill, 6 },
    { "the 16 x 32 glyph 'O'", 0x15, 0x8C00, 16, 32, FX_ART_OUTLINED, 1, 1, big16_O_fill, 0 },
    { "the 16 x 32 glyph 'P'", 0x15, 0x8C04, 16, 32, FX_ART_OUTLINED, 1, 1, big16_P_fill, 0 },
    { "the 16 x 32 glyph 'R'", 0x15, 0x8C0C, 16, 32, FX_ART_OUTLINED, 1, 1, big16_R_fill, 1 },
    { "the 16 x 32 glyph 'S'", 0x15, 0x8C10, 16, 32, FX_ART_OUTLINED, 1, 1, big16_S_fill, 0 },
    { "the 16 x 32 glyph 'T'", 0x15, 0x8C14, 16, 32, FX_ART_OUTLINED, 1, 1, big16_T_fill, 0 },
    { "the 16 x 32 glyph 'U'", 0x15, 0x8C18, 16, 32, FX_ART_OUTLINED, 1, 1, big16_U_fill, 0 },
    { "the 16 x 32 glyph 'V'", 0x15, 0x8C1C, 16, 32, FX_ART_OUTLINED, 1, 1, big16_V_fill, 5 },
    { "the 16 x 32 glyph 'W'", 0x15, 0x9000, 16, 32, FX_ART_OUTLINED, 1, 1, big16_W_fill, 0 },
    { "the 16 x 32 glyph 'X'", 0x15, 0x9004, 16, 32, FX_ART_OUTLINED, 1, 1, big16_X_fill, 3 },
    { "the 16 x 32 glyph 'Y'", 0x15, 0x9008, 16, 32, FX_ART_OUTLINED, 1, 1, big16_Y_fill, 9 },
    { "the 16 x 32 glyph 'Z'", 0x15, 0x900C, 16, 32, FX_ART_OUTLINED, 1, 1, big16_Z_fill, 1 },
    { "the 16 x 32 glyph '.'", 0x15, 0x9010, 16, 32, FX_ART_OUTLINED, 1, 1, big16_dot_fill, 0 },
    { "the 16 x 32 glyph '''", 0x15, 0x9014, 16, 32, FX_ART_OUTLINED, 1, 1, big16_quote_fill, 0 },
    { "the 16 x 32 glyph '\"'", 0x15, 0x9018, 16, 32, FX_ART_OUTLINED, 1, 1, big16_quotes_fill, 8 },
    { "the 16 x 32 glyph '!'", 0x15, 0x901C, 16, 32, FX_ART_OUTLINED, 1, 1, big16_bang_fill, 0 },
    { "the 8 x 16 glyph '0'", 0x15, 0x9400, 8, 16, FX_ART_BEVELLED, 0, 1, small8_0_fill, 0 },
    { "the 8 x 16 glyph '1'", 0x15, 0x9402, 8, 16, FX_ART_BEVELLED, 0, 1, small8_1_fill, 0 },
    { "the 8 x 16 glyph '2'", 0x15, 0x9404, 8, 16, FX_ART_BEVELLED, 0, 1, small8_2_fill, 0 },
    { "the 8 x 16 glyph '3'", 0x15, 0x9406, 8, 16, FX_ART_BEVELLED, 0, 1, small8_3_fill, 0 },
    { "the 8 x 16 glyph '4'", 0x15, 0x9408, 8, 16, FX_ART_BEVELLED, 0, 1, small8_4_fill, 11 },
    { "the 8 x 16 glyph '5'", 0x15, 0x940A, 8, 16, FX_ART_BEVELLED, 0, 1, small8_5_fill, 0 },
    { "the 8 x 16 glyph '6'", 0x15, 0x940C, 8, 16, FX_ART_BEVELLED, 0, 1, small8_6_fill, 0 },
    { "the 8 x 16 glyph '7'", 0x15, 0x940E, 8, 16, FX_ART_BEVELLED, 0, 1, small8_7_fill, 13 },
    { "the 8 x 16 glyph '8'", 0x15, 0x9410, 8, 16, FX_ART_BEVELLED, 0, 1, small8_8_fill, 0 },
    { "the 8 x 16 glyph '9'", 0x15, 0x9412, 8, 16, FX_ART_BEVELLED, 0, 1, small8_9_fill, 0 },
    { "the 8 x 16 glyph 'A'", 0x15, 0x9414, 8, 16, FX_ART_BEVELLED, 0, 1, small8_A_fill, 0 },
    { "the 8 x 16 glyph 'B'", 0x15, 0x9416, 8, 16, FX_ART_BEVELLED, 0, 1, small8_B_fill, 5 },
    { "the 8 x 16 glyph 'C'", 0x15, 0x9418, 8, 16, FX_ART_BEVELLED, 0, 1, small8_C_fill, 2 },
    { "the 8 x 16 glyph 'D'", 0x15, 0x941A, 8, 16, FX_ART_BEVELLED, 0, 1, small8_D_fill, 0 },
    { "the 8 x 16 glyph 'E'", 0x15, 0x941C, 8, 16, FX_ART_BEVELLED, 0, 1, small8_E_fill, 0 },
    { "the 8 x 16 glyph 'F'", 0x15, 0x941E, 8, 16, FX_ART_BEVELLED, 0, 1, small8_F_fill, 0 },
    { "the 8 x 16 glyph 'G'", 0x15, 0x9600, 8, 16, FX_ART_BEVELLED, 0, 1, small8_G_fill, 2 },
    { "the 8 x 16 glyph 'H'", 0x15, 0x9602, 8, 16, FX_ART_BEVELLED, 0, 1, small8_H_fill, 0 },
    { "the 8 x 16 glyph 'I'", 0x15, 0x9604, 8, 16, FX_ART_BEVELLED, 0, 1, small8_I_fill, 0 },
    { "the 8 x 16 glyph 'J'", 0x15, 0x9606, 8, 16, FX_ART_BEVELLED, 0, 1, small8_J_fill, 0 },
    { "the 8 x 16 glyph 'K'", 0x15, 0x9608, 8, 16, FX_ART_BEVELLED, 0, 1, small8_K_fill, 8 },
    { "the 8 x 16 glyph 'L'", 0x15, 0x960A, 8, 16, FX_ART_BEVELLED, 0, 1, small8_L_fill, 0 },
    { "the 8 x 16 glyph 'M'", 0x15, 0x960C, 8, 16, FX_ART_BEVELLED, 0, 1, small8_M_fill, 10 },
    { "the 8 x 16 glyph 'N'", 0x15, 0x960E, 8, 16, FX_ART_BEVELLED, 0, 1, small8_N_fill, 4 },
    { "the 8 x 16 glyph 'O'", 0x15, 0x9610, 8, 16, FX_ART_BEVELLED, 0, 1, small8_O_fill, 0 },
    { "the 8 x 16 glyph 'P'", 0x15, 0x9612, 8, 16, FX_ART_BEVELLED, 0, 1, small8_P_fill, 0 },
    { "the 8 x 16 glyph 'Q'", 0x15, 0x9614, 8, 16, FX_ART_BEVELLED, 0, 1, small8_Q_fill, 3 },
    { "the 8 x 16 glyph 'R'", 0x15, 0x9616, 8, 16, FX_ART_BEVELLED, 0, 1, small8_R_fill, 0 },
    { "the 8 x 16 glyph 'S'", 0x15, 0x9618, 8, 16, FX_ART_BEVELLED, 0, 1, small8_S_fill, 0 },
    { "the 8 x 16 glyph 'T'", 0x15, 0x961A, 8, 16, FX_ART_BEVELLED, 0, 1, small8_T_fill, 4 },
    { "the 8 x 16 glyph 'U'", 0x15, 0x961C, 8, 16, FX_ART_BEVELLED, 0, 1, small8_U_fill, 0 },
    { "the 8 x 16 glyph 'V'", 0x15, 0x961E, 8, 16, FX_ART_BEVELLED, 0, 1, small8_V_fill, 3 },
    { "the 8 x 16 glyph 'W'", 0x15, 0x9800, 8, 16, FX_ART_BEVELLED, 0, 1, small8_W_fill, 11 },
    { "the 8 x 16 glyph 'X'", 0x15, 0x9802, 8, 16, FX_ART_BEVELLED, 0, 1, small8_X_fill, 11 },
    { "the 8 x 16 glyph 'Y'", 0x15, 0x9804, 8, 16, FX_ART_BEVELLED, 0, 1, small8_Y_fill, 5 },
    { "the 8 x 16 glyph 'Z'", 0x15, 0x9806, 8, 16, FX_ART_BEVELLED, 0, 1, small8_Z_fill, 6 },
    { "the 8 x 16 glyph '.'", 0x15, 0x9808, 8, 16, FX_ART_BEVELLED, 0, 1, small8_dot_fill, 0 },
    { "the 8 x 16 glyph '''", 0x15, 0x980A, 8, 16, FX_ART_BEVELLED, 0, 1, small8_quote_fill, 6 },
    { "the 8 x 16 glyph '\"'", 0x15, 0x980C, 8, 16, FX_ART_BEVELLED, 0, 1, small8_quotes_fill, 11 },
    { "the 24 x 40 letter 'A'", 0x15, 0xA000, 24, 40, FX_ART_OUTLINED, 2, 1, banner24_A_fill, 10 },
    { "the 24 x 40 letter 'C'", 0x15, 0xA006, 24, 40, FX_ART_OUTLINED, 2, 1, banner24_C_fill, 12 },
    { "the 24 x 40 letter 'E'", 0x15, 0xA00C, 24, 40, FX_ART_OUTLINED, 2, 1, banner24_E_fill, 24 },
    { "the 24 x 40 letter 'F'", 0x15, 0xA012, 24, 40, FX_ART_OUTLINED, 2, 1, banner24_F_fill, 16 },
    { "the 24 x 40 letter 'G'", 0x15, 0xA018, 24, 40, FX_ART_OUTLINED, 2, 1, banner24_G_fill, 13 },
    { "the 24 x 40 letter 'H'", 0x15, 0xA500, 24, 40, FX_ART_OUTLINED, 2, 1, banner24_H_fill, 11 },
    { "the 24 x 40 letter 'I'", 0x15, 0xA506, 24, 40, FX_ART_OUTLINED, 2, 1, banner24_I_fill, 21 },
    { "the 24 x 40 letter 'K'", 0x15, 0xA50C, 24, 40, FX_ART_OUTLINED, 2, 1, banner24_K_fill, 26 },
    { "the 24 x 40 letter 'L'", 0x15, 0xA512, 24, 40, FX_ART_OUTLINED, 2, 1, banner24_L_fill, 15 },
    { "the 24 x 40 letter 'M'", 0x15, 0xA518, 24, 40, FX_ART_OUTLINED, 2, 1, banner24_M_fill, 13 },
    { "the 24 x 40 letter 'N'", 0x15, 0xAA00, 24, 40, FX_ART_OUTLINED, 2, 1, banner24_N_fill, 14 },
    { "the 24 x 40 letter 'O'", 0x15, 0xAA06, 24, 40, FX_ART_OUTLINED, 2, 1, banner24_O_fill, 8 },
    { "the 24 x 40 letter 'P'", 0x15, 0xAA0C, 24, 40, FX_ART_OUTLINED, 2, 1, banner24_P_fill, 12 },
    { "the 24 x 40 letter 'R'", 0x15, 0xAA12, 24, 40, FX_ART_OUTLINED, 2, 1, banner24_R_fill, 8 },
    { "the 24 x 40 letter 'S'", 0x15, 0xAA18, 24, 40, FX_ART_OUTLINED, 2, 1, banner24_S_fill, 17 },
    { "the 24 x 40 letter 'T'", 0x15, 0xB000, 24, 40, FX_ART_OUTLINED, 2, 1, banner24_T_fill, 9 },
    { "the 24 x 40 letter 'U'", 0x15, 0xB006, 24, 40, FX_ART_OUTLINED, 2, 1, banner24_U_fill, 9 },
    { "the 24 x 40 letter 'V'", 0x15, 0xB00C, 24, 40, FX_ART_OUTLINED, 2, 1, banner24_V_fill, 19 },
    { "the places' 24 x 32 glyph '1'", 0x04, 0x8000, 24, 32, FX_ART_OUTLINED, 2, 1, places_1_fill, 0 },
    { "the places' 24 x 32 glyph '2'", 0x04, 0x8006, 24, 32, FX_ART_OUTLINED, 2, 1, places_2_fill, 13 },
    { "the places' 24 x 32 glyph '3'", 0x04, 0x800C, 24, 32, FX_ART_OUTLINED, 2, 1, places_3_fill, 20 },
    { "the places' 24 x 32 glyph 't'", 0x04, 0x8012, 24, 32, FX_ART_OUTLINED, 2, 1, places_t_fill, 21 },
    { "the places' 24 x 32 glyph 'd'", 0x04, 0x8018, 24, 32, FX_ART_OUTLINED, 2, 1, places_d_fill, 33 },
    { "the places' 24 x 32 glyph 's'", 0x04, 0x8400, 24, 32, FX_ART_OUTLINED, 2, 1, places_s_fill, 9 },
    { "the places' 24 x 32 glyph 'n'", 0x04, 0x8406, 24, 32, FX_ART_OUTLINED, 2, 1, places_n_fill, 16 },
    { "the places' 24 x 32 glyph 'r'", 0x04, 0x840C, 24, 32, FX_ART_OUTLINED, 2, 1, places_r_fill, 6 },
};
const int fx_art_picture_count = (int)(sizeof fx_art_pictures / sizeof fx_art_pictures[0]);
