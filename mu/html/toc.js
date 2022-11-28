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
        tagname = heading.tagName.toLowerCase();
        anchor = heading.parentNode;

        var link = doc.createElement( 'a' );
        link.setAttribute( 'href', '#' + anchor.getAttribute( 'name' ) );
        if ( tagname == 'h1' )
            link.setAttribute( 'onclick', 'clickTOC( event )' );
        link.textContent = heading.textContent;

        if ( tagname > level )
        {
            var ol = doc.createElement( 'ol' );
            last.appendChild( ol );
            cur = ol;
        }

        if ( tagname < level )
            cur = cur.parentNode.parentNode;

        level = tagname;

        var li = doc.createElement( 'li' );
        li.setAttribute( 'class', tagname );
        last = li;

        li.appendChild( link );
        cur.appendChild( li );
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
