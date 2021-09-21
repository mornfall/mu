use strict;
use MIME::Base64;

sub font($$)
{
    my ( $file, $family ) = @_;
    open FILE, $file or die "could not read $file";
    my ( $b64, $buf );
    while ( read( FILE, $buf, 57 ) )
    {
        $b64 .= encode_base64( $buf );
        chomp $b64;
    }
    print <<EOF
    \@font-face {
        font-family: "$family";
        src: url('data:application/octet-stream;base64,$b64');
    }
EOF
}

font( "fonts/aleo-light.otf", "Aleo Light" );
font( "fonts/aleo-regular.otf", "Aleo Regular" );
font( "fonts/lato-light.ttf", "Lato Light" );
font( "fonts/iosevka-slab-extralight.ttf", "Iosevka Slab Extralight" );
font( "fonts/iosevka-slab-light.ttf", "Iosevka Slab Light" );
