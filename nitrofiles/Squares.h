
//{{BLOCK(Squares)

//======================================================================
//
//	Squares, 256x256@8, 
//	Transparent color : FF,00,FF
//	+ palette 256 entries, not compressed
//	+ 3 tiles (t|f reduced) not compressed
//	+ regular map (in SBBs), not compressed, 32x32 
//	Total size: 512 + 192 + 2048 = 2752
//
//	Time-stamp: 2024-01-15, 15:14:52
//	Exported by Cearn's GBA Image Transmogrifier, v0.8.9
//	( http://www.coranac.com/projects/#grit )
//
//======================================================================

#ifndef GRIT_SQUARES_H
#define GRIT_SQUARES_H

#define SquaresTilesLen 192
extern const unsigned int SquaresTiles[48];

#define SquaresMapLen 2048
extern const unsigned short SquaresMap[1024];

#define SquaresPalLen 512
extern const unsigned short SquaresPal[256];

#endif // GRIT_SQUARES_H

//}}BLOCK(Squares)
