from fontforge import *
from sys import stderr

suff = '.otf'
f = open( 'latinmodern-math.otf' )
big = [ 'integral', 'radical', 'summation', 'product', 'parenleft', 'parenright', 'contourintegral' ]
retain = [ 'uni22C5', 'minute', 'uni2033', 'minute.st', 'uni2033.st', 'minute.sts', 'uni2033.sts' ]

for bg in big[:]:
    if not bg in f: continue
    if f[ bg ].verticalVariants is not None:
        for g in f[ bg ].verticalVariants.split(' '):
            big.append( g )
    if f[ bg ].verticalComponents is not None:
        for g in f[ bg ].verticalComponents:
            big.append( g[ 0 ] )

retain.extend( big )

print( 'cleaning up', file=stderr )

for g in f:
    if g not in retain:
        f.removeGlyph( g )

print( 'merging', file=stderr )

f.mergeFonts( "xccmi10" + suff )
f.mergeFonts( "xccsy10" + suff )
f.mergeFonts( "xccam10" + suff )
f.mergeFonts( "ccr10" + suff )
f.mergeFonts( "latinmodern-copy.otf" )

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
