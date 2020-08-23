from fontforge import *
from sys import stderr

suff = '.otf'
f = open( 'latinmodern-math.otf' )

x = open( "xccmi10" + suff )
x.mergeFonts( "xccsy10" + suff )
x.mergeFonts( "xccam10" + suff )
x.mergeFonts( "ccr10" + suff )

big = [ 'integral', 'radical', 'summation', 'product', 'parenleft', 'parenright', 'contourintegral' ]

for bg in big[:]:
    if not bg in f: continue
    if f[ bg ].verticalVariants is not None:
        for g in f[ bg ].verticalVariants.split(' '):
            big.append( g )
    if f[ bg ].verticalComponents is not None:
        for g in f[ bg ].verticalComponents:
            big.append( g[ 0 ] )

print( 'cleaning up', file=stderr )
removed = 0

for g in x:
    if g in f:
        f.removeGlyph( g )
        removed += 1

for g in f:
    if g.startswith( 'u1D' ) or g.startswith( 'u1E' ):
        f.removeGlyph( g )
        removed += 1

print( 'removed', removed, 'glyphs', file=stderr )

print( 'merging', file=stderr )
f.mergeFonts( x )

print( 'adjusting strokes', file=stderr )

for g in big:
    if not g in f: continue
    f[ g ].changeWeight( -15 if g.count( 'v1' ) else -10 )

def squish(g):
    g.genericGlyphChange( stemType='horizontalVertical', stemHeightScale=1, stemWidthScale=0.7,
                          hCounterScale=1, vCounterType='scaled', vCounterScale=1 )

squish( f['product.v1'] )
squish( f['summation.v1'] )
f[ 'f' ].addAnchorPoint( '',  'basemark', 0, 0 )

for g in f:
    if f[ g ].anchorPoints:
        print( f[ g ].anchorPoints )

f.math.SubscriptTopMax = 300
f.math.SuperscriptBottomMin = 300
f.math.FractionNumeratorDisplayStyleShiftUp = 500
f.math.FractionNumeratorDisplayStyleGapMin = 50
f.math.FractionDenominatorDisplayStyleShiftDown = 500
f.math.FractionDenominatorDisplayStyleGapMin = 50

print( 'writing result', file=stderr )

f.familyname = 'xccl'
f.fontname = 'xccl-regular'
f.fullname = 'xccl-regular'
f.appendSFNTName( 'English (US)', 'Preferred Family', 'xccl' )
f.generate( "xccl.otf" )
