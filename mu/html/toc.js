var toc = undefined;

function makeTOC( doc )
{
    var doc = doc || document;
    toc = doc.getElementById( 'toc' );
    var headings = [].slice.call( doc.body.querySelectorAll( 'h1, h2' ) );

    var cur = toc;
    var last = undefined;
    var level = 'h1';

    headings.forEach( function ( heading, index )
    {
        name = heading.tagName.toLowerCase();

        var anchor = doc.createElement( 'a' );
        anchor.setAttribute( 'name', 'toc' + index );
        anchor.setAttribute( 'id', 'toc' + index );

        var link = doc.createElement( 'a' );
        link.setAttribute( 'href', '#toc' + index );
        if ( name == 'h1' )
            link.setAttribute( 'onclick', 'clickTOC( event )' );
        link.textContent = heading.textContent;

        if ( name > level )
        {
            var ol = doc.createElement( 'ol' );
            last.appendChild( ol );
            cur = ol;
        }

        if ( name < level )
            cur = cur.parentNode.parentNode;

        level = name;

        var li = doc.createElement( 'li' );
        li.setAttribute( 'class', name );
        last = li;

        li.appendChild( link );
        cur.appendChild( li );
        heading.parentNode.insertBefore( anchor, heading );
    } );
}

function clickTOC(e)
{
    var exp = [].slice.call( toc.querySelectorAll( '.expanded' ) );

    exp.forEach( function ( elem, idx )
    {
        elem.classList.remove( "expanded" );
    } );

    e.target.parentNode.classList.add( "expanded" );
}
